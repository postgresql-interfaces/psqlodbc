#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#else
#include "config.h"
#ifndef TRUE
#define TRUE    (BOOL)1
#endif /* TRUE */
#ifndef FALSE
#define FALSE   (BOOL)0
#endif /* FALSE */
#endif

#include <sql.h>
#include <sqlext.h>

#ifdef WIN32
#define snprintf _snprintf
#endif /* WIN32 */

/* use safe memset if available */
#ifndef pg_memset // May already be defined by psqlodbc.h in some tests
#ifdef WIN32
#define pg_memset(dest, ch, count)  SecureZeroMemory(dest, count)
#else
#ifdef __STDC_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
#define pg_memset(dest, ch, count)	memset_s(dest, count, ch, count)
#endif /* __STDC_LIB_EXT1__ */
#define pg_memset(dest, ch, count)	memset(dest, ch, count) // default to memset if functionality isn't available
#endif /* WIN32 */
#endif /* pg_memset */

extern SQLHENV env;
extern SQLHDBC conn;

#define CHECK_STMT_RESULT(rc, msg, hstmt)	\
	if (!SQL_SUCCEEDED(rc)) \
	{ \
		print_diag(msg, SQL_HANDLE_STMT, hstmt);	\
		exit(1);									\
    }

#define CHECK_CONN_RESULT(rc, msg, hconn)	\
	if (!SQL_SUCCEEDED(rc)) \
	{ \
		print_diag(msg, SQL_HANDLE_DBC, hconn);	\
		exit(1);									\
    }

extern void print_diag(char *msg, SQLSMALLINT htype, SQLHANDLE handle);
extern const char *get_test_dsn(void);
extern int  IsAnsi(void);
extern void test_connect_ext(char *extraparams);
extern void test_connect(void);
extern void test_disconnect(void);
extern void print_result_meta_series(HSTMT hstmt,
									 SQLSMALLINT *colids,
									 SQLSMALLINT numcols);
extern void print_result_series(HSTMT hstmt,
								SQLSMALLINT *colids,
								SQLSMALLINT numcols,
								SQLINTEGER rowcount,
								BOOL printcolnames);
extern void print_result_meta(HSTMT hstmt);
extern void print_result(HSTMT hstmt);
extern void print_result_with_column_names(HSTMT hstmt);
extern const char *datatype_str(SQLSMALLINT datatype);
extern const char *nullable_str(SQLSMALLINT nullable);
