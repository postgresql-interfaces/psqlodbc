/* File:			connexp.h
 *
 * Description:		See "connection.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __CONNEXPORT_H__
#define __CONNEXPORT_H__

/*
 *	The psqlodbc dll exports functions below used in the pgenlist dll.
 *
 */

#undef	DLL_DECLARE
#ifdef	_PGDTC_FUNCS_IMPLEMENT_
#define	DLL_DECLARE	_declspec(dllexport)
#else
#ifdef	_PGDTC_FUNCS_IMPORT_
#define	DLL_DECLARE	_declspec(dllimport)
#else
#define	DLL_DECLARE
#endif /* _PGDTC_FUNC_IMPORT_ */
#endif /* _PGDTC_FUNCS_IMPLEMENT_ */

#ifdef	__cplusplus
extern "C" {
#endif

#define	KEYWORD_DTC_CHECK	"dtchk"
#define	DTC_CHECK_LINK_ONLY	1
#define	DTC_CHECK_BEFORE_LINK	2
#define	DTC_CHECK_RM_CONNECTION	3

/*	Property */
enum {
	inprogress
	,enlisted
	,inTrans			/* read-only */
	,errorNumber			/* read_only */
	,idleInGlobalTransaction	/* read-only */
	,connected			/* read-only */
	,prepareRequested
};

/* 	PgDtc_isolate option */
enum {
	disposingConnection = 1L
	,useAnotherRoom = (1L << 1)
};

/* 	One phase commit operations */
enum {
	ONE_PHASE_COMMIT = 0
	,ONE_PHASE_ROLLBACK
	,ABORT_GLOBAL_TRANSACTION
	,SHUTDOWN_LOCAL_TRANSACTION
};

/* 	Two phase commit operations */
enum {
	PREPARE_TRANSACTION = 0
	,COMMIT_PREPARED
	,ROLLBACK_PREPARED
};

DLL_DECLARE void PgDtc_create_connect_string(void *self, char *connstr, int strsize);
DLL_DECLARE int  PgDtc_is_recovery_available(void *self, char *reason, int rsize);
DLL_DECLARE void PgDtc_set_async(void *self, void *async);
DLL_DECLARE void *PgDtc_get_async(void *self);
DLL_DECLARE void PgDtc_set_property(void *self, int property, void *value);
DLL_DECLARE void PgDtc_set_error(void *self, const char *message, const char *func);
DLL_DECLARE int	 PgDtc_get_property(void *self, int property);
DLL_DECLARE BOOL PgDtc_connect(void *self);
DLL_DECLARE void PgDtc_free_connect(void *self);
DLL_DECLARE BOOL PgDtc_one_phase_operation(void *self, int operation);
DLL_DECLARE BOOL PgDtc_two_phase_operation(void *self, int operation, const char *gxid);
DLL_DECLARE BOOL PgDtc_lock_cntrl(void *self, BOOL acquire, BOOL bTrial);
DLL_DECLARE void *PgDtc_isolate(void *self, DWORD option);

#ifdef	__cplusplus
}
#endif
#endif /* __CONNEXPORT_H__ */
