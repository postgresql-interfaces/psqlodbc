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
	int			pid = 0;

#ifndef WIN32
	struct passwd *ptr = 0;

	ptr = getpwuid(getuid());
#endif
	pid = getpid();
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
CRITICAL_SECTION	qlog_cs, mylog_cs;
#elif defined(POSIX_MULTITHREAD_SUPPORT)
pthread_mutex_t	qlog_cs, mylog_cs;
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

#ifdef MY_LOG
static FILE *LOGFP = NULL;
void
mylog(char *fmt,...)
{
	va_list		args;
	char		filebuf[80];

	ENTER_MYLOG_CS;
	if (mylog_on)
	{
		va_start(args, fmt);

		if (!LOGFP)
		{
			generate_filename(MYLOGDIR, MYLOGFILE, filebuf);
			LOGFP = fopen(filebuf, PG_BINARY_A);
			if (LOGFP)
				setbuf(LOGFP, NULL);
		}

#ifdef	WIN_MULTITHREAD_SUPPORT
#ifdef	WIN32
		if (LOGFP)
			fprintf(LOGFP, "[%d]", GetCurrentThreadId());
#endif /* WIN32 */
#endif /* WIN_MULTITHREAD_SUPPORT */
#if defined(POSIX_MULTITHREAD_SUPPORT)
		if (LOGFP)
			fprintf(LOGFP, "[%d]", pthread_self());
#endif /* POSIX_MULTITHREAD_SUPPORT */
		if (LOGFP)
			vfprintf(LOGFP, fmt, args);

		va_end(args);
	}
	LEAVE_MYLOG_CS;
}
void
forcelog(const char *fmt,...)
{
	va_list		args;
	char		filebuf[80];

	ENTER_MYLOG_CS;
	va_start(args, fmt);

	if (!LOGFP)
	{
		generate_filename(MYLOGDIR, MYLOGFILE, filebuf);
		LOGFP = fopen(filebuf, PG_BINARY_A);
		if (LOGFP)
			setbuf(LOGFP, NULL);
	}
	if (!LOGFP)
	{
		generate_filename("C:\\podbclog", MYLOGFILE, filebuf);
		LOGFP = fopen(filebuf, PG_BINARY_A);
		if (LOGFP)
			setbuf(LOGFP, NULL);
	}
	if (LOGFP)
	{
#ifdef	WIN_MULTITHREAD_SUPPORT
#ifdef	WIN32
		time_t	ntime;
		char	ctim[128];

		time(&ntime);
		strcpy(ctim, ctime(&ntime));
		ctim[strlen(ctim) - 1] = '\0';
		fprintf(LOGFP, "[%d.%d(%s)]", GetCurrentProcessId(), GetCurrentThreadId(), ctim);
#endif /* WIN32 */
#endif /* WIN_MULTITHREAD_SUPPORT */
#if defined(POSIX_MULTITHREAD_SUPPORT)
		fprintf(LOGFP, "[%d]", pthread_self());
#endif /* POSIX_MULTITHREAD_SUPPORT */
		vfprintf(LOGFP, fmt, args);
	}
	va_end(args);
	LEAVE_MYLOG_CS;
}
#else
void
MyLog(char *fmt,...)
{
}
#endif


#ifdef Q_LOG
void
qlog(char *fmt,...)
{
	va_list		args;
	char		filebuf[80];
	static FILE *LOGFP = NULL;

	ENTER_QLOG_CS;
	if (qlog_on)
	{
		va_start(args, fmt);

		if (!LOGFP)
		{
			generate_filename(QLOGDIR, QLOGFILE, filebuf);
			LOGFP = fopen(filebuf, PG_BINARY_A);
			if (LOGFP)
				setbuf(LOGFP, NULL);
		}

		if (LOGFP)
			vfprintf(LOGFP, fmt, args);

		va_end(args);
	}
	LEAVE_QLOG_CS;
}
#endif
