/*-------
  Module:			drvconn.c
 *
 * Description:		This module contains only routines related to
 *					implementing SQLDriverConnect.
 *
 * Classes:			n/a
 *
 * API functions:	SQLDriverConnect
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */

#include "psqlodbc.h"

#include <stdio.h>
#include <stdlib.h>

#include "connection.h"
#include "misc.h"

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif

#include <string.h>

#ifdef WIN32
#include <windowsx.h>
#include "resource.h"
#endif
#include "pgapifunc.h"

#include "dlg_specific.h"
#include "multibyte.h"

#define	NULL_IF_NULL(a) (a ? a : "(NULL)")

/* prototypes */
static BOOL dconn_get_DSN_or_Driver(const char *connect_string, ConnInfo *ci, int ccsc);
static BOOL dconn_get_connect_attributes(const char *connect_string, ConnInfo *ci, int ccsc);

#ifdef WIN32
LRESULT CALLBACK dconn_FDriverConnectProc(HWND hdlg, UINT wMsg, WPARAM wParam, LPARAM lParam);
RETCODE		dconn_DoDialog(HWND hwnd, ConnInfo *ci);

extern HINSTANCE s_hModule;	/* Saved module handle. */
#endif

#define	PASSWORD_IS_REQUIRED	1
static int
paramRequired(const ConnInfo *ci, int reqs)
{
	int	required = 0;
	const char *pw = SAFE_NAME(ci->password);

	/* Password is not necessarily a required parameter. */
	if ((reqs & PASSWORD_IS_REQUIRED) != 0)
		if ('\0' == pw[0])
			required |= PASSWORD_IS_REQUIRED;

	return required;
}

RETCODE		SQL_API
PGAPI_DriverConnect(HDBC hdbc,
					HWND hwnd,
					const SQLCHAR * szConnStrIn,
					SQLSMALLINT cbConnStrIn,
					SQLCHAR * szConnStrOut,
					SQLSMALLINT cbConnStrOutMax,
					SQLSMALLINT * pcbConnStrOut,
					SQLUSMALLINT fDriverCompletion)
{
	CSTR func = "PGAPI_DriverConnect";
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	ConnInfo   *ci;

#ifdef WIN32
	RETCODE		dialog_result;
	BOOL		didUI = FALSE;
#endif
	const char 	*lackMessage = NULL;
	RETCODE		result;
	char		*connStrIn = NULL;
	char		connStrOut[MAX_CONNECT_STRING];
	int			retval;
	char		salt[5];
	ssize_t		len = 0;
	SQLSMALLINT	lenStrout;
	int		reqs = 0;
	int		ccsc;


	MYLOG(0, "entering...\n");

	if (!conn)
	{
		CC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	connStrIn = make_string(szConnStrIn, cbConnStrIn, NULL, 0);

#ifdef	FORCE_PASSWORD_DISPLAY
	MYLOG(0, "**** fDriverCompletion=%d, connStrIn='%s'\n", fDriverCompletion, connStrIn);
#else
	if (get_mylog())
	{
		char	*hide_str = hide_password(connStrIn);

		MYLOG(0, "**** fDriverCompletion=%d, connStrIn='%s'\n", fDriverCompletion, NULL_IF_NULL(hide_str));
		if (hide_str)
			free(hide_str);
	}
#endif	/* FORCE_PASSWORD_DISPLAY */

	ci = &(conn->connInfo);

	/*
	 * The encoding of the connect string: UTF-8 when it came through
	 * the Unicode entry point, otherwise that of the locale (the same
	 * one CC_connect() sends as client_encoding).
	 */
	ccsc = CC_is_in_unicode_driver(conn) ? UTF8 : derive_locale_encoding_code();

	/* First parse the connect string and get the name of DSN or Driver */
	if (!dconn_get_DSN_or_Driver(connStrIn, ci, ccsc))
	{
		CC_set_error(conn, CONN_OPENDB_ERROR, "Connection string parse error", func);
		if (connStrIn)
			free(connStrIn);
		return SQL_ERROR;
	}
	/*
	 * If the ConnInfo in the hdbc is missing anything, this function will
	 * fill them in from the registry (assuming of course there is a DSN
	 * given -- if not, it does nothing!)
	 */
	getDSNinfo(ci, NULL);
	/* Parse the connect string and fill in conninfo for this hdbc. */
	if (!dconn_get_connect_attributes(connStrIn, ci, ccsc))
	{
		CC_set_error(conn, CONN_OPENDB_ERROR, "Connection string parse error", func);
		if (connStrIn)
			free(connStrIn);
		return SQL_ERROR;
	}
	logs_on_off(1, ci->drivers.debug, ci->drivers.commlog);
	if (connStrIn)
	{
		free(connStrIn);
		connStrIn = NULL;
	}

	/* initialize pg_version */
	CC_initialize_pg_version(conn);
	pg_memset(salt, 0, sizeof(salt));

#ifdef WIN32
dialog:
#endif
MYLOG(DETAIL_LOG_LEVEL, "DriverCompletion=%d\n", fDriverCompletion);
	switch (fDriverCompletion)
	{
#ifdef WIN32
		case SQL_DRIVER_PROMPT:
			if (NULL == hwnd)
				break;
			dialog_result = dconn_DoDialog(hwnd, ci);
			didUI = TRUE;
			if (dialog_result != SQL_SUCCESS)
				return dialog_result;
			break;

		case SQL_DRIVER_COMPLETE_REQUIRED:

			/* Fall through */

		case SQL_DRIVER_COMPLETE:

			if (NULL == hwnd)
				break;
			if (paramRequired(ci, reqs))
			{
				dialog_result = dconn_DoDialog(hwnd, ci);
				didUI = TRUE;
				if (dialog_result != SQL_SUCCESS)
					return dialog_result;
			}
			break;
#else
		case SQL_DRIVER_PROMPT:
		case SQL_DRIVER_COMPLETE:
		case SQL_DRIVER_COMPLETE_REQUIRED:
#endif
		case SQL_DRIVER_NOPROMPT:
			break;
	}

	/*
	 * Password is not a required parameter unless authentication asks for
	 * it. For now, I think it's better to just let the application ask
	 * over and over until a password is entered (the user can always hit
	 * Cancel to get out)
	 */
	if (paramRequired(ci, reqs))
	{
		/* if (didUI)
			return SQL_NO_DATA_FOUND; */
		if (!lackMessage)
			lackMessage = "Please supply password";
		CC_set_error(conn, CONN_OPENDB_ERROR, lackMessage, func);
		return SQL_ERROR;
	}
	reqs = 0;

MYLOG(DETAIL_LOG_LEVEL, "before CC_connect\n");
	/* do the actual connect */
	retval = CC_connect(conn, salt);
	if (retval < 0)
	{							/* need a password */
		if (fDriverCompletion == SQL_DRIVER_NOPROMPT)
		{
			CC_log_error(func, "Need password but Driver_NoPrompt", conn);
			return SQL_ERROR;	/* need a password but not allowed to
								 * prompt so error */
		}
		else
		{
#ifdef WIN32
			if (ci->password_required)
				reqs |= PASSWORD_IS_REQUIRED;
			if (hwnd && paramRequired(ci, reqs))
				goto dialog;
#endif /* WIN32 */
			/* Prompting for missing options is only supported on Windows. */
			return SQL_ERROR;
		}
	}
	else if (retval == 0)
	{
		/* error msg filled in above */
		CC_log_error(func, "Error from CC_Connect", conn);
		return SQL_ERROR;
	}

	/*
	 * Create the Output Connection String
	 */
	result = (1 == retval ? SQL_SUCCESS : SQL_SUCCESS_WITH_INFO);

	lenStrout = cbConnStrOutMax;
	if (conn->ms_jet && lenStrout > 255)
		lenStrout = 255;
	makeConnectString(connStrOut, ci, lenStrout, ccsc);
	len = strlen(connStrOut);

	if (szConnStrOut)
	{
		/*
		 * Return the completed string to the caller. The correct method
		 * is to only construct the connect string if a dialog was put up,
		 * otherwise, it should just copy the connection input string to
		 * the output. However, it seems ok to just always construct an
		 * output string.  There are possible bad side effects on working
		 * applications (Access) by implementing the correct behavior,
		 * anyway.
		 */
		/*strncpy_null(szConnStrOut, connStrOut, cbConnStrOutMax);*/
		strncpy((char *) szConnStrOut, connStrOut, cbConnStrOutMax);

		if (len >= cbConnStrOutMax)
		{
			int			clen;

			for (clen = cbConnStrOutMax - 1; clen >= 0 && szConnStrOut[clen] != ';'; clen--)
				szConnStrOut[clen] = '\0';
			result = SQL_SUCCESS_WITH_INFO;
			CC_set_error(conn, CONN_TRUNCATED, "The buffer was too small for the ConnStrOut.", func);
		}
	}

	if (pcbConnStrOut)
		*pcbConnStrOut = (SQLSMALLINT) len;

#ifdef	FORCE_PASSWORD_DISPLAY
	if (cbConnStrOutMax > 0)
	{
		MYLOG(0, "szConnStrOut = '%s' len=" FORMAT_SSIZE_T ",%d\n", NULL_IF_NULL((char *) szConnStrOut), len, cbConnStrOutMax);
	}
#else
	if (get_mylog())
	{
		char	*hide_str = NULL;

		if (cbConnStrOutMax > 0)
			hide_str = hide_password((char *) szConnStrOut);
		MYLOG(0, "szConnStrOut = '%s' len=" FORMAT_SSIZE_T ",%d\n", NULL_IF_NULL(hide_str), len, cbConnStrOutMax);
		if (hide_str)
			free(hide_str);
	}
#endif /* FORCE_PASSWORD_DISPLAY */

	MYLOG(0, "leaving %d\n", result);
	return result;
}


#ifdef WIN32
RETCODE
dconn_DoDialog(HWND hwnd, ConnInfo *ci)
{
	LRESULT			dialog_result;

	MYLOG(0, "entering ci = %p\n", ci);

	if (hwnd)
	{
		dialog_result = DialogBoxParam(s_hModule, MAKEINTRESOURCE(DLG_CONFIG),
				hwnd, dconn_FDriverConnectProc, (LPARAM) ci);
		if (-1 == dialog_result)
		{
			int errc = GetLastError();
			MYLOG(0, " LastError=%d\n", errc);
		}
		if (!dialog_result || (dialog_result == -1))
			return SQL_NO_DATA_FOUND;
		else
			return SQL_SUCCESS;
	}

	MYLOG(0, " No window specified\n");
	return SQL_ERROR;
}


LRESULT CALLBACK
dconn_FDriverConnectProc(
						 HWND hdlg,
						 UINT wMsg,
						 WPARAM wParam,
						 LPARAM lParam)
{
	ConnInfo   *ci;
	char	strbuf[64];

	switch (wMsg)
	{
		case WM_INITDIALOG:
			ci = (ConnInfo *) lParam;

			/* Change the caption for the setup dialog */
			SetWindowText(hdlg, "PostgreSQL Connection");

			LoadString(s_hModule, IDS_ADVANCE_CONNECTION, strbuf, sizeof(strbuf));
			SetWindowText(GetDlgItem(hdlg, IDC_DATASOURCE), strbuf);

			/* Hide the DSN and description fields */
			ShowWindow(GetDlgItem(hdlg, IDC_DSNAMETEXT), SW_HIDE);
			ShowWindow(GetDlgItem(hdlg, IDC_DSNAME), SW_HIDE);
			ShowWindow(GetDlgItem(hdlg, IDC_DESCTEXT), SW_HIDE);
			ShowWindow(GetDlgItem(hdlg, IDC_DESC), SW_HIDE);
			ShowWindow(GetDlgItem(hdlg, IDC_DRIVER), SW_HIDE);
			ShowWindow(GetDlgItem(hdlg, IDC_TEST), SW_HIDE);
			ShowWindow(GetDlgItem(hdlg, IDC_MANAGEDSN), SW_HIDE);
			// ShowWindow(GetDlgItem(hdlg, IDC_DATASOURCE), SW_HIDE);
			if ('\0' != ci->server[0])
				EnableWindow(GetDlgItem(hdlg, IDC_SERVER), FALSE);
			if ('\0' != ci->port[0])
				EnableWindow(GetDlgItem(hdlg, IDC_PORT), FALSE);

			SetWindowLongPtr(hdlg, DWLP_USER, lParam);					/* Save the ConnInfo for the "OK" */
			SetDlgStuff(hdlg, ci);

			if (ci->password_required)
			{
				HWND notu = GetDlgItem(hdlg, IDC_NOTICE_USER);

				SetWindowText(notu, "  Supply password       ");
				ShowWindow(notu, SW_SHOW);
				SendMessage(notu, WM_CTLCOLOR, 0, 0);
			}
			if (ci->database[0] == '\0')
				SetFocus(GetDlgItem(hdlg, IDC_DATABASE));
			else if (ci->server[0] == '\0')
				SetFocus(GetDlgItem(hdlg, IDC_SERVER));
			else if (ci->port[0] == '\0')
				SetFocus(GetDlgItem(hdlg, IDC_PORT));
			else if (ci->username[0] == '\0')
				SetFocus(GetDlgItem(hdlg, IDC_USER));
			else if (SAFE_NAME(ci->password)[0] == '\0' &&
				 ci->password_required)
				SetFocus(GetDlgItem(hdlg, IDC_PASSWORD));
			break;

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
					ci = (ConnInfo *) GetWindowLongPtr(hdlg, DWLP_USER);

					GetDlgStuff(hdlg, ci);

				case IDCANCEL:
					EndDialog(hdlg, GET_WM_COMMAND_ID(wParam, lParam) == IDOK);
					return TRUE;

				case IDC_DATASOURCE:
					ci = (ConnInfo *) GetWindowLongPtr(hdlg, DWLP_USER);
					DialogBoxParam(s_hModule, MAKEINTRESOURCE(DLG_OPTIONS_DRV),
								   hdlg, ds_options1Proc, (LPARAM) ci);
					break;
			}
			break;
		case WM_CTLCOLORSTATIC:
			if (lParam == (LPARAM)GetDlgItem(hdlg, IDC_NOTICE_USER))
			{
				HBRUSH hBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
				SetTextColor((HDC)wParam, RGB(255, 0, 0));
				return (LRESULT)hBrush;
			}
			break;
	}

	return FALSE;
}
#endif   /* WIN32 */

#define	ATTRIBUTE_DELIMITER	';'
#define	OPENING_BRACKET		'{'
#define	CLOSING_BRACKET		'}'

/*
 *	Find the next closing bracket in str, which must start at a character
 *	boundary, and return NULL if there is none.
 *
 *	A multibyte lead byte followed by 0x7d ('}') can't be read safely, so
 *	*ambiguous is set and NULL returned for it. In SJIS, BIG5, GBK, GB18030
 *	and JOHAB the pair is one character, and an application that escapes
 *	by character leaves its 0x7d undoubled; an application that escapes by
 *	byte doubles it. Read either way, the other kind of application can
 *	end the value early, and the rest of it is parsed as further keywords.
 */
static const char *
find_closing_bracket(const char *str, int ccsc, BOOL *ambiguous)
{
	encoded_str	encstr;
	UCHAR		tchar;

	encoded_str_constr(&encstr, ccsc, str);
	for (tchar = encoded_nextchar(&encstr); tchar; tchar = encoded_nextchar(&encstr))
	{
		if (MBCS_NON_ASCII(encstr))
		{
			if (CLOSING_BRACKET == tchar)
			{
				*ambiguous = TRUE;
				return NULL;
			}
			continue;
		}
		if (CLOSING_BRACKET == tchar)
			return (const char *) ENCODE_PTR(encstr);
	}
	return NULL;
}

typedef	BOOL (*copyfunc)(ConnInfo *, const char *attribute, const char *value);
static BOOL
dconn_get_attributes(copyfunc func, const char *connect_string, ConnInfo *ci, int ccsc)
{
	BOOL	ret = TRUE;
	char	*our_connect_string;
	const	char	*pair,
			*attribute,
			*value,
			*termp;
	BOOL	eoftok;
	BOOL	ambiguous = FALSE;
	char	*equals, *delp;
	char	*strtok_arg;
#ifdef	HAVE_STRTOK_R
	char	*last = NULL;
#endif /* HAVE_STRTOK_R */

	if (our_connect_string = strdup(connect_string), NULL == our_connect_string)
		return FALSE;
	strtok_arg = our_connect_string;

#ifdef	FORCE_PASSWORD_DISPLAY
	MYLOG(0, "our_connect_string = '%s'\n", our_connect_string);
#else
	if (get_mylog())
	{
		char	*hide_str = hide_password(our_connect_string);

		MYLOG(0, "our_connect_string = '%s'\n", hide_str);
		free(hide_str);
	}
#endif /* FORCE_PASSWORD_DISPLAY */

	termp = strchr(our_connect_string, '\0');
	eoftok = FALSE;
	while (!eoftok)
	{
		if (strtok_arg != NULL && strtok_arg >= termp)	/* for safety */
			break;
#ifdef	HAVE_STRTOK_R
		pair = strtok_r(strtok_arg, ";", &last);
#else
		pair = strtok(strtok_arg, ";");
#endif /* HAVE_STRTOK_R */
		if (strtok_arg)
			strtok_arg = NULL;
		if (!pair)
			break;

		equals = strchr(pair, '=');
		if (!equals)
			continue;

		*equals = '\0';
		attribute = pair;		/* ex. DSN */
		value = equals + 1;		/* ex. 'CEO co1' */
		/*
		 * Values enclosed with braces({}) can contain ; etc
		 * We don't remove the braces here because
		 * decode_or_remove_braces() in dlg_specific.c
		 * would remove them later.
		 * Just correct the misdetected delimiter(;).
		 */
		switch (*value)
		{
			const char *valuen, *closep;

			case OPENING_BRACKET:
				delp = strchr(value, '\0');
				if (delp >= termp)
				{
					eoftok = TRUE;
					break;
				}
				/* Where's a corresponding closing bracket? */
				closep = find_closing_bracket(value, ccsc, &ambiguous);
				if (ambiguous)
					goto ambiguous_brace;
				if (NULL != closep &&
				    closep[1] == '\0')
				break;

				for (valuen = value; valuen < termp; closep = find_closing_bracket(valuen, ccsc, &ambiguous))
				{
					if (ambiguous)
						goto ambiguous_brace;
					if (NULL == closep)
					{
						if (!delp)	/* error */
						{
							MYLOG(0, "closing bracket doesn't exist 1\n");
							ret = FALSE;
							goto cleanup;
						}
						closep = find_closing_bracket(delp + 1, ccsc, &ambiguous);
						if (ambiguous)
							goto ambiguous_brace;
						if (!closep)	/* error */
						{
							MYLOG(0, "closing bracket doesn't exist 2\n");
							ret = FALSE;
							goto cleanup;
						}
						*delp = ATTRIBUTE_DELIMITER;	/* restore delimiter */
						delp = NULL;
					}
					if (CLOSING_BRACKET == closep[1])
					{
						valuen = closep + 2;
						/*
						 * The string ends right after an escaped
						 * brace, so the value is never closed.
						 * Don't break out here: the tokenizer would
						 * resume inside the value and parse it again
						 * as attribute=value pairs.
						 */
						if (valuen >= termp)	/* error */
						{
							MYLOG(0, "closing bracket doesn't exist 3\n");
							ret = FALSE;
							goto cleanup;
						}
						else if (valuen == delp)
						{
							*delp = ATTRIBUTE_DELIMITER;
							delp = NULL;
						}
						continue;
					}
					else if (ATTRIBUTE_DELIMITER == closep[1] ||
						 '\0' == closep[1] ||
						 delp == closep + 1)
					{
						delp = (char *) (closep + 1);
						*delp = '\0';
						strtok_arg = delp + 1;
						if (strtok_arg + 1 >= termp)
							eoftok = TRUE;
						break;
					}
MYLOG(0, "subsequent char to the closing bracket is %c\n", closep[1]);
					ret = FALSE;
					goto cleanup;
				}
		}

		/* Copy the appropriate value to the conninfo  */
		(*func)(ci, attribute, value);

	}
	goto cleanup;

ambiguous_brace:
	MYLOG(0, "a multibyte lead byte precedes a closing bracket\n");
	ret = FALSE;
cleanup:
	free(our_connect_string);

	return ret;
}

static BOOL
dconn_get_DSN_or_Driver(const char *connect_string, ConnInfo *ci, int ccsc)
{
	CC_conninfo_init(ci, INIT_GLOBALS);
	return dconn_get_attributes(get_DSN_or_Driver, connect_string, ci, ccsc);
}

static BOOL
dconn_get_connect_attributes(const char *connect_string, ConnInfo *ci, int ccsc)
{
	return dconn_get_attributes(copyConnAttributes, connect_string, ci, ccsc);
}
