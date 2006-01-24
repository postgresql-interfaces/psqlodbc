/*------
 * Module:			loadlib.c
 *
 * Description:		This module contains routines related to
 *			loading libpq related library.
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

#ifdef	WIN32
extern	HINSTANCE	s_hModule;
#endif	/* WIN32 */
BOOL LIBPQ_check()
{
	HMODULE	hmodule = NULL;
#ifdef	WIN32
	CSTR	libpq = "libpq";
	char	szFileName[MAX_PATH];

	mylog("checking libpq library\n");
	/* First try the driver's directory */

	if (GetModuleFileName(s_hModule, szFileName, sizeof(szFileName)) > 0)
	{
		char drive[_MAX_DRIVE], dir[_MAX_DIR];

		_splitpath(szFileName, drive, dir, NULL, NULL);
		snprintf(szFileName, sizeof(szFileName), "%s%s%s.dll", drive, dir, libpq);
		hmodule = LoadLibrary(szFileName);
	}
	if (!hmodule)
		hmodule	= LoadLibrary(libpq);
#endif	/* WIN32 */
	return (NULL != hmodule);
}
