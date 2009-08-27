/*-------
 * Module:			setup.c
 *
 * Description:		This module contains the setup functions for
 *					adding/modifying a Data Source in the ODBC.INI portion
 *					of the registry.
 *
 * Classes:			n/a
 *
 * API functions:	ConfigDSN, ConfigDriver
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#include  "psqlodbc.h"

#include  "environ.h"
#include  "connection.h"
#include  <windowsx.h>
#include  <string.h>
#include  <stdlib.h>
#include  "resource.h"
#include  "pgapifunc.h"
#include  "dlg_specific.h"
#include  "win_setup.h"


#define INTFUNC  __stdcall

extern HINSTANCE NEAR s_hModule;	/* Saved module handle. */
extern GLOBAL_VALUES	globals;

/* Constants */
#define MIN(x,y)	  ((x) < (y) ? (x) : (y))

#define MAXKEYLEN		(32+1)	/* Max keyword length */
#define MAXDESC			(255+1) /* Max description length */
#define MAXDSNAME		(32+1)	/* Max data source name length */


/*--------
 *	ConfigDSN
 *
 *	Description:	ODBC Setup entry point
 *				This entry point is called by the ODBC Installer
 *				(see file header for more details)
 *	Input	 :	hwnd ----------- Parent window handle
 *				fRequest ------- Request type (i.e., add, config, or remove)
 *				lpszDriver ----- Driver name
 *				lpszAttributes - data source attribute string
 *	Output	 :	TRUE success, FALSE otherwise
 *--------
 */
BOOL		CALLBACK
ConfigDSN(HWND hwnd,
		  WORD fRequest,
		  LPCSTR lpszDriver,
		  LPCSTR lpszAttributes)
{
	BOOL		fSuccess;		/* Success/fail flag */
	GLOBALHANDLE hglbAttr;
	LPSETUPDLG	lpsetupdlg;


	/* Allocate attribute array */
	hglbAttr = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(SETUPDLG));
	if (!hglbAttr)
		return FALSE;
	lpsetupdlg = (LPSETUPDLG) GlobalLock(hglbAttr);
	/* Parse attribute string */
	if (lpszAttributes)
		ParseAttributes(lpszAttributes, lpsetupdlg);

	/* Save original data source name */
	if (lpsetupdlg->ci.dsn[0])
		lstrcpy(lpsetupdlg->szDSN, lpsetupdlg->ci.dsn);
	else
		lpsetupdlg->szDSN[0] = '\0';

	/* Remove data source */
	if (ODBC_REMOVE_DSN == fRequest)
	{
		/* Fail if no data source name was supplied */
		if (!lpsetupdlg->ci.dsn[0])
			fSuccess = FALSE;

		/* Otherwise remove data source from ODBC.INI */
		else
			fSuccess = SQLRemoveDSNFromIni(lpsetupdlg->ci.dsn);
	}
	/* Add or Configure data source */
	else
	{
		/* Save passed variables for global access (e.g., dialog access) */
		lpsetupdlg->hwndParent = hwnd;
		lpsetupdlg->lpszDrvr = lpszDriver;
		lpsetupdlg->fNewDSN = (ODBC_ADD_DSN == fRequest);
		lpsetupdlg->fDefault = !lstrcmpi(lpsetupdlg->ci.dsn, INI_DSN);

		/*
		 * Display the appropriate dialog (if parent window handle
		 * supplied)
		 */
		if (hwnd)
		{
			/* Display dialog(s) */
			fSuccess = (IDOK == DialogBoxParam(s_hModule,
											 MAKEINTRESOURCE(DLG_CONFIG),
											   hwnd,
											   ConfigDlgProc,
											 (LONG) (LPSTR) lpsetupdlg));
		}
		else if (lpsetupdlg->ci.dsn[0])
			fSuccess = SetDSNAttributes(hwnd, lpsetupdlg, NULL);
		else
			fSuccess = FALSE;
	}

	GlobalUnlock(hglbAttr);
	GlobalFree(hglbAttr);

	return fSuccess;
}

/*--------
 *	ConfigDriver
 *
 *	Description:	ODBC Setup entry point
 *			This entry point is called by the ODBC Installer
 *			(see file header for more details)
 *	Arguments :	hwnd ----------- Parent window handle
 *			fRequest ------- Request type (i.e., add, config, or remove)
 *			lpszDriver ----- Driver name
 *			lpszArgs ------- A null-terminated string containing
				arguments for a driver specific fRequest
 *			lpszMsg -------- A null-terimated string containing
			  	an output message from the driver setup
 *			cnMsgMax ------- Length of lpszMSg
 *			pcbMsgOut ------ Total number of bytes available to
				return in lpszMsg 
 *	Returns :	TRUE success, FALSE otherwise
 *--------
 */
static BOOL SetDriverAttributes(LPCSTR lpszDriver);
BOOL		CALLBACK
ConfigDriver(HWND hwnd,
		WORD fRequest,
		LPCSTR lpszDriver,
		LPCSTR lpszArgs,
		LPSTR lpszMsg,
		WORD cbMsgMax,
		WORD *pcbMsgOut)
{
	BOOL	fSuccess = TRUE;	/* Success/fail flag */

	/* Add the driver */
	if (ODBC_INSTALL_DRIVER == fRequest)
		fSuccess = SetDriverAttributes(lpszDriver);

	return fSuccess;
}


/*-------
 * CenterDialog
 *
 *		Description:  Center the dialog over the frame window
 *		Input	   :  hdlg -- Dialog window handle
 *		Output	   :  None
 *-------
 */
void		INTFUNC
CenterDialog(HWND hdlg)
{
	HWND		hwndFrame;
	RECT		rcDlg,
				rcScr,
				rcFrame;
	int			cx,
				cy;

	hwndFrame = GetParent(hdlg);

	GetWindowRect(hdlg, &rcDlg);
	cx = rcDlg.right - rcDlg.left;
	cy = rcDlg.bottom - rcDlg.top;

	GetClientRect(hwndFrame, &rcFrame);
	ClientToScreen(hwndFrame, (LPPOINT) (&rcFrame.left));
	ClientToScreen(hwndFrame, (LPPOINT) (&rcFrame.right));
	rcDlg.top = rcFrame.top + (((rcFrame.bottom - rcFrame.top) - cy) >> 1);
	rcDlg.left = rcFrame.left + (((rcFrame.right - rcFrame.left) - cx) >> 1);
	rcDlg.bottom = rcDlg.top + cy;
	rcDlg.right = rcDlg.left + cx;

	GetWindowRect(GetDesktopWindow(), &rcScr);
	if (rcDlg.bottom > rcScr.bottom)
	{
		rcDlg.bottom = rcScr.bottom;
		rcDlg.top = rcDlg.bottom - cy;
	}
	if (rcDlg.right > rcScr.right)
	{
		rcDlg.right = rcScr.right;
		rcDlg.left = rcDlg.right - cx;
	}

	if (rcDlg.left < 0)
		rcDlg.left = 0;
	if (rcDlg.top < 0)
		rcDlg.top = 0;

	MoveWindow(hdlg, rcDlg.left, rcDlg.top, cx, cy, TRUE);
	return;
}

/*-------
 * ConfigDlgProc
 *	Description:	Manage add data source name dialog
 *	Input	 :	hdlg --- Dialog window handle
 *				wMsg --- Message
 *				wParam - Message parameter
 *				lParam - Message parameter
 *	Output	 :	TRUE if message processed, FALSE otherwise
 *-------
 */
LRESULT			CALLBACK
ConfigDlgProc(HWND hdlg,
			  UINT wMsg,
			  WPARAM wParam,
			  LPARAM lParam)
{
	LPSETUPDLG	lpsetupdlg;
	ConnInfo   *ci;
	DWORD		cmd;
	char		strbuf[64];

	switch (wMsg)
	{
			/* Initialize the dialog */
		case WM_INITDIALOG:
			lpsetupdlg = (LPSETUPDLG) lParam;
			ci = &lpsetupdlg->ci;

			/* Hide the driver connect message */
			ShowWindow(GetDlgItem(hdlg, DRV_MSG_LABEL), SW_HIDE);
			LoadString(s_hModule, IDS_ADVANCE_SAVE, strbuf, sizeof(strbuf));
			SetWindowText(GetDlgItem(hdlg, IDOK), strbuf);

			SetWindowLongPtr(hdlg, DWLP_USER, lParam);
			CenterDialog(hdlg); /* Center dialog */

			/*
			 * NOTE: Values supplied in the attribute string will always
			 */
			/* override settings in ODBC.INI */

			memcpy(&ci->drivers, &globals, sizeof(globals));
			/* Get the rest of the common attributes */
			getDSNinfo(ci, CONN_DONT_OVERWRITE);

			/* Fill in any defaults */
			getDSNdefaults(ci);

			/* Initialize dialog fields */
			SetDlgStuff(hdlg, ci);

			if (lpsetupdlg->fNewDSN || !ci->dsn[0])
				ShowWindow(GetDlgItem(hdlg, IDC_MANAGEDSN), SW_HIDE);
			if (lpsetupdlg->fDefault)
			{
				EnableWindow(GetDlgItem(hdlg, IDC_DSNAME), FALSE);
				EnableWindow(GetDlgItem(hdlg, IDC_DSNAMETEXT), FALSE);
			}
			else
				SendDlgItemMessage(hdlg, IDC_DSNAME,
							 EM_LIMITTEXT, (WPARAM) (MAXDSNAME - 1), 0L);

			SendDlgItemMessage(hdlg, IDC_DESC,
							   EM_LIMITTEXT, (WPARAM) (MAXDESC - 1), 0L);
			return TRUE;		/* Focus was not set */

			/* Process buttons */
		case WM_COMMAND:
			switch (cmd = GET_WM_COMMAND_ID(wParam, lParam))
			{
					/*
					 * Ensure the OK button is enabled only when a data
					 * source name
					 */
					/* is entered */
				case IDC_DSNAME:
					if (GET_WM_COMMAND_CMD(wParam, lParam) == EN_CHANGE)
					{
						char		szItem[MAXDSNAME];	/* Edit control text */

						/* Enable/disable the OK button */
						EnableWindow(GetDlgItem(hdlg, IDOK),
									 GetDlgItemText(hdlg, IDC_DSNAME,
												szItem, sizeof(szItem)));
						return TRUE;
					}
					break;

					/* Accept results */
				case IDOK:
				case IDAPPLY:
					lpsetupdlg = (LPSETUPDLG) GetWindowLong(hdlg, DWLP_USER);
					/* Retrieve dialog values */
					if (!lpsetupdlg->fDefault)
						GetDlgItemText(hdlg, IDC_DSNAME,
									   lpsetupdlg->ci.dsn,
									   sizeof(lpsetupdlg->ci.dsn));
					/* Get Dialog Values */
					GetDlgStuff(hdlg, &lpsetupdlg->ci);

					/* Update ODBC.INI */
					SetDSNAttributes(hdlg, lpsetupdlg, NULL);
					if (IDAPPLY == cmd)
						break;
					/* Return to caller */
				case IDCANCEL:
					EndDialog(hdlg, wParam);
					return TRUE;

				case IDC_TEST:
				{
					lpsetupdlg = (LPSETUPDLG) GetWindowLong(hdlg, DWLP_USER);
					if (NULL != lpsetupdlg)
					{
						EnvironmentClass *env = EN_Constructor();
						ConnectionClass *conn = NULL;
						char    szMsg[SQL_MAX_MESSAGE_LENGTH];

						/* Get Dialog Values */
						GetDlgStuff(hdlg, &lpsetupdlg->ci);
						if (env)
							conn = CC_Constructor();
						if (conn)
						{
							char *emsg, *allocstr = NULL;
#ifdef	UNICODE_SUPPORT
							int	tlen;
							SQLWCHAR *wermsg = NULL;
							SQLULEN	ulen;
#endif /* UNICODE_SUPPORT */
							int errnum;

							EN_add_connection(env, conn);
							memcpy(&conn->connInfo, &lpsetupdlg->ci, sizeof(ConnInfo));
							CC_initialize_pg_version(conn);
							logs_on_off(1, conn->connInfo.drivers.debug, conn->connInfo.drivers.commlog);
#ifdef	UNICODE_SUPPORT
							CC_set_in_unicode_driver(conn);
#endif /* UNICODE_SUPPORT */
							if (CC_connect(conn, AUTH_REQ_OK, NULL) > 0)
							{
								if (CC_get_errornumber(conn) != 0)
								{
									CC_get_error(conn, &errnum, &emsg);
									snprintf(szMsg, sizeof(szMsg), "Warning: %s", emsg);
								}
								else
									strncpy_null(szMsg, "Connection successful", sizeof(szMsg));
								emsg = szMsg;
							}
							else
							{
								CC_get_error(conn, &errnum, &emsg);
							}
#ifdef	UNICODE_SUPPORT
							tlen = strlen(emsg);
							wermsg = (SQLWCHAR *) malloc(sizeof(SQLWCHAR) * (tlen + 1));
							ulen = utf8_to_ucs2_lf1(emsg, SQL_NTS, FALSE, wermsg, tlen + 1);
							if (ulen != (SQLULEN) -1)
							{
								allocstr = malloc(4 * tlen + 1);
								tlen = (SQLSMALLINT) wstrtomsg(NULL, wermsg,
					(int) tlen, allocstr, (int) 4 * tlen + 1);
								emsg = allocstr;
							}
							if (NULL != wermsg)
								free(wermsg);
#endif /* UNICODE_SUPPORT */
							MessageBox(lpsetupdlg->hwndParent, emsg, "Connection Test", MB_ICONEXCLAMATION | MB_OK);
							logs_on_off(-1, conn->connInfo.drivers.debug, conn->connInfo.drivers.commlog);
							EN_remove_connection(env, conn);
							CC_Destructor(conn);
							if (NULL != allocstr)
								free(allocstr);
						}
						if (env)
							EN_Destructor(env);
						return TRUE;
					}
					break;
				}
				case IDC_DATASOURCE:
					lpsetupdlg = (LPSETUPDLG) GetWindowLong(hdlg, DWLP_USER);
					DialogBoxParam(s_hModule, MAKEINTRESOURCE(DLG_OPTIONS_DRV),
					 hdlg, ds_options1Proc, (LPARAM) &lpsetupdlg->ci);
					return TRUE;

				case IDC_DRIVER:
					lpsetupdlg = (LPSETUPDLG) GetWindowLong(hdlg, DWLP_USER);
					DialogBoxParam(s_hModule, MAKEINTRESOURCE(DLG_OPTIONS_GLOBAL),
						 hdlg, global_optionsProc, (LPARAM) &lpsetupdlg->ci);

					return TRUE;
				case IDC_MANAGEDSN:
					lpsetupdlg = (LPSETUPDLG) GetWindowLong(hdlg, DWLP_USER);
					if (DialogBoxParam(s_hModule, MAKEINTRESOURCE(DLG_DRIVER_CHANGE),
						hdlg, manage_dsnProc,
						(LPARAM) lpsetupdlg) > 0)
						EndDialog(hdlg, 0);

					return TRUE;
			}
			break;
		case WM_CTLCOLORSTATIC:
			if (lParam == (LPARAM)GetDlgItem(hdlg, IDC_NOTICE_USER))
			{
				HBRUSH hBrush = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
				SetTextColor((HDC)wParam, RGB(255, 0, 0));
				return (long)hBrush;
			}
			break;
	}

	/* Message not processed */
	return FALSE;
}


/*-------
 * ParseAttributes
 *
 *	Description:	Parse attribute string moving values into the aAttr array
 *	Input	 :	lpszAttributes - Pointer to attribute string
 *	Output	 :	None (global aAttr normally updated)
 *-------
 */
void		INTFUNC
ParseAttributes(LPCSTR lpszAttributes, LPSETUPDLG lpsetupdlg)
{
	LPCSTR		lpsz;
	LPCSTR		lpszStart;
	char		aszKey[MAXKEYLEN];
	int			cbKey;
	char		value[MAXPGPATH];

	CC_conninfo_init(&(lpsetupdlg->ci));

	for (lpsz = lpszAttributes; *lpsz; lpsz++)
	{
		/*
		 * Extract key name (e.g., DSN), it must be terminated by an
		 * equals
		 */
		lpszStart = lpsz;
		for (;; lpsz++)
		{
			if (!*lpsz)
				return;			/* No key was found */
			else if (*lpsz == '=')
				break;			/* Valid key found */
		}
		/* Determine the key's index in the key table (-1 if not found) */
		cbKey = lpsz - lpszStart;
		if (cbKey < sizeof(aszKey))
		{
			_fmemcpy(aszKey, lpszStart, cbKey);
			aszKey[cbKey] = '\0';
		}

		/* Locate end of key value */
		lpszStart = ++lpsz;
		for (; *lpsz; lpsz++)
			;

		/* lpsetupdlg->aAttr[iElement].fSupplied = TRUE; */
		_fmemcpy(value, lpszStart, MIN(lpsz - lpszStart + 1, MAXPGPATH));

		mylog("aszKey='%s', value='%s'\n", aszKey, value);

		/* Copy the appropriate value to the conninfo  */
		if (!copyAttributes(&lpsetupdlg->ci, aszKey, value))
			copyCommonAttributes(&lpsetupdlg->ci, aszKey, value);
	}
	return;
}


/*--------
 * SetDSNAttributes
 *
 *	Description:	Write data source attributes to ODBC.INI
 *	Input	 :	hwnd - Parent window handle (plus globals)
 *	Output	 :	TRUE if successful, FALSE otherwise
 *--------
 */
BOOL		INTFUNC
SetDSNAttributes(HWND hwndParent, LPSETUPDLG lpsetupdlg, DWORD *errcode)
{
	LPCSTR		lpszDSN;		/* Pointer to data source name */

	lpszDSN = lpsetupdlg->ci.dsn;

	if (errcode)
		*errcode = 0;
	/* Validate arguments */
	if (lpsetupdlg->fNewDSN && !*lpsetupdlg->ci.dsn)
		return FALSE;

	/* Write the data source name */
	if (!SQLWriteDSNToIni(lpszDSN, lpsetupdlg->lpszDrvr))
	{
		RETCODE	ret = SQL_ERROR;
		DWORD	err = SQL_ERROR;
		char    szMsg[SQL_MAX_MESSAGE_LENGTH];

#if (ODBCVER >= 0x0300)
		ret = SQLInstallerError(1, &err, szMsg, sizeof(szMsg), NULL);
#endif /* ODBCVER */
		if (hwndParent)
		{
			char		szBuf[MAXPGPATH];

			if (SQL_SUCCESS != ret)
			{
				LoadString(s_hModule, IDS_BADDSN, szBuf, sizeof(szBuf));
				wsprintf(szMsg, szBuf, lpszDSN);
			}
			LoadString(s_hModule, IDS_MSGTITLE, szBuf, sizeof(szBuf));
			MessageBox(hwndParent, szMsg, szBuf, MB_ICONEXCLAMATION | MB_OK);
		}
		if (errcode)
			*errcode = err;
		return FALSE;
	}

	/* Update ODBC.INI */
	writeDriverCommoninfo(ODBC_INI, lpsetupdlg->ci.dsn, &(lpsetupdlg->ci.drivers));
	writeDSNinfo(&lpsetupdlg->ci);

	/* If the data source name has changed, remove the old name */
	if (lstrcmpi(lpsetupdlg->szDSN, lpsetupdlg->ci.dsn))
		SQLRemoveDSNFromIni(lpsetupdlg->szDSN);
	return TRUE;
}

/*--------
 * SetDriverAttributes
 *
 *	Description:	Write driver information attributes to ODBCINST.INI
 *	Input	 :	lpszDriver - The driver name
 *	Output	 :	TRUE if successful, FALSE otherwise
 *--------
 */
static BOOL
SetDriverAttributes(LPCSTR lpszDriver)
{
	BOOL	ret = FALSE;

	/* Validate arguments */
	if (!lpszDriver || !lpszDriver[0])
		return FALSE;

	if (!SQLWritePrivateProfileString(lpszDriver, "APILevel", "1", ODBCINST_INI))
		goto cleanup;
	if (!SQLWritePrivateProfileString(lpszDriver, "ConnectFunctions", "YYN", ODBCINST_INI))
		goto cleanup;
	if (!SQLWritePrivateProfileString(lpszDriver, "DriverODBCVer",
#ifdef UNICODE_SUPPORT
		 "03.51",
#else
		 "03.00",
#endif
		ODBCINST_INI))
		goto cleanup;
	if (!SQLWritePrivateProfileString(lpszDriver, "FileUsage", "0", ODBCINST_INI))
		goto cleanup;
	if (!SQLWritePrivateProfileString(lpszDriver, "SQLLevel", "1", ODBCINST_INI))
		goto cleanup;

	ret = TRUE;
cleanup:

	return ret;
}


#ifdef	WIN32

BOOL	INTFUNC
ChangeDriverName(HWND hwndParent, LPSETUPDLG lpsetupdlg, LPCSTR driver_name)
{
	DWORD   err = 0;
	ConnInfo	*ci = &lpsetupdlg->ci;

	if (!ci->dsn[0])
	{
		err = IDS_BADDSN;
	}
	else if (!driver_name || strnicmp(driver_name, "postgresql", 10))
	{
		err = IDS_BADDSN;
	}
	else
	{
		LPCSTR	lpszDrvr = lpsetupdlg->lpszDrvr;

		lpsetupdlg->lpszDrvr = driver_name;
		if (!SetDSNAttributes(hwndParent, lpsetupdlg, &err))
		{
			if (!err)
				err = IDS_BADDSN;
			lpsetupdlg->lpszDrvr = lpszDrvr;
		}
	}
	return (err == 0);
}

#endif /* WIN32 */
