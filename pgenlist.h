/* File:			enlsit.h
 *
 * Description:		See "msdtc_enlist.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __PGENLIST_H__
#define __PGENLIST_H__

#include "connection.h"

#ifdef	__cplusplus
extern "C" {
#endif
#ifdef	WIN32
#ifdef	_HANDLE_ENLIST_IN_DTC_
RETCODE	EnlistInDtc(ConnectionClass *conn, void *pTra, int method);
RETCODE	DtcOnDisconnect(ConnectionClass *);
RETCODE	DtcOnRelease(void);
const char *GetXaLibName(void);
const char *GetXaLibPath(void);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
#endif /* WIN32 */

#ifdef	__cplusplus
}
#endif
#endif /* __PGENLIST_H__ */
