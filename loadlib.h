/* File:			loadlib.h
 *
 * Description:		See "loadlib.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __LOADLIB_H__
#define __LOADLIB_H__

#include "psqlodbc.h"
#ifdef	HAVE_LIBLTDL
#include <ltdl.h>
#else
#ifdef	HAVE_DLFCN_H
#include <dlfcn.h>
#endif /* HAVE_DLFCN_H */
#endif /* HAVE_LIBLTDL */

#include <stdlib.h>
#ifdef  __cplusplus
extern "C" {
#endif

BOOL	SSLLIB_check(void);
#ifndef	NOT_USE_LIBPQ
void	*CALL_PQconnectdb(const char *conninfo, BOOL *);
#endif /* NOT_USE_LIBPQ */
BOOL	ssl_verify_available(void);
#ifdef	_HANDLE_ENLIST_IN_DTC_
RETCODE	CALL_EnlistInDtc(ConnectionClass *conn, void * pTra, int method);
RETCODE	CALL_DtcOnDisconnect(ConnectionClass *);
RETCODE	CALL_DtcOnRelease(void);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
/* void	UnloadDelayLoadedDLLs(BOOL); */
void	CleanupDelayLoadedDLLs(void);

#ifdef	__cplusplus
}
#endif
#endif /* __LOADLIB_H__ */
