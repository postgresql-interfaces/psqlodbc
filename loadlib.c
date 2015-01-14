/*------
 * Module:			loadlib.c
 *
 * Description:		This module contains routines related to
 *			delay load import libraries.
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifndef	WIN32
#include <errno.h>
#endif /* WIN32 */

#include "loadlib.h"
#include "pgenlist.h"

#ifdef  WIN32
#ifdef  _MSC_VER
#pragma comment(lib, "Delayimp")
#ifdef	_HANDLE_ENLIST_IN_DTC_
#ifdef	UNICODE_SUPPORT
#pragma comment(lib, "pgenlist")
#else
#pragma comment(lib, "pgenlista")
#endif /* UNICODE_SUPPORT */
#endif /* _HANDLE_ENLIST_IN_DTC_ */
// The followings works under VC++6.0 but doesn't work under VC++7.0.
// Please add the equivalent linker options using command line etc.
#if (_MSC_VER == 1200) && defined(DYNAMIC_LOAD) // VC6.0
#ifdef	UNICODE_SUPPORT
#pragma comment(linker, "/Delayload:pgenlist.dll")
#else
#pragma comment(linker, "/Delayload:pgenlista.dll")
#endif /* UNICODE_SUPPORT */
#pragma comment(linker, "/Delay:UNLOAD")
#endif /* _MSC_VER */
#endif /* _MSC_VER */

#if defined(DYNAMIC_LOAD)
#define	WIN_DYN_LOAD
#ifdef	UNICODE_SUPPORT
CSTR	pgenlist = "pgenlist";
CSTR	pgenlistdll = "PGENLIST.dll";
#else
CSTR	pgenlist = "pgenlista";
CSTR	pgenlistdll = "PGENLISTA.dll";
#endif /* UNICODE_SUPPORT */
#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#define	_MSC_DELAY_LOAD_IMPORT
#endif /* MSC_VER */
#endif /* DYNAMIC_LOAD */
#endif /* WIN32 */

#if defined(_MSC_DELAY_LOAD_IMPORT)
#if (_MSC_VER < 1300)
#define	TRY_DLI_HOOK \
		__try { \
			__pfnDliFailureHook = DliErrorHook; \
			__pfnDliNotifyHook = DliErrorHook;
#define	RELEASE_NOTIFY_HOOK \
			__pfnDliNotifyHook = NULL;
#else
#define	TRY_DLI_HOOK \
		__try { \
			__pfnDliFailureHook2 = DliErrorHook; \
			__pfnDliNotifyHook2 = DliErrorHook;
#define	RELEASE_NOTIFY_HOOK \
			__pfnDliNotifyHook2 = NULL;
#endif /* _MSC_VER */
#else
#define	TRY_DLI_HOOK
#define	RELEASE_NOTIFY_HOOK
#endif /* _MSC_DELAY_LOAD_IMPORT */

#if defined(_MSC_DELAY_LOAD_IMPORT)
static BOOL	loaded_pgenlist = FALSE;
/*
 *	Load a DLL based on psqlodbc path.
 */
static HMODULE MODULE_load_from_psqlodbc_path(const char *module_name)
{
	extern	HINSTANCE	s_hModule;
	HMODULE	hmodule = NULL;
	char	szFileName[MAX_PATH];

	if (GetModuleFileName(s_hModule, szFileName, sizeof(szFileName)) > 0)
	{
		char drive[_MAX_DRIVE], dir[_MAX_DIR], sysdir[MAX_PATH];

		_splitpath(szFileName, drive, dir, NULL, NULL);
		GetSystemDirectory(sysdir, MAX_PATH);
		snprintf(szFileName, sizeof(szFileName), "%s%s%s.dll", drive, dir, module_name);
		if (_strnicmp(szFileName, sysdir, strlen(sysdir)) != 0)
		{
			hmodule = LoadLibraryEx(szFileName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
			mylog("psqlodbc path based %s loaded module=%p\n", module_name, hmodule);
		}
	}
	return hmodule;
}

/*
 *	Error hook function for delay load import.
 *	Try to load a DLL based on psqlodbc path.
 */
#if (_MSC_VER < 1300)
extern PfnDliHook __pfnDliFailureHook;
extern PfnDliHook __pfnDliNotifyHook;
#else
extern PfnDliHook __pfnDliFailureHook2;
extern PfnDliHook __pfnDliNotifyHook2;
#endif /* _MSC_VER */

static FARPROC WINAPI
DliErrorHook(unsigned	dliNotify,
		PDelayLoadInfo	pdli)
{
	HMODULE	hmodule = NULL;

	mylog("Dli%sHook %s Notify=%d\n", (dliFailLoadLib == dliNotify || dliFailGetProc == dliNotify) ? "Error" : "Notify", NULL != pdli->szDll ? pdli->szDll : pdli->dlp.szProcName, dliNotify);
	switch (dliNotify)
	{
		case dliNotePreLoadLibrary:
		case dliFailLoadLib:
			RELEASE_NOTIFY_HOOK
			if (_strnicmp(pdli->szDll, pgenlist, strlen(pgenlist)) == 0)
			{
				if (hmodule = MODULE_load_from_psqlodbc_path(pgenlist), NULL == hmodule)
					hmodule = LoadLibrary(pgenlist);
			}
			break;
	}
	return (FARPROC) hmodule;
}

/*
 *	unload delay loaded libraries.
 */

typedef BOOL (WINAPI *UnloadFunc)(LPCSTR);
void CleanupDelayLoadedDLLs(void)
{
	BOOL	success;
#if (_MSC_VER < 1300) /* VC6 DELAYLOAD IMPORT */
	UnloadFunc	func = __FUnloadDelayLoadedDLL;
#else
	UnloadFunc	func = __FUnloadDelayLoadedDLL2;
#endif
	/* The dll names are case sensitive for the unload helper */
	if (loaded_pgenlist)
	{
		success = (*func)(pgenlistdll);
		mylog("%s unload success=%d\n", pgenlistdll, success);
	}
	return;
}
#else
void CleanupDelayLoadedDLLs(void)
{
	return;
}
#endif	/* _MSC_DELAY_LOAD_IMPORT */

#if defined(_MSC_DELAY_LOAD_IMPORT)
static int filter_env2encoding(int level)
{
	switch (level & 0xffff)
	{
		case ERROR_MOD_NOT_FOUND:
		case ERROR_PROC_NOT_FOUND:
			return EXCEPTION_EXECUTE_HANDLER;
	}
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif /* _MSC_DELAY_LOAD_IMPORT */

#ifdef	_HANDLE_ENLIST_IN_DTC_
RETCODE	CALL_EnlistInDtc(ConnectionClass *conn, void *pTra, int method)
{
	RETCODE	ret;
	BOOL	loaded = TRUE;

#if defined(_MSC_DELAY_LOAD_IMPORT)
	if (!loaded_pgenlist)
	{
		TRY_DLI_HOOK
			ret = EnlistInDtc(conn, pTra, method);
		}
		__except ((GetExceptionCode() & 0xffff) == ERROR_MOD_NOT_FOUND ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
			loaded = FALSE;
		}
		if (loaded)
			loaded_pgenlist = TRUE;
		RELEASE_NOTIFY_HOOK
	}
	else
		ret = EnlistInDtc(conn, pTra, method);
#else
	ret = EnlistInDtc(conn, pTra, method);
	loaded_pgenlist = TRUE;
#endif /* _MSC_DELAY_LOAD_IMPORT */
	return ret;
}
RETCODE	CALL_DtcOnDisconnect(ConnectionClass *conn)
{
	if (loaded_pgenlist)
		return DtcOnDisconnect(conn);
	return FALSE;
}
RETCODE	CALL_IsolateDtcConn(ConnectionClass *conn, BOOL continueConnection)
{
	if (loaded_pgenlist)
		return IsolateDtcConn(conn, continueConnection);
	return FALSE;
}

void	*CALL_GetTransactionObject(HRESULT *hres)
{
	void	*ret = NULL;
	BOOL	loaded = TRUE;

#if defined(_MSC_DELAY_LOAD_IMPORT)
	if (!loaded_pgenlist)
	{
		TRY_DLI_HOOK
			ret = GetTransactionObject(hres);
		}
		__except ((GetExceptionCode() & 0xffff) == ERROR_MOD_NOT_FOUND ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
			loaded = FALSE;
		}
		if (loaded)
			loaded_pgenlist = TRUE;
		RELEASE_NOTIFY_HOOK
	}
	else
		ret = GetTransactionObject(hres);
#else
	ret = GetTransactionObject(hres);
	loaded_pgenlist = TRUE;
#endif /* _MSC_DELAY_LOAD_IMPORT */
	return ret;
}
void	CALL_ReleaseTransactionObject(void *pObj)
{
	if (loaded_pgenlist)
		ReleaseTransactionObject(pObj);
	return;
}
#endif /* _HANDLE_ENLIST_IN_DTC_ */
