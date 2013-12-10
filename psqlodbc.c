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
#ifdef	USE_SSPI
#include "sspisvcs.h"
#endif /* USE_SSPI */
#include "misc.h"

#ifdef WIN32
#include "loadlib.h"
int	platformId = 0;
#endif

static int	exepgm = 0;
BOOL isMsAccess() {return 1 == exepgm;}
BOOL isMsQuery() {return 2 == exepgm;}
BOOL isSqlServr() {return 3 == exepgm;}

GLOBAL_VALUES globals;

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
	memset(&globals, 0, sizeof(globals));
	INIT_CONNS_CS;
	INIT_COMMON_CS;

	return 0;
}

#define	CORR_STRCPY(item)	strncpy_null(to->item, from->item, sizeof(to->item))
#define	CORR_VALCPY(item)	(to->item = from->item)

void	copy_globals(GLOBAL_VALUES *to, const GLOBAL_VALUES *from)
{
	memset(to, 0, sizeof(*to));
	/***
	memcpy(to, from, sizeof(GLOBAL_VALUES));
	SET_NAME_DIRECTLY(to->drivername, NULL);
	SET_NAME_DIRECTLY(to->conn_settings, NULL);
	***/
	NAME_TO_NAME(to->drivername, from->drivername);
	CORR_VALCPY(fetch_max);
	CORR_VALCPY(socket_buffersize);
	CORR_VALCPY(unknown_sizes);
	CORR_VALCPY(max_varchar_size);
	CORR_VALCPY(max_longvarchar_size);
	CORR_VALCPY(debug);
	CORR_VALCPY(commlog);
	CORR_VALCPY(disable_optimizer);
	CORR_VALCPY(ksqo);
	CORR_VALCPY(unique_index);
	CORR_VALCPY(onlyread);		/* readonly is reserved on Digital C++
								 * compiler */
	CORR_VALCPY(use_declarefetch);
	CORR_VALCPY(text_as_longvarchar);
	CORR_VALCPY(unknowns_as_longvarchar);
	CORR_VALCPY(bools_as_char);
	CORR_VALCPY(lie);
	CORR_VALCPY(parse);
	CORR_VALCPY(cancel_as_freestmt);
	CORR_STRCPY(extra_systable_prefixes);
	CORR_STRCPY(protocol);
	NAME_TO_NAME(to->conn_settings, from->conn_settings);

	mylog("copy_globals driver=%s socket_buffersize=%d\n", SAFE_NAME(to->drivername), to->socket_buffersize);
}
#undef	CORR_STRCPY
#undef	CORR_VALCPY

void	finalize_globals(GLOBAL_VALUES *glbv)
{
	NULL_THE_NAME(glbv->drivername);
	NULL_THE_NAME(glbv->conn_settings);
}

static void finalize_global_cs(void)
{
	DELETE_COMMON_CS;
	DELETE_CONNS_CS;
	finalize_globals(&globals);
	FinalizeLogging();
#ifdef	_DEBUG
#ifdef	_MEMORY_DEBUG_
	// _CrtDumpMemoryLeaks();
#endif /* _MEMORY_DEBUG_ */
#endif /* _DEBUG */
}

#ifdef WIN32
HINSTANCE NEAR s_hModule;		/* Saved module handle. */
/*	This is where the Driver Manager attaches to this Driver */
BOOL		WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			s_hModule = hInst;	/* Save for dialog boxes */

			if (initialize_global_cs() == 0)
			{
				char	pathname[_MAX_PATH], fname[_MAX_FNAME];
				OSVERSIONINFO	osversion;
				
				getCommonDefaults(DBMS_NAME, ODBCINST_INI, NULL);
				if (GetModuleFileName(NULL, pathname, sizeof(pathname)) > 0)
				{
					_splitpath(pathname, NULL, NULL, fname, NULL);
					if (stricmp(fname, "msaccess") == 0)
						exepgm = 1;
					else if (strnicmp(fname, "msqry", 5) == 0)
						exepgm = 2;
					else if (strnicmp(fname, "sqlservr", 8) == 0)
						exepgm = 3;
				}
				osversion.dwOSVersionInfoSize = sizeof(osversion);
				if (GetVersionEx(&osversion))
				{
					platformId=osversion.dwPlatformId;
				}
				mylog("exe name=%s plaformId=%d\n", fname, platformId);
			}
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_PROCESS_DETACH:
			mylog("DETACHING PROCESS\n");
#ifdef	USE_SSPI
			LeaveSSPIService();
#endif /* USE_SSPI */
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
	getCommonDefaults(DBMS_NAME, ODBCINST_INI, NULL);
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
