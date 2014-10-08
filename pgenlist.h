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

#undef	DLL_DECLARE
#ifdef	_PGENLIST_FUNCS_IMPLEMENT_
#define	DLL_DECLARE	_declspec(dllexport)
#else
#ifdef	_PGENLIST_FUNCS_IMPORT_
#define	DLL_DECLARE	_declspec(dllimport)
#else
#define	DLL_DECLARE
#endif /* _PGENLIST_FUNCS_IMPORT_ */
#endif /* _PGENLIST_FUNCS_IMPLEMENT_ */

RETCODE	EnlistInDtc(void *conn, void *pTra, int method);
RETCODE	DtcOnDisconnect(void *);
RETCODE	IsolateDtcConn(void *, BOOL continueConnection);
//	for testing
DLL_DECLARE	void	*GetTransactionObject(HRESULT *hres);
DLL_DECLARE	void	ReleaseTransactionObject(void *);

#endif /* _HANDLE_ENLIST_IN_DTC_ */
#endif /* WIN32 */

#ifdef	__cplusplus
}
#endif
#endif /* __PGENLIST_H__ */
