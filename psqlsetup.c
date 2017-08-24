#include	<windows.h>

#include	"psqlodbc.h"
#include	"dlg_specific.h"
#include	"loadlib.h"
#include	"misc.h"

HINSTANCE s_hModule;		/* Saved module handle. */

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


static HINSTANCE s_hLModule = NULL;	/* for libpq */
static HINSTANCE s_hLModule2 = NULL;
/*	This is where the Driver Manager attaches to this Driver */


int	initialize_global_cs(void)
{
	static	int	init = 1;

	if (!init)
		return 0;
	init = 0;
	InitializeLogging();

	return 0;
}

static void finalize_global_cs(void)
{
	FinalizeLogging();
#ifdef	_DEBUG
#ifdef	_MEMORY_DEBUG_
	// _CrtDumpMemoryLeaks();
#endif /* _MEMORY_DEBUG_ */
#endif /* _DEBUG */
}

#ifdef	UNICODE_SUPPORT
CSTR	psqlodbc = "psqlodbc35w";
#else
CSTR	psqlodbc = "psqlodbc30a";
#endif


BOOL		WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	char dllPath[MAX_PATH] = "";

	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			s_hModule = hInst;	/* Save for dialog boxes */
			initialize_global_cs();
#ifdef	PG_BIN
			if (s_hLModule = LoadLibraryEx(PG_BIN "\\libpq.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH), s_hLModule == NULL)
				MYLOG(0, "libpq in the folder %s couldn't be loaded\n", PG_BIN);
#endif /* PG_BIN */
			if (NULL == s_hLModule)
			{
				char message[MAX_PATH] = "";

				SQLGetPrivateProfileString(DBMS_NAME, "Driver", "", dllPath, sizeof(dllPath), ODBCINST_INI);
				if (dllPath[0])
				{
					char drive[_MAX_DRIVE], dir[_MAX_DIR];

					_splitpath(dllPath, drive, dir, NULL, NULL);
					SPRINTF_FIXED(dllPath, "%s%slibpq.dll", drive, dir);
					if (s_hLModule = LoadLibraryEx(dllPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH), s_hLModule == NULL)
					{
						MYLOG(0, "libpq in the folder %s%s couldn't be loaded\n", drive, dir);
						SPRINTF_FIXED(message, "libpq in neither %s nor %s%s could be loaded", PG_BIN, drive, dir);
					}
				}
#ifdef	PG_BIN
				else
					SPRINTF_FIXED(message, "libpq in the folder %s couldn't be loaded", PG_BIN);
#endif /* PG_BIN */
				if (message[0])
					MessageBox(NULL, message, "psqlsetup", MB_OK);
			}
			if (s_hLModule2 = MODULE_load_from_psqlodbc_path(psqlodbc), s_hLModule2 == NULL)
			{
				MessageBox(NULL, "psqlodbc load error", "psqlsetup",  MB_OK);
				return TRUE;
			}
			else
				AlreadyLoadedPsqlodbc();
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_PROCESS_DETACH:
			MYLOG(0, "DETACHING psqlsetup\n");
			CleanupDelayLoadedDLLs();
			if (NULL != s_hLModule)
			{
				MYLOG(0, "Freeing Library libpq\n");
				FreeLibrary(s_hLModule);
			}
			if (NULL != s_hLModule2)
			{
				MYLOG(0, "Freeing Library %s\n", psqlodbc);
				FreeLibrary(s_hLModule2);
			}
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
