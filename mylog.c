/*-------
 * Module:			mylog.c
 *
 * Description:		This module contains miscellaneous routines
 *					such as for debugging/logging and string functions.
 *
 * Classes:			n/a
 *
 * API functions:	none
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */

#define	_MYLOG_FUNCS_IMPLEMENT_
#include "psqlodbc.h"
#include "dlg_specific.h"
#include "misc.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#define	GENERAL_ERRNO		(errno)
#define	GENERAL_ERRNO_SET(e)	(errno = e)
#else
#define	GENERAL_ERRNO		(GetLastError())
#define	GENERAL_ERRNO_SET(e)	SetLastError(e)
#include <process.h>			/* Byron: is this where Windows keeps def.
								 * of getpid ? */
#endif

#ifdef WIN32
#define DIRSEPARATOR		"\\"
#define PG_BINARY			O_BINARY
#define PG_BINARY_R			"rb"
#define PG_BINARY_W			"wb"
#define PG_BINARY_A			"ab"
#else
#define DIRSEPARATOR		"/"
#define PG_BINARY			0
#define PG_BINARY_R			"r"
#define PG_BINARY_W			"w"
#define PG_BINARY_A			"a"
#endif /* WIN32 */


static char *logdir = NULL;

void
generate_filename(const char *dirname, const char *prefix, char *filename, size_t filenamelen)
{
#ifdef	WIN32
	int	pid;

	pid = _getpid();
#else
	pid_t	pid;
	struct passwd *ptr;

	ptr = getpwuid(getuid());
	pid = getpid();
#endif
	if (dirname == 0 || filename == 0)
		return;

	snprintf(filename, filenamelen, "%s%s", dirname, DIRSEPARATOR);
	if (prefix != 0)
		strlcat(filename, prefix, filenamelen);
#ifndef WIN32
	if (ptr)
		strlcat(filename, ptr->pw_name, filenamelen);
#endif
	snprintfcat(filename, filenamelen, "%u%s", pid, ".log");
	return;
}

static void
generate_homefile(const char *prefix, char *filename, size_t filenamelen)
{
	char	dir[PATH_MAX];
#ifdef	WIN32
	const char *ptr;

	dir[0] = '\0';
	if (ptr=getenv("HOMEDRIVE"), NULL != ptr)
		strlcat(dir, ptr, filenamelen);
	if (ptr=getenv("HOMEPATH"), NULL != ptr)
		strlcat(dir, ptr, filenamelen);
#else
	STRCPY_FIXED(dir, "~");
#endif /* WIN32 */
	generate_filename(dir, prefix, filename, filenamelen);

	return;
}

#if defined(WIN_MULTITHREAD_SUPPORT)
static	CRITICAL_SECTION	qlog_cs, mylog_cs;
#elif defined(POSIX_MULTITHREAD_SUPPORT)
static	pthread_mutex_t	qlog_cs, mylog_cs;
#endif /* WIN_MULTITHREAD_SUPPORT */
static int	mylog_on = 0, qlog_on = 0;

#if defined(WIN_MULTITHREAD_SUPPORT)
#define	INIT_QLOG_CS	InitializeCriticalSection(&qlog_cs)
#define	ENTER_QLOG_CS	EnterCriticalSection(&qlog_cs)
#define	LEAVE_QLOG_CS	LeaveCriticalSection(&qlog_cs)
#define	DELETE_QLOG_CS	DeleteCriticalSection(&qlog_cs)
#define	INIT_MYLOG_CS	InitializeCriticalSection(&mylog_cs)
#define	ENTER_MYLOG_CS	EnterCriticalSection(&mylog_cs)
#define	LEAVE_MYLOG_CS	LeaveCriticalSection(&mylog_cs)
#define	DELETE_MYLOG_CS	DeleteCriticalSection(&mylog_cs)
#elif defined(POSIX_MULTITHREAD_SUPPORT)
#define	INIT_QLOG_CS	pthread_mutex_init(&qlog_cs,0)
#define	ENTER_QLOG_CS	pthread_mutex_lock(&qlog_cs)
#define	LEAVE_QLOG_CS	pthread_mutex_unlock(&qlog_cs)
#define	DELETE_QLOG_CS	pthread_mutex_destroy(&qlog_cs)
#define	INIT_MYLOG_CS	pthread_mutex_init(&mylog_cs,0)
#define	ENTER_MYLOG_CS	pthread_mutex_lock(&mylog_cs)
#define	LEAVE_MYLOG_CS	pthread_mutex_unlock(&mylog_cs)
#define	DELETE_MYLOG_CS	pthread_mutex_destroy(&mylog_cs)
#else
#define	INIT_QLOG_CS
#define	ENTER_QLOG_CS
#define	LEAVE_QLOG_CS
#define	DELETE_QLOG_CS
#define	INIT_MYLOG_CS
#define	ENTER_MYLOG_CS
#define	LEAVE_MYLOG_CS
#define	DELETE_MYLOG_CS
#endif /* WIN_MULTITHREAD_SUPPORT */

#define MYLOGFILE			"mylog_"
#ifndef WIN32
#define MYLOGDIR			"/tmp"
#else
#define MYLOGDIR			"c:"
#endif /* WIN32 */

#define QLOGFILE			"psqlodbc_"
#ifndef WIN32
#define QLOGDIR				"/tmp"
#else
#define QLOGDIR				"c:"
#endif /* WIN32 */


int	get_mylog(void)
{
	return mylog_on;
}
int	get_qlog(void)
{
	return qlog_on;
}

void
logs_on_off(int cnopen, int mylog_onoff, int qlog_onoff)
{
	static int	mylog_on_count = 0,
			mylog_off_count = 0,
			qlog_on_count = 0,
			qlog_off_count = 0;

	ENTER_MYLOG_CS;
	ENTER_QLOG_CS;
	if (mylog_onoff)
		mylog_on_count += cnopen;
	else
		mylog_off_count += cnopen;
	if (mylog_on_count > 0)
	{
		if (mylog_onoff > mylog_on)
			mylog_on = mylog_onoff;
		else if (mylog_on < 1)
			mylog_on = 1;
	}
	else if (mylog_off_count > 0)
		mylog_on = 0;
	else if (getGlobalDebug() > 0)
		mylog_on = getGlobalDebug();
	if (qlog_onoff)
		qlog_on_count += cnopen;
	else
		qlog_off_count += cnopen;
	if (qlog_on_count > 0)
		qlog_on = 1;
	else if (qlog_off_count > 0)
		qlog_on = 0;
	else if (getGlobalCommlog() > 0)
		qlog_on = getGlobalCommlog();
	LEAVE_QLOG_CS;
	LEAVE_MYLOG_CS;
}

#ifdef	WIN32
#define	LOGGING_PROCESS_TIME
#include <direct.h>
#endif /* WIN32 */
#ifdef	LOGGING_PROCESS_TIME
#include <mmsystem.h>
	static	DWORD	start_time = 0;
#endif /* LOGGING_PROCESS_TIME */
static FILE *MLOGFP = NULL;

static void MLOG_open()
{
	char		filebuf[80], errbuf[160];
	BOOL		open_error = FALSE;

	if (MLOGFP) return;

	generate_filename(logdir ? logdir : MYLOGDIR, MYLOGFILE, filebuf, sizeof(filebuf));
	MLOGFP = fopen(filebuf, PG_BINARY_A);
	if (!MLOGFP)
	{
		int lasterror = GENERAL_ERRNO;
 
		open_error = TRUE;
		SPRINTF_FIXED(errbuf, "%s open error %d\n", filebuf, lasterror);
		generate_homefile(MYLOGFILE, filebuf, sizeof(filebuf));
		MLOGFP = fopen(filebuf, PG_BINARY_A);
	}
	if (MLOGFP)
	{
		setbuf(MLOGFP, NULL);
		if (open_error)
			fputs(errbuf, MLOGFP);
	}
}

DLL_DECLARE void
mylog(const char *fmt,...)
{
	va_list		args;
	int		gerrno;

	if (!mylog_on)	return;

	gerrno = GENERAL_ERRNO;
	ENTER_MYLOG_CS;
#ifdef	LOGGING_PROCESS_TIME
	if (!start_time)
		start_time = timeGetTime();
#endif /* LOGGING_PROCESS_TIME */
	va_start(args, fmt);

	if (!MLOGFP)
	{
		MLOG_open();
		if (!MLOGFP)
			mylog_on = 0;
	}

	if (MLOGFP)
	{
#ifdef	WIN_MULTITHREAD_SUPPORT
#ifdef	LOGGING_PROCESS_TIME
		DWORD	proc_time = timeGetTime() - start_time;
		fprintf(MLOGFP, "[%u-%d.%03d]", GetCurrentThreadId(), proc_time / 1000, proc_time % 1000);
#else
		fprintf(MLOGFP, "[%u]", GetCurrentThreadId());
#endif /* LOGGING_PROCESS_TIME */
#endif /* WIN_MULTITHREAD_SUPPORT */
#if defined(POSIX_MULTITHREAD_SUPPORT)
		fprintf(MLOGFP, "[%lu]", pthread_self());
#endif /* POSIX_MULTITHREAD_SUPPORT */
		vfprintf(MLOGFP, fmt, args);
	}

	va_end(args);
	LEAVE_MYLOG_CS;
	GENERAL_ERRNO_SET(gerrno);
}
static void mylog_initialize(void)
{
	INIT_MYLOG_CS;
}
static void mylog_finalize(void)
{
	mylog_on = 0;
	if (MLOGFP)
	{
		fclose(MLOGFP);
		MLOGFP = NULL;
	}
	DELETE_MYLOG_CS;
}


static FILE *QLOGFP = NULL;
void
qlog(char *fmt,...)
{
	va_list		args;
	char		filebuf[80];
	int		gerrno;

	if (!qlog_on)	return;

	gerrno = GENERAL_ERRNO;
	ENTER_QLOG_CS;
#ifdef	LOGGING_PROCESS_TIME
	if (!start_time)
		start_time = timeGetTime();
#endif /* LOGGING_PROCESS_TIME */
	va_start(args, fmt);

	if (!QLOGFP)
	{
		generate_filename(logdir ? logdir : QLOGDIR, QLOGFILE, filebuf, sizeof(filebuf));
		QLOGFP = fopen(filebuf, PG_BINARY_A);
		if (!QLOGFP)
		{
			generate_homefile(QLOGFILE, filebuf, sizeof(filebuf));
			QLOGFP = fopen(filebuf, PG_BINARY_A);
		}
		if (QLOGFP)
			setbuf(QLOGFP, NULL);
		else
			qlog_on = 0;
	}

	if (QLOGFP)
	{
#ifdef	LOGGING_PROCESS_TIME
		DWORD	proc_time = timeGetTime() - start_time;
		fprintf(QLOGFP, "[%d.%03d]", proc_time / 1000, proc_time % 1000);
#endif /* LOGGING_PROCESS_TIME */
		vfprintf(QLOGFP, fmt, args);
	}

	va_end(args);
	LEAVE_QLOG_CS;
	GENERAL_ERRNO_SET(gerrno);
}
static void qlog_initialize(void)
{
	INIT_QLOG_CS;
}
static void qlog_finalize(void)
{
	qlog_on = 0;
	if (QLOGFP)
	{
		fclose(QLOGFP);
		QLOGFP = NULL;
	}
	DELETE_QLOG_CS;
}

static int	globalDebug = -1;
int
getGlobalDebug()
{
	char	temp[16];

	if (globalDebug >=0)
		return globalDebug;
	/* Debug is stored in the driver section */
	SQLGetPrivateProfileString(DBMS_NAME, INI_DEBUG, "", temp, sizeof(temp), ODBCINST_INI);
	if (temp[0])
		globalDebug = atoi(temp);
	else
		globalDebug = DEFAULT_DEBUG;

	return globalDebug;
}

int
setGlobalDebug(int val)
{
	return (globalDebug = val);
}

static int	globalCommlog = -1;
int
getGlobalCommlog()
{
	char	temp[16];

	if (globalCommlog >= 0)
		return globalCommlog;
	/* Commlog is stored in the driver section */
	SQLGetPrivateProfileString(DBMS_NAME, INI_COMMLOG, "", temp, sizeof(temp), ODBCINST_INI);
	if (temp[0])
		globalCommlog = atoi(temp);
	else
		globalCommlog = DEFAULT_COMMLOG;

	return globalCommlog;
}

int
setGlobalCommlog(int val)
{
	return (globalCommlog = val);
}

int
writeGlobalLogs()
{
	char	temp[10];

	ITOA_FIXED(temp, globalDebug);
	SQLWritePrivateProfileString(DBMS_NAME, INI_DEBUG, temp, ODBCINST_INI);
	ITOA_FIXED(temp, globalCommlog);
	SQLWritePrivateProfileString(DBMS_NAME, INI_COMMLOG, temp, ODBCINST_INI);
	return 0;
}

int
getLogDir(char *dir, int dirmax)
{
	return SQLGetPrivateProfileString(DBMS_NAME, INI_LOGDIR, "", dir, dirmax, ODBCINST_INI);
}

int
setLogDir(const char *dir)
{
	return SQLWritePrivateProfileString(DBMS_NAME, INI_LOGDIR, dir, ODBCINST_INI);
}

/*
 *	This function starts a logging out of connections according the ODBCINST.INI
 *	portion of the DBMS_NAME registry.
 */
static void
start_logging()
{
	/*
	 * GlobalDebug or GlobalCommlog means whether take mylog or commlog
	 * out of the connection time or not but doesn't mean the default of
	 * ci->drivers.debug(commlog).
	 */
	logs_on_off(0, 0, 0);
	mylog("\t%s:Global.debug&commlog=%d&%d\n", __FUNCTION__, getGlobalDebug(), getGlobalCommlog());
}

void InitializeLogging(void)
{
	char dir[PATH_MAX];

	getLogDir(dir, sizeof(dir));
	if (dir[0])
		logdir = strdup(dir);
	mylog_initialize();
	qlog_initialize();
	start_logging();
}

void FinalizeLogging(void)
{
	mylog_finalize();
	qlog_finalize();
	if (logdir)
	{
		free(logdir);
		logdir = NULL;
	}
}
