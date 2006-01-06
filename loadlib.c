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
HMODULE	LIBPQ_load(BOOL checkOnly)
{
	HMODULE	hmodule = NULL;
#ifdef	WIN32
	CSTR	libpq = "libpq.dll";
	char	szFileName[MAX_PATH];

	mylog("loading libpq library\n");
	/* First try the driver's directory */

	if (GetModuleFileName(s_hModule, szFileName, sizeof(szFileName)) > 0)
	{
		char drive[_MAX_DRIVE], dir[_MAX_DIR];

		_splitpath(szFileName, drive, dir, NULL, NULL);
		snprintf(szFileName, sizeof(szFileName), "%s%s%s", drive, dir, libpq);
		hmodule = LoadLibrary(szFileName);
	}
	if (!hmodule)
		hmodule	= LoadLibrary(libpq);
	if (checkOnly)
		if (hmodule)
			FreeLibrary(hmodule);
#endif	/* WIN32 */
	return hmodule;
}
HMODULE	GetOpenssl(PQFUNC funcs[], int *count)
{
	HMODULE	hmodule = NULL;
#ifdef	WIN32
	PQFUNC	prc;
	int	i, limit;
	static const char * const libarray[] = {"ssleay32", "libssl32", "libeay32"};

	mylog("getting openssl library\n");

	limit = *count;
	*count = -1;
	for (i = 0, prc = NULL; i < sizeof(libarray) / sizeof(const char * const) && !prc; i++)
	{
		if (hmodule = GetModuleHandle(libarray[i]), NULL != hmodule)
			prc = (PQFUNC) GetProcAddress(hmodule, "SSL_read");
	}
	if (!hmodule)
		return hmodule;
	*count = 0;
	if (!prc)
		return hmodule;
	if (*count < limit)
		funcs[*count] = prc;
	(*count)++;
	prc = (PQFUNC) GetProcAddress(hmodule, "SSL_write");
	if (!prc)
		return hmodule;
	if (*count < limit)
		funcs[*count] = prc;
	(*count)++;
	prc = (PQFUNC) GetProcAddress(hmodule, "SSL_get_error");
	if (!prc)
		return hmodule;
	if (*count < limit)
		funcs[*count] = prc;
	(*count)++;
#endif /* WIN32 */
	return hmodule;
}
