/*------
 * Module:			xalibname.c
 *
 * Description:
 *		This module gets the (path) name of xalib.
 *
 *-------
 */

#ifdef	_HANDLE_ENLIST_IN_DTC_

#ifndef	_WIN32_WINNT
#define	_WIN32_WINNT	0x0400
#endif	/* _WIN32_WINNT */

#define	WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

extern HMODULE	s_hModule;

static char	xalibpath[_MAX_PATH] = "";
static char	xalibname[_MAX_FNAME] = "";

const char *GetXaLibName()
{
	char	dllpath[_MAX_PATH], drive[_MAX_DRIVE], dir[_MAX_DIR],
		fname[_MAX_FNAME], ext[_MAX_EXT];
	if (!xalibpath[0])
	{
		GetModuleFileName(s_hModule, dllpath, sizeof(dllpath));
		/* In Windows XP SP2, the dllname should be specified */
		/* instead of the dll path name, because it looks up */
		/* the HKLM\Microfost\MSDTC\XADLL\(dllname) registry */
		/* entry for security reason. */
		_splitpath(dllpath, drive, dir, fname, ext);
		// _snprintf(xalibname, sizeof(xalibname), "%s%s", fname, ext);
		strcpy(xalibname, "pgxalib.dll");
		_snprintf(xalibpath, sizeof(xalibpath), "%s%s%s", drive, dir, xalibname);
	}
	return xalibname;
}

const char *GetXaLibPath()
{
	GetXaLibName();
	return xalibpath;
}
#endif /* _HANDLE_ENLIST_IN_DTC_ */
