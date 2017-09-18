/* File:			connection.h
 *
 * Description:		See "connection.c"
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#ifdef	__INCLUDE_POSTGRES_FE_H__ /* currently not defined */
/*
 *	Unfortunately #including postgres_fe.h causes various trobles.
 */
#include "postgres_fe.h"
#else /* currently */
#if defined(__GNUC__) || defined(__IBMC__)
#define PG_PRINTF_ATTRIBUTE gnu_printf
#define pg_attribute_printf(f,a) __attribute__((format(PG_PRINTF_ATTRIBUTE, f, a)))
#else
#define pg_attribute_printf(f,a)
#endif	/* __GNUC__ || __IBMC__ */
#endif	/* __INCLUDE_POSTGRES_FE_H__ */

#include <libpq-fe.h>
#include "pqexpbuffer.h"

#include "psqlodbc.h"
#include <time.h>

#include <stdlib.h>
#include <string.h>
#include "descriptor.h"

#if defined (POSIX_MULTITHREAD_SUPPORT)
#include <pthread.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif
typedef enum
{
	CONN_NOT_CONNECTED,	/* Connection has not been established */
	CONN_CONNECTED,		/* Connection is up and has been established */
	CONN_DOWN,		/* Connection is broken */
	CONN_EXECUTING		/* the connection is currently executing a
				 * statement */
} CONN_Status;

/*	These errors have general sql error state */
#define CONNECTION_SERVER_NOT_REACHED				101
#define CONNECTION_MSG_TOO_LONG					103
#define CONNECTION_COULD_NOT_SEND				104
#define CONNECTION_NO_SUCH_DATABASE				105
#define CONNECTION_BACKEND_CRAZY				106
#define CONNECTION_NO_RESPONSE					107
#define CONNECTION_SERVER_REPORTED_SEVERITY_FATAL		108
#define CONNECTION_COULD_NOT_RECEIVE				109
#define CONNECTION_SERVER_REPORTED_SEVERITY_ERROR		110
#define CONNECTION_NEED_PASSWORD				112
#define CONNECTION_COMMUNICATION_ERROR				113

#define CONN_ERROR_IGNORED						(-3)
#define CONN_TRUNCATED							(-2)
#define CONN_OPTION_VALUE_CHANGED					(-1)
/*	These errors correspond to specific SQL states */
#define CONN_INIREAD_ERROR						201
#define CONN_OPENDB_ERROR						202
#define CONN_STMT_ALLOC_ERROR						203
#define CONN_IN_USE							204
#define CONN_UNSUPPORTED_OPTION						205
/* Used by SetConnectoption to indicate unsupported options */
#define CONN_INVALID_ARGUMENT_NO					206
/* SetConnectOption: corresponds to ODBC--"S1009" */
#define CONN_TRANSACT_IN_PROGRES					207
#define CONN_NO_MEMORY_ERROR						208
#define CONN_NOT_IMPLEMENTED_ERROR					209
#define CONN_INVALID_AUTHENTICATION					210
#define CONN_AUTH_TYPE_UNSUPPORTED					211
#define CONN_UNABLE_TO_LOAD_DLL						212
#define CONN_ILLEGAL_TRANSACT_STATE					213
#define CONN_VALUE_OUT_OF_RANGE						214

#define CONN_OPTION_NOT_FOR_THE_DRIVER					216
#define CONN_EXEC_ERROR							217

/* Conn_status defines */
#define CONN_IN_AUTOCOMMIT		1L
#define CONN_IN_TRANSACTION		(1L<<1)
#define CONN_IN_MANUAL_TRANSACTION	(1L<<2)
#define CONN_IN_ERROR_BEFORE_IDLE	(1L<<3)

/* not connected yet || already disconnected */
#define	CC_not_connected(x)	(!(x) || CONN_DOWN == (x)->status || CONN_NOT_CONNECTED == (x)->status)

/* AutoCommit functions */
#define CC_is_in_autocommit(x)		(x->transact_status & CONN_IN_AUTOCOMMIT)
#define CC_does_autocommit(x) (CONN_IN_AUTOCOMMIT == ((x)->transact_status & (CONN_IN_AUTOCOMMIT | CONN_IN_MANUAL_TRANSACTION)))
#define CC_loves_visible_trans(x) ((0 == ((x)->transact_status & CONN_IN_AUTOCOMMIT)) || (0 != ((x)->transact_status & CONN_IN_MANUAL_TRANSACTION)))

/* Transaction in/not functions */
#define CC_set_in_trans(x)	(x->transact_status |= CONN_IN_TRANSACTION)
#define CC_set_no_trans(x)	(x->transact_status &= ~(CONN_IN_TRANSACTION | CONN_IN_ERROR_BEFORE_IDLE))
#define CC_is_in_trans(x)	(0 != (x->transact_status & CONN_IN_TRANSACTION))

/* Manual transaction in/not functions */
#define CC_set_in_manual_trans(x) (x->transact_status |= CONN_IN_MANUAL_TRANSACTION)
#define CC_set_no_manual_trans(x) (x->transact_status &= ~CONN_IN_MANUAL_TRANSACTION)
#define CC_is_in_manual_trans(x) (0 != (x->transact_status & CONN_IN_MANUAL_TRANSACTION))

/* Error waiting for ROLLBACK */
#define CC_set_in_error_trans(x) (x->transact_status |= CONN_IN_ERROR_BEFORE_IDLE)
#define CC_set_no_error_trans(x) (x->transact_status &= ~CONN_IN_ERROR_BEFORE_IDLE)
#define CC_is_in_error_trans(x) (x->transact_status & CONN_IN_ERROR_BEFORE_IDLE)

#define CC_get_errornumber(x)	(x->__error_number)
#define CC_get_errormsg(x)	(x->__error_message)
#define CC_set_errornumber(x, n)	(x->__error_number = n)

/* Unicode handling */
#define	CONN_UNICODE_DRIVER	(1L)
#define	CONN_ANSI_APP		(1L << 1)
#define	CONN_DISALLOW_WCHAR	(1L << 2)
#define	CC_set_in_unicode_driver(x)	(x->unicode |= CONN_UNICODE_DRIVER)
#define	CC_set_in_ansi_app(x)	(x->unicode |= CONN_ANSI_APP)
#define	CC_is_in_unicode_driver(x)	(0 != (x->unicode & CONN_UNICODE_DRIVER))
#define	CC_is_in_ansi_app(x)	(0 != (x->unicode & CONN_ANSI_APP))
#define	CC_is_in_global_trans(x)	(NULL != (x)->asdum)
#define	ALLOW_WCHAR(x)	(0 != (x->unicode & CONN_UNICODE_DRIVER) && 0 == (x->unicode & CONN_DISALLOW_WCHAR))

#define CC_MALLOC_return_with_error(t, tp, s, x, m, ret) \
do { \
	if (t = malloc(s), NULL == t) \
	{ \
		CC_set_error(x, CONN_NO_MEMORY_ERROR, m, ""); \
		return ret; \
	} \
} while (0)
#define CC_REALLOC_return_with_error(t, tp, s, x, m, ret) \
do { \
	tp *tmp; \
	if (tmp = (tp *) realloc(t, s), NULL == tmp) \
	{ \
		CC_set_error(x, CONN_NO_MEMORY_ERROR, m, ""); \
		return ret; \
	} \
	t = tmp; \
} while (0)

/* For Multi-thread */
#if defined(WIN_MULTITHREAD_SUPPORT)
#define INIT_CONN_CS(x)		InitializeCriticalSection(&((x)->cs))
#define INIT_CONNLOCK(x)	InitializeCriticalSection(&((x)->slock))
#define ENTER_CONN_CS(x)	EnterCriticalSection(&((x)->cs))
#define CONNLOCK_ACQUIRE(x)	EnterCriticalSection(&((x)->slock))
#define TRY_ENTER_CONN_CS(x)	TryEnterCriticalSection(&((x)->cs))
#define ENTER_INNER_CONN_CS(x, entered) \
do { \
	EnterCriticalSection(&((x)->cs)); \
	entered++; \
} while (0)
#define LEAVE_CONN_CS(x)	LeaveCriticalSection(&((x)->cs))
#define CONNLOCK_RELEASE(x)	LeaveCriticalSection(&((x)->slock))
#define DELETE_CONN_CS(x)	DeleteCriticalSection(&((x)->cs))
#define DELETE_CONNLOCK(x)	DeleteCriticalSection(&((x)->slock))
#elif defined(POSIX_THREADMUTEX_SUPPORT)
#define INIT_CONN_CS(x)		pthread_mutex_init(&((x)->cs), getMutexAttr())
#define INIT_CONNLOCK(x)	pthread_mutex_init(&((x)->slock), getMutexAttr())
#define ENTER_CONN_CS(x)	pthread_mutex_lock(&((x)->cs))
#define CONNLOCK_ACQUIRE(x)		pthread_mutex_lock(&((x)->slock))
#define TRY_ENTER_CONN_CS(x)	(0 == pthread_mutex_trylock(&((x)->cs)))
#define ENTER_INNER_CONN_CS(x, entered) \
do { \
	if (getMutexAttr()) \
	{ \
		if (pthread_mutex_lock(&((x)->cs)) == 0) \
			entered++; \
	} \
} while (0)
#define LEAVE_CONN_CS(x)	pthread_mutex_unlock(&((x)->cs))
#define CONNLOCK_RELEASE(x) 	pthread_mutex_unlock(&((x)->slock))
#define DELETE_CONN_CS(x)	pthread_mutex_destroy(&((x)->cs))
#define DELETE_CONNLOCK(x)	pthread_mutex_destroy(&((x)->slock))
#else
#define INIT_CONN_CS(x)
#define INIT_CONNLOCK(x)
#define TRY_ENTER_CONN_CS(x)	(1)
#define ENTER_CONN_CS(x)
#define CONNLOCK_ACQUIRE(x)
#define ENTER_INNER_CONN_CS(x, entered)
#define LEAVE_CONN_CS(x)
#define CONNLOCK_RELEASE(x)
#define DELETE_CONN_CS(x)
#define DELETE_CONNLOCK(x)
#endif /* WIN_MULTITHREAD_SUPPORT */

#define	LEAVE_INNER_CONN_CS(entered, conn) \
do { \
	if (entered > 0) \
	{ \
		LEAVE_CONN_CS(conn); \
		entered--; \
	} \
} while (0)
#define CLEANUP_FUNC_CONN_CS(entered, conn) \
do { \
	while (entered > 0) \
	{ \
		LEAVE_CONN_CS(conn); \
		entered--; \
	} \
} while (0)

/*
 *	Macros to compare the server's version with a specified version
 *		1st parameter: pointer to a ConnectionClass object
 *		2nd parameter: major version number
 *		3rd parameter: minor version number
 */
#define SERVER_VERSION_GT(conn, major, minor) \
	((conn)->pg_version_major > major || \
	((conn)->pg_version_major == major && (conn)->pg_version_minor > minor))
#define SERVER_VERSION_GE(conn, major, minor) \
	((conn)->pg_version_major > major || \
	((conn)->pg_version_major == major && (conn)->pg_version_minor >= minor))
#define SERVER_VERSION_EQ(conn, major, minor) \
	((conn)->pg_version_major == major && (conn)->pg_version_minor == minor)
#define STRING_AFTER_DOT(string)	(strchr(#string, '.') + 1)

/*
 *	Simplified macros to compare the server's version with a
 *		specified version
 *	Note: Never pass a variable as the second parameter.
 *		  It must be a decimal constant of the form %d.%d .
 */
#define PG_VERSION_GT(conn, ver) \
 (SERVER_VERSION_GT(conn, (int) ver, atoi(STRING_AFTER_DOT(ver))))
#define PG_VERSION_GE(conn, ver) \
 (SERVER_VERSION_GE(conn, (int) ver, atoi(STRING_AFTER_DOT(ver))))
#define PG_VERSION_EQ(conn, ver) \
 (SERVER_VERSION_EQ(conn, (int) ver, atoi(STRING_AFTER_DOT(ver))))
#define PG_VERSION_LE(conn, ver) (! PG_VERSION_GT(conn, ver))
#define PG_VERSION_LT(conn, ver) (! PG_VERSION_GE(conn, ver))

/*	This is used to store cached table information in the connection */
struct col_info
{
	Int2		refcnt;
	QResultClass	*result;
	pgNAME		schema_name;
	pgNAME		table_name;
	OID		table_oid;
	time_t		acc_time;
};
#define free_col_info_contents(coli) \
{ \
	if (NULL != coli->result) \
		QR_Destructor(coli->result); \
	coli->result = NULL; \
	NULL_THE_NAME(coli->schema_name); \
	NULL_THE_NAME(coli->table_name); \
	coli->table_oid = 0; \
	coli->refcnt = 0; \
	coli->acc_time = 0; \
}
#define col_info_initialize(coli) (memset(coli, 0, sizeof(COL_INFO)))

 /* Translation DLL entry points */
#ifdef WIN32
#define DLLHANDLE HINSTANCE
#else
#define WINAPI CALLBACK
#define DLLHANDLE void *
#define HINSTANCE void *
#endif

typedef BOOL (WINAPI * DataSourceToDriverProc) (UDWORD, SWORD, PTR,
		SDWORD, PTR, SDWORD, SDWORD *, UCHAR *, SWORD,
		SWORD *);
typedef BOOL (WINAPI * DriverToDataSourceProc) (UDWORD, SWORD, PTR,
		SDWORD, PTR, SDWORD, SDWORD *, UCHAR *, SWORD,
		SWORD *);

/*******	The Connection handle	************/
struct ConnectionClass_
{
	HENV		henv;		/* environment this connection was
					 * created on */
	SQLUINTEGER	login_timeout;
	signed char	autocommit_public;
	StatementOptions stmtOptions;
	ARDFields	ardOptions;
	APDFields	apdOptions;
	char		*__error_message;
	int		__error_number;
	char		sqlstate[8];
	CONN_Status	status;
	ConnInfo	connInfo;
	StatementClass	**stmts;
	Int2		num_stmts;
	Int2		ncursors;
	PGconn	   *pqconn;
	Int4		lobj_type;
	Int2		coli_allocated;
	Int2		ntables;
	COL_INFO	**col_info;
	long		translation_option;
	HINSTANCE	translation_handle;
	DataSourceToDriverProc DataSourceToDriver;
	DriverToDataSourceProc DriverToDataSource;
	char		transact_status;	/* Is a transaction is currently
						 * in progress */
	char		pg_version[MAX_INFO_STRING];	/* Version of PostgreSQL
							 * we're connected to -
							 * DJP 25-1-2001 */
	Int2		pg_version_major;
	Int2		pg_version_minor;
	char		ms_jet;
	char		unicode;
	char		result_uncommitted;
	char		lo_is_domain;
	char		current_schema_valid;	/* is current_schema valid? TRUE when
						 * current_schema == NULL means it's
						 * really NULL, while FALSE means it's
						 * unknown */
	unsigned char	on_commit_in_progress;
	/* for per statement rollback */
	char		internal_svp;		/* is set? */
	char		internal_op;		/* operation being executed as to internal savepoint */
	unsigned char	lock_CC_for_rb;
	unsigned char	rbonerr;
	unsigned char	opt_in_progress;
	unsigned char	opt_previous;

	char		*original_client_encoding;
	char		*locale_encoding;
	char		*server_encoding;
	Int2		ccsc;
	Int2		mb_maxbyte_per_char;
	SQLUINTEGER	isolation;		/* isolation level initially unknown */
	SQLUINTEGER	server_isolation;	/* isolation at server initially unknown */
	char		*current_schema;
	StatementClass *unnamed_prepared_stmt;
	Int2		max_identifier_length;
	Int2		num_discardp;
	char		**discardp;
	int		num_descs;
	SQLUINTEGER	default_isolation;	/* server's default isolation initially unkown */
	DescriptorClass	**descs;
	pgNAME		schemaIns;
	pgNAME		tableIns;
	SQLULEN		stmt_timeout_in_effect;
#if defined(WIN_MULTITHREAD_SUPPORT)
	CRITICAL_SECTION	cs;
	CRITICAL_SECTION	slock;
#elif defined(POSIX_THREADMUTEX_SUPPORT)
	pthread_mutex_t		cs;
	pthread_mutex_t		slock;
#endif /* WIN_MULTITHREAD_SUPPORT */
#ifdef	_HANDLE_ENLIST_IN_DTC_
	UInt4		gTranInfo;
	void		*asdum;
#endif /* _HANDLE_ENLIST_IN_DTC_ */
};


/* Accessor functions */
#define CC_get_env(x)				((x)->henv)
#define CC_get_database(x)			(x->connInfo.database)
#define CC_get_server(x)			(x->connInfo.server)
#define CC_get_DSN(x)				(x->connInfo.dsn)
#define CC_get_username(x)			(x->connInfo.username)
#define CC_is_onlyread(x)			(x->connInfo.onlyread[0] == '1')
#define CC_fake_mss(x)	(/* 0 != (x)->ms_jet && */ 0 < (x)->connInfo.fake_mss)
#define CC_accessible_only(x)	(0 < (x)->connInfo.accessible_only)
#define CC_default_is_c(x)	(CC_is_in_ansi_app(x) || x->ms_jet /* not only */ || TRUE /* but for any other ? */)

#ifdef	_HANDLE_ENLIST_IN_DTC_
enum {
	DTC_IN_PROGRESS	= 1L
	,DTC_ENLISTED	= (1L << 1)
	,DTC_REQUEST_EXECUTING	= (1L << 2)
	,DTC_ISOLATED	= (1L << 3)
	,DTC_PREPARE_REQUESTED	= (1L << 4)
};
#define	CC_set_dtc_clear(x)	((x)->gTranInfo = 0)
#define	CC_set_dtc_enlisted(x)	((x)->gTranInfo |= (DTC_IN_PROGRESS | DTC_ENLISTED))
#define	CC_no_dtc_enlisted(x)	((x)->gTranInfo &= (~DTC_ENLISTED))
#define	CC_is_dtc_enlisted(x)	(0 != ((x)->gTranInfo & DTC_ENLISTED))
#define	CC_set_dtc_executing(x)	((x)->gTranInfo |= DTC_REQUEST_EXECUTING)
#define	CC_no_dtc_executing(x)	((x)->gTranInfo &= (~DTC_REQUEST_EXECUTING))
#define	CC_is_dtc_executing(x)	(0 != ((x)->gTranInfo & DTC_REQUEST_EXECUTING))
#define	CC_set_dtc_prepareRequested(x)	((x)->gTranInfo |= (DTC_PREPARE_REQUESTED))
#define	CC_no_dtc_prepareRequested(x)	((x)->gTranInfo &= (~DTC_PREPARE_REQUESTED))
#define	CC_is_dtc_prepareRequested(x)	(0 != ((x)->gTranInfo & DTC_PREPARE_REQUESTED))
#define	CC_is_dtc_executing(x)	(0 != ((x)->gTranInfo & DTC_REQUEST_EXECUTING))
#define	CC_set_dtc_isolated(x)	((x)->gTranInfo |= DTC_ISOLATED)
#define	CC_is_idle_in_global_transaction(x)	(0 != ((x)->gTranInfo & DTC_PREPARE_REQUESTED) || (x)->gTranInfo == DTC_IN_PROGRESS)
#endif /* _HANDLE_ENLIST_IN_DTC_ */
/* statement callback */
#define CC_start_stmt(a)        ((a)->rbonerr = 0)
#define CC_start_tc_stmt(a)     ((a)->rbonerr = (1L << 1))
#define CC_is_tc_stmt(a)        (((a)->rbonerr & (1L << 1)) != 0)
#define CC_start_rb_stmt(a)     ((a)->rbonerr = (1L << 2))
#define CC_is_rb_stmt(a)        (((a)->rbonerr & (1L << 2)) != 0)
#define CC_set_accessed_db(a)   ((a)->rbonerr |= (1L << 3))
#define CC_accessed_db(a)       (((a)->rbonerr & (1L << 3)) != 0)
#define CC_start_rbpoint(a)     ((a)->rbonerr |= (1L << 4), (a)->internal_svp = 1)
#define CC_started_rbpoint(a)   (((a)->rbonerr & (1L << 4)) != 0)

/*	prototypes */
ConnectionClass *CC_Constructor(void);
char		CC_Destructor(ConnectionClass *self);
int		CC_cursor_count(ConnectionClass *self);
char		CC_cleanup(ConnectionClass *self, BOOL keepCommunication);
char		CC_begin(ConnectionClass *self);
char		CC_commit(ConnectionClass *self);
char		CC_abort(ConnectionClass *self);
char		CC_set_autocommit(ConnectionClass *self, BOOL on);
int		CC_set_translation(ConnectionClass *self);
char		CC_connect(ConnectionClass *self, char *salt);
char		CC_add_statement(ConnectionClass *self, StatementClass *stmt);
char		CC_remove_statement(ConnectionClass *self, StatementClass *stmt)
;
char		CC_add_descriptor(ConnectionClass *self, DescriptorClass *desc);
char		CC_remove_descriptor(ConnectionClass *self, DescriptorClass *desc);
void		CC_set_error(ConnectionClass *self, int number, const char *message, const char *func);
void		CC_set_errormsg(ConnectionClass *self, const char *message);
char		CC_get_error(ConnectionClass *self, int *number, char **message);
QResultClass *CC_send_query_append(ConnectionClass *self, const char *query, QueryInfo *qi, UDWORD flag, StatementClass *stmt, const char *appendq);
#define CC_send_query(self, query, qi, flag, stmt) CC_send_query_append(self, query, qi, flag, stmt, NULL)
void		handle_pgres_error(ConnectionClass *self, const PGresult *pgres,
				   const char *comment,
				   QResultClass *res, BOOL error_not_a_notice);
void		CC_clear_error(ConnectionClass *self);
int		CC_send_function(ConnectionClass *conn, const char *fn_name, void *result_buf, int *actual_result_len, int result_is_int, LO_ARG *argv, int nargs);
char		CC_send_settings(ConnectionClass *self, const char *set_query);
void		CC_initialize_pg_version(ConnectionClass *conn);
void		CC_log_error(const char *func, const char *desc, const ConnectionClass *self);
int			CC_send_cancel_request(const ConnectionClass *conn);
void		CC_on_commit(ConnectionClass *conn);
void		CC_on_abort(ConnectionClass *conn, unsigned int opt);
void		CC_on_abort_partial(ConnectionClass *conn);
void		ProcessRollback(ConnectionClass *conn, BOOL undo, BOOL partial);
const char	*CC_get_current_schema(ConnectionClass *conn);
int             CC_mark_a_object_to_discard(ConnectionClass *conn, int type, const char *plan);
int             CC_discard_marked_objects(ConnectionClass *conn);

int		CC_get_max_idlen(ConnectionClass *self);
char	CC_get_escape(const ConnectionClass *self);
char *		identifierEscape(const SQLCHAR *src, SQLLEN srclen, const ConnectionClass *conn, char *buf, size_t bufsize, BOOL double_quote);
int		findIdentifier(const UCHAR *str, int ccsc, const UCHAR **next_token);
int		eatTableIdentifiers(const UCHAR *str, int ccsc, pgNAME *table, pgNAME *schema);


const		char *CurrCat(const ConnectionClass *self);
const		char *CurrCatString(const ConnectionClass *self);
SQLUINTEGER	CC_get_isolation(ConnectionClass *self);

SQLCHAR	*make_lstring_ifneeded(ConnectionClass *, const SQLCHAR *s, ssize_t len, BOOL);

#define	TABLE_IS_VALID(tbname, tblen)	((tbname) && (tblen > 0 || SQL_NTS == tblen))
int	schema_str(char *buf, int buflen, const SQLCHAR *s, SQLLEN len, BOOL table_is_valid, ConnectionClass *conn);
char	*schema_strcat(char *buf, int buflen, const char *fmt, const SQLCHAR *s, SQLLEN len,
		BOOL table_is_valid, ConnectionClass *conn);
char	*schema_strcat1(char *buf, int buflen, const char *fmt, const char *s1,
				const char *s,
				const SQLCHAR *, int, ConnectionClass *conn);

void	schema_appendPQExpBuffer(PQExpBufferData *buf, const char *fmt, const SQLCHAR *s, SQLLEN len,
		BOOL table_is_valid, ConnectionClass *conn);
void	schema_appendPQExpBuffer1(PQExpBufferData *buf, const char *fmt, const char *s1, const char *s,
				BOOL table_is_valid, ConnectionClass *conn);

void	CC_examine_global_transaction(ConnectionClass *self);


BOOL CC_set_transact(ConnectionClass *self, UInt4 isolation);
int CC_send_client_encoding(ConnectionClass *self, const char *encoding);

/* CC_send_query options */
enum {
	IGNORE_ABORT_ON_CONN	= 1L /* not set the error result even when  */
	,CREATE_KEYSET		= (1L << 1) /* create keyset for updatable cursors */
	,GO_INTO_TRANSACTION	= (1L << 2) /* issue BEGIN in advance */
	,ROLLBACK_ON_ERROR	= (1L << 3) /* rollback the query when an error occurs */
	,END_WITH_COMMIT	= (1L << 4) /* the query ends with COMMIT command */
	,READ_ONLY_QUERY	= (1L << 5) /* the query is read-only */
};
/* CC_on_abort options */
#define	NO_TRANS		1L
#define	CONN_DEAD		(1L << 1) /* connection is no longer valid */

/*
 *	internal savepoint related
 */

#define	_RELEASE_INTERNAL_SAVEPOINT

/*      Internal rollback */
enum {
	PER_STATEMENT_ROLLBACK = 1
	,PER_QUERY_ROLLBACK
};
int CC_internal_rollback(ConnectionClass *, int rollback_type, BOOL ignore_abort);

/*     Commands generated */
enum {
	INTERNAL_SAVEPOINT_OPERATION = 1
	,INTERNAL_ROLLBACK_OPERATION
};
int	GenerateSvpCommand(ConnectionClass *conn, int type, char *cmd, int bufsize);

/*      Operations in progress */
enum {
        SAVEPOINT_IN_PROGRESS = 1
        ,PREPEND_IN_PROGRESS
};
/*      StatementSvp entry option */
enum {
        SVPOPT_RDONLY = 1L
        ,SVPOPT_REDUCE_ROUNDTRIP = (1L << 1)
};
#define	INIT_SVPOPT	(SVPOPT_RDONLY)
#define CC_svp_init(a) ((a)->internal_svp = (a)->internal_op = 0, (a)->opt_in_progress = (a)->opt_previous = INIT_SVPOPT)
#define CC_init_opt_in_progress(a) ((a)->opt_in_progress = INIT_SVPOPT)
#define CC_init_opt_previous(a) ((a)->opt_previous = INIT_SVPOPT)

#ifdef	__cplusplus
}
#endif
#endif /* __CONNECTION_H__ */
