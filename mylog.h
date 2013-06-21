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
/*	Uncomment MY_LOG define to compile in the mylog() statements.
	Then, debug logging will occur if 'Debug' is set to 1 in the ODBCINST.INI
	portion of the registry.  You may have to manually add this key.
	This logfile is intended for development use, not for an end user!
*/
#define MY_LOG


/*	Uncomment Q_LOG to compile in the qlog() statements (Communications log, i.e. CommLog).
	This logfile contains serious log statements that are intended for an
	end user to be able to read and understand.  It is controlled by the
	'CommLog' flag in the ODBCINST.INI portion of the registry (see above),
	which is manipulated on the setup/connection dialog boxes.
*/
#define Q_LOG

#ifdef MY_LOG
DLL_DECLARE void mylog(const char *fmt,...);
DLL_DECLARE void forcelog(const char *fmt,...);
#define	inolog	if (get_mylog() > 1) mylog /* for really temporary debug */

#else /* MY_LOG */
#ifndef WIN32
#define mylog(args...)		/* GNU convention for variable arguments */
#define forcelog(args...)	/* GNU convention for variable arguments */
#define inolog(args...)		/* GNU convention for variable arguments */
#else
#define	_DUMMY_LOG_IMPL_
static void DumLog(const char *fmt,...) {}
#define mylog		if (0) DumLog		/* mylog */
#define forcelog	if (0) DumLog		/* forcelog */
#define inolog		if (0) DumLog		/* inolog */
#endif /* WIN32 */
#endif /* MY_LOG */

#ifdef Q_LOG
extern void qlog(char *fmt,...);
#define	inoqlog	if (get_qlog() > 1) qlog /* for really temporary debug */
#else
#ifndef WIN32
#define qlog(args...)		/* GNU convention for variable arguments */
#define inoqlog(args...)	/* GNU convention for variable arguments */
#else
#ifndef	_DUMMY_LOG_IMPL_
#define	_DUMMY_LOG_IMPL_
static void DumLog(const char *fmt,...) {}
#endif /* _DUMMY_LOG_IMPL_ */
#define qlog	if (0) DumLog			/* qlog */
#define inoqlog	if (0) DumLog			/* inoqlog */
#endif /* WIN32 */
#endif /* QLOG */

int	get_qlog(void);
int	get_mylog(void);

void	InitializeLogging();
void	FinalizeLogging();

#ifdef __cplusplus
}
#endif
#endif /* __MYLOG_H__ */
