/* File:			psqlodbc.h
 *
 * Description:		This file contains defines and declarations that are related to
 *					the entire driver.
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 * $Id: psqlodbc.h,v 1.132 2009/10/25 13:36:31 hinoue Exp $
 *
 */

#ifndef __PSQLODBC_H__
#define __PSQLODBC_H__

/* #define	__MS_REPORTS_ANSI_CHAR__ */

#ifndef WIN32
#include "config.h"
#else
#define	WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <stdio.h>				/* for FILE* pointers: see GLOBAL_VALUES */
#ifdef POSIX_MULTITHREAD_SUPPORT
#include <pthread.h>
#endif
#include "version.h"

#ifdef	WIN32
#ifdef	_DEBUG
#ifndef	_MEMORY_DEBUG_
#include <stdlib.h>
#if (_MSC_VER < 1400) /* in case of VC7 or under */
#include <malloc.h>
#endif /* _MSC_VER */
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif /* _MEMORY_DEBUG_ */
#endif /* _DEBUG */
#endif /* WIN32 */

#ifdef	WIN32
#include <delayimp.h>
#endif /* WIN32 */
/* Must come before sql.h */
#ifndef ODBCVER
#define ODBCVER						0x0250
#endif   /* ODBCVER_REP */

#define NAMEDATALEN_V72					32
#define NAMEDATALEN_V73					64
#ifndef NAMESTORAGELEN
#define NAMESTORAGELEN					64
#endif   /* NAMEDATALEN */


#ifndef	WIN32
#undef	WIN_MULTITHREAD_SUPPORT
#endif
#if defined(WIN32) || defined(WITH_UNIXODBC) || defined(WITH_IODBC)
#include <sql.h>
#include <sqlext.h>
#if defined(WIN32) && (_MSC_VER < 1300) /* in case of VC6 or under */
#define SQLLEN SQLINTEGER
#define SQLULEN SQLUINTEGER
#define SQLSETPOSIROW SQLUSMALLINT
/* VC6 bypasses 64bit mode. */
#define DWLP_USER DWL_USER
#define ULONG_PTR ULONG
#define LONG_PTR LONG
#define SetWindowLongPtr(hdlg, DWLP_USER, lParam) SetWindowLong(hdlg, DWLP_USER, lParam)
#define GetWindowLongPtr(hdlg, DWLP_USER) GetWindowLong(hdlg, DWLP_USER);
#endif
#else
#include "iodbc.h"
#include "isql.h"
#include "isqlext.h"
#endif /* WIN32 */

#if defined(WIN32)
#include <odbcinst.h>
#elif defined(WITH_UNIXODBC)
#include <odbcinst.h>
#elif defined(WITH_IODBC)
#include <iodbcinst.h>
#else
#include "gpps.h"
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define Int4 int
#define UInt4 unsigned int
#define Int2 short
#define UInt2 unsigned short
typedef	UInt4	OID;

#define	FORMAT_INT4	"%d"	/* Int4 */
#define	FORMAT_UINT4	"%u"	/* UInt4 */

#ifdef	WIN32
#define	ssize_t	SSIZE_T
#define	FORMAT_SIZE_T	"%Iu"	/* size_t */
#define	FORMAT_SSIZE_T	"%Id"	/* ssize_t */
#define	FORMAT_INTEGER	"%ld"	/* SQLINTEGER */
#define	FORMAT_UINTEGER	"%lu"	/* SQLUINTEGER */
#ifdef	_WIN64
#define	FORMAT_LEN	"%I64d" /* SQLLEN */
#define	FORMAT_ULEN	"%I64u" /* SQLULEN */
#define	FORMAT_LPTR	"%I64d" /* LONG_PTR */
#define	FORMAT_ULPTR	"%I64u" /* ULONG_PTR */
#else
#define	FORMAT_LEN	"%ld"	/* SQLLEN */
#define	FORMAT_ULEN	"%lu"	/* SQLULEN */
#define	FORMAT_LPTR	"%ld"	/* LONG_PTR */
#define	FORMAT_ULPTR	"%lu"	/* ULONG_PTR */
#endif /* _WIN64 */
#else
#define	FORMAT_SIZE_T	"%zu"	/* size_t */	
#define	FORMAT_SSIZE_T	"%zd"	/* ssize_t */
#ifndef	HAVE_SSIZE_T
typedef	long	ssize_t
#endif /* HAVE_SSIZE_T */

#if (SIZEOF_VOID_P == SIZEOF_LONG)
typedef	long 	LONG_PTR;
typedef	unsigned long 	ULONG_PTR;
#define	FORMAT_LPTR	"%ld"	/* LONG_PTR */
#define	FORMAT_ULPTR	"%lu"	/* ULONG_PTR */
#elif defined (HAVE_LONG_LONG)
typedef	long long LONG_PTR;
typedef	unsigned long long ULONG_PTR;
#define	FORMAT_LPTR	"%lld"	/* LONG_PTR */
#define	FORMAT_ULPTR	"%llu"	/* ULONG_PTR */
#else
#error appropriate long pointer type not found 
#endif /* SIZEOF_VOID_P */
#if (SIZEOF_VOID_P == 8)
#define	FORMAT_INTEGER	"%d"	/* SQLINTEGER */
#define	FORMAT_UINTEGER	"%u"	/* SQLUINTEGER */
#if defined(WITH_UNIXODBC) && !defined(BUILD_REAL_64_BIT_MODE)
#define FORMAT_LEN	"%d"	/* SQLLEN */
#define FORMAT_ULEN	"%u"	/* SQLULEN */
#else
#define FORMAT_LEN	"%ld"	/* SQLLEN */
#define FORMAT_ULEN	"%lu"	/* SQLULEN */
#endif /* WITH_UNIXODBC */
#else
#define	FORMAT_LEN	"%ld"	/* SQLLEN */
#define	FORMAT_ULEN	"%lu"	/* SQLULEN */
#define	FORMAT_INTEGER	"%ld"	/* SQLINTEGER */
#define	FORMAT_UINTEGER	"%lu"	/* SQLUINTEGER */
#endif /* SIZEOF_VOID_P */
#endif /* WIN32 */
#define	CAST_PTR(type, ptr)	(type)((LONG_PTR)(ptr))
#define	CAST_UPTR(type, ptr)	(type)((ULONG_PTR)(ptr))
#ifndef	SQL_IS_LEN
#define	SQL_IS_LEN	(-1000)
#endif /* SQL_IS_LEN */
#ifdef	HAVE_SIGNED_CHAR
typedef	signed char	po_ind_t;
#else
typedef char	po_ind_t;
#endif /* HAVE_SIGNED_CHAR */

#ifndef WIN32
#if !defined(WITH_UNIXODBC) && !defined(WITH_IODBC)
typedef float SFLOAT;
typedef double SDOUBLE;
#endif /* WITH_UNIXODBC */

#ifndef CALLBACK
#define CALLBACK
#endif /* CALLBACK */
#endif /* WIN32 */

#ifndef WIN32
#define stricmp strcasecmp
#define strnicmp strncasecmp
#ifndef TRUE
#define	TRUE	(BOOL)1
#endif /* TRUE */
#ifndef	FALSE
#define	FALSE	(BOOL)0
#endif /* FALSE */
#else
#if (_MSC_VER >= 1400) && !defined(_WIN64)
#define snprintf sprintf_s
#define strncat(d, s, l) strcat_s(d, l, s)
#else
#define snprintf _snprintf
#endif
#ifndef strdup
#define strdup _strdup
#endif /* strdup */
#define strnicmp _strnicmp
#define stricmp _stricmp
#define vsnprintf _vsnprintf
#endif /* WIN32 */

#ifndef	SQL_ATTR_APP_ROW_DESC
#define	SQL_ATTR_APP_ROW_DESC	10010
#endif
#ifndef	SQL_ATTR_APP_PARAM_DESC
#define	SQL_ATTR_APP_PARAM_DESC	10011
#endif
#ifndef	SQL_ATTR_IMP_ROW_DESC
#define	SQL_ATTR_IMP_ROW_DESC	10012
#endif
#ifndef	SQL_ATTR_IMP_PARAM_DESC
#define	SQL_ATTR_IMP_PARAM_DESC	10013
#endif

/* Driver stuff */

#define DRIVERNAME				"PostgreSQL ODBC"
#if (ODBCVER >= 0x0300)
#if (ODBCVER >= 0x0351)
#define DRIVER_ODBC_VER				"03.51"
#else
#define DRIVER_ODBC_VER				"03.00"
#endif /* ODBCVER 0x0351 */
#ifndef DBMS_NAME
#ifdef	UNICODE_SUPPORT
#define DBMS_NAME				"PostgreSQL Unicode"
#else
#define DBMS_NAME				"PostgreSQL ANSI"
#endif /* UNICODE_SUPPORT */
#endif /* DBMS_NAME */
#else
#define DRIVER_ODBC_VER				"02.50"
#define DBMS_NAME				"PostgreSQL Legacy"
#endif   /* ODBCVER */

#ifdef WIN32
#if (ODBCVER >= 0x0300)
#ifdef	UNICODE_SUPPORT
#if (ODBCVER >= 0x0350)
#define DRIVER_FILE_NAME			"PSQLODBC35W.DLL"
#else
#define DRIVER_FILE_NAME			"PSQLODBC30W.DLL"
#endif /* ODBCVER 0x0350 */
#else
#define DRIVER_FILE_NAME			"PSQLODBC.DLL"
#endif   /* UNICODE_SUPPORT */
#else
#define DRIVER_FILE_NAME			"PSQLODBC25.DLL"
#endif   /* ODBCVER 0x0300 */
#else
#ifdef  UNICODE_SUPPORT
#define DRIVER_FILE_NAME                        "psqlodbcw.so"
#else
#define DRIVER_FILE_NAME                        "psqlodbca.so"
#endif
#endif   /* WIN32 */
BOOL isMsAccess();
BOOL isMsQuery();
BOOL isSqlServr();

#define	NULL_CATALOG_NAME				""

/* ESCAPEs */
#define	ESCAPE_IN_LITERAL				'\\'
#define	BYTEA_ESCAPE_CHAR				'\\'
#define	SEARCH_PATTERN_ESCAPE				'\\'
#define	LITERAL_QUOTE					'\''
#define	IDENTIFIER_QUOTE				'\"'
#define	ODBC_ESCAPE_START				'{'
#define	ODBC_ESCAPE_END					'}'
#define	DOLLAR_QUOTE					'$'
#define	LITERAL_EXT					'E'
#define	PG_CARRIAGE_RETURN				'\r'
#define	PG_LINEFEED					'\n'

/* Limits */
#define BLCKSZ						4096
#define MAXPGPATH					1024

#define MAX_MESSAGE_LEN				65536		/* This puts a limit on
												 * query size but I don't */
 /* see an easy way round this - DJP 24-1-2001 */
#define MAX_CONNECT_STRING			4096
#define ERROR_MSG_LENGTH			4096
#define FETCH_MAX					100 /* default number of rows to cache
										 * for declare/fetch */
#define TUPLE_MALLOC_INC			100
#define SOCK_BUFFER_SIZE			4096		/* default socket buffer
												 * size */
#define MAX_CONNECTIONS				128 /* conns per environment
										 * (arbitrary)	*/
#define MAX_FIELDS					512
#define BYTELEN						8
#define VARHDRSZ					sizeof(Int4)

#ifdef	NAMEDATALEN
#define MAX_SCHEMA_LEN				NAMEDATALEN
#define MAX_TABLE_LEN				NAMEDATALEN
#define MAX_COLUMN_LEN				NAMEDATALEN
#define NAME_FIELD_SIZE				NAMEDATALEN /* size of name fields */
#if (NAMEDATALEN > NAMESTORAGELEN)
#undef	NAMESTORAGELEN
#define	NAMESTORAGELEN	NAMEDATALEN
#endif
#endif /* NAMEDATALEN */
#define MAX_CURSOR_LEN				32

#define SCHEMA_NAME_STORAGE_LEN			NAMESTORAGELEN
#define TABLE_NAME_STORAGE_LEN			NAMESTORAGELEN
#define COLUMN_NAME_STORAGE_LEN			NAMESTORAGELEN
#define INDEX_KEYS_STORAGE_COUNT		32

/*	Registry length limits */
#define LARGE_REGISTRY_LEN			4096		/* used for special cases */
#define MEDIUM_REGISTRY_LEN			256 /* normal size for
										 * user,database,etc. */
#define SMALL_REGISTRY_LEN			10	/* for 1/0 settings */


/*	These prefixes denote system tables */
#define POSTGRES_SYS_PREFIX			"pg_"
#define KEYS_TABLE					"dd_fkey"

/*	Info limits */
#define MAX_INFO_STRING				128
#define MAX_KEYPARTS				20
#define MAX_KEYLEN				512 	/* max key of the form

										 * "date+outlet+invoice" */
/* POSIX defines a PATH_MAX.( wondows is _MAX_PATH ..) */
#ifndef PATH_MAX
#ifdef _MAX_PATH
#define PATH_MAX	_MAX_PATH
#else
#define PATH_MAX       1024
#endif /* _MAX_PATH */
#endif /* PATH_MAX */

#define MAX_ROW_SIZE				0	/* Unlimited rowsize with the
										 * Tuple Toaster */
#define MAX_STATEMENT_LEN			0	/* Unlimited statement size with
										 * 7.0 */

/* Previously, numerous query strings were defined of length MAX_STATEMENT_LEN */
/* Now that's 0, lets use this instead. DJP 24-1-2001 */
#define STD_STATEMENT_LEN			MAX_MESSAGE_LEN

#define PG62						"6.2"		/* "Protocol" key setting
												 * to force Postgres 6.2 */
#define PG63						"6.3"		/* "Protocol" key setting
												 * to force postgres 6.3 */
#define PG64						"6.4"
#define PG74REJECTED					"reject7.4"
#define PG74						"7.4"

typedef int	(*PQFUNC)();

typedef struct ConnectionClass_ ConnectionClass;
typedef struct StatementClass_ StatementClass;
typedef struct QResultClass_ QResultClass;
typedef struct SocketClass_ SocketClass;
typedef struct BindInfoClass_ BindInfoClass;
typedef struct ParameterInfoClass_ ParameterInfoClass;
typedef struct ParameterImplClass_ ParameterImplClass;
typedef struct ColumnInfoClass_ ColumnInfoClass;
typedef struct EnvironmentClass_ EnvironmentClass;
typedef struct TupleField_ TupleField;
typedef struct KeySet_ KeySet;
typedef struct Rollback_ Rollback;
typedef struct ARDFields_ ARDFields;
typedef struct APDFields_ APDFields;
typedef struct IRDFields_ IRDFields;
typedef struct IPDFields_ IPDFields;

typedef struct col_info COL_INFO;
typedef struct lo_arg LO_ARG;

typedef struct GlobalValues_
{
	int			fetch_max;
	int			socket_buffersize;
	int			unknown_sizes;
	int			max_varchar_size;
	int			max_longvarchar_size;
	char		debug;
	char		commlog;
	char		disable_optimizer;
	char		ksqo;
	char		unique_index;
	char		onlyread;		/* readonly is reserved on Digital C++
								 * compiler */
	char		use_declarefetch;
	char		text_as_longvarchar;
	char		unknowns_as_longvarchar;
	char		bools_as_char;
	char		lie;
	char		parse;
	char		cancel_as_freestmt;
	char		extra_systable_prefixes[MEDIUM_REGISTRY_LEN];
	char		conn_settings[LARGE_REGISTRY_LEN];
	char		protocol[SMALL_REGISTRY_LEN];
} GLOBAL_VALUES;

typedef struct StatementOptions_
{
	SQLLEN			maxRows;
	SQLLEN			maxLength;
	SQLLEN			keyset_size;
	SQLUINTEGER		cursor_type;
	SQLUINTEGER		scroll_concurrency;
	SQLUINTEGER		retrieve_data;
	SQLUINTEGER		use_bookmarks;
	void			*bookmark_ptr;
#if (ODBCVER >= 0x0300)
	SQLUINTEGER		metadata_id;
#endif /* ODBCVER */
} StatementOptions;

/*	Used to pass extra query info to send_query */
typedef struct QueryInfo_
{
	SQLLEN		row_size;
	QResultClass	*result_in;
	const char	*cursor;
} QueryInfo;

/*	Used to save the error information */
typedef struct
{
        UInt4	status;
        Int4	errorsize;
        Int2	recsize;
        Int2	errorpos;
        char    sqlstate[8];
        SQLLEN	diag_row_count;
        char    __error_message[1];
}       PG_ErrorInfo;
PG_ErrorInfo	*ER_Constructor(SDWORD errornumber, const char *errormsg);
PG_ErrorInfo	*ER_Dup(const PG_ErrorInfo *from);
void ER_Destructor(PG_ErrorInfo *);
RETCODE SQL_API ER_ReturnError(PG_ErrorInfo **, SQLSMALLINT, UCHAR FAR *,
		SQLINTEGER FAR *, UCHAR FAR *, SQLSMALLINT, SQLSMALLINT FAR *, UWORD);

void		logs_on_off(int cnopen, int, int);

#define PG_TYPE_LO_UNDEFINED			(-999)		/* hack until permanent
												 * type available */
#define PG_TYPE_LO_NAME				"lo"
#define CTID_ATTNUM				(-1)	/* the attnum of ctid */
#define OID_ATTNUM				(-2)	/* the attnum of oid */
#define XMIN_ATTNUM				(-3)	/* the attnum of xmin */

/* sizes */
#define TEXT_FIELD_SIZE			8190	/* size of default text fields
						 * (not including null term) */
#define MAX_VARCHAR_SIZE		255	/* default maximum size of
						 * varchar fields (not including null term) */
#define INFO_VARCHAR_SIZE		254	/* varchar field size
						 * used in info.c */

#define PG_NUMERIC_MAX_PRECISION	1000
#define PG_NUMERIC_MAX_SCALE		1000

#define INFO_INQUIRY_LEN		8192	/* this seems sufficiently big for
										 * queries used in info.c inoue
										 * 2001/05/17 */

#define	LENADDR_SHIFT(x, sft)	((x) ? (SQLLEN *)((char *)(x) + (sft)) : NULL)

int	initialize_global_cs(void);
#ifdef	POSIX_MULTITHREAD_SUPPORT
#if	!defined(HAVE_ECO_THREAD_LOCKS)
#define	POSIX_THREADMUTEX_SUPPORT
#endif /* HAVE_ECO_THREAD_LOCKS */
#endif /* POSIX_MULTITHREAD_SUPPORT */

#ifdef	POSIX_THREADMUTEX_SUPPORT
const pthread_mutexattr_t *getMutexAttr(void);
#endif /* POSIX_THREADMUTEX_SUPPORT */
#ifdef	UNICODE_SUPPORT
#define WCLEN sizeof(SQLWCHAR)
SQLULEN	ucs2strlen(const SQLWCHAR *ucs2str);
char	*ucs2_to_utf8(const SQLWCHAR *ucs2str, SQLLEN ilen, SQLLEN *olen, BOOL tolower);
SQLULEN	utf8_to_ucs2_lf0(const char * utf8str, SQLLEN ilen, BOOL lfconv, SQLWCHAR *ucs2str, SQLULEN buflen);
SQLULEN	utf8_to_ucs2_lf1(const char * utf8str, SQLLEN ilen, BOOL lfconv, SQLWCHAR *ucs2str, SQLULEN buflen);
int	msgtowstr(const char *, const char *, int, LPWSTR, int);
int	wstrtomsg(const char *, const LPWSTR, int, char *, int);
#define	utf8_to_ucs2_lf(utf8str, ilen, lfconv, ucs2str, buflen) utf8_to_ucs2_lf0(utf8str, ilen, lfconv, ucs2str, buflen)
#define	utf8_to_ucs2(utf8str, ilen, ucs2str, buflen) utf8_to_ucs2_lf0(utf8str, ilen, FALSE, ucs2str, buflen)
#endif /* UNICODE_SUPPORT */


#ifdef	_MEMORY_DEBUG_
void		*debug_alloc(size_t);
void		*debug_calloc(size_t, size_t);
void		*debug_realloc(void *, size_t);
char		*debug_strdup(const char *);
void		*debug_memcpy(void *, const void *, size_t);
void		*debug_memset(void *, int c, size_t);
char		*debug_strcpy(char *, const char *);
char		*debug_strncpy(char *, const char *, size_t);
char		*debug_strncpy_null(char *, const char *, size_t);
void		debug_free(void *);
void		debug_memory_check(void);

#ifdef	WIN32
#undef strdup
#endif /* WIN32 */
#define malloc	debug_alloc
#define realloc debug_realloc
#define calloc	debug_calloc
#define strdup	debug_strdup
#define free	debug_free
#define strcpy	debug_strcpy
#define strncpy	debug_strncpy
/* #define strncpy_null	debug_strncpy_null */
#define memcpy	debug_memcpy
#define memset	debug_memset
#endif   /* _MEMORY_DEBUG_ */

#ifdef	__cplusplus
}
#endif

#include "misc.h"

CSTR	NULL_STRING = "";
CSTR	PRINT_NULL = "(null)";
CSTR	OID_NAME = "oid";
#endif /* __PSQLODBC_H__ */
