/*------
 * Module:			pgxalib.cpp
 *
 * Description:
 *		This module implements XA like routines
 *			invoked from MSDTC process.
 *
 *		xa_open(), xa_close(), xa_commit(),
 *		xa_rollback() and xa_recover()
 *		are really invoked AFAIC.
 *-------
 */

#include <oleTx2xa.h>
/*#define	_SLEEP_FOR_TEST_*/
#include <sqlext.h>
#include <odbcinst.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <process.h>
#include <time.h>

#include <string>
#include <map>
#include <vector>


EXTERN_C static void mylog(const char *fmt,...);

using namespace std;

class	XAConnection
{
private:
	string	connstr;
	string	dsnstr;
	HDBC	xaconn;
	vector<string>	qvec;
	int		pos;
	bool	immediateConnection;
	string	sqlState;
	string	errMsg;

	void	parse_xa_info();
public:
	XAConnection(LPCTSTR str) : connstr(str), xaconn(NULL), pos(-1), immediateConnection(false) {parse_xa_info();}
	~XAConnection();
	void	Reset(void);
	HDBC	ActivateConnection(void);
	void	SetPos(int spos) {pos = spos;}
	HDBC	GetConnection(void) const {return xaconn;}
	vector<string>	&GetResultVec(void) {return qvec;}
	int	GetPos(void) {return pos;}
	const string &GetConnstr(void) {return connstr;}
	const string &GetDsnstr(void) {return dsnstr;}
	bool	GetImmediateConnection(void) {return immediateConnection;}
	bool	AuthenticationError(void) {return sqlState == "08001" && errMsg.find("could not connect") == std::string::npos;}
};

static class INIT_CRIT
{
private:
public:
	bool			cs_init;
	CRITICAL_SECTION	map_cs;
	CRITICAL_SECTION	mylog_cs;
	map<int, XAConnection>	xatab;
	FILE			*LOGFP;
	HENV			env;

	INIT_CRIT() : LOGFP(NULL), env(NULL)
	{
		InitializeCriticalSection(&map_cs);
		InitializeCriticalSection(&mylog_cs);
		cs_init = true;
	}
	~INIT_CRIT()
	{
// mylog("Leaving INIT_CRIT\n");
		leave();
	}
	void leave()
	{
		if (cs_init)
		{
			mylog("leaving pgxadll\n");
			xatab.clear();
			FreeEnv();
			if (LOGFP)
				fclose(LOGFP);
			LOGFP = NULL;
			DeleteCriticalSection(&mylog_cs);
			DeleteCriticalSection(&map_cs);
			cs_init = false;
		}
	}
	void finalize()
	{
		leave();
	}
	void FreeEnv()
	{
		if (env)
		{
			mylog("Freeing env\n");
			SQLFreeHandle(SQL_HANDLE_ENV, env);
			env = NULL;
		}
	}
} init_crit;
#define	MLOCK_ACQUIRE	EnterCriticalSection(&init_crit.map_cs)
#define	MLOCK_RELEASE	LeaveCriticalSection(&init_crit.map_cs)

static int dtclog = 0;

XAConnection::~XAConnection()
{
}

void XAConnection::Reset()
{
	qvec.clear();
	if (xaconn)
	{
		mylog("about to Disconnect\n");
		SQLDisconnect(xaconn);
		mylog("Freeing HANDLE_DBC\n");
		SQLFreeHandle(SQL_HANDLE_DBC, xaconn);
	}
}

HDBC	XAConnection::ActivateConnection(void)
{
	RETCODE	ret;

	MLOCK_ACQUIRE;
	if (!init_crit.env)
	{
		ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &init_crit.env);
		if (!SQL_SUCCEEDED(ret))
		{
			MLOCK_RELEASE;
			return NULL;
		}
	}
	MLOCK_RELEASE;
	if (xaconn)
		return xaconn;

	ret = SQLSetEnvAttr(init_crit.env, SQL_ATTR_ODBC_VERSION, (PTR) SQL_OV_ODBC3, 0);
	ret = SQLAllocHandle(SQL_HANDLE_DBC, init_crit.env, &xaconn);
	if (!SQL_SUCCEEDED(ret))
		return NULL;

	ret = SQLDriverConnect(xaconn, NULL,
		 (SQLCHAR *) dsnstr.c_str(), SQL_NTS, NULL, SQL_NULL_DATA, NULL, SQL_DRIVER_COMPLETE);
	if (SQL_SUCCEEDED(ret))
		return xaconn;
	SQLCHAR	sqlstate[8], errmsg[256];

	SQLGetDiagRec(SQL_HANDLE_DBC, xaconn,
			1, sqlstate, NULL, errmsg,
			sizeof(errmsg), NULL);
	mylog("first SQLDriverConnect return=%d sqlstate=%s error=%s\n", ret, sqlstate, errmsg);
	sqlState = (char *) sqlstate;
	errMsg = (char *) errmsg;
	if (!AuthenticationError())
	{
		SQLFreeHandle(SQL_HANDLE_DBC, xaconn);
		return NULL;
	}
	sqlState.empty();
	errMsg.empty();
	ret = SQLDriverConnect(xaconn, NULL,
		 (SQLCHAR *) (dsnstr + ";Username=postgres;Password=postgres;sslmode=allow").c_str(), SQL_NTS, NULL, SQL_NULL_DATA, NULL, SQL_DRIVER_COMPLETE);
	if (SQL_SUCCEEDED(ret))
	{
		mylog("second SQLDriverConnect success\n");
		return xaconn;
	}

	SQLGetDiagRec(SQL_HANDLE_DBC, xaconn,
			1, sqlstate, NULL, errmsg,
			sizeof(errmsg), NULL);
	mylog("second SQLDriverConnect return=%d sqlstate=%s error=%s\n", ret, sqlstate, errmsg);
	sqlState = (char *) sqlstate;
	errMsg = (char *) errmsg;
	return NULL;
}

#define _BUILD_DLL_
#ifdef _BUILD_DLL_

EXTERN_C {

#define	DIRSEPARATOR	"\\"
#define	PG_BINARY_A	"ab"
#define	MYLOGDIR	"c:"
#define	MYLOGFILE	"mylog_"

static void
generate_filename(const char *dirname, const char *prefix, char *filename)
{
	int			pid = 0;

	pid = _getpid();
	if (dirname == 0 || filename == 0)
		return;

	strcpy(filename, dirname);
	strcat(filename, DIRSEPARATOR);
	if (prefix != 0)
		strcat(filename, prefix);
	sprintf(filename, "%s%u%s", filename, pid, ".log");
	return;
}

static void FreeEnv()
{
	init_crit.FreeEnv();
}

#define	DTCLOGDIR	"c:\\pgdtclog"
#include <direct.h>

static const char * const DBMSNAME = "PostgreSQL";
static const char * const KEY_NAME = "MsdtcLog";
static const char * const ODBCINST_INI = "ODBCINST.INI";
INT_PTR FAR WINAPI GetMsdtclog()
{
	char	temp[16];

        SQLGetPrivateProfileString(DBMSNAME, KEY_NAME, "", temp, sizeof(temp), ODBCINST_INI);
	dtclog = atoi(temp);
	return dtclog;
}
INT_PTR FAR WINAPI SetMsdtclog(int dtclog)
{
	char	temp[16];

        sprintf(temp, "%d", dtclog);
        SQLWritePrivateProfileString(DBMSNAME, KEY_NAME, temp, ODBCINST_INI);
	return dtclog;
}

static BOOL
output_mylog()
{
	return (0 != dtclog);
}

static void
mylog(const char *fmt,...)
{
	va_list		args;
	char		filebuf[80];
	static	BOOL	init = TRUE;
	FILE	*logfp = init_crit.LOGFP;

	if (!output_mylog())
		return;
	EnterCriticalSection(&init_crit.mylog_cs);
	va_start(args, fmt);

	if (init)
	{
		if (!logfp)
		{
			generate_filename(MYLOGDIR, MYLOGFILE, filebuf);
			logfp = fopen(filebuf, PG_BINARY_A);
		}
		if (!logfp)
		{
			generate_filename(DTCLOGDIR, MYLOGFILE, filebuf);
			logfp = fopen(filebuf, PG_BINARY_A);
#ifdef	WIN32
			if (NULL == logfp)
			{
				if (0 == _mkdir(DTCLOGDIR))
					logfp = fopen(filebuf, PG_BINARY_A);
			}
#endif /* WIN32 */
		}
		if (logfp)
		{
			setbuf(logfp, NULL);
			init_crit.LOGFP = logfp;
		}
	}
	init = FALSE;
	if (logfp)
	{
		time_t	ntime;
		char	ctim[128];

		time(&ntime);
		strcpy(ctim, ctime(&ntime));
		ctim[strlen(ctim) - 1] = '\0';
		fprintf(logfp, "[%d.%d(%s)]", GetCurrentProcessId(), GetCurrentThreadId(), ctim);
		vfprintf(logfp, fmt, args);
	}
	va_end(args);
	LeaveCriticalSection(&init_crit.mylog_cs);
}
static int	initialize_globals(void)
{
	static	int	init = 1;

	if (!init)
		init = 0;

	return 0;
}

static void XatabClear(void);
static void finalize_globals(void)
{
	/* my(q)log is unavailable from here */
	mylog("DETACHING From MSDTC PROCESS\n");
	init_crit.finalize();
}

HINSTANCE s_hModule;               /* Saved module handle. */
/*      This is where the Driver Manager attaches to this Driver */
BOOL	WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	WORD	wVersionRequested;
	WSADATA	wsaData;

	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			s_hModule = (HINSTANCE) hInst;	/* Save for dialog boxes */

			/* Load the WinSock Library */
			wVersionRequested = MAKEWORD(1, 1);

			if (WSAStartup(wVersionRequested, &wsaData))
				return FALSE;

			/* Verify that this is the minimum version of WinSock */
			if (LOBYTE(wsaData.wVersion) != 1 ||
				HIBYTE(wsaData.wVersion) != 1)
			{
				WSACleanup();
				return FALSE;
			}
			initialize_globals();
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_PROCESS_DETACH:
			finalize_globals();
			WSACleanup();
			return TRUE;

		case DLL_THREAD_DETACH:
			break;

		default:
			break;
	}
	return TRUE;
}

} /* end of EXTERN_C */

#endif /* _BUILD_DLL_ */

static void XatabClear(void)
{
	init_crit.xatab.clear();
}

static const char *XidToText(const XID &xid, char *rtext)
{
	int	glen = xid.gtrid_length, blen = xid.bqual_length;
	int	i, j;

	for (i = 0, j = 0; i < glen; i++, j += 2)
		sprintf(rtext + j, "%02x", (unsigned char) xid.data[i]);
	strcat(rtext, "-"); j++;
	for (; i < glen + blen; i++, j += 2)
		sprintf(rtext + j, "%02x", (unsigned char) xid.data[i]);
	return rtext;
}

static int
pg_hex2bin(const UCHAR *src, UCHAR *dst, int length)
{
	UCHAR		chr;
	const UCHAR	*src_wk;
	UCHAR		*dst_wk;
	int		i, val;
	BOOL		HByte = TRUE;

	for (i = 0, src_wk = src, dst_wk = dst; i < length; i++, src_wk++)
	{
		chr = *src_wk;
		if (!chr)
			break;
		if (chr >= 'a' && chr <= 'f')
			val = chr - 'a' + 10;
		else if (chr >= 'A' && chr <= 'F')
			val = chr - 'A' + 10;
		else
			val = chr - '0';
		if (HByte)
			*dst_wk = (val << 4);
		else
		{
			*dst_wk += val;
			dst_wk++;
		}
		HByte = !HByte;
	}
	return length;
}

static int	TextToXid(XID &xid, const char *rtext)
{
	int	slen, glen, blen;
	char	*sptr;

	slen = (int) strlen(rtext);
	sptr = (char *)strchr(rtext, '-');
	if (sptr)
	{
		glen = (int) (sptr - rtext);
		blen = slen - glen - 1;
	}
	else
	{
		glen = slen;
		blen = 0;
	}
	xid.gtrid_length = glen / 2;
	xid.bqual_length = blen / 2;
	pg_hex2bin((const UCHAR *) rtext, (UCHAR *) &xid.data[0], glen);
	pg_hex2bin((const UCHAR *) sptr + 1, (UCHAR *) &xid.data[glen / 2], blen);
	return (glen + blen) / 2;
}


static	bool bImmediateConnectDefault = false;

#define	CHAR_OPENING_BRACE	'{'
#define	CHAR_CLOSING_BRACE	'}'
#define	CHAR_SEMI_COLON		';'
#define	CHAR_EQUAL		'='
#define	STR_OPENING_BRACE	"{"
#define	STR_CLOSING_BRACE	"}"
#define	STR_SEMI_COLON		";"
#define	STR_EQUAL		"="

#define	KEYWORD_DTC_CHECK	"dtchk"
#define	KEYWORD_DEBUG		"debug"
#define	KEYWORD_ABDEBUG		"B2"

void XAConnection::parse_xa_info()
{
	const char	*cstr, *pstr, *pnstr;
	char		keyword[64], value[64];
	bool		keyhasbr, valhasbr;
	int		debugv = -1;

	cstr = connstr.c_str();

	immediateConnection = bImmediateConnectDefault;
	GetMsdtclog();

	for (pstr = cstr; *pstr;)
	{
		keyhasbr = valhasbr = false;
		if (CHAR_OPENING_BRACE == *pstr)
		{
			keyhasbr = true;
			if (pnstr = strchr(pstr, (int) CHAR_CLOSING_BRACE), NULL != pnstr)
				strncpy_s(keyword, sizeof(keyword), pstr + 1, pnstr - pstr - 1);
			else
				break;
			pstr = pnstr + 1;
			if (CHAR_EQUAL != *pstr)
				break; 
		}
		else
		{
			if (pnstr = strchr(pstr, (int) CHAR_EQUAL), NULL != pnstr)
				strncpy_s(keyword, sizeof(keyword), pstr, pnstr - pstr);
			else
				break;
			pstr = pnstr;
		}
		pstr++;
		if (CHAR_OPENING_BRACE == *pstr)
		{
			valhasbr = true;
			if (pnstr = strchr(pstr, (int) CHAR_CLOSING_BRACE), NULL != pnstr)
				strncpy_s(value, sizeof(value), pstr + 1, pnstr - pstr - 1);
			else
				break;
			pstr = pnstr + 1;
		}
		else
		{
			if (pnstr = strchr(pstr, (int) CHAR_SEMI_COLON), NULL != pnstr)
			{
				strncpy_s(value, sizeof(value), pstr, pnstr - pstr);
				pstr = pnstr;
			}
			else
			{
				strcpy(value, pstr);
				pstr = strchr(pstr, '\0');
			}
		}

		if (0 == _stricmp(keyword, KEYWORD_DTC_CHECK))
			immediateConnection = (0 != atoi(value)); 
		else if (0 == _stricmp(keyword, KEYWORD_DEBUG) ||
			 0 == _stricmp(keyword, KEYWORD_ABDEBUG))
			debugv = atoi(value); 
		else
		{
			if (keyhasbr)
				dsnstr += STR_OPENING_BRACE;
			dsnstr += keyword;
			if (keyhasbr)
				dsnstr += STR_CLOSING_BRACE;
			dsnstr += STR_EQUAL;
			if (valhasbr)
				dsnstr += STR_OPENING_BRACE;
			dsnstr += value;
			if (valhasbr)
				dsnstr += STR_CLOSING_BRACE;
			dsnstr += STR_SEMI_COLON;
		}

		if (NULL == pstr ||
		    CHAR_SEMI_COLON != *pstr)
			break;
		pstr++;
	}
	if (0 != dtclog)
	{
		char	dbgopt[16];

		if (debugv <= 0)
			debugv = 1;
		_snprintf(dbgopt, sizeof(dbgopt), KEYWORD_ABDEBUG "=%d;", debugv);
		dsnstr += dbgopt;
	}
}

EXTERN_C static int __cdecl xa_open(char *xa_info, int rmid, long flags)
{
	int	xartn = XA_OK;
	bool	bActivateConnection = bImmediateConnectDefault;

	mylog("xa_open %s rmid=%d flags=%ld\n", xa_info, rmid, flags);
	MLOCK_ACQUIRE;
	init_crit.xatab.insert(pair<int, XAConnection>(rmid, XAConnection(xa_info)));
	MLOCK_RELEASE;
	map<int, XAConnection>::iterator p;
	p = init_crit.xatab.find(rmid);
	if (p == init_crit.xatab.end())
		xartn = XAER_RMERR;
	else if (p->second.GetImmediateConnection())
	{
		mylog("xa_open calls ActivateConnection %s\n", p->second.GetDsnstr().c_str());
		if (p->second.ActivateConnection())
			xartn = XA_OK;
		else
			xartn = XAER_RMERR;
	}

	mylog("xa_open %s rmid=%d returning %d\n", xa_info, rmid, xartn);
	return	xartn;
}
EXTERN_C static int __cdecl xa_close(char *xa_info, int rmid, long flags)
{
	mylog("xa_close rmid=%d flags=%ld\n", rmid, flags);
	map<int, XAConnection>::iterator p;
	MLOCK_ACQUIRE;
	p = init_crit.xatab.find(rmid);
	if (p != init_crit.xatab.end())
		p->second.Reset();
	init_crit.xatab.erase(rmid);
	if (init_crit.xatab.size() == 0)
		FreeEnv();
	GetMsdtclog();
	MLOCK_RELEASE;
	return XA_OK;
}

//
//	Dummy implementation (not called from MSDTC).
//
EXTERN_C static int __cdecl xa_start(XID *xid, int rmid, long flags)
{
	char	pgxid[258];

	XidToText(*xid, pgxid);
	mylog("xa_start %s rmid=%d flags=%ld\n", pgxid, rmid, flags);
	init_crit.xatab.find(rmid)->second.ActivateConnection();
	return XA_OK;
}
//
//	Dummy implementation (not called from MSDTC).
//
EXTERN_C static int __cdecl xa_end(XID *xid, int rmid, long flags)
{
	char	pgxid[258];

	XidToText(*xid, pgxid);
	mylog("xa_end %s rmid=%d flags=%ld\n", pgxid, rmid, flags);
	return XA_OK;
}

EXTERN_C static int __cdecl xa_rollback(XID *xid, int rmid, long flags)
{
	int	rmcode = XAER_RMERR;
	char	pgxid[258];

	XidToText(*xid, pgxid);
	mylog("xa_rollback %s rmid=%d flags=%ld\n", pgxid, rmid, flags);
	map<int, XAConnection>::iterator p;
	p = init_crit.xatab.find(rmid);
	if (p != init_crit.xatab.end())
	{
		HDBC	conn = p->second.ActivateConnection();
		if (conn)
		{
			SQLCHAR	cmdmsg[512], sqlstate[8];
			HSTMT	stmt;
			RETCODE	ret;

			ret = SQLAllocHandle(SQL_HANDLE_STMT, conn, &stmt);
			if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
			{
				mylog("Statement allocation error\n");
				return rmcode;
			}
			_snprintf((char *) cmdmsg, sizeof(cmdmsg), "ROLLBACK PREPARED '%s'", pgxid);
			ret = SQLExecDirect(stmt, (SQLCHAR *) cmdmsg, SQL_NTS);
			switch (ret)
			{
				case SQL_SUCCESS:
				case SQL_SUCCESS_WITH_INFO:
					rmcode = XA_OK;
					break;
				case SQL_ERROR:
					SQLGetDiagRec(SQL_HANDLE_STMT, stmt,
								  1, sqlstate, NULL, cmdmsg,
								  sizeof(cmdmsg), NULL);
					mylog("xa_commit error %s '%s'\n", sqlstate, cmdmsg);
					if (_stricmp((char *) sqlstate, "42704") == 0)
						rmcode = XA_HEURHAZ;
					break;
			}
			SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		}
	}
	return rmcode;
}
//
//	Dummy implementation (not called from MSDTC).
//	Anyway it's almost impossible to implement this routine properly.
//
EXTERN_C static int __cdecl xa_prepare(XID *xid, int rmid, long flags)
{
	char	pgxid[258];

	XidToText(*xid, pgxid);
	mylog("xa_prepare %s rmid=%d\n", pgxid, rmid);
#ifdef	_SLEEP_FOR_TEST_
	Sleep(2000);
#endif	/* _SLEEP_FOR_TEST_ */
	map<int, XAConnection>::iterator p;
	p = init_crit.xatab.find(rmid);
	if (p != init_crit.xatab.end())
	{
		HDBC	conn = p->second.GetConnection();
		if (conn)
		{
		}
	}
	return XAER_RMERR;
}
EXTERN_C static int __cdecl xa_commit(XID *xid, int rmid, long flags)
{
	int	rmcode = XAER_RMERR;
	char	pgxid[258];

	XidToText(*xid, pgxid);
	mylog("xa_commit %s rmid=%d flags=%ld\n", pgxid, rmid, flags);
#ifdef	_SLEEP_FOR_TEST_
	Sleep(2000);
#endif	/* _SLEEP_FOR_TEST_ */
	map<int, XAConnection>::iterator p;
	p = init_crit.xatab.find(rmid);
	if (p != init_crit.xatab.end())
	{
		HDBC	conn = p->second.ActivateConnection();
		if (conn)
		{
			SQLCHAR	cmdmsg[512], sqlstate[8];
			HSTMT	stmt;
			RETCODE	ret;

			SQLAllocHandle(SQL_HANDLE_STMT, conn, &stmt);
			_snprintf((char *) cmdmsg, sizeof(cmdmsg), "COMMIT PREPARED '%s'", pgxid);
			ret = SQLExecDirect(stmt, (SQLCHAR *) cmdmsg, SQL_NTS);
			switch (ret)
			{
				case SQL_SUCCESS:
				case SQL_SUCCESS_WITH_INFO:
					rmcode = XA_OK;
					break;
				case SQL_ERROR:
					SQLGetDiagRec(SQL_HANDLE_STMT, stmt,
								  1, sqlstate, NULL, cmdmsg,
								  sizeof(cmdmsg), NULL);
					if (_stricmp((char *) sqlstate, "42704") == 0)
						rmcode = XA_HEURHAZ;
					break;
			}
			SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		}
	}
	return rmcode;
}
EXTERN_C static int __cdecl xa_recover(XID *xids, long count, int rmid, long flags)
{
	int	rmcode = XAER_RMERR, rcount;

	mylog("xa_recover rmid=%d count=%d flags=%ld\n", rmid, count, flags);
	map<int, XAConnection>::iterator p;
	p = init_crit.xatab.find(rmid);
	if (p == init_crit.xatab.end())
		return rmcode;
	HDBC	conn = p->second.ActivateConnection();
	if (!conn)
	{
		if (p->second.AuthenticationError())
			rmcode = 0;
		mylog("%s returns %d\n", __FUNCTION__, rmcode);
		return rmcode;
	}
	vector<string>	&vec = p->second.GetResultVec();
	int	pos = p->second.GetPos();
	if ((flags & TMSTARTRSCAN) != 0)
	{
		HSTMT	stmt;
		RETCODE	ret;
		char	buf[512];

		vec.clear();
		SQLAllocHandle(SQL_HANDLE_STMT, conn, &stmt);
		ret = SQLExecDirect(stmt, (SQLCHAR *) "select gid from pg_prepared_xacts", SQL_NTS);
		if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
		{
			SQLFreeHandle(SQL_HANDLE_STMT, stmt);
			pos = -1;
			goto onExit;
		}
		SQLBindCol(stmt, 1, SQL_C_CHAR, buf, sizeof(buf), NULL);
		ret = SQLFetch(stmt);
		while (SQL_NO_DATA_FOUND != ret)
		{
			vec.push_back(buf);
			ret = SQLFetch(stmt);
		}
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		pos = 0;
	}
	rcount = (int) vec.size();
	rmcode = rcount - pos;
	if (rmcode > count)
		rmcode = count;
	for (int i = 0; i < rmcode; i++, pos++)
		TextToXid(xids[i], vec[pos].c_str());

	if ((flags & TMENDRSCAN) != 0)
	{
		vec.clear();
		pos = -1;
	}
	mylog("return count=%d\n", rmcode);
onExit:
	p->second.SetPos(pos);
	return rmcode;
}

//
//	I'm not sure if this is invoked from MSDTC
//	Anyway there's nothing to do with it.
//
EXTERN_C static int __cdecl xa_forget(XID *xid, int rmid, long flags)
{
	char	pgxid[258];

	XidToText(*xid, pgxid);
	mylog("xa_forget %s rmid=%d\n", pgxid, rmid);
	return XA_OK;
}
//
//	I'm not sure if this can be invoked from MSDTC.
//
EXTERN_C static int __cdecl xa_complete(int *handle, int *retval, int rmid, long flags)
{
	mylog("xa_complete rmid=%d\n", rmid);
	return XA_OK;
}

EXTERN_C static xa_switch_t	xapsw = { "psotgres_xa", TMNOMIGRATE,
		0, xa_open, xa_close, xa_start, xa_end, xa_rollback,
		xa_prepare, xa_commit, xa_recover, xa_forget,
		xa_complete};

EXTERN_C HRESULT __cdecl   GetXaSwitch (XA_SWITCH_FLAGS  XaSwitchFlags,
		xa_switch_t **  ppXaSwitch)
{
	mylog("GetXaSwitch called\n");

	GetMsdtclog();
	*ppXaSwitch = &xapsw;
	return	S_OK;
}
