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
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#include "psqlodbc.h"
#include "dlg_specific.h"

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

extern GLOBAL_VALUES globals;

static char *logdir = NULL;

void
generate_filename(const char *dirname, const char *prefix, char *filename)
{
#ifdef	WIN32
	int	pid = 0;

	pid = _getpid();
#else
	pid_t	pid = 0;
	struct passwd *ptr = 0;

	ptr = getpwuid(getuid());
	pid = getpid();
#endif
	if (dirname == 0 || filename == 0)
		return;

	strcpy(filename, dirname);
	strcat(filename, DIRSEPARATOR);
	if (prefix != 0)
		strcat(filename, prefix);
#ifndef WIN32
	strcat(filename, ptr->pw_name);
#endif
	sprintf(filename, "%s%u%s", filename, pid, ".log");
	return;
}

static void
generate_homefile(const char *prefix, char *filename)
{
	char	dir[PATH_MAX];
#ifdef	WIN32
	const char *ptr;

	dir[0] = '\0';
	if (ptr=getenv("HOMEDRIVE"), NULL != ptr)
		strcat(dir, ptr);
	if (ptr=getenv("HOMEPATH"), NULL != ptr)
		strcat(dir, ptr);
#else
	strcpy(dir, "~");	
#endif /* WIN32 */
	generate_filename(dir, prefix, filename);

	return;
}

#if defined(WIN_MULTITHREAD_SUPPORT)
static	CRITICAL_SECTION	qlog_cs, mylog_cs;
#elif defined(POSIX_MULTITHREAD_SUPPORT)
static	pthread_mutex_t	qlog_cs, mylog_cs;
#endif /* WIN_MULTITHREAD_SUPPORT */
static int	mylog_on = 0, qlog_on = 0;

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
	else
		mylog_on = globals.debug;
	if (qlog_onoff)
		qlog_on_count += cnopen;
	else
		qlog_off_count += cnopen;
	if (qlog_on_count > 0)
		qlog_on = 1;
	else if (qlog_off_count > 0)
		qlog_on = 0;
	else
		qlog_on = globals.commlog;
	LEAVE_QLOG_CS;
	LEAVE_MYLOG_CS;
}

#ifdef	WIN32
#define	LOGGING_PROCESS_TIME
#endif /* WIN32 */
#ifdef	LOGGING_PROCESS_TIME
#include <mmsystem.h>
	static	DWORD	start_time = 0;
#endif /* LOGGING_PROCESS_TIME */
#ifdef MY_LOG
static FILE *MLOGFP = NULL;
void
mylog(const char *fmt,...)
{
	va_list		args;
	char		filebuf[80];
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
		generate_filename(logdir ? logdir : MYLOGDIR, MYLOGFILE, filebuf);
		MLOGFP = fopen(filebuf, PG_BINARY_A);
		if (!MLOGFP)
		{
			generate_homefile(MYLOGFILE, filebuf);
			MLOGFP = fopen(filebuf, PG_BINARY_A);
			if (!MLOGFP)
			{
				generate_filename("C:\\podbclog", MYLOGFILE, filebuf);
				MLOGFP = fopen(filebuf, PG_BINARY_A);
			}
		}
		if (MLOGFP)
			setbuf(MLOGFP, NULL);
		else
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
void
forcelog(const char *fmt,...)
{
	static BOOL	force_on = TRUE;
	va_list		args;
	char		filebuf[80];
	int		gerrno = GENERAL_ERRNO;

	if (!force_on)
		return;

	ENTER_MYLOG_CS;
	va_start(args, fmt);

	if (!MLOGFP)
	{
		generate_filename(logdir ? logdir : MYLOGDIR, MYLOGFILE, filebuf);
		MLOGFP = fopen(filebuf, PG_BINARY_A);
		if (MLOGFP)
			setbuf(MLOGFP, NULL);
		if (!MLOGFP)
		{
			generate_homefile(MYLOGFILE, filebuf);
			MLOGFP = fopen(filebuf, PG_BINARY_A);
		}
		if (!MLOGFP)
		{
			generate_filename("C:\\podbclog", MYLOGFILE, filebuf);
			MLOGFP = fopen(filebuf, PG_BINARY_A);
		}
		if (MLOGFP)
			setbuf(MLOGFP, NULL);
		else
			force_on = FALSE;
	}
	if (MLOGFP)
	{
#ifdef	WIN_MULTITHREAD_SUPPORT
#ifdef	WIN32
		time_t	ntime;
		char	ctim[128];

		time(&ntime);
		strcpy(ctim, ctime(&ntime));
		ctim[strlen(ctim) - 1] = '\0';
		fprintf(MLOGFP, "[%d.%d(%s)]", GetCurrentProcessId(), GetCurrentThreadId(), ctim);
#endif /* WIN32 */
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
static void mylog_initialize()
{
	INIT_MYLOG_CS;
}
static void mylog_finalize()
{
	mylog_on = 0;
	if (MLOGFP)
	{
		fclose(MLOGFP);
		MLOGFP = NULL;
	}
	DELETE_MYLOG_CS;
}
#else
void
MyLog(char *fmt,...)
{
}
static void mylog_initialize() {}
static void mylog_finalize() {}
#endif /* MY_LOG */


#ifdef Q_LOG
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
		generate_filename(logdir ? logdir : QLOGDIR, QLOGFILE, filebuf);
		QLOGFP = fopen(filebuf, PG_BINARY_A);
		if (!QLOGFP)
		{
			generate_homefile(QLOGFILE, filebuf);
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
static void qlog_initialize()
{
	INIT_QLOG_CS;
}
static void qlog_finalize()
{
	qlog_on = 0;
	if (QLOGFP)
	{
		fclose(QLOGFP);
		QLOGFP = NULL;
	}
	DELETE_QLOG_CS;
}
#else
static void qlog_initialize() {}
static void qlog_finalize() {}
#endif /* Q_LOG */

void InitializeLogging()
{
	char dir[PATH_MAX];

	getLogDir(dir, sizeof(dir));
	if (dir[0])
		logdir = strdup(dir);
	mylog_initialize();
	qlog_initialize();
}

void FinalizeLogging()
{
	mylog_finalize();
	qlog_finalize();
	if (logdir)
	{
		free(logdir);
		logdir = NULL;
	}
}
