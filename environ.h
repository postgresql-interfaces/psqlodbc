/* File:			environ.h
 *
 * Description:		See "environ.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __ENVIRON_H__
#define __ENVIRON_H__

#include "psqlodbc.h"

#define ENV_ALLOC_ERROR 1

/**********		Environment Handle	*************/
struct EnvironmentClass_
{
	char	   *errormsg;
	int		errornumber;
	Int4	flag;
#ifdef	WIN_MULTITHREAD_SUPPORT
	CRITICAL_SECTION	cs;
#endif /* WIN_MULTITHREAD_SUPPORT */
};

/*	Environment prototypes */
EnvironmentClass *EN_Constructor(void);
char		EN_Destructor(EnvironmentClass *self);
char		EN_get_error(EnvironmentClass *self, int *number, char **message);
char		EN_add_connection(EnvironmentClass *self, ConnectionClass *conn);
char		EN_remove_connection(EnvironmentClass *self, ConnectionClass *conn);
void		EN_log_error(char *func, char *desc, EnvironmentClass *self);

#define	EN_OV_ODBC2	1L
#define	EN_CONN_POOLING	(1L<<1)
#define	EN_is_odbc2(env) ((env->flag & EN_OV_ODBC2) != 0)
#define	EN_is_odbc3(env) ((env->flag & EN_OV_ODBC2) == 0)
#define EN_set_odbc2(env) (env->flag |= EN_OV_ODBC2)
#define EN_set_odbc3(env) (env->flag &= EN_OV_ODBC2)
#define	EN_is_pooling(env) ((env->flag & EN_CONN_POOLING) != 0)
#define	EN_set_pooling(env) (env->flag |= EN_CONN_POOLING)
#define	EN_unset_pooling(env) (env->flag &= ~EN_CONN_POOLING)

/* For Multi-thread */
#ifdef  WIN_MULTITHREAD_SUPPORT
#define	INIT_CONNS_CS	InitializeCriticalSection(&conns_cs)
#define	ENTER_CONNS_CS	EnterCriticalSection(&conns_cs)
#define	LEAVE_CONNS_CS	LeaveCriticalSection(&conns_cs)
#define	DELETE_CONNS_CS	DeleteCriticalSection(&conns_cs)
#define INIT_ENV_CS(x)		InitializeCriticalSection(&((x)->cs))
#define ENTER_ENV_CS(x)		EnterCriticalSection(&((x)->cs))
#define LEAVE_ENV_CS(x)		LeaveCriticalSection(&((x)->cs))
#define DELETE_ENV_CS(x)	DeleteCriticalSection(&((x)->cs))
#else
#define	INIT_CONNS_CS
#define	ENTER_CONNS_CS
#define	LEAVE_CONNS_CS
#define	DELETE_CONNS_CS
#define INIT_ENV_CS(x)
#define ENTER_ENV_CS(x)
#define LEAVE_ENV_CS(x)
#define DELETE_ENV_CS(x)
#endif /* WIN_MULTITHREAD_SUPPORT */
#endif
