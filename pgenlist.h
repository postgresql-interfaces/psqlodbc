/* File:			enlist.h
 *
 * Description:		See "msdtc_enlist.c"
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __PGENLIST_H__
#define __PGENLIST_H__

#ifdef	__cplusplus
extern "C" {
#endif
#ifdef	WIN32
#ifdef	_HANDLE_ENLIST_IN_DTC_
RETCODE	EnlistInDtc(void *conn, void *pTra, int method);
RETCODE	DtcOnDisconnect(void *);
RETCODE	IsolateDtcConn(void *, BOOL continueConnection);
const char *GetXaLibName(void);
const char *GetXaLibPath(void);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
#endif /* WIN32 */

#ifdef	__cplusplus
}
#endif
#endif /* __PGENLIST_H__ */
