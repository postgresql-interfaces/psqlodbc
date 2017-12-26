#ifdef	WIN32
/*-------
 * Module:			dlg_wingui.c
 *
 * Description:		This module contains any specific code for handling
 *					dialog boxes such as driver/datasource options.  Both the
 *					ConfigDSN() and the SQLDriverConnect() functions use
 *					functions in this module.  If you were to add a new option
 *					to any dialog box, you would most likely only have to change
 *					things in here rather than in 2 separate places as before.
 *
 * Classes:			none
 *
 * API functions:	none
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */
/* Multibyte support	Eiji Tokuya 2001-03-15 */

#include "dlg_specific.h"
#include "misc.h" // strncpy_null
#include "win_setup.h"

#include "loadlib.h"

#include "pgapifunc.h"
#ifdef _HANDLE_ENLIST_IN_DTC_
#include "xalibname.h"
#include "connexp.h"
#endif /* _HANDLE_ENLIST_IN_DTC_ */


extern HINSTANCE s_hModule;
static int	driver_optionsDraw(HWND, const ConnInfo *, int src, BOOL enable);
static int	driver_options_update(HWND hdlg, ConnInfo *ci);

static int	ds_options_update(HWND hdlg, ConnInfo *ci);

static int	ds_options3Draw(HWND, const ConnInfo *);
static int	ds_options3_update(HWND hdlg, ConnInfo *ci);

static struct {
	int	ids;
	const char * const	modestr;
} modetab[] = {
		  {IDS_SSLREQUEST_DISABLE, SSLMODE_DISABLE}
		, {IDS_SSLREQUEST_ALLOW, SSLMODE_ALLOW}
		, {IDS_SSLREQUEST_PREFER, SSLMODE_PREFER}
		, {IDS_SSLREQUEST_REQUIRE, SSLMODE_REQUIRE}
		, {IDS_SSLREQUEST_VERIFY_CA, SSLMODE_VERIFY_CA}
		, {IDS_SSLREQUEST_VERIFY_FULL, SSLMODE_VERIFY_FULL}
	};
static int	dspcount_bylevel[] = {1, 4, 6};

void
SetDlgStuff(HWND hdlg, const ConnInfo *ci)
{
	char	buff[MEDIUM_REGISTRY_LEN + 1];
	int	i, dsplevel, selidx, dspcount;

	/*
	 * If driver attribute NOT present, then set the datasource name and
	 * description
	 */
	SetDlgItemText(hdlg, IDC_DSNAME, ci->dsn);
	SetDlgItemText(hdlg, IDC_DESC, ci->desc);

	SetDlgItemText(hdlg, IDC_DATABASE, ci->database);
	SetDlgItemText(hdlg, IDC_SERVER, ci->server);
	SetDlgItemText(hdlg, IDC_USER, ci->username);
	SetDlgItemText(hdlg, IDC_PASSWORD, SAFE_NAME(ci->password));
	SetDlgItemText(hdlg, IDC_PORT, ci->port);

	dsplevel = 0;

	/*
	 * XXX: We used to hide or show this depending whether libpq was loaded,
	 * but we depend on libpq directly nowadays, so it's always loaded.
	 */
	ShowWindow(GetDlgItem(hdlg, IDC_NOTICE_USER), SW_HIDE);
	dsplevel = 2;

	selidx = -1;
	for (i = 0; i < sizeof(modetab) / sizeof(modetab[0]); i++)
	{
		if (!stricmp(ci->sslmode, modetab[i].modestr))
		{
			selidx = i;
			break;
		}
	}
	for (i = dsplevel; i < sizeof(dspcount_bylevel) / sizeof(int); i++)
	{
		if (selidx < dspcount_bylevel[i])
			break;
		dsplevel++;
	}

	dspcount = dspcount_bylevel[dsplevel];
	for (i = 0; i < dspcount; i++)
	{
		LoadString(GetWindowInstance(hdlg), modetab[i].ids, buff, MEDIUM_REGISTRY_LEN);
		SendDlgItemMessage(hdlg, IDC_SSLMODE, CB_ADDSTRING, 0, (WPARAM) buff);
	}

	SendDlgItemMessage(hdlg, IDC_SSLMODE, CB_SETCURSEL, selidx, (WPARAM) 0);
}


void
GetDlgStuff(HWND hdlg, ConnInfo *ci)
{
	int	sslposition;
	char	medium_buf[MEDIUM_REGISTRY_LEN];

	GetDlgItemText(hdlg, IDC_DESC, ci->desc, sizeof(ci->desc));

	GetDlgItemText(hdlg, IDC_DATABASE, ci->database, sizeof(ci->database));
	GetDlgItemText(hdlg, IDC_SERVER, ci->server, sizeof(ci->server));
	GetDlgItemText(hdlg, IDC_USER, ci->username, sizeof(ci->username));
	GetDlgItemText(hdlg, IDC_PASSWORD, medium_buf, sizeof(medium_buf));
	STR_TO_NAME(ci->password, medium_buf);
	GetDlgItemText(hdlg, IDC_PORT, ci->port, sizeof(ci->port));
	sslposition = (int)(DWORD)SendMessage(GetDlgItem(hdlg, IDC_SSLMODE), CB_GETCURSEL, 0L, 0L);
	STRCPY_FIXED(ci->sslmode, modetab[sslposition].modestr);
}

static void
getDriversDefaultsOfCi(const ConnInfo *ci, GLOBAL_VALUES *glbv)
{
	const char *drivername = NULL;

	if (ci->drivername[0])
		drivername = ci->drivername;
	else if (NAME_IS_VALID(ci->drivers.drivername))
		drivername = SAFE_NAME(ci->drivers.drivername);	
	if (drivername && drivername[0])
		getDriversDefaults(drivername, glbv);
	else
		getDriversDefaults(INVALID_DRIVER, glbv);
}

static int
driver_optionsDraw(HWND hdlg, const ConnInfo *ci, int src, BOOL enable)
{
	const GLOBAL_VALUES *comval = NULL;
	const char * drivername = NULL;
	GLOBAL_VALUES defval;

MYLOG(0, "entering src=%d\n", src);
	init_globals(&defval);
	switch (src)
	{
		case 0:			/* default */
			getDriversDefaultsOfCi(ci, &defval);
			defval.debug = DEFAULT_DEBUG;
			defval.commlog = DEFAULT_COMMLOG;
			comval = &defval;
			break;
		case 1:			/* dsn specific */
			comval = &(ci->drivers);
			break;
		default:
			return 0;
	}

	ShowWindow(GetDlgItem(hdlg, DRV_MSG_LABEL2), enable ? SW_SHOW : SW_HIDE);

	CheckDlgButton(hdlg, DRV_COMMLOG, comval->commlog > 0);
	SetDlgItemInt(hdlg, DS_COMMLOG, comval->commlog, FALSE);
	ShowWindow(GetDlgItem(hdlg, DS_COMMLOG), comval->commlog > 0 ? SW_SHOW : SW_HIDE);

	CheckDlgButton(hdlg, DRV_UNIQUEINDEX, comval->unique_index);
	/* EnableWindow(GetDlgItem(hdlg, DRV_UNIQUEINDEX), enable); */
	EnableWindow(GetDlgItem(hdlg, DRV_READONLY), FALSE);
	CheckDlgButton(hdlg, DRV_USEDECLAREFETCH, comval->use_declarefetch);

	/* Unknown Sizes clear */
	CheckDlgButton(hdlg, DRV_UNKNOWN_DONTKNOW, 0);
	CheckDlgButton(hdlg, DRV_UNKNOWN_LONGEST, 0);
	CheckDlgButton(hdlg, DRV_UNKNOWN_MAX, 0);
	/* Unknown (Default) Data Type sizes */
	switch (comval->unknown_sizes)
	{
		case UNKNOWNS_AS_DONTKNOW:
			CheckDlgButton(hdlg, DRV_UNKNOWN_DONTKNOW, 1);
			break;
		case UNKNOWNS_AS_LONGEST:
			CheckDlgButton(hdlg, DRV_UNKNOWN_LONGEST, 1);
			break;
		case UNKNOWNS_AS_MAX:
		default:
			CheckDlgButton(hdlg, DRV_UNKNOWN_MAX, 1);
			break;
	}

	CheckDlgButton(hdlg, DRV_TEXT_LONGVARCHAR, comval->text_as_longvarchar);
	CheckDlgButton(hdlg, DRV_UNKNOWNS_LONGVARCHAR, comval->unknowns_as_longvarchar);
	CheckDlgButton(hdlg, DRV_BOOLS_CHAR, comval->bools_as_char);
	CheckDlgButton(hdlg, DRV_PARSE, comval->parse);

	CheckDlgButton(hdlg, DRV_DEBUG, comval->debug > 0);
	SetDlgItemInt(hdlg, DS_DEBUG, comval->debug, FALSE);
	ShowWindow(GetDlgItem(hdlg, DS_DEBUG), comval->debug > 0 ? SW_SHOW : SW_HIDE);

	SetDlgItemInt(hdlg, DRV_CACHE_SIZE, comval->fetch_max, FALSE);
	SetDlgItemInt(hdlg, DRV_VARCHAR_SIZE, comval->max_varchar_size, FALSE);
	SetDlgItemInt(hdlg, DRV_LONGVARCHAR_SIZE, comval->max_longvarchar_size, TRUE);
	SetDlgItemText(hdlg, DRV_EXTRASYSTABLEPREFIXES, comval->extra_systable_prefixes);

	/* Driver Connection Settings */
	EnableWindow(GetDlgItem(hdlg, DRV_CONNSETTINGS), FALSE);
	ShowWindow(GetDlgItem(hdlg, ID2NDPAGE), enable ? SW_HIDE : SW_SHOW);
	ShowWindow(GetDlgItem(hdlg, ID3RDPAGE), enable ? SW_HIDE : SW_SHOW);

	finalize_globals(&defval);
	return 0;
}

#define	INIT_DISP_LOGVAL	2

static int
driver_options_update(HWND hdlg, ConnInfo *ci)
{
	GLOBAL_VALUES *comval;
	BOOL	bTranslated;

MYLOG(3, "entering\n");
	comval = &(ci->drivers);

	(comval->commlog = GetDlgItemInt(hdlg, DS_COMMLOG, &bTranslated, FALSE)) || bTranslated || (comval->commlog = INIT_DISP_LOGVAL);
	comval->unique_index = IsDlgButtonChecked(hdlg, DRV_UNIQUEINDEX);
	comval->use_declarefetch = IsDlgButtonChecked(hdlg, DRV_USEDECLAREFETCH);

	/* Unknown (Default) Data Type sizes */
	if (IsDlgButtonChecked(hdlg, DRV_UNKNOWN_MAX))
		comval->unknown_sizes = UNKNOWNS_AS_MAX;
	else if (IsDlgButtonChecked(hdlg, DRV_UNKNOWN_DONTKNOW))
		comval->unknown_sizes = UNKNOWNS_AS_DONTKNOW;
	else if (IsDlgButtonChecked(hdlg, DRV_UNKNOWN_LONGEST))
		comval->unknown_sizes = UNKNOWNS_AS_LONGEST;
	else
		comval->unknown_sizes = UNKNOWNS_AS_MAX;

	comval->text_as_longvarchar = IsDlgButtonChecked(hdlg, DRV_TEXT_LONGVARCHAR);
	comval->unknowns_as_longvarchar = IsDlgButtonChecked(hdlg, DRV_UNKNOWNS_LONGVARCHAR);
	comval->bools_as_char = IsDlgButtonChecked(hdlg, DRV_BOOLS_CHAR);

	comval->parse = IsDlgButtonChecked(hdlg, DRV_PARSE);

	(comval->debug = GetDlgItemInt(hdlg, DS_DEBUG, &bTranslated, FALSE)) || bTranslated || (comval->debug = INIT_DISP_LOGVAL);

	comval->fetch_max = GetDlgItemInt(hdlg, DRV_CACHE_SIZE, NULL, FALSE);
	comval->max_varchar_size = GetDlgItemInt(hdlg, DRV_VARCHAR_SIZE, NULL, FALSE);
	comval->max_longvarchar_size = GetDlgItemInt(hdlg, DRV_LONGVARCHAR_SIZE, NULL, TRUE);		/* allows for
																								 * SQL_NO_TOTAL */

	GetDlgItemText(hdlg, DRV_EXTRASYSTABLEPREFIXES, comval->extra_systable_prefixes, sizeof(comval->extra_systable_prefixes));

	/* fall through */
	return 0;
}


#ifdef _HANDLE_ENLIST_IN_DTC_
static
HMODULE DtcProc(const char *procname, FARPROC *proc)
{
	HMODULE	hmodule;

	*proc = NULL;
	if (hmodule = LoadLibraryEx(GetXaLibPath(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH), NULL != hmodule)
	{
MYLOG(0, "GetProcAddres for %s\n", procname);
		*proc = GetProcAddress(hmodule, procname);
	}

	return hmodule;
}
#endif /* _HANDLE_ENLIST_IN_DTC_ */

#include <sys/stat.h>
static const char *IsAnExistingDirectory(const char *path)
{
		
	struct stat st;

	if (stat(path, &st) < 0)
	{
		CSTR errmsg_doesnt_exist = "doesn't exist";

		return errmsg_doesnt_exist;
	}
	if ((st.st_mode & S_IFDIR) == 0)
	{
		CSTR errmsg_isnt_a_dir = "isn't a directory";
		
		return errmsg_isnt_a_dir;
	}
	return NULL;
}

LRESULT			CALLBACK
global_optionsProc(HWND hdlg,
				   UINT wMsg,
				   WPARAM wParam,
				   LPARAM lParam)
{
#ifdef _HANDLE_ENLIST_IN_DTC_
	HMODULE	hmodule;
	FARPROC	proc;
#endif /* _HANDLE_ENLIST_IN_DTC_ */
	ConnInfo	*ci;
	char logdir[PATH_MAX];
	const char *logmsg;
	GLOBAL_VALUES	defval;

// if (WM_INITDIALOG == wMsg || WM_COMMAND == wMsg)
// MYLOG(0, "entering wMsg=%d\n", wMsg);
	init_globals(&defval);
	switch (wMsg)
	{
		case WM_INITDIALOG:
			SetWindowLongPtr(hdlg, DWLP_USER, lParam); /* save for test etc */ 
			ci = (ConnInfo *) lParam;
			getDriversDefaultsOfCi(ci, &defval);
			CheckDlgButton(hdlg, DRV_COMMLOG, getGlobalCommlog());
			CheckDlgButton(hdlg, DRV_DEBUG, getGlobalDebug());
			getLogDir(logdir, sizeof(logdir));
			SetDlgItemText(hdlg, DS_LOGDIR, logdir);
#ifdef _HANDLE_ENLIST_IN_DTC_
			hmodule = DtcProc("GetMsdtclog", &proc);
			if (proc)
			{
				INT_PTR res = (*proc)();
				CheckDlgButton(hdlg, DRV_DTCLOG, 0 != res);
			}
			else
				EnableWindow(GetDlgItem(hdlg, DRV_DTCLOG), FALSE);
			if (hmodule)
				FreeLibrary(hmodule);
#else
			ShowWindow(GetDlgItem(hdlg, DRV_DTCLOG), SW_HIDE);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
			break;

		case WM_COMMAND:
			ci = (ConnInfo *) GetWindowLongPtr(hdlg, DWLP_USER);
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
					getDriversDefaultsOfCi(ci, &defval);
					GetDlgItemText(hdlg, DS_LOGDIR, logdir, sizeof(logdir));
					if (logdir[0] && (logmsg = IsAnExistingDirectory(logdir)) != NULL)
					{
						MessageBox(hdlg, "Folder for logging error", logmsg, MB_ICONEXCLAMATION | MB_OK);
						break;
					}
					setGlobalCommlog(IsDlgButtonChecked(hdlg, DRV_COMMLOG));
					setGlobalDebug(IsDlgButtonChecked(hdlg, DRV_DEBUG));
					writeGlobalLogs();
					if (writeDriversDefaults(ci->drivername, &defval) < 0)
						MessageBox(hdlg, "Sorry, impossible to update the values\nWrite permission seems to be needed", "Update Error", MB_ICONEXCLAMATION | MB_OK);
					setLogDir(logdir[0] ? logdir : NULL);
#ifdef _HANDLE_ENLIST_IN_DTC_
					hmodule = DtcProc("SetMsdtclog", &proc);
					if (proc)
						(*proc)(IsDlgButtonChecked(hdlg, DRV_DTCLOG));
					if (hmodule)
						FreeLibrary(hmodule);
#endif /* _HANDLE_ENLIST_IN_DTC_ */

				case IDCANCEL:
					EndDialog(hdlg, GET_WM_COMMAND_ID(wParam, lParam) == IDOK);
					return TRUE;
			}
	}

	finalize_globals(&defval);
	return FALSE;
}

static void
CtrlCheckButton(HWND hdlg, int nIDcheck, int nIDint)
{
	BOOL	bTranslated;

	switch (Button_GetCheck(GetDlgItem(hdlg, nIDcheck)))
	{
		case BST_CHECKED:
			if (!GetDlgItemInt(hdlg, nIDint, &bTranslated, FALSE))
			{
				ShowWindow(GetDlgItem(hdlg, nIDint), SW_SHOW);
				if (bTranslated)
					SetDlgItemInt(hdlg, nIDint, INIT_DISP_LOGVAL, FALSE);
			}
			break;
		case BST_UNCHECKED:
			if (GetDlgItemInt(hdlg, nIDint, &bTranslated, FALSE))
			{
				ShowWindow(GetDlgItem(hdlg, nIDint), SW_HIDE);
				SetDlgItemInt(hdlg, nIDint, 0, FALSE);
			}
			break;
	}
}

LRESULT			CALLBACK
ds_options1Proc(HWND hdlg,
				   UINT wMsg,
				   WPARAM wParam,
				   LPARAM lParam)
{
	ConnInfo   *ci;
	char	strbuf[128];

// if (WM_INITDIALOG == wMsg || WM_COMMAND == wMsg)
// MYLOG(0, "entering wMsg=%d in\n", wMsg);
	switch (wMsg)
	{
		case WM_INITDIALOG:
			SetWindowLongPtr(hdlg, DWLP_USER, lParam);		/* save for OK etc */
			ci = (ConnInfo *) lParam;
			if (ci && ci->dsn && ci->dsn[0])
			{
				DWORD	cmd;
				char	fbuf[64];

				cmd = LoadString(s_hModule,
						IDS_ADVANCE_OPTION_DSN1,
						fbuf,
						sizeof(fbuf));
				if (cmd <= 0)
					STRCPY_FIXED(fbuf, "Advanced Options (%s) 1/3");
				SPRINTF_FIXED(strbuf, fbuf, ci->dsn);
				SetWindowText(hdlg, strbuf);
			}
			else
			{
				LoadString(s_hModule, IDS_ADVANCE_OPTION_CON1, strbuf, sizeof(strbuf));
				SetWindowText(hdlg, strbuf);
				ShowWindow(GetDlgItem(hdlg, IDAPPLY), SW_HIDE);
			}
			driver_optionsDraw(hdlg, ci, 1, FALSE);
			break;

		case WM_COMMAND:
			ci = (ConnInfo *) GetWindowLongPtr(hdlg, DWLP_USER);
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
					driver_options_update(hdlg, ci);

				case IDCANCEL:
					EndDialog(hdlg, GET_WM_COMMAND_ID(wParam, lParam) == IDOK);
					return TRUE;

				case IDAPPLY:
					driver_options_update(hdlg, ci);
					SendMessage(GetWindow(hdlg, GW_OWNER), WM_COMMAND, wParam, lParam);
					break;

				case IDDEFAULTS:
					driver_optionsDraw(hdlg, ci, 0, FALSE);
					break;

				case ID2NDPAGE:
					driver_options_update(hdlg, ci);
					EndDialog(hdlg, FALSE);
					DialogBoxParam(s_hModule,
								   MAKEINTRESOURCE(DLG_OPTIONS_DS),
								   hdlg, ds_options2Proc, (LPARAM) ci);
					break;
				case ID3RDPAGE:
					driver_options_update(hdlg, ci);
					EndDialog(hdlg, FALSE);
					DialogBoxParam(s_hModule,
								   MAKEINTRESOURCE(DLG_OPTIONS_DS3),
								   hdlg, ds_options3Proc, (LPARAM) ci);
					break;
				case DRV_COMMLOG:
				case DS_COMMLOG:
					CtrlCheckButton(hdlg, DRV_COMMLOG, DS_COMMLOG);
					break;
				case DRV_DEBUG:
				case DS_DEBUG:
					CtrlCheckButton(hdlg, DRV_DEBUG, DS_DEBUG);
					break;
			}
	}

	return FALSE;
}

static int
ds_options_update(HWND hdlg, ConnInfo *ci)
{
	char		buf[128];

	MYLOG(0, "entering got ci=%p\n", ci);

	/* Readonly */
	ITOA_FIXED(ci->onlyread, IsDlgButtonChecked(hdlg, DS_READONLY));

	/* Issue rollback command on error */
	if (IsDlgButtonChecked(hdlg, DS_NO_ROLLBACK))
		ci->rollback_on_error = 0;
	else if (IsDlgButtonChecked(hdlg, DS_TRANSACTION_ROLLBACK))
		ci->rollback_on_error = 1;
	else if (IsDlgButtonChecked(hdlg, DS_STATEMENT_ROLLBACK))
		ci->rollback_on_error = 2;
	else
		/* no button is checked */
		ci->rollback_on_error = -1;

	/* Int8 As */
	if (IsDlgButtonChecked(hdlg, DS_INT8_AS_DEFAULT))
		ci->int8_as = 0;
	else if (IsDlgButtonChecked(hdlg, DS_INT8_AS_BIGINT))
		ci->int8_as = SQL_BIGINT;
	else if (IsDlgButtonChecked(hdlg, DS_INT8_AS_NUMERIC))
		ci->int8_as = SQL_NUMERIC;
	else if (IsDlgButtonChecked(hdlg, DS_INT8_AS_DOUBLE))
		ci->int8_as = SQL_DOUBLE;
	else if (IsDlgButtonChecked(hdlg, DS_INT8_AS_INT4))
		ci->int8_as = SQL_INTEGER;
	else
		ci->int8_as = SQL_VARCHAR;

	GetDlgItemText(hdlg, DS_EXTRA_OPTIONS, buf, sizeof(buf));
	setExtraOptions(ci, buf, NULL);
	ITOA_FIXED(ci->show_system_tables, IsDlgButtonChecked(hdlg, DS_SHOWSYSTEMTABLES));

	ITOA_FIXED(ci->row_versioning, IsDlgButtonChecked(hdlg, DS_ROWVERSIONING));
	ci->lf_conversion = IsDlgButtonChecked(hdlg, DS_LFCONVERSION);
	ci->true_is_minus1 = IsDlgButtonChecked(hdlg, DS_TRUEISMINUS1);
	ci->allow_keyset = IsDlgButtonChecked(hdlg, DS_UPDATABLECURSORS);
	ci->use_server_side_prepare = IsDlgButtonChecked(hdlg, DS_SERVERSIDEPREPARE);
	ci->bytea_as_longvarbinary = IsDlgButtonChecked(hdlg, DS_BYTEAASLONGVARBINARY);
	/*ci->lower_case_identifier = IsDlgButtonChecked(hdlg, DS_LOWERCASEIDENTIFIER);*/

	/* OID Options */
	ITOA_FIXED(ci->fake_oid_index, IsDlgButtonChecked(hdlg, DS_FAKEOIDINDEX));
	ITOA_FIXED(ci->show_oid_column, IsDlgButtonChecked(hdlg, DS_SHOWOIDCOLUMN));

	/* Datasource Connection Settings */
	{
		char conn_settings[LARGE_REGISTRY_LEN];
		GetDlgItemText(hdlg, DS_CONNSETTINGS, conn_settings, sizeof(conn_settings));
		if ('\0' != conn_settings[0])
			STR_TO_NAME(ci->conn_settings, conn_settings);
	}

	/* TCP KEEPALIVE */
	ci->disable_keepalive = IsDlgButtonChecked(hdlg, DS_DISABLE_KEEPALIVE);
	if (ci->disable_keepalive)
	{
		ci->keepalive_idle = -1;
		ci->keepalive_interval = -1;
	}
	else
	{
		char	temp[64];
		int	val;

		GetDlgItemText(hdlg, DS_KEEPALIVETIME, temp, sizeof(temp));
		if  (val = atoi(temp), 0 == val)
			val = -1;
		ci->keepalive_idle = val;
		GetDlgItemText(hdlg, DS_KEEPALIVEINTERVAL, temp, sizeof(temp));
		if  (val = atoi(temp), 0 == val)
			val = -1;
		ci->keepalive_interval = val;
	}
	return 0;
}

LRESULT			CALLBACK
ds_options2Proc(HWND hdlg,
			   UINT wMsg,
			   WPARAM wParam,
			   LPARAM lParam)
{
	ConnInfo   *ci;
	char		buf[128];
	DWORD		cmd;
	BOOL		enable;

// if (WM_INITDIALOG == wMsg || WM_COMMAND == wMsg)
// MYLOG(0, "entering wMsg=%d in\n", wMsg);
	switch (wMsg)
	{
		case WM_INITDIALOG:
			ci = (ConnInfo *) lParam;
			SetWindowLongPtr(hdlg, DWLP_USER, lParam);		/* save for OK */

			/* Change window caption */
			if (ci && ci->dsn && ci->dsn[0])
			{
				char	fbuf[64];

				cmd = LoadString(s_hModule,
						IDS_ADVANCE_OPTION_DSN2,
						fbuf,
						sizeof(fbuf));
				if (cmd <= 0)
					STRCPY_FIXED(fbuf, "Advanced Options (%s) 2/3");
				SPRINTF_FIXED(buf, fbuf, ci->dsn);
				SetWindowText(hdlg, buf);
			}
			else
			{
				LoadString(s_hModule, IDS_ADVANCE_OPTION_CON2, buf, sizeof(buf));
				SetWindowText(hdlg, buf);
				ShowWindow(GetDlgItem(hdlg, IDAPPLY), SW_HIDE);				}

			/* Readonly */
			CheckDlgButton(hdlg, DS_READONLY, atoi(ci->onlyread));

			/* Protocol */
			enable = (ci->sslmode[0] == SSLLBYTE_DISABLE || ci->username[0] == '\0');

			/* How to issue Rollback */
			switch (ci->rollback_on_error)
			{
				case 0:
					CheckDlgButton(hdlg, DS_NO_ROLLBACK, 1);
					break;
				case 1:
					CheckDlgButton(hdlg, DS_TRANSACTION_ROLLBACK, 1);
					break;
				case 2:
					CheckDlgButton(hdlg, DS_STATEMENT_ROLLBACK, 1);
					break;
			}

			/* Int8 As */
			switch (ci->int8_as)
			{
				case SQL_BIGINT:
					CheckDlgButton(hdlg, DS_INT8_AS_BIGINT, 1);
					break;
				case SQL_NUMERIC:
					CheckDlgButton(hdlg, DS_INT8_AS_NUMERIC, 1);
					break;
				case SQL_VARCHAR:
					CheckDlgButton(hdlg, DS_INT8_AS_VARCHAR, 1);
					break;
				case SQL_DOUBLE:
					CheckDlgButton(hdlg, DS_INT8_AS_DOUBLE, 1);
					break;
				case SQL_INTEGER:
					CheckDlgButton(hdlg, DS_INT8_AS_INT4, 1);
					break;
				default:
					CheckDlgButton(hdlg, DS_INT8_AS_DEFAULT, 1);
			}
			SPRINTF_FIXED(buf, "0x%x", getExtraOptions(ci));
			SetDlgItemText(hdlg, DS_EXTRA_OPTIONS, buf);

			CheckDlgButton(hdlg, DS_SHOWOIDCOLUMN, atoi(ci->show_oid_column));
			CheckDlgButton(hdlg, DS_FAKEOIDINDEX, atoi(ci->fake_oid_index));
			CheckDlgButton(hdlg, DS_ROWVERSIONING, atoi(ci->row_versioning));
			CheckDlgButton(hdlg, DS_SHOWSYSTEMTABLES, atoi(ci->show_system_tables));
			CheckDlgButton(hdlg, DS_LFCONVERSION, ci->lf_conversion);
			CheckDlgButton(hdlg, DS_TRUEISMINUS1, ci->true_is_minus1);
			CheckDlgButton(hdlg, DS_UPDATABLECURSORS, ci->allow_keyset);
			CheckDlgButton(hdlg, DS_SERVERSIDEPREPARE, ci->use_server_side_prepare);
			CheckDlgButton(hdlg, DS_BYTEAASLONGVARBINARY, ci->bytea_as_longvarbinary);
			/*CheckDlgButton(hdlg, DS_LOWERCASEIDENTIFIER, ci->lower_case_identifier);*/

			EnableWindow(GetDlgItem(hdlg, DS_FAKEOIDINDEX), atoi(ci->show_oid_column));

			/* Datasource Connection Settings */
			SetDlgItemText(hdlg, DS_CONNSETTINGS, SAFE_NAME(ci->conn_settings));
			/* KEEPALIVE */
			enable = (0 == (ci->extra_opts & BIT_DISABLE_KEEPALIVE));
			CheckDlgButton(hdlg, DS_DISABLE_KEEPALIVE, !enable);
			if (enable)
			{
				if (ci->keepalive_idle > 0)
				{
					ITOA_FIXED(buf, ci->keepalive_idle);
					SetDlgItemText(hdlg, DS_KEEPALIVETIME, buf);
				}
				if (ci->keepalive_interval > 0)
				{
					ITOA_FIXED(buf, ci->keepalive_interval);
					SetDlgItemText(hdlg, DS_KEEPALIVEINTERVAL, buf);
				}
			}
			break;

		case WM_COMMAND:
			ci = (ConnInfo *) GetWindowLongPtr(hdlg, DWLP_USER);
			switch (cmd = GET_WM_COMMAND_ID(wParam, lParam))
			{
				case DS_SHOWOIDCOLUMN:
					MYLOG(0, "WM_COMMAND: DS_SHOWOIDCOLUMN\n");
					EnableWindow(GetDlgItem(hdlg, DS_FAKEOIDINDEX), IsDlgButtonChecked(hdlg, DS_SHOWOIDCOLUMN));
					return TRUE;
				case DS_DISABLE_KEEPALIVE:
					MYLOG(0, "WM_COMMAND: DS_SHOWOIDCOLUMN\n");
					EnableWindow(GetDlgItem(hdlg, DS_KEEPALIVETIME), !IsDlgButtonChecked(hdlg, cmd));
					EnableWindow(GetDlgItem(hdlg, DS_KEEPALIVEINTERVAL), !IsDlgButtonChecked(hdlg, cmd));
					return TRUE;

				case IDOK:
					ds_options_update(hdlg, ci);
				case IDCANCEL:
					EndDialog(hdlg, IDOK == cmd);
					return TRUE;
				case IDAPPLY:
					ds_options_update(hdlg, ci);
					SendMessage(GetWindow(hdlg, GW_OWNER), WM_COMMAND, wParam, lParam);
					break;
				case ID1STPAGE:
					ds_options_update(hdlg, ci);
					EndDialog(hdlg, cmd == IDOK);
					DialogBoxParam(s_hModule,
						MAKEINTRESOURCE(DLG_OPTIONS_DRV),
						   hdlg, ds_options1Proc, (LPARAM) ci);
					break;
				case ID3RDPAGE:
					ds_options_update(hdlg, ci);
					EndDialog(hdlg, cmd == IDOK);
					DialogBoxParam(s_hModule,
						MAKEINTRESOURCE(DLG_OPTIONS_DS3),
						   hdlg, ds_options3Proc, (LPARAM) ci);
					break;
			}
	}

	return FALSE;
}

static int
ds_options3Draw(HWND hdlg, const ConnInfo *ci)
{
	BOOL	enable = TRUE;
	static BOOL defset = FALSE;

MYLOG(0, "entering\n");
#ifdef	_HANDLE_ENLIST_IN_DTC_
	switch (ci->xa_opt)
	{
		case 0:
			enable = FALSE;
			break;
		case DTC_CHECK_LINK_ONLY:
			CheckDlgButton(hdlg, DS_DTC_LINK_ONLY, 1);
			break;
		case DTC_CHECK_BEFORE_LINK:
			CheckDlgButton(hdlg, DS_DTC_SIMPLE_PRECHECK, 1);
			break;
		case DTC_CHECK_RM_CONNECTION:
			CheckDlgButton(hdlg, DS_DTC_CONFIRM_RM_CONNECTION, 1);
			break;
	}
#else
	enable = FALSE;
#endif /* _HANDLE_ENLIST_IN_DTC_ */
	if (!enable)
	{
		EnableWindow(GetDlgItem(hdlg, DS_DTC_LINK_ONLY), enable);
		EnableWindow(GetDlgItem(hdlg, DS_DTC_SIMPLE_PRECHECK), enable);
		EnableWindow(GetDlgItem(hdlg, DS_DTC_CONFIRM_RM_CONNECTION), enable);
	}
	/* Datasource libpq parameters */
	SetDlgItemText(hdlg, DS_LIBPQOPT, SAFE_NAME(ci->pqopt));

	return 0;
}

void *PQconninfoParse(const char *, char **);
void PQconninfoFree(void *);
typedef void *(*PQCONNINFOPARSEPROC)(const char *, char **);
typedef void (*PQCONNINFOFREEPROC)(void *); 
static int
ds_options3_update(HWND hdlg, ConnInfo *ci)
{
	void	*connOpt = NULL;
	char	*ermsg = NULL;
	char	pqopt[LARGE_REGISTRY_LEN];
	HMODULE	hmodule;
	PQCONNINFOPARSEPROC	pproc = NULL;
	PQCONNINFOFREEPROC	fproc = NULL;	

	MYLOG(0, "entering got ci=%p\n", ci);

	/* Datasource libpq parameters */
	GetDlgItemText(hdlg, DS_LIBPQOPT, pqopt, sizeof(pqopt));
	if (hmodule = LoadLibraryEx("libpq.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH), NULL != hmodule)
	{
		pproc = (PQCONNINFOPARSEPROC) GetProcAddress(hmodule, "PQconninfoParse");
		if (NULL != pproc && NULL == (connOpt= (*pproc)(pqopt, &ermsg)))
		{
			const char *logmsg = "libpq parameter error";

			MessageBox(hdlg, ermsg ? ermsg : "memory over?", logmsg, MB_ICONEXCLAMATION | MB_OK);
			if (NULL != ermsg)
				free(ermsg);
			FreeLibrary(hmodule);

			return 1;
		}
		fproc = (PQCONNINFOFREEPROC) GetProcAddress(hmodule, "PQconninfoFree");
		if (fproc)
			(*fproc)(connOpt);
		FreeLibrary(hmodule);
	}
	STR_TO_NAME(ci->pqopt, pqopt);

#ifdef	_HANDLE_ENLIST_IN_DTC_
	if (IsDlgButtonChecked(hdlg, DS_DTC_LINK_ONLY))
		ci->xa_opt = DTC_CHECK_LINK_ONLY;
	else if (IsDlgButtonChecked(hdlg, DS_DTC_SIMPLE_PRECHECK))
		ci->xa_opt = DTC_CHECK_BEFORE_LINK;
	else if (IsDlgButtonChecked(hdlg, DS_DTC_CONFIRM_RM_CONNECTION))
		ci->xa_opt = DTC_CHECK_RM_CONNECTION;
	else
		ci->xa_opt = 0;
#endif /* _HANDLE_ENLIST_IN_DTC_ */

	return 0;
}

LRESULT			CALLBACK
ds_options3Proc(HWND hdlg,
			   UINT wMsg,
			   WPARAM wParam,
			   LPARAM lParam)
{
	ConnInfo   *ci, tmpInfo;
	char		buf[128];
	DWORD		cmd;

if (WM_INITDIALOG == wMsg || WM_COMMAND == wMsg)
MYLOG(0, "entering wMsg=%d\n", wMsg);
	switch (wMsg)
	{
		case WM_INITDIALOG:
			ci = (ConnInfo *) lParam;
			SetWindowLongPtr(hdlg, DWLP_USER, lParam);		/* save for OK */

			/* Change window caption */
			if (ci && ci->dsn && ci->dsn[0])
			{
				char	fbuf[64];

				cmd = LoadString(s_hModule,
						IDS_ADVANCE_OPTION_DSN3,
						fbuf,
						sizeof(fbuf));
				if (cmd <= 0)
					STRCPY_FIXED(fbuf, "Advanced Options (%s) 3/3");
				SPRINTF_FIXED(buf, fbuf, ci->dsn);
				SetWindowText(hdlg, buf);
			}
			else
			{
				LoadString(s_hModule, IDS_ADVANCE_OPTION_CON3, buf, sizeof(buf));
				SetWindowText(hdlg, buf);
				ShowWindow(GetDlgItem(hdlg, IDAPPLY), SW_HIDE);				}

			ds_options3Draw(hdlg, ci);
			break;

		case WM_COMMAND:
			ci = (ConnInfo *) GetWindowLongPtr(hdlg, DWLP_USER);
			switch (cmd = GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
					ds_options3_update(hdlg, ci);
				case IDCANCEL:
					EndDialog(hdlg, IDOK == cmd);
					return TRUE;
				case IDAPPLY:
					ds_options3_update(hdlg, ci);
					SendMessage(GetWindow(hdlg, GW_OWNER), WM_COMMAND, wParam, lParam);
					break;
				case IDC_TEST:
					CC_copy_conninfo(&tmpInfo, ci);
					ds_options3_update(hdlg, &tmpInfo);
					test_connection(hdlg, &tmpInfo, TRUE);
					CC_conninfo_release(&tmpInfo);
					break;
				case ID1STPAGE:
					ds_options3_update(hdlg, ci);
					EndDialog(hdlg, cmd == IDOK);
					DialogBoxParam(s_hModule,
						MAKEINTRESOURCE(DLG_OPTIONS_DRV),
						   hdlg, ds_options1Proc, (LPARAM) ci);
					break;
				case ID2NDPAGE:
					ds_options3_update(hdlg, ci);
					EndDialog(hdlg, cmd == IDOK);
					DialogBoxParam(s_hModule,
						MAKEINTRESOURCE(DLG_OPTIONS_DS),
						   hdlg, ds_options2Proc, (LPARAM) ci);
					break;
			}
	}

	return FALSE;
}


typedef	SQLRETURN (SQL_API *SQLAPIPROC)();
static int
makeDriversList(HWND lwnd, const ConnInfo *ci)
{
	HMODULE	hmodule;
	SQLHENV	henv;
	int	lcount = 0;
	LRESULT iidx;
	char	drvname[64], drvatt[128];
	SQLUSMALLINT	direction = SQL_FETCH_FIRST;
	SQLSMALLINT	drvncount, drvacount;
	SQLRETURN	ret;
	SQLAPIPROC	addr;

	hmodule = GetModuleHandle("ODBC32");
	if (!hmodule)	return lcount;
	addr = (SQLAPIPROC) GetProcAddress(hmodule, "SQLAllocEnv");
	if (!addr)	return lcount;
	ret = (*addr)(&henv);
	if (SQL_SUCCESS != ret)	return lcount;
	do
	{
		ret = SQLDrivers(henv, direction,
						 drvname, sizeof(drvname), &drvncount,
						 drvatt, sizeof(drvatt), &drvacount);
		if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
			break;
		if (strnicmp(drvname, "postgresql", 10) == 0)
		{
			iidx = SendMessage(lwnd, LB_ADDSTRING, 0, (LPARAM) drvname);
			if (LB_ERR != iidx && stricmp(drvname, ci->drivername) == 0)
			{
				SendMessage(lwnd, LB_SETCURSEL, (WPARAM) iidx, (LPARAM) 0);
			}
			lcount++;
		}
		direction = SQL_FETCH_NEXT;
	} while (1);
	addr = (SQLAPIPROC) GetProcAddress(hmodule, "SQLFreeEnv");
	if (addr)
		(*addr)(henv);

	return lcount;
}

LRESULT		CALLBACK
manage_dsnProc(HWND hdlg, UINT wMsg,
			   WPARAM wParam, LPARAM lParam)
{
	LPSETUPDLG	lpsetupdlg;
	ConnInfo	*ci;
	HWND		lwnd;
	LRESULT		sidx;
	char		drvname[64];

	switch (wMsg)
	{
		case WM_INITDIALOG:
			SetWindowLongPtr(hdlg, DWLP_USER, lParam);
			lpsetupdlg = (LPSETUPDLG) lParam;
			ci = &lpsetupdlg->ci;
			lwnd = GetDlgItem(hdlg, IDC_DRIVER_LIST);
			makeDriversList(lwnd, ci);
			break;

		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
				case IDOK:
					lpsetupdlg = (LPSETUPDLG) GetWindowLongPtr(hdlg, DWLP_USER);
					lwnd = GetDlgItem(hdlg, IDC_DRIVER_LIST);
					sidx = SendMessage(lwnd, LB_GETCURSEL,
									   (WPARAM) 0, (LPARAM) 0);
					if (LB_ERR == sidx)
						return FALSE;
					sidx = SendMessage(lwnd, LB_GETTEXT,
									   (WPARAM) sidx, (LPARAM) drvname);
					if (LB_ERR == sidx)
						return FALSE;
					ChangeDriverName(hdlg, lpsetupdlg, drvname);

				case IDCANCEL:
					EndDialog(hdlg, GET_WM_COMMAND_ID(wParam, lParam) == IDOK);
					return TRUE;
			}
	}

	return FALSE;
}

#endif /* WIN32 */
