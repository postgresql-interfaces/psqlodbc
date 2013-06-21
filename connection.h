/* File:			connection.h
 *
 * Description:		See "connection.c"
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

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

enum
{
	DISALLOW_UPDATABLE_CURSORS = 0,	/* No cursors are updatable */
	ALLOW_STATIC_CURSORS = 1L, /* Static cursors are updatable */
	ALLOW_KEYSET_DRIVEN_CURSORS = (1L << 1), /* Keyset-driven cursors are updatable */
	ALLOW_DYNAMIC_CURSORS = (1L << 2), /* Dynamic cursors are updatable */
	ALLOW_BULK_OPERATIONS = (1L << 3), /* Bulk operations available */
	SENSE_SELF_OPERATIONS = (1L << 4), /* Sense self update/delete/add */
};

/*	These errors have general sql error state */
#define CONNECTION_SERVER_NOT_REACHED				101
#define CONNECTION_MSG_TOO_LONG					103
#define CONNECTION_COULD_NOT_SEND				104
#define CONNECTION_NO_SUCH_DATABASE				105
#define CONNECTION_BACKEND_CRAZY				106
#define CONNECTION_NO_RESPONSE					107
#define CONNECTION_SERVER_REPORTED_ERROR			108
#define CONNECTION_COULD_NOT_RECEIVE				109
#define CONNECTION_SERVER_REPORTED_WARNING			110
#define CONNECTION_NEED_PASSWORD				112
#define CONNECTION_COMMUNICATION_ERROR				113

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

#define CONN_VALUE_OUT_OF_RANGE						214

#define CONN_OPTION_NOT_FOR_THE_DRIVER					216
#define CONN_EXEC_ERROR							217

/* Conn_status defines */
#define CONN_IN_AUTOCOMMIT		1L 
#define CONN_IN_TRANSACTION		(1L<<1)
#define CONN_IN_MANUAL_TRANSACTION	(1L<<2)
#define CONN_IN_ERROR_BEFORE_IDLE	(1L<<3)

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
#define CONNLOCK_ACQUIRE(x) 	pthread_mutex_lock(&((x)->slock))
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

/* Authentication types */
#define AUTH_REQ_OK		0
#define AUTH_REQ_KRB4		1
#define AUTH_REQ_KRB5		2
#define AUTH_REQ_PASSWORD	3
#define AUTH_REQ_CRYPT		4
#define AUTH_REQ_MD5		5
#define AUTH_REQ_SCM_CREDS	6
#define AUTH_REQ_GSS		7
#define AUTH_REQ_GSS_CONT	8
#define AUTH_REQ_SSPI		9

/*	Startup Packet sizes */
#define SM_DATABASE		64
#define SM_USER			32
#define SM_OPTIONS		64
#define SM_UNUSED		64
#define SM_TTY			64

/*	Old 6.2 protocol defines */
#define NO_AUTHENTICATION	7
#define PATH_SIZE		64
#define ARGV_SIZE		64
#define USRNAMEDATALEN		16

typedef unsigned int ProtocolVersion;

#define PG_PROTOCOL(major, minor)	(((major) << 16) | (minor))
#define PG_PROTOCOL_LATEST	PG_PROTOCOL(3, 0) 
#define PG_PROTOCOL_74	PG_PROTOCOL(3, 0) 
#define PG_PROTOCOL_64	PG_PROTOCOL(2, 0) 
#define PG_PROTOCOL_63	PG_PROTOCOL(1, 0)
#define PG_PROTOCOL_62	PG_PROTOCOL(0, 0)
#define PG_NEGOTIATE_SSLMODE	PG_PROTOCOL(1234, 5679)

/*	This startup packet is to support latest Postgres protocol (6.4, 6.3) */
typedef struct _StartupPacket
{
	ProtocolVersion protoVersion;
	char		database[SM_DATABASE];
	char		user[SM_USER];
	char		options[SM_OPTIONS];
	char		unused[SM_UNUSED];
	char		tty[SM_TTY];
} StartupPacket;


/*	This startup packet is to support pre-Postgres 6.3 protocol */
typedef struct _StartupPacket6_2
{
	unsigned int authtype;
	char		database[PATH_SIZE];
	char		user[USRNAMEDATALEN];
	char		options[ARGV_SIZE];
	char		execfile[ARGV_SIZE];
	char		tty[PATH_SIZE];
} StartupPacket6_2;

/* Transferred from pqcomm.h:  */


typedef ProtocolVersion MsgType;

#define CANCEL_REQUEST_CODE PG_PROTOCOL(1234,5678)

typedef struct CancelRequestPacket
{
	/* Note that each field is stored in network byte order! */
	MsgType		cancelRequestCode;	/* code to identify a cancel request */
	unsigned int	backendPID;	/* PID of client's backend */
	unsigned int	cancelAuthCode; /* secret key to authorize cancel */
} CancelRequestPacket;

/*	Structure to hold all the connection attributes for a specific
	connection (used for both registry and file, DSN and DRIVER)
*/
typedef struct
{
	char		dsn[MEDIUM_REGISTRY_LEN];
	char		desc[MEDIUM_REGISTRY_LEN];
	char		drivername[MEDIUM_REGISTRY_LEN];
	char		server[MEDIUM_REGISTRY_LEN];
	char		database[MEDIUM_REGISTRY_LEN];
	char		username[MEDIUM_REGISTRY_LEN];
	pgNAME		password;
	char		protocol[SMALL_REGISTRY_LEN];
	char		port[SMALL_REGISTRY_LEN];
	char		sslmode[16];
	char		onlyread[SMALL_REGISTRY_LEN];
	char		fake_oid_index[SMALL_REGISTRY_LEN];
	char		show_oid_column[SMALL_REGISTRY_LEN];
	char		row_versioning[SMALL_REGISTRY_LEN];
	char		show_system_tables[SMALL_REGISTRY_LEN];
	char		translation_dll[MEDIUM_REGISTRY_LEN];
	char		translation_option[SMALL_REGISTRY_LEN];
	char		focus_password;
	pgNAME		conn_settings;
	signed char	disallow_premature;
	signed char	allow_keyset;
	signed char	updatable_cursors;
	signed char	lf_conversion;
	signed char	true_is_minus1;
	signed char	int8_as;
	signed char	bytea_as_longvarbinary;
	signed char	use_server_side_prepare;
	signed char	lower_case_identifier;
	signed char	rollback_on_error;
	signed char	force_abbrev_connstr;
	signed char	bde_environment;
	signed char	fake_mss;
	signed char	cvt_null_date_string;
	signed char	autocommit_public;
	signed char	accessible_only;
	signed char	gssauth_use_gssapi;
	UInt4		extra_opts;
#ifdef	_HANDLE_ENLIST_IN_DTC_
	signed char	xa_opt;
#endif /* _HANDLE_ENLIST_IN_DTC_ */
	GLOBAL_VALUES drivers;		/* moved from driver's option */
} ConnInfo;

/*	Macro to determine is the connection using 6.2 protocol? */
#define PROTOCOL_62(conninfo_)		(strncmp((conninfo_)->protocol, PG62, strlen(PG62)) == 0)

/*	Macro to determine is the connection using 6.3 protocol? */
#define PROTOCOL_63(conninfo_)		(strncmp((conninfo_)->protocol, PG63, strlen(PG63)) == 0)

/*	Macro to determine is the connection using 6.4 protocol? */
#define PROTOCOL_64(conninfo_)		(strncmp((conninfo_)->protocol, PG64, strlen(PG64)) == 0)

/*	Macro to determine is the connection using 7.4 protocol? */
#define PROTOCOL_74(conninfo_)		(strncmp((conninfo_)->protocol, PG74, strlen(PG74)) == 0)

/*	Macro to determine is the connection using 7.4 rejected? */
#define PROTOCOL_74REJECTED(conninfo_)	(strncmp((conninfo_)->protocol, PG74REJECTED, strlen(PG74REJECTED)) == 0)

#define SUPPORT_DESCRIBE_PARAM(conninfo_) (PROTOCOL_74(conninfo_) && conninfo_->use_server_side_prepare)
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
#define SERVER_VERSION_LE(conn, major, minor) (! SERVER_VERSION_GT(conn, major, minor))
#define SERVER_VERSION_LT(conn, major, minor) (! SERVER_VERSION_GE(conn, major, minor))
/*#if ! defined(HAVE_CONFIG_H) || defined(HAVE_STRINGIZE)*/
#define STRING_AFTER_DOT(string)	(strchr(#string, '.') + 1)
/*#else
#define STRING_AFTER_DOT(str)	(strchr("str", '.') + 1)
#endif*/
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
	Int2		num_reserved_cols;
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

typedef BOOL (FAR WINAPI * DataSourceToDriverProc) (UDWORD, SWORD, PTR,
		SDWORD, PTR, SDWORD, SDWORD FAR *, UCHAR FAR *, SWORD,
		SWORD FAR *); 
typedef BOOL (FAR WINAPI * DriverToDataSourceProc) (UDWORD, SWORD, PTR,
		SDWORD, PTR, SDWORD, SDWORD FAR *, UCHAR FAR *, SWORD,
		SWORD FAR *);

/*******	The Connection handle	************/
struct ConnectionClass_
{
	HENV		henv;		/* environment this connection was
					 * created on */
	SQLUINTEGER	login_timeout;
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
	SocketClass	*sock;
	Int4		lobj_type;
	Int2		coli_allocated;
	Int2		ntables;
	COL_INFO	**col_info;
	long		translation_option;
	HINSTANCE	translation_handle;
	DataSourceToDriverProc DataSourceToDriver;
	DriverToDataSourceProc DriverToDataSource;
	Int2		driver_version;		/* prepared for ODBC3.0 */
	char		transact_status;	/* Is a transaction is currently
						 * in progress */
	char		errormsg_created;	/* has an informative error msg
						 * been created ? */
	char		pg_version[MAX_INFO_STRING];	/* Version of PostgreSQL
							 * we're connected to -
							 * DJP 25-1-2001 */
	float		pg_version_number;
	Int2		pg_version_major;
	Int2		pg_version_minor;
	char		ms_jet;
	char		unicode;
	char		result_uncommitted;
	char		schema_support;
	char		lo_is_domain;
	char		escape_in_literal;
	char		*original_client_encoding;
	char		*current_client_encoding;
	char		*server_encoding;
	Int2		ccsc;
	Int2		mb_maxbyte_per_char;
	int		be_pid;	/* pid returned by backend */
	int		be_key; /* auth code needed to send cancel */
	UInt4		isolation;
	char		*current_schema;
	StatementClass	*stmt_in_extquery;
	Int2		max_identifier_length;
	Int2		num_discardp;
	char		**discardp;
#if (ODBCVER >= 0x0300)
	int		num_descs;
	DescriptorClass	**descs;
#endif /* ODBCVER */
	pgNAME		schemaIns;
	pgNAME		tableIns;
#ifdef	USE_SSPI
	UInt4		svcs_allowed;
	UInt4		auth_svcs;
#endif /* USE_SSPI */
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
#define CC_get_socket(x)			(x->sock)
#define CC_get_database(x)			(x->connInfo.database)
#define CC_get_server(x)			(x->connInfo.server)
#define CC_get_DSN(x)				(x->connInfo.dsn)
#define CC_get_username(x)			(x->connInfo.username)
#define CC_is_onlyread(x)			(x->connInfo.onlyread[0] == '1')
#define CC_get_escape(x)			(x->escape_in_literal)
#define CC_fake_mss(x)	(/* 0 != (x)->ms_jet && */ 0 < (x)->connInfo.fake_mss)
#define CC_accessible_only(x)	(0 < (x)->connInfo.accessible_only && PG_VERSION_GE((x), 7.2))
#define CC_default_is_c(x)	(CC_is_in_ansi_app(x) || x->ms_jet /* not only */ || TRUE /* but for any other ? */)
/*	for CC_DSN_info */
#define CONN_DONT_OVERWRITE		0
#define CONN_OVERWRITE			1

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


/*	prototypes */
ConnectionClass *CC_Constructor(void);
enum { /* CC_conninfo_init option */
	CLEANUP_FOR_REUSE	= 1L		/* reuse the info */
	,COPY_GLOBALS		= (1L << 1) /* copy globals to drivers */
};
void		CC_conninfo_init(ConnInfo *conninfo, UInt4 option);
void		CC_copy_conninfo(ConnInfo *to, const ConnInfo *from);
char		CC_Destructor(ConnectionClass *self);
int		CC_cursor_count(ConnectionClass *self);
char		CC_cleanup(ConnectionClass *self, BOOL keepCommunication);
char		CC_begin(ConnectionClass *self);
char		CC_commit(ConnectionClass *self);
char		CC_abort(ConnectionClass *self);
char		CC_set_autocommit(ConnectionClass *self, BOOL on);
int		CC_set_translation(ConnectionClass *self);
char		CC_connect(ConnectionClass *self, char password_req, char *salt);
char		CC_add_statement(ConnectionClass *self, StatementClass *stmt);
char		CC_remove_statement(ConnectionClass *self, StatementClass *stmt)
;
#if (ODBCVER >= 0x0300)
char		CC_add_descriptor(ConnectionClass *self, DescriptorClass *desc);
char		CC_remove_descriptor(ConnectionClass *self, DescriptorClass *desc);
#endif /* ODBCVER */
void		CC_set_error(ConnectionClass *self, int number, const char *message, const char *func);
void		CC_set_errormsg(ConnectionClass *self, const char *message);
char		CC_get_error(ConnectionClass *self, int *number, char **message);
QResultClass *CC_send_query_append(ConnectionClass *self, const char *query, QueryInfo *qi, UDWORD flag, StatementClass *stmt, const char *appendq);
#define CC_send_query(self, query, qi, flag, stmt) CC_send_query_append(self, query, qi, flag, stmt, NULL)
void		CC_clear_error(ConnectionClass *self);
int		CC_send_function(ConnectionClass *conn, int fnid, void *result_buf, int *actual_result_len, int result_is_int, LO_ARG *argv, int nargs);
char		CC_send_settings(ConnectionClass *self);
/*
char		*CC_create_errormsg(ConnectionClass *self);
void		CC_lookup_lo(ConnectionClass *conn);
void		CC_lookup_pg_version(ConnectionClass *conn);
*/
void		CC_initialize_pg_version(ConnectionClass *conn);
void		CC_log_error(const char *func, const char *desc, const ConnectionClass *self);
int		CC_get_max_query_len(const ConnectionClass *self);
int		CC_send_cancel_request(const ConnectionClass *conn);
void		CC_on_commit(ConnectionClass *conn);
void		CC_on_abort(ConnectionClass *conn, UDWORD opt);
void		CC_on_abort_partial(ConnectionClass *conn);
void		ProcessRollback(ConnectionClass *conn, BOOL undo, BOOL partial);
const char	*CC_get_current_schema(ConnectionClass *conn);
int             CC_mark_a_object_to_discard(ConnectionClass *conn, int type, const char *plan);
int             CC_discard_marked_objects(ConnectionClass *conn);

int	handle_error_message(ConnectionClass *self, char *msgbuf, size_t buflen,
		 char *sqlstate, const char *comment, QResultClass *res);
int	handle_notice_message(ConnectionClass *self, char *msgbuf, size_t buflen,
		 char *sqlstate, const char *comment, QResultClass *res);
int		EatReadyForQuery(ConnectionClass *self);
void		getParameterValues(ConnectionClass *self);
int		CC_get_max_idlen(ConnectionClass *self);

BOOL		SendSyncRequest(ConnectionClass *self);

const		char *CurrCat(const ConnectionClass *self);
const		char *CurrCatString(const ConnectionClass *self);

void	CC_examine_global_transaction(ConnectionClass *self);

/* CC_send_query options */
enum {
	IGNORE_ABORT_ON_CONN	= 1L /* not set the error result even when  */
	,CREATE_KEYSET		= (1L << 1) /* create keyset for updatable curosrs */
	,GO_INTO_TRANSACTION	= (1L << 2) /* issue begin in advance */
	,ROLLBACK_ON_ERROR	= (1L << 3) /* rollback the query when an error occurs */
	,END_WITH_COMMIT	= (1L << 4) /* the query ends with COMMMIT command */
	,IGNORE_ROUND_TRIP	= (1L << 5) /* the commincation round trip time is considered ignorable */
};
/* CC_on_abort options */
#define	NO_TRANS		1L
#define	CONN_DEAD		(1L << 1) /* connection is no longer valid */

#ifdef	__cplusplus
}
#endif
#endif /* __CONNECTION_H__ */

