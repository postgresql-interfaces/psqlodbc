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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <process.h>			/* Byron: is this where Windows keeps def.
								 * of getpid ? */
#endif

extern GLOBAL_VALUES globals;
void		generate_filename(const char *, const char *, char *);


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

	if (!mylog_on)	return;
	ENTER_MYLOG_CS;
#ifdef	LOGGING_PROCESS_TIME
	if (!start_time)
		start_time = timeGetTime();
#endif /* LOGGING_PROCESS_TIME */
	va_start(args, fmt);

	if (!MLOGFP)
	{
		generate_filename(MYLOGDIR, MYLOGFILE, filebuf);
		MLOGFP = fopen(filebuf, PG_BINARY_A);
		if (MLOGFP)
			setbuf(MLOGFP, NULL);
	}

#ifdef	WIN_MULTITHREAD_SUPPORT
#ifdef	WIN32
	if (MLOGFP)
#ifdef	LOGGING_PROCESS_TIME
	{
		DWORD	proc_time = timeGetTime() - start_time;
		fprintf(MLOGFP, "[%u-%d.%03d]", GetCurrentThreadId(), proc_time / 1000, proc_time % 1000);
	}
#else
		fprintf(MLOGFP, "[%u]", GetCurrentThreadId());
#endif /* LOGGING_PROCESS_TIME */
#endif /* WIN32 */
#endif /* WIN_MULTITHREAD_SUPPORT */
#if defined(POSIX_MULTITHREAD_SUPPORT)
	if (MLOGFP)
		fprintf(MLOGFP, "[%lu]", pthread_self());
#endif /* POSIX_MULTITHREAD_SUPPORT */
	if (MLOGFP)
		vfprintf(MLOGFP, fmt, args);

	va_end(args);
	LEAVE_MYLOG_CS;
}
void
forcelog(const char *fmt,...)
{
	va_list		args;
	char		filebuf[80];

	ENTER_MYLOG_CS;
	va_start(args, fmt);

	if (!MLOGFP)
	{
		generate_filename(MYLOGDIR, MYLOGFILE, filebuf);
		MLOGFP = fopen(filebuf, PG_BINARY_A);
		if (MLOGFP)
			setbuf(MLOGFP, NULL);
	}
	if (!MLOGFP)
	{
		generate_filename("C:\\podbclog", MYLOGFILE, filebuf);
		MLOGFP = fopen(filebuf, PG_BINARY_A);
		if (MLOGFP)
			setbuf(MLOGFP, NULL);
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
		fprintf(MLOGFP, "[%u]", pthread_self());
#endif /* POSIX_MULTITHREAD_SUPPORT */
		vfprintf(MLOGFP, fmt, args);
	}
	va_end(args);
	LEAVE_MYLOG_CS;
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

	if (!qlog_on)	return;

	ENTER_QLOG_CS;
	va_start(args, fmt);

	if (!QLOGFP)
	{
		generate_filename(QLOGDIR, QLOGFILE, filebuf);
		QLOGFP = fopen(filebuf, PG_BINARY_A);
		if (QLOGFP)
			setbuf(QLOGFP, NULL);
	}

	if (QLOGFP)
		vfprintf(QLOGFP, fmt, args);

	va_end(args);
	LEAVE_QLOG_CS;
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
	mylog_initialize();
	qlog_initialize();
}

void FinalizeLogging()
{
	mylog_finalize();
	qlog_finalize();
}
