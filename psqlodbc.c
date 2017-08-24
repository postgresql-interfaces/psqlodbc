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
 * Comments:		See "readme.txt" for copyright and license information.
 *--------
 */

#ifdef	WIN32
#ifdef	_DEBUG
#include <crtdbg.h>
#endif /* _DEBUG */
#endif /* WIN32 */
#include "psqlodbc.h"
#include "dlg_specific.h"
#include "environ.h"
#include "misc.h"
#include <string.h>

#ifdef WIN32
#include "loadlib.h"
#else
#include <libgen.h>
#endif

static int	exepgm = 0;
BOOL isMsAccess(void) {return 1 == exepgm;}
BOOL isMsQuery(void) {return 2 == exepgm;}
BOOL isSqlServr(void) {return 3 == exepgm;}


RETCODE SQL_API SQLDummyOrdinal(void);

#if defined(WIN_MULTITHREAD_SUPPORT)
extern	CRITICAL_SECTION	conns_cs, common_cs;
#elif defined(POSIX_MULTITHREAD_SUPPORT)
extern	pthread_mutex_t 	conns_cs, common_cs;

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
#ifdef	WIN32
#ifdef	_DEBUG
#ifdef	_MEMORY_DEBUG_
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif /* _MEMORY_DEBUG_ */
#endif /* _DEBUG */
#endif /* WIN32 */
#ifdef	POSIX_THREADMUTEX_SUPPORT
	getMutexAttr();
#endif /* POSIX_THREADMUTEX_SUPPORT */
	InitializeLogging();
	INIT_CONNS_CS;
	INIT_COMMON_CS;

	return 0;
}

static void finalize_global_cs(void)
{
	DELETE_COMMON_CS;
	DELETE_CONNS_CS;
	FinalizeLogging();
#ifdef	_DEBUG
#ifdef	_MEMORY_DEBUG_
	// _CrtDumpMemoryLeaks();
#endif /* _MEMORY_DEBUG_ */
#endif /* _DEBUG */
}

#ifdef WIN32
HINSTANCE s_hModule;		/* Saved module handle. */
/*	This is where the Driver Manager attaches to this Driver */
BOOL		WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	const char *exename = GetExeProgramName();

	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			s_hModule = hInst;	/* Save for dialog boxes */


			if (stricmp(exename, "msaccess") == 0)
				exepgm = 1;
			else if (strnicmp(exename, "msqry", 5) == 0)
				exepgm = 2;
			else if (strnicmp(exename, "sqlservr", 8) == 0)
				exepgm = 3;
			initialize_global_cs();
			MYLOG(0, "exe name=%s\n", exename);
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_PROCESS_DETACH:
			MYLOG(0, "DETACHING %s\n", DRIVER_FILE_NAME);
			CleanupDelayLoadedDLLs();
			/* my(q)log is unavailable from here */
			finalize_global_cs();
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

#ifdef __GNUC__

/* Shared library initializer and destructor, using gcc's attributes */

static void
__attribute__((constructor))
psqlodbc_init(void)
{
	initialize_global_cs();
}

static void
__attribute__((destructor))
psqlodbc_fini(void)
{
	finalize_global_cs();
}

#else							/* not __GNUC__ */

/* Shared library initialization on non-gcc systems. */
BOOL
_init(void)
{
	getExeName();
	initialize_global_cs();
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
