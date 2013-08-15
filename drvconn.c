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
#define NEAR
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

#define	FORCE_PASSWORD_DISPLAY
#define	NULL_IF_NULL(a) (a ? a : "(NULL)")

#ifndef FORCE_PASSWORD_DISPLAY
static char * hide_password(const char *str)
{
	char *outstr, *pwdp;

	if (!str)	return NULL;
	outstr = strdup(str);
	if (pwdp = strstr(outstr, "PWD="), !pwdp)
		pwdp = strstr(outstr, "pwd=");
	if (pwdp)
	{
		char	*p;

		for (p=pwdp + 4; *p && *p != ';'; p++)
			*p = 'x';
	}
	return outstr;
}
#endif

/* prototypes */
void		dconn_get_connect_attributes(const SQLCHAR FAR * connect_string, ConnInfo *ci);
static void dconn_get_common_attributes(const SQLCHAR FAR * connect_string, ConnInfo *ci);

#ifdef WIN32
LRESULT CALLBACK dconn_FDriverConnectProc(HWND hdlg, UINT wMsg, WPARAM wParam, LPARAM lParam);
RETCODE		dconn_DoDialog(HWND hwnd, ConnInfo *ci);

extern HINSTANCE NEAR s_hModule;	/* Saved module handle. */
#endif


RETCODE		SQL_API
PGAPI_DriverConnect(
					HDBC hdbc,
					HWND hwnd,
					const SQLCHAR FAR * szConnStrIn,
					SQLSMALLINT cbConnStrIn,
					SQLCHAR FAR * szConnStrOut,
					SQLSMALLINT cbConnStrOutMax,
					SQLSMALLINT FAR * pcbConnStrOut,
					SQLUSMALLINT fDriverCompletion)
{
	CSTR func = "PGAPI_DriverConnect";
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	ConnInfo   *ci;

#ifdef WIN32
	RETCODE		dialog_result;
#endif
	BOOL		paramRequired, didUI = FALSE;
	RETCODE		result;
	char		*connStrIn = NULL;
	char		connStrOut[MAX_CONNECT_STRING];
	int			retval;
	char		salt[5];
	char		password_required = AUTH_REQ_OK;
	ssize_t		len = 0;
	SQLSMALLINT	lenStrout;


	mylog("%s: entering...\n", func);

	if (!conn)
	{
		CC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	connStrIn = make_string(szConnStrIn, cbConnStrIn, NULL, 0);

#ifdef	FORCE_PASSWORD_DISPLAY
	mylog("**** PGAPI_DriverConnect: fDriverCompletion=%d, connStrIn='%s'\n", fDriverCompletion, connStrIn);
	qlog("conn=%p, PGAPI_DriverConnect( in)='%s', fDriverCompletion=%d\n", conn, connStrIn, fDriverCompletion);
#else
	if (get_qlog() || get_mylog())
	{
		char	*hide_str = hide_password(connStrIn);

		mylog("**** PGAPI_DriverConnect: fDriverCompletion=%d, connStrIn='%s'\n", fDriverCompletion, NULL_IF_NULL(hide_str));
		qlog("conn=%p, PGAPI_DriverConnect( in)='%s', fDriverCompletion=%d\n", conn, NULL_IF_NULL(hide_str), fDriverCompletion);
		if (hide_str)
			free(hide_str);
	}
#endif	/* FORCE_PASSWORD_DISPLAY */

	ci = &(conn->connInfo);

	/* Parse the connect string and fill in conninfo for this hdbc. */
	dconn_get_connect_attributes(connStrIn, ci);

	/*
	 * If the ConnInfo in the hdbc is missing anything, this function will
	 * fill them in from the registry (assuming of course there is a DSN
	 * given -- if not, it does nothing!)
	 */
	getDSNinfo(ci, CONN_DONT_OVERWRITE);
	dconn_get_common_attributes(connStrIn, ci);
	logs_on_off(1, ci->drivers.debug, ci->drivers.commlog);
	if (connStrIn)
	{
		free(connStrIn);
		connStrIn = NULL;
	}

	/* Fill in any default parameters if they are not there. */
	getDSNdefaults(ci);
	/* initialize pg_version */
	CC_initialize_pg_version(conn);
	memset(salt, 0, sizeof(salt));

#ifdef WIN32
dialog:
#endif
	ci->focus_password = password_required;

inolog("DriverCompletion=%d\n", fDriverCompletion);
	switch (fDriverCompletion)
	{
#ifdef WIN32
		case SQL_DRIVER_PROMPT:
			dialog_result = dconn_DoDialog(hwnd, ci);
			didUI = TRUE;
			if (dialog_result != SQL_SUCCESS)
				return dialog_result;
			break;

		case SQL_DRIVER_COMPLETE_REQUIRED:

			/* Fall through */

		case SQL_DRIVER_COMPLETE:

			paramRequired = password_required;
			/* Password is not a required parameter. */
			if (ci->database[0] == '\0')
				paramRequired = TRUE;
			else if (ci->port[0] == '\0')
				paramRequired = TRUE;
#ifdef	WIN32
			else if (ci->server[0] == '\0')
				paramRequired = TRUE;
#endif /* WIN32 */
			if (paramRequired)
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
	paramRequired = FALSE;
	if (ci->database[0] == '\0')
		paramRequired = TRUE;
	else if (ci->port[0] == '\0')
		paramRequired = TRUE;
#ifdef	WIN32
	else if (ci->server[0] == '\0')
		paramRequired = TRUE;
#endif /* WIN32 */
	if (paramRequired)
	{
		if (didUI)
			return SQL_NO_DATA_FOUND;
		CC_set_error(conn, CONN_OPENDB_ERROR, "connction string lacks some options", func);
		return SQL_ERROR;
	}

inolog("before CC_connect\n");
	/* do the actual connect */
	retval = CC_connect(conn, password_required, salt);
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
			password_required = -retval;
			goto dialog;
#else
			return SQL_ERROR;	/* until a better solution is found. */
#endif
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
	makeConnectString(connStrOut, ci, lenStrout);
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
		strncpy(szConnStrOut, connStrOut, cbConnStrOutMax);

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
		mylog("szConnStrOut = '%s' len=%d,%d\n", NULL_IF_NULL((char *) szConnStrOut), len, cbConnStrOutMax);
		qlog("conn=%p, PGAPI_DriverConnect(out)='%s'\n", conn, NULL_IF_NULL((char *) szConnStrOut));
	}
#else
	if (get_qlog() || get_mylog())
	{
		char	*hide_str = NULL;

		if (cbConnStrOutMax > 0)
			hide_str = hide_password(szConnStrOut);
		mylog("szConnStrOut = '%s' len=%d,%d\n", NULL_IF_NULL(hide_str), len, cbConnStrOutMax);
		qlog("conn=%p, PGAPI_DriverConnect(out)='%s'\n", conn, NULL_IF_NULL(hide_str));
		if (hide_str)
			free(hide_str);
	}
#endif /* FORCE_PASSWORD_DISPLAY */

	if (connStrIn)
		free(connStrIn);
	mylog("PGAPI_DriverConnect: returning %d\n", result);
	return result;
}


#ifdef WIN32
RETCODE
dconn_DoDialog(HWND hwnd, ConnInfo *ci)
{
	LRESULT			dialog_result;

	mylog("dconn_DoDialog: ci = %p\n", ci);

	if (hwnd)
	{
		dialog_result = DialogBoxParam(s_hModule, MAKEINTRESOURCE(DLG_CONFIG),
				hwnd, dconn_FDriverConnectProc, (LPARAM) ci);
		if (!dialog_result || (dialog_result == -1))
			return SQL_NO_DATA_FOUND;
		else
			return SQL_SUCCESS;
	}

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
			if ('\0' != ci->server[0])
				EnableWindow(GetDlgItem(hdlg, IDC_SERVER), FALSE);
			if ('\0' != ci->port[0])
				EnableWindow(GetDlgItem(hdlg, IDC_PORT), FALSE);

			SetWindowLongPtr(hdlg, DWLP_USER, lParam);		/* Save the ConnInfo for
														 * the "OK" */
			SetDlgStuff(hdlg, ci);

			if (ci->database[0] == '\0')
				;				/* default focus */
			else if (ci->server[0] == '\0')
				SetFocus(GetDlgItem(hdlg, IDC_SERVER));
			else if (ci->port[0] == '\0')
				SetFocus(GetDlgItem(hdlg, IDC_PORT));
			else if (ci->username[0] == '\0')
				SetFocus(GetDlgItem(hdlg, IDC_USER));
			else if (ci->focus_password)
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

				case IDC_DRIVER:
					ci = (ConnInfo *) GetWindowLongPtr(hdlg, DWLP_USER);
					DialogBoxParam(s_hModule, MAKEINTRESOURCE(DLG_OPTIONS_DRV),
								   hdlg, driver_optionsProc, (LPARAM) ci);
					break;
			}
	}

	return FALSE;
}
#endif   /* WIN32 */

#define	ATTRIBUTE_DELIMITER	';'
#define	OPENING_BRACKET		'{'
#define	CLOSING_BRACKET		'}'

typedef	BOOL (*copyfunc)(ConnInfo *, const char *attribute, const char *value);
static void
dconn_get_attributes(copyfunc func, const SQLCHAR FAR * connect_string, ConnInfo *ci)
{
	char	*our_connect_string;
	const	char	*pair,
			*attribute,
			*value,
			*termp;
	BOOL	eoftok;
	char	*equals, *delp;
	char	*strtok_arg;
#ifdef	HAVE_STRTOK_R
	char	*last;
#endif /* HAVE_STRTOK_R */

	if (our_connect_string = strdup(connect_string), NULL == our_connect_string)
		return;
	strtok_arg = our_connect_string;

#ifdef	FORCE_PASSWORD_DISPLAY
	mylog("our_connect_string = '%s'\n", our_connect_string);
#else
	if (get_mylog())
	{
		char	*hide_str = hide_password(our_connect_string);

		mylog("our_connect_string = '%s'\n", hide_str);
		free(hide_str);
	}
#endif /* FORCE_PASSWORD_DISPLAY */

	termp = strchr(our_connect_string, '\0');
	eoftok = FALSE;
	while (!eoftok)
	{
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
		 * decode_or_remove_braces() in dlg_specifi.c
		 * would remove them later.
		 * Just correct the misdetected delimter(;).  
		 */
		if (OPENING_BRACKET == *value)
		{
			delp = strchr(value, '\0');
			if (NULL == delp) continue; /* shouldn't occur */
			if (delp == termp)
			{
				/* there's a corresponding closing bracket? */
				if (CLOSING_BRACKET == delp[-1])
					eoftok = TRUE;
			}
			else
			{
				char	*closep;

				/* Where's a corresponding closing bracket? */
				closep = strchr(value, CLOSING_BRACKET);
				if (NULL == closep)
				{
					closep = strchr(delp + 1, CLOSING_BRACKET);
					if (NULL != closep) /* the delimiter is misdetected */
					{
						*delp = ATTRIBUTE_DELIMITER;
						strtok_arg = closep + 1;
						if (delp = strchr(closep + 1, ATTRIBUTE_DELIMITER), NULL != delp)
						{
							*delp = '\0'; 
							strtok_arg = delp + 1;
						}
						if (strtok_arg + 1 >= termp)
							eoftok = TRUE;
					}
				}
			}
		}

#ifndef	FORCE_PASSWORD_DISPLAY
		if (stricmp(attribute, INI_PASSWORD) == 0 ||
		    stricmp(attribute, "pwd") == 0)
			mylog("attribute = '%s', value = 'xxxxx'\n", attribute);
		else
#endif /* FORCE_PASSWORD_DISPLAY */
			mylog("attribute = '%s', value = '%s'\n", attribute, value);

		if (!attribute || !value)
			continue;

		/* Copy the appropriate value to the conninfo  */
		(*func)(ci, attribute, value);

	}

	free(our_connect_string);
}

void
dconn_get_connect_attributes(const SQLCHAR FAR * connect_string, ConnInfo *ci)
{

	CC_conninfo_init(ci, COPY_GLOBALS);
	dconn_get_attributes(copyAttributes, connect_string, ci);
}

static void
dconn_get_common_attributes(const SQLCHAR FAR * connect_string, ConnInfo *ci)
{
	dconn_get_attributes(copyCommonAttributes, connect_string, ci);
}
