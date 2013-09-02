#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>

#ifdef WIN32
#define snprintf _snprintf
#endif

extern SQLHENV env;
extern SQLHDBC conn;

#define CHECK_STMT_RESULT(rc, msg, hstmt)	\
	if (!SQL_SUCCEEDED(rc)) \
	{ \
		print_diag(msg, SQL_HANDLE_STMT, hstmt);	\
		exit(1);									\
    }

extern void print_diag(char *msg, SQLSMALLINT htype, SQLHANDLE handle);
extern void test_connect_ext(char *extraparams);
extern void test_connect(void);
extern void test_disconnect(void);
extern void print_result_meta(HSTMT hstmt);
extern void print_result(HSTMT hstmt);
extern const char *datatype_str(SQLSMALLINT datatype);
extern const char *nullable_str(SQLSMALLINT nullable);
