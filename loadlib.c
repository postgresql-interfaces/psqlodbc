/*------
 * Module:			loadlib.c
 *
 * Description:		This module contains routines related to
 *			delay load import libraries.
 *			
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifndef	WIN32
#include <errno.h>
#endif /* WIN32 */
#include "loadlib.h"
#include <libpq-fe.h>

#ifdef  WIN32
#ifdef  _MSC_VER
#pragma comment(lib, "Delayimp")
#pragma comment(lib, "libpqdll")
#pragma comment(lib, "ssleay32")
// The followings works under VC++6.0 but doesn't work under VC++7.0.
// Please add the equivalent linker options using command line etc.
#if (_MSC_VER == 1200) // VC6.0
#pragma comment(linker, "/Delayload:libpq")
#pragma comment(linker, "/Delayload:ssleay32")
#endif /* _MSC_VER */
#endif /* _MSC_VER */
#if defined(DYNAMIC_LOAD)
#define	WIN_DYN_LOAD
CSTR	libpq = "libpq";
#if defined(_MSC_VER)
#define	_MSC_DELAY_LOAD_IMPORT
#endif /* MSC_VER */
#endif /* DYNAMIC_LOAD */
#endif /* WIN32 */

#if defined(_MSC_DELAY_LOAD_IMPORT)
/*
 *	Load psqlodbc path based libpq dll.
 */
static HMODULE LIBPQ_load_from_psqlodbc_path()
{
	extern	HINSTANCE	s_hModule;
	HMODULE	hmodule = NULL;
	char	szFileName[MAX_PATH];

	if (GetModuleFileName(s_hModule, szFileName, sizeof(szFileName)) > 0)
	{
		char drive[_MAX_DRIVE], dir[_MAX_DIR], sysdir[MAX_PATH];

		_splitpath(szFileName, drive, dir, NULL, NULL);
		GetSystemDirectory(sysdir, MAX_PATH);
		snprintf(szFileName, sizeof(szFileName), "%s%s%s.dll", drive, dir, libpq);
		if (strnicmp(szFileName, sysdir, strlen(sysdir)) != 0)
		{
			hmodule = LoadLibraryEx(szFileName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
			mylog("psqlodbc path based libpq loaded module=%x\n", hmodule);
		}
	}
	return hmodule;
}

/*
 *	Error hook function for delay load import.
 *	Try to load psqlodbc path based libpq.
 *	Load alternative ssl library SSLEAY32 or LIBSSL32. 
 */
extern PfnDliHook __pfnDliFailureHook2;

static FARPROC WINAPI
DliErrorHook(unsigned	dliNotify,
		PDelayLoadInfo	pdli)
{
	HMODULE	hmodule = NULL;
	int	i;
        static const char * const libarray[] = {"ssleay32", "libssl32"};

        mylog("DliErrorHook Notify=%d %x\n", dliNotify, pdli);
	switch (dliNotify)
	{
		case dliFailLoadLib:
			if (strnicmp(pdli->szDll, libpq, 5) == 0)
				hmodule = LIBPQ_load_from_psqlodbc_path();
			else
			{
        			mylog("getting alternative ssl library instead of %s\n", pdli->szDll);
        			for (i = 0; i < sizeof(libarray) / sizeof(const char * const); i++)
        			{
                			if (hmodule = GetModuleHandle(libarray[i]), NULL != hmodule)
						break;
				}
        		}
			break;
	}
	return (FARPROC) hmodule;
}

/*
 *	unload delay loaded libraries.
 *
 *	Openssl Library nmake defined
 *	ssleay32.dll is vc make, libssl32.dll is mingw make.
 */
#ifndef SSL_DLL
#define SSL_DLL "SSLEAY32.dll"
#endif /* SSL_DLL */
void UnloadDelayLoadedDLLs(BOOL ssllibLoaded)
{
#if (_MSC_VER > 1200)
	BOOL	success;

	/* The dll names are case sensitive for the unload helper*/
	success = __FUnloadDelayLoadedDLL2("LIBPQ.dll");
	forcelog("libpq unload success=%d\n", success);
	if (ssllibLoaded)
	{
		success = __FUnloadDelayLoadedDLL2(SSL_DLL);
		forcelog("sslib unload success=%d\n", success);
	}
#endif  /* VC7 DELAYLOAD IMPORT */
	return;
}
#else
void UnloadDelayLoadedDLLs(BOOL SSLLoaded)
{
	return;
}
#endif	/* _MSC_DELAY_LOAD_IMPORT */

void *CALL_PQconnectdb(const char *conninfo, BOOL *libpqLoaded)
{
	void *pqconn;
	*libpqLoaded = TRUE;
#if defined(_MSC_DELAY_LOAD_IMPORT)
	__try {
		__pfnDliFailureHook2 = DliErrorHook;
		pqconn = PQconnectdb(conninfo);
	}
	__except ((GetExceptionCode() & 0xffff) == ERROR_MOD_NOT_FOUND ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		*libpqLoaded = FALSE;
	}
#else
	pqconn = PQconnectdb(conninfo);
#endif /* _MSC_DELAY_LOAD_IMPORT */
	return pqconn;
}

#if defined(WIN_DYN_LOAD)
BOOL LIBPQ_check()
{
	extern	HINSTANCE	s_hModule;
	HMODULE	hmodule = NULL;

	mylog("checking libpq library\n");
	if (!(hmodule = LoadLibrary(libpq)))
		/* Second try the driver's directory */
		hmodule = LIBPQ_load_from_psqlodbc_path();
	mylog("hmodule=%x\n", hmodule);
	if (hmodule)
		FreeLibrary(hmodule);
	return (NULL != hmodule);
}
#else
BOOL LIBPQ_check()
{
	return TRUE;
}
#endif	/* WIN_DYN_LOAD */
