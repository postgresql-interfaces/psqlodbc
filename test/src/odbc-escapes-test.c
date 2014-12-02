/*
 * Test ODBC escape syntax
 */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"


/* bind string param as CHAR  */
static void
bindParamString(HSTMT hstmt, int paramno, char *str)
{
	SQLRETURN	rc;
	static SQLLEN		cbParams[10];

	cbParams[paramno] = SQL_NTS;
	rc = SQLBindParameter(hstmt, paramno, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,		/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  str,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParams[paramno]		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	printf("Param %d: %s\n", paramno, str);
}

static void
executeQuery(HSTMT hstmt)
{
	SQLRETURN	rc;

	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

static void
prepareQuery(HSTMT hstmt, char *str)
{
	SQLRETURN	rc;

	rc = SQLPrepare(hstmt, (SQLCHAR *) str, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	printf("\nQuery: %s\n", str);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/**** Function escapes ****/

	/* CONCAT */
	prepareQuery(hstmt, "SELECT {fn CONCAT(?, ?) }");
	bindParamString(hstmt, 1, "foo");
	bindParamString(hstmt, 2, "bar");
	executeQuery(hstmt);

	/* LOCATE */
	prepareQuery(hstmt, "SELECT {fn LOCATE(?, ?, 2) }");
	bindParamString(hstmt, 1, "needle");
	bindParamString(hstmt, 2, "this is a needle in an ol' haystack");
	executeQuery(hstmt);

	/* SPACE */
	prepareQuery(hstmt, "SELECT 'x' || {fn SPACE(10) } || 'x'");
	executeQuery(hstmt);

	/**** CALL escapes ****/

	prepareQuery(hstmt, "{ call length(?) }");
	bindParamString(hstmt, 1, "foobar");
	executeQuery(hstmt);

	prepareQuery(hstmt, "{ call right(?, ?) }");
	bindParamString(hstmt, 1, "foobar");
	bindParamString(hstmt, 2, "3");
	executeQuery(hstmt);

	/* TODO: This doesn't currently work.
	prepareQuery(hstmt, "{ ? = call concat(?, ?) }");
	*/

	/**** Date, Time, and Timestamp literals ****/

	prepareQuery(hstmt, "SELECT {d '2014-12-21' } + '1 day'::interval");
	executeQuery(hstmt);

	prepareQuery(hstmt, "SELECT {t '20:30:40' } + '1 hour 1 minute 1 second'::interval");
	executeQuery(hstmt);

	prepareQuery(hstmt, "SELECT {ts '2014-12-21 20:30:40' } + '1 day 1 hour 1 minute 1 second'::interval");
	executeQuery(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
