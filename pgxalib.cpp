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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <process.h>
#include <time.h>

#include <string>
#include <map>
#include <vector>


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

	pid = getpid();
	if (dirname == 0 || filename == 0)
		return;

	strcpy(filename, dirname);
	strcat(filename, DIRSEPARATOR);
	if (prefix != 0)
		strcat(filename, prefix);
	sprintf(filename, "%s%u%s", filename, pid, ".log");
	return;
}

static	HENV	env = NULL;
static void FreeEnv()
{
	if (env)
	{
		SQLFreeHandle(SQL_HANDLE_ENV, env);
		env = NULL;
	}
}
static	FILE	*LOGFP = NULL;
static	CRITICAL_SECTION	mylog_cs;

static void
mylog(const char *fmt,...)
{
	va_list		args;
	char		filebuf[80];
	static	BOOL	init = TRUE;

	EnterCriticalSection(&mylog_cs);
	va_start(args, fmt);

	if (init)
	{
		if (!LOGFP)
		{
			generate_filename(MYLOGDIR, MYLOGFILE, filebuf);
			LOGFP = fopen(filebuf, PG_BINARY_A);
		}
		if (!LOGFP)
		{
			generate_filename("C:\\podbclog", MYLOGFILE, filebuf);
			LOGFP = fopen(filebuf, PG_BINARY_A);
		}
		if (LOGFP)
			setbuf(LOGFP, NULL);
	}
	init = FALSE;
	if (LOGFP)
	{
		time_t	ntime;
		char	ctim[128];

		time(&ntime);
		strcpy(ctim, ctime(&ntime));
		ctim[strlen(ctim) - 1] = '\0';
		fprintf(LOGFP, "[%d.%d(%s)]", GetCurrentProcessId(), GetCurrentThreadId(), ctim);
		vfprintf(LOGFP, fmt, args);
	}
	va_end(args);
	LeaveCriticalSection(&mylog_cs);
}
static int	initialize_globals(void)
{
	static	int	init = 1;

	if (!init)
		return 0;
	init = 0;
	InitializeCriticalSection(&mylog_cs);

	return 0;
}

static void XatabClear(void);
static void finalize_globals(void)
{
	XatabClear();
	FreeEnv();
	/* my(q)log is unavailable from here */
	mylog("DETACHING PROCESS\n");
	DeleteCriticalSection(&mylog_cs);
	fclose(LOGFP);
	LOGFP = NULL;
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

using namespace std;

static CRITICAL_SECTION	map_cs;
#define	MLOCK_ACQUIRE	EnterCriticalSection(&map_cs)
#define	MLOCK_RELEASE	LeaveCriticalSection(&map_cs)

class	XAConnection
{
private:
	string	connstr;
	HDBC	xaconn;
	vector<string>	qvec;
	int		pos;
public:
	XAConnection(LPCTSTR str) : connstr(str), xaconn(NULL), pos(-1) {}
	~XAConnection();
	HDBC	ActivateConnection(void);
	void	SetPos(int spos) {pos = spos;}
	HDBC	GetConnection(void) const {return xaconn;}
	vector<string>	&GetResultVec(void) {return qvec;}
	int	GetPos(void) {return pos;}
	string GetConnstr(void) {return connstr;}
};

XAConnection::~XAConnection()
{
	qvec.clear();
	if (xaconn)
		SQLFreeHandle(SQL_HANDLE_DBC, xaconn);
}

HDBC	XAConnection::ActivateConnection(void)
{
	RETCODE	ret;

	MLOCK_ACQUIRE;
	if (!env)
	{
		ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
		if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
			return NULL;
	}
	MLOCK_RELEASE;
	if (!xaconn)
	{
		ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (PTR) SQL_OV_ODBC3, 0);
		ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &xaconn);
		if (SQL_SUCCESS == ret || SQL_SUCCESS_WITH_INFO)
		{
			ret = SQLDriverConnect(xaconn, NULL, (SQLCHAR *) connstr.c_str(), SQL_NTS, NULL, SQL_NULL_DATA, NULL, SQL_DRIVER_COMPLETE);
			if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
			{
				mylog("SQLDriverConnect return=%d\n", ret);
				SQLFreeHandle(SQL_HANDLE_DBC, xaconn);
				xaconn = NULL;
			}
		}
	}
	return xaconn;
}

static map<int, XAConnection>	xatab;

static class INIT_CRIT
{
private:
public:
	INIT_CRIT() {InitializeCriticalSection(&map_cs);}
	~INIT_CRIT()
	{
mylog("Leaving INIT_CRIT\n");
		FreeEnv();
		xatab.clear();
		DeleteCriticalSection(&map_cs);
	}
} init_crit;

static void XatabClear(void)
{
	xatab.clear();
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

	slen = strlen(rtext);
	sptr = strchr(rtext, '-');
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


EXTERN_C static int __cdecl xa_open(char *xa_info, int rmid, long flags)
{
	mylog("xa_open %s rmid=%d flags=%ld\n", xa_info, rmid, flags);
	MLOCK_ACQUIRE;
	xatab.insert(pair<int, XAConnection>(rmid, XAConnection(xa_info)));
	MLOCK_RELEASE;
	return	S_OK;
}
EXTERN_C static int __cdecl xa_close(char *xa_info, int rmid, long flags)
{
	mylog("xa_close rmid=%d flags=%ld\n", rmid, flags);
	MLOCK_ACQUIRE;
	xatab.erase(rmid);
	if (xatab.size() == 0)
		FreeEnv();
	MLOCK_RELEASE;
	return XA_OK;
}

//
//	Dummy implmentation
//
EXTERN_C static int __cdecl xa_start(XID *xid, int rmid, long flags)
{
	char	pgxid[258];

	XidToText(*xid, pgxid);
	mylog("xa_start %s rmid=%d flags=%ld\n", pgxid, rmid, flags);
	xatab.find(rmid)->second.ActivateConnection();
	return XA_OK;
}
//
//	Dummy implementation
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
	p = xatab.find(rmid);
	if (p != xatab.end())
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
//	Dummy implementation
//	It's almost impossible to implement this routine properly.
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
	p = xatab.find(rmid);
	if (p != xatab.end())
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
	p = xatab.find(rmid);
	if (p != xatab.end())
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
	p = xatab.find(rmid);
	if (p == xatab.end())
		return rmcode;
	HDBC	conn = p->second.ActivateConnection();
	if (!conn)
		return rmcode;
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
	rcount = vec.size();
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
//	I'm not sure if this invoked from MSDTC.
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

	*ppXaSwitch = &xapsw;
	return	S_OK;
}
