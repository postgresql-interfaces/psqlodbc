#include	<windows.h>

#include	"psqlodbc.h"
#include	"dlg_specific.h"

HINSTANCE s_hModule;		/* Saved module handle. */

#ifdef	PG_BIN

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

static HINSTANCE s_hLModule = NULL;
static HINSTANCE s_hLModule2 = NULL;
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

#ifdef	UNICODE_SUPPORT
CSTR	psqlodbcdll = "psqlodbc35w.dll";
#else
CSTR	psqlodbcdll = "psqlodbc30a.dll";

#endif

BOOL		WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	char dllPath[MAX_PATH] = "";
	char *sptr;

	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			s_hModule = hInst;	/* Save for dialog boxes */

			if (initialize_global_cs() == 0)
				getCommonDefaults(DBMS_NAME, ODBCINST_INI, NULL);
#ifdef	PG_BIN
			if (s_hLModule = LoadLibraryEx(PG_BIN "\\libpq.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH), s_hLModule == NULL)
				mylog("libpq in the folder %s couldn't be loaded\n", PG_BIN);
#endif /* PG_BIN */
			if (NULL == s_hLModule)
			{
				char message[MAX_PATH] = "";

				SQLGetPrivateProfileString(DBMS_NAME, "Driver", "", dllPath, sizeof(dllPath), ODBCINST_INI);
				if (dllPath[0])
				{
					char drive[_MAX_DRIVE], dir[_MAX_DIR];

					_splitpath(dllPath, drive, dir, NULL, NULL);
					snprintf(dllPath, sizeof(dllPath), "%s%slibpq.dll", drive, dir);
					if (s_hLModule = LoadLibraryEx(dllPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH), s_hLModule == NULL)
					{
						mylog("libpq in the folder %s%s couldn't be loaded\n", drive, dir);
						snprintf(message, sizeof(message), "libpq in neither %s nor %s%s could be loaded", PG_BIN, drive, dir);
					}
				}
#ifdef	PG_BIN
				else
					snprintf(message, sizeof(message),  "libpq in the folder %s couldn't be loaded", PG_BIN);
#endif /* PG_BIN */
				if (message[0])
					MessageBox(NULL, message, "psqlsetup", MB_OK);
			}
			if (GetModuleFileName(s_hModule, dllPath, sizeof(dllPath)) <= 0)
			{
				MessageBox(NULL, "GetModuleFileName error", "psqlsetup", MB_OK);
				return TRUE;
			}
			if (sptr = strrchr(dllPath, '\\'), NULL == sptr)
			{
				MessageBox(NULL, "strrchr error", "psqlsetup", MB_OK);
				return FALSE;
			}
			strcpy_s(sptr + 1, MAX_PATH - (size_t)(sptr - dllPath), psqlodbcdll);
			if (s_hLModule2 = LoadLibraryEx(dllPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH), s_hLModule2 == NULL)
			{
				MessageBox(NULL, dllPath, "psqlodbc load error", MB_OK);
				return TRUE;
			}
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_PROCESS_DETACH:
			if (NULL != s_hLModule2)
				FreeLibrary(s_hLModule2);
			if (NULL != s_hLModule)
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
