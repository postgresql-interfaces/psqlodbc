/*--------
 * Module:			psqlodbc.c
 *
 * Description:		This module contains the main entry point (DllMain)
 *					for the library.  It also contains functions to get
 *					and set global variables for the driver in the registry.
 *
 * Classes:			n/a
 *
 * API functions:	none
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *--------
 */

#include "psqlodbc.h"
#include "dlg_specific.h"
#include "environ.h"

#ifdef WIN32
#include <winsock.h>
#endif

GLOBAL_VALUES globals;

RETCODE SQL_API SQLDummyOrdinal(void);

#if defined(WIN_MULTITHREAD_SUPPORT)
extern	CRITICAL_SECTION	qlog_cs, mylog_cs, conns_cs;
#elif defined(POSIX_MULTITHREAD_SUPPORT)
extern	pthread_mutex_t 	qlog_cs, mylog_cs, conns_cs;

#ifdef	POSIX_THREADMUTEX_SUPPORT
#ifdef	PG_RECURSIVE_MUTEXATTR
static	pthread_mutexattr_t	recur_attr;
const	pthread_mutexattr_t*	getMutexAttr(void)
{
	static int	init = 1;

	if (init)
	{
		if (0 != pthread_mutexattr_init(&recur_attr))
			return NULL;
		if (0 != pthread_mutexattr_settype(&recur_attr, PG_RECURSIVE_MUTEXATTR))
			return NULL;
	}
	init = 0;

	return	&recur_attr;
}
#else
const	pthread_mutexattr_t*	getMutexAttr(void)
{
	return NULL;
}
#endif /* PG_RECURSIVE_MUTEXATTR */
#endif /* POSIX_THREADMUTEX_SUPPORT */
#endif /* WIN_MULTITHREAD_SUPPORT */

int	initialize_global_cs(void)
{
	static	int	init = 1;

	if (!init)
		return 0;
	init = 0;
#ifdef	POSIX_THREADMUTEX_SUPPORT
	getMutexAttr();
#endif /* POSIX_THREADMUTEX_SUPPORT */
	INIT_QLOG_CS;
	INIT_MYLOG_CS;
	INIT_CONNS_CS;

	return 0;
}

static void finalize_global_cs(void)
{
	DELETE_CONNS_CS;
	DELETE_QLOG_CS;
	DELETE_MYLOG_CS;
}

#ifdef WIN32
HINSTANCE NEAR s_hModule;		/* Saved module handle. */
/*	This is where the Driver Manager attaches to this Driver */
BOOL		WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	WORD		wVersionRequested;
	WSADATA		wsaData;

	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			s_hModule = hInst;	/* Save for dialog boxes */

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

			initialize_global_cs();
			getCommonDefaults(DBMS_NAME, ODBCINST_INI, NULL);
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_PROCESS_DETACH:
			finalize_global_cs();
forcelog("DETACH PROCESS\n");
			WSACleanup();
			return TRUE;

		case DLL_THREAD_DETACH:
			break;

		default:
			break;
	}

	return TRUE;

	UNREFERENCED_PARAMETER(lpReserved);
}

#else							/* not WIN32 */

#ifndef TRUE
#define TRUE	(BOOL)1
#endif
#ifndef FALSE
#define FALSE	(BOOL)0
#endif

#ifdef __GNUC__

/* This function is called at library initialization time.	*/

static BOOL
__attribute__((constructor))
init(void)
{
	initialize_global_cs();
	getCommonDefaults(DBMS_NAME, ODBCINST_INI, NULL);
	return TRUE;
}

#else							/* not __GNUC__ */

/*
 * These two functions do shared library initialziation on UNIX, well at least
 * on Linux. I don't know about other systems.
 */
BOOL
_init(void)
{
	initialize_global_cs();
	getCommonDefaults(DBMS_NAME, ODBCINST_INI, NULL);
	return TRUE;
}

BOOL
_fini(void)
{
	finalize_global_cs();
	return TRUE;
}
#endif   /* not __GNUC__ */
#endif   /* not WIN32 */


/*
 *	This function is used to cause the Driver Manager to
 *	call functions by number rather than name, which is faster.
 *	The ordinal value of this function must be 199 to have the
 *	Driver Manager do this.  Also, the ordinal values of the
 *	functions must match the value of fFunction in SQLGetFunctions()
 */
RETCODE		SQL_API
SQLDummyOrdinal(void)
{
	return SQL_SUCCESS;
}
