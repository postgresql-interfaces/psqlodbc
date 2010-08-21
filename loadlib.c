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
#ifndef NOT_USE_LIBPQ
#ifdef	RESET_CRYPTO_CALLBACKS
#include <openssl/ssl.h>
#endif /* RESET_CRYPTO_CALLBACKS */
#include <libpq-fe.h>
#endif /* NOT_USE_LIBPQ */
#include "pgenlist.h"

#ifdef  WIN32
#ifdef  _MSC_VER
#pragma comment(lib, "Delayimp")
#ifndef	NOT_USE_LIBPQ
#pragma comment(lib, "libpq")
#pragma comment(lib, "ssleay32")
#ifdef	RESET_CRYPTO_CALLBACKS
#pragma comment(lib, "libeay32")
#endif /* RESET_CRYPTO_CALLBACKS */
#endif /* NOT_USE_LIBPQ */
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
#ifndef	NOT_USE_LIBPQ
#pragma comment(linker, "/Delayload:libpq.dll")
#pragma comment(linker, "/Delayload:ssleay32.dll")
#ifdef	RESET_CRYPTO_CALLBACKS
#pragma comment(linker, "/Delayload:libeay32.dll")
#endif /* RESET_CRYPTO_CALLBACKS */
#endif /* NOT_USE_LIBPQ */
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
CSTR	libpqdll = "LIBPQ.dll";
#ifdef	_WIN64
CSTR	gssapidll = "GSSAPI64.dll";
#else
CSTR	gssapidll = "GSSAPI32.dll";
#endif /* _WIN64 */
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

CSTR	libpqlib = "libpq";
#ifdef	_WIN64
CSTR	gssapilib = "gssapi64";
#else
CSTR	gssapilib = "gssapi32";
#endif /* _WIN64 */
#ifndef	NOT_USE_LIBPQ
CSTR	checkproc1 = "PQconnectdbParams";
static int	connect_withparam_available = -1;
CSTR	checkproc2 = "PQconninfoParse";
static int	sslverify_available = -1;
#endif /* NOT_USE_LIBPQ */

#if defined(_MSC_DELAY_LOAD_IMPORT)
static BOOL	loaded_libpq = FALSE, loaded_ssllib = FALSE;
static BOOL	loaded_pgenlist = FALSE, loaded_gssapi = FALSE;
/*
 *	Load psqlodbc path based libpq dll.
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
 *	Try to load psqlodbc path based libpq.
 *	Load alternative ssl library SSLEAY32 or LIBSSL32. 
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
	int	i;
	static const char * const libarray[] = {"libssl32", "ssleay32"};

	mylog("Dli%sHook %s Notify=%d\n", (dliFailLoadLib == dliNotify || dliFailGetProc == dliNotify) ? "Error" : "Notify", NULL != pdli->szDll ? pdli->szDll : pdli->dlp.szProcName, dliNotify);
	switch (dliNotify)
	{
		case dliNotePreLoadLibrary:
		case dliFailLoadLib:
#if (_MSC_VER < 1300)
			__pfnDliNotifyHook = NULL;
#else
			__pfnDliNotifyHook2 = NULL;
#endif /* _MSC_VER */
			if (_strnicmp(pdli->szDll, libpqlib, strlen(libpqlib)) == 0)
			{
				if (hmodule = MODULE_load_from_psqlodbc_path(libpqlib), NULL == hmodule)
					hmodule = LoadLibrary(libpqlib);
#ifndef	NOT_USE_LIBPQ
				if (NULL == hmodule)
					connect_withparam_available = sslverify_available = FALSE;
				if (connect_withparam_available < 0)
				{
					if (NULL == GetProcAddress(hmodule, checkproc1))
						connect_withparam_available = FALSE;
					else
						connect_withparam_available = TRUE;
inolog("connect_withparam_available=%d\n", connect_withparam_available); 
				}
				if (sslverify_available < 0)
				{
					if (NULL == GetProcAddress(hmodule, checkproc2))
						sslverify_available = FALSE;
					else
						sslverify_available = TRUE;
				}
#endif /* NOT_USE_LIBPQ */
			}
			else if (_strnicmp(pdli->szDll, pgenlist, strlen(pgenlist)) == 0)
			{
				if (hmodule = MODULE_load_from_psqlodbc_path(pgenlist), NULL == hmodule)
					hmodule = LoadLibrary(pgenlist);
			}
#ifdef	USE_GSS
			else if (_strnicmp(pdli->szDll, gssapilib, strlen(gssapilib)) == 0)
			{
#ifndef	NOT_USE_LIBPQ
                		if (hmodule = GetModuleHandle(gssapilib), NULL == hmodule)
#endif
				{
					if (hmodule = MODULE_load_from_psqlodbc_path(gssapilib), NULL == hmodule)
					{
						if (hmodule = LoadLibrary(gssapilib), NULL != hmodule)
							loaded_gssapi = TRUE;
					}
					else
						loaded_gssapi = TRUE;
				}
			}
#endif /* USE_GSS */
			else if (0 == _stricmp(pdli->szDll, libarray[0]) ||
				 0 == _stricmp(pdli->szDll, libarray[1]))
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
	if (loaded_libpq)
	{
#ifdef	RESET_CRYPTO_CALLBACKS
		/*
		 *	May be needed to avoid crash on exit
		 *	when libpq doesn't reset the callbacks.
		 */
		CRYPTO_set_locking_callback(NULL);
		CRYPTO_set_id_callback(NULL);
		mylog("passed RESET_CRYPTO_CALLBACKS\n");
#else
		mylog("not passed RESET_CRYPTO_CALLBACKS\n");
#endif /* RESET_CRYPTO_CALLBACKS */
		success = (*func)(libpqdll);
		mylog("%s unload success=%d\n", libpqdll, success);
	}
	if (loaded_ssllib)
	{
		success = (*func)(SSL_DLL);
		mylog("ssldll unload success=%d\n", success);
	}
	if (loaded_pgenlist)
	{
		success = (*func)(pgenlistdll);
		mylog("%s unload success=%d\n", pgenlistdll, success);
	}
	if (loaded_gssapi)
	{
		success = (*func)(gssapidll);
		mylog("%s unload success=%d\n", gssapidll, success);
	}
	return;
}
#else
void CleanupDelayLoadedDLLs(void)
{
	return;
}
#endif	/* _MSC_DELAY_LOAD_IMPORT */

#ifndef	NOT_USE_LIBPQ
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

BOOL	ssl_verify_available(void)
{
	if (sslverify_available < 0)
	{
#if defined(_MSC_DELAY_LOAD_IMPORT)
		__try {
#if (_MSC_VER < 1300)
			__pfnDliFailureHook = DliErrorHook;
			__pfnDliNotifyHook = DliErrorHook;
#else
			__pfnDliFailureHook2 = DliErrorHook;
			__pfnDliNotifyHook2 = DliErrorHook;
#endif /* _MSC_VER */
			PQenv2encoding();
		}
		__except (filter_env2encoding(GetExceptionCode())) {
		}
		if (sslverify_available < 0)
			sslverify_available = 0;
#else
#ifdef HAVE_LIBLTDL
		lt_dlhandle dlhandle = lt_dlopenext(libpqlib);

		sslverify_available = 1;
		if (NULL != dlhandle)
		{
			if (NULL == lt_dlsym(dlhandle, checkproc2))
				sslverify_available = 0;
			lt_dlclose(dlhandle);
		}
#endif /* HAVE_LIBLTDL */
#endif /* _MSC_DELAY_LOAD_IMPORT */
	}

	return (0 != sslverify_available);
}

BOOL	connect_with_param_available(void)
{
	if (connect_withparam_available < 0)
	{
#if defined(_MSC_DELAY_LOAD_IMPORT)
		__try {
#if (_MSC_VER < 1300)
			__pfnDliFailureHook = DliErrorHook;
			__pfnDliNotifyHook = DliErrorHook;
#else
			__pfnDliFailureHook2 = DliErrorHook;
			__pfnDliNotifyHook2 = DliErrorHook;
#endif /* _MSC_VER */
			PQescapeLiteral(NULL, NULL, 0);
		}
		__except (filter_env2encoding(GetExceptionCode())) {
		}
		if (connect_withparam_available < 0)
{
inolog("connect_withparam_available is set to false\n"); 
			connect_withparam_available = 0;
}
#else
#ifdef HAVE_LIBLTDL
		lt_dlhandle dlhandle = lt_dlopenext(libpqlib);

		connect_withparam_available = 1;
		if (NULL != dlhandle)
		{
			if (NULL == lt_dlsym(dlhandle, checkproc1))
				connect_withparam_available = 0;
			lt_dlclose(dlhandle);
		}
#endif /* HAVE_LIBLTDL */
#endif /* _MSC_DELAY_LOAD_IMPORT */
	}

	return (0 != connect_withparam_available);
}

void *CALL_PQconnectdb(const char *conninfo, BOOL *libpqLoaded)
{
	void *pqconn = NULL;
	*libpqLoaded = TRUE;
#if defined(_MSC_DELAY_LOAD_IMPORT)
	__try {
#if (_MSC_VER < 1300)
		__pfnDliFailureHook = DliErrorHook;
		__pfnDliNotifyHook = DliErrorHook;
#else
		__pfnDliFailureHook2 = DliErrorHook;
		__pfnDliNotifyHook2 = DliErrorHook;
#endif /* _MSC_VER */
inolog("calling PQconnectdb\n");
		pqconn = PQconnectdb(conninfo);
	}
	__except ((GetExceptionCode() & 0xffff) == ERROR_MOD_NOT_FOUND ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		*libpqLoaded = FALSE;
	}
#if (_MSC_VER < 1300)
	__pfnDliNotifyHook = NULL;
#else
	__pfnDliNotifyHook2 = NULL;
#endif /* _MSC_VER */
	if (*libpqLoaded)
	{
		loaded_libpq = TRUE;
		/* ssllibs are already loaded by libpq
		if (PQgetssl(pqconn))
			loaded_ssllib = TRUE;
		*/
	}
#else
	pqconn = PQconnectdb(conninfo);
#endif /* _MSC_DELAY_LOAD_IMPORT */
	return pqconn;
}

void *CALL_PQconnectdbParams(const char *opts[], const char *vals[], BOOL *libpqLoaded)
{
	void *pqconn = NULL;
	*libpqLoaded = TRUE;
#if defined(_MSC_DELAY_LOAD_IMPORT)
	__try {
#if (_MSC_VER < 1300)
		__pfnDliFailureHook = DliErrorHook;
		__pfnDliNotifyHook = DliErrorHook;
#else
		__pfnDliFailureHook2 = DliErrorHook;
		__pfnDliNotifyHook2 = DliErrorHook;
#endif /* _MSC_VER */
inolog("calling PQconnectdbParams\n");
		pqconn = PQconnectdbParams(opts, vals, 0);
	}
	__except ((GetExceptionCode() & 0xffff) == ERROR_MOD_NOT_FOUND ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		*libpqLoaded = FALSE;
	}
#if (_MSC_VER < 1300)
	__pfnDliNotifyHook = NULL;
#else
	__pfnDliNotifyHook2 = NULL;
#endif /* _MSC_VER */
	if (*libpqLoaded)
	{
		loaded_libpq = TRUE;
		/* ssllibs are already loaded by libpq
		if (PQgetssl(pqconn))
			loaded_ssllib = TRUE;
		*/
	}
#else
	pqconn = PQconnectdbParams(opts, vals, 0);
#endif /* _MSC_DELAY_LOAD_IMPORT */
	return pqconn;
}
#else
BOOL	ssl_verify_available(void)
{
	return	FALSE;
}
BOOL	connect_with_param_available(void)
{
	return	FALSE;
}
#endif /* NOT_USE_LIBPQ */

#ifdef	_HANDLE_ENLIST_IN_DTC_
RETCODE	CALL_EnlistInDtc(ConnectionClass *conn, void *pTra, int method)
{
	RETCODE	ret;
	BOOL	loaded = TRUE;
	
#if defined(_MSC_DELAY_LOAD_IMPORT)
	__try {
#if (_MSC_VER < 1300)
		__pfnDliFailureHook = DliErrorHook;
		__pfnDliNotifyHook = DliErrorHook;
#else
		__pfnDliFailureHook2 = DliErrorHook;
		__pfnDliNotifyHook2 = DliErrorHook;
#endif /* _MSC_VER */
		ret = EnlistInDtc(conn, pTra, method);
	}
	__except ((GetExceptionCode() & 0xffff) == ERROR_MOD_NOT_FOUND ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		loaded = FALSE;
	}
	if (loaded)
		loaded_pgenlist = TRUE;
#if (_MSC_VER < 1300)
	__pfnDliNotifyHook = NULL;
#else
	__pfnDliNotifyHook2 = NULL;
#endif /* _MSC_VER */
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
RETCODE	CALL_DtcOnRelease(void)
{
	if (loaded_pgenlist)
		return DtcOnRelease();
	return FALSE;
}
#endif /* _HANDLE_ENLIST_IN_DTC_ */

#if defined(WIN_DYN_LOAD)
BOOL SSLLIB_check()
{
	extern	HINSTANCE	s_hModule;
	HMODULE	hmodule = NULL;

	mylog("checking libpq library\n");
	/* First search the driver's folder */
#ifndef	NOT_USE_LIBPQ
	if (NULL == (hmodule = MODULE_load_from_psqlodbc_path(libpqlib)))
		/* Second try the PATH ordinarily */
		hmodule = LoadLibrary(libpqlib);
	mylog("libpq hmodule=%p\n", hmodule);
#endif /* NOT_USE_LIBPQ */
#ifdef	USE_SSPI
	if (NULL == hmodule)
	{
		hmodule = LoadLibrary("secur32.dll");
		mylog("secur32 hmodule=%p\n", hmodule);
	}
#endif /* USE_SSPI */
	if (hmodule)
		FreeLibrary(hmodule);
	return (NULL != hmodule);
}
#else
BOOL SSLLIB_check()
{
	return TRUE;
}
#endif	/* WIN_DYN_LOAD */
