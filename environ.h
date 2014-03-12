/*
 *
 * Description:		See "environ.c"
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __ENVIRON_H__
#define __ENVIRON_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include "psqlodbc.h"

#if defined (POSIX_MULTITHREAD_SUPPORT)
#include <pthread.h>
#endif

#define ENV_ALLOC_ERROR 1

/**********		Environment Handle	*************/
struct EnvironmentClass_
{
	char	   *errormsg;
	int		errornumber;
	Int4	flag;
#if defined(WIN_MULTITHREAD_SUPPORT)
	CRITICAL_SECTION	cs;
#elif defined(POSIX_MULTITHREAD_SUPPORT)
	pthread_mutex_t		cs;
#endif /* WIN_MULTITHREAD_SUPPORT */
};

/*	Environment prototypes */
EnvironmentClass *EN_Constructor(void);
char		EN_Destructor(EnvironmentClass *self);
char		EN_get_error(EnvironmentClass *self, int *number, char **message);
char		EN_add_connection(EnvironmentClass *self, ConnectionClass *conn);
char		EN_remove_connection(EnvironmentClass *self, ConnectionClass *conn);
void		EN_log_error(const char *func, char *desc, EnvironmentClass *self);
int	getConnCount(void);
ConnectionClass * const *getConnList(void);

#define	EN_OV_ODBC2	1L
#define	EN_CONN_POOLING	(1L<<1)
#define	EN_is_odbc2(env) ((env->flag & EN_OV_ODBC2) != 0)
#define	EN_is_odbc3(env) (env && (env->flag & EN_OV_ODBC2) == 0)
#define EN_set_odbc2(env) (env->flag |= EN_OV_ODBC2)
#define EN_set_odbc3(env) (env->flag &= ~EN_OV_ODBC2)
#define	EN_is_pooling(env) (env && (env->flag & EN_CONN_POOLING) != 0)
#define	EN_set_pooling(env) (env->flag |= EN_CONN_POOLING)
#define	EN_unset_pooling(env) (env->flag &= ~EN_CONN_POOLING)

/* For Multi-thread */
#if defined( WIN_MULTITHREAD_SUPPORT)
#define	INIT_CONNS_CS	InitializeCriticalSection(&conns_cs)
#define	ENTER_CONNS_CS	EnterCriticalSection(&conns_cs)
#define	LEAVE_CONNS_CS	LeaveCriticalSection(&conns_cs)
#define	DELETE_CONNS_CS	DeleteCriticalSection(&conns_cs)
#define INIT_ENV_CS(x)		InitializeCriticalSection(&((x)->cs))
#define ENTER_ENV_CS(x)	EnterCriticalSection(&((x)->cs))
#define LEAVE_ENV_CS(x)		LeaveCriticalSection(&((x)->cs))
#define DELETE_ENV_CS(x)	DeleteCriticalSection(&((x)->cs))
#define INIT_COMMON_CS		InitializeCriticalSection(&common_cs)
#define ENTER_COMMON_CS		EnterCriticalSection(&common_cs)
#define LEAVE_COMMON_CS		LeaveCriticalSection(&common_cs)
#define DELETE_COMMON_CS	DeleteCriticalSection(&common_cs)
#elif defined(POSIX_MULTITHREAD_SUPPORT)
#define	INIT_CONNS_CS	pthread_mutex_init(&conns_cs,0)
#define	ENTER_CONNS_CS	pthread_mutex_lock(&conns_cs)
#define	LEAVE_CONNS_CS	pthread_mutex_unlock(&conns_cs)
#define	DELETE_CONNS_CS	pthread_mutex_destroy(&conns_cs)
#define INIT_ENV_CS(x)		pthread_mutex_init(&((x)->cs),0)
#define ENTER_ENV_CS(x)		pthread_mutex_lock(&((x)->cs))
#define LEAVE_ENV_CS(x)		pthread_mutex_unlock(&((x)->cs))
#define DELETE_ENV_CS(x)	pthread_mutex_destroy(&((x)->cs))
#define INIT_COMMON_CS		pthread_mutex_init(&common_cs,0)
#define ENTER_COMMON_CS		pthread_mutex_lock(&common_cs)
#define LEAVE_COMMON_CS		pthread_mutex_unlock(&common_cs)
#define DELETE_COMMON_CS	pthread_mutex_destroy(&common_cs)
#else
#define	INIT_CONNS_CS
#define	ENTER_CONNS_CS
#define	LEAVE_CONNS_CS
#define	DELETE_CONNS_CS
#define INIT_ENV_CS(x)
#define ENTER_ENV_CS(x)
#define LEAVE_ENV_CS(x)
#define DELETE_ENV_CS(x)
#define INIT_COMMON_CS
#define ENTER_COMMON_CS
#define LEAVE_COMMON_CS
#define DELETE_COMMON_CS
#endif /* WIN_MULTITHREAD_SUPPORT */

void shortterm_common_lock(void);
void shortterm_common_unlock(void);
#ifdef	__cplusplus
}
#endif
#endif /* __ENVIRON_H_ */
