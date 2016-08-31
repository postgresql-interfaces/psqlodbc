#include	<windows.h>

#include	"psqlodbc.h"
#include	"dlg_specific.h"

HINSTANCE s_hModule;		/* Saved module handle. */

#ifdef	PG_BIN

#include "loadlib.h"
#include <DelayImp.h>
#include <stdio.h>
#include <string.h>
#include <sql.h>

RETCODE SQL_API SQLDummyOrdinal(void);

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

#endif /* PG_BIN */

static HINSTANCE s_hLModule;
/*	This is where the Driver Manager attaches to this Driver */

extern GLOBAL_VALUES globals;

int	initialize_global_cs(void)
{
	static	int	init = 1;

	if (!init)
		return 0;
	init = 0;
	InitializeLogging();
	memset(&globals, 0, sizeof(globals));

	return 0;
}

static void finalize_global_cs(void)
{
	finalize_globals(&globals);
	FinalizeLogging();
#ifdef	_DEBUG
#ifdef	_MEMORY_DEBUG_
	// _CrtDumpMemoryLeaks();
#endif /* _MEMORY_DEBUG_ */
#endif /* _DEBUG */
}

BOOL		WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			s_hModule = hInst;	/* Save for dialog boxes */

			if (initialize_global_cs() == 0)
				getCommonDefaults(DBMS_NAME, ODBCINST_INI, NULL);
#ifdef	PG_BIN
			if (s_hLModule = LoadLibraryEx(PG_BIN "\\libpq.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH), s_hLModule == NULL)
			{
				mylog("libpq folder %s couldn't be loaded\n", PG_BIN);
			}
			EnableDelayLoadHook();
#endif
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_PROCESS_DETACH:
			CleanupDelayLoadedDLLs();
			FreeLibrary(s_hLModule);
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
