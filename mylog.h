/* File:			mylog.h
 *
 * Description:		See "mylog.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __MYLOG_H__
#define __MYLOG_H__

#undef	DLL_DECLARE
#ifdef	WIN32
#ifdef	_MYLOG_FUNCS_IMPLEMENT_
#define	DLL_DECLARE	_declspec(dllexport)
#else
#ifdef	_MYLOG_FUNCS_IMPORT_
#define	DLL_DECLARE	_declspec(dllimport)
#else
#define	DLL_DECLARE
#endif /* _MYLOG_FUNCS_IMPORT_ */
#endif /* _MYLOG_FUNCS_IMPLEMENT_ */
#else
#define	DLL_DECLARE
#endif /* WIN32 */

#include <stdio.h>
#ifndef  WIN32
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

DLL_DECLARE void mylog(const char *fmt,...);
#define	inolog	if (get_mylog() > 1) mylog /* for really temporary debug */

extern void qlog(char *fmt,...);
#define	inoqlog	if (get_qlog() > 1) qlog /* for really temporary debug */

int	get_qlog(void);
int	get_mylog(void);

int	getGlobalDebug();
int	setGlobalDebug(int val);
int	getGlobalCommlog();
int	setGlobalCommlog(int val);
int	writeGlobalLogs();
int	getLogDir(char *dir, int dirmax);
int	setLogDir(const char *dir);

void	InitializeLogging(void);
void	FinalizeLogging(void);

#ifdef __cplusplus
}
#endif
#endif /* __MYLOG_H__ */
