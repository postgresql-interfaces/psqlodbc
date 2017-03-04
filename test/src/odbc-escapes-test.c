/*
 * Test ODBC escape syntax
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
bindOutParamString(HSTMT hstmt, int paramno, char *outbuf, int outbuflen, BOOL inout)
{
	SQLRETURN	rc;
	static SQLLEN		cbParams[10];

	cbParams[paramno] = SQL_NTS;
	rc = SQLBindParameter(hstmt, paramno, inout ? SQL_PARAM_INPUT_OUTPUT : SQL_PARAM_OUTPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,		/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  outbuf,		/* param value ptr */
						  outbuflen,	/* buffer len */
						  &cbParams[paramno]		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	printf("Param %d is an %s parameter\n", paramno, inout ? "I-O": "OUT");
}

static BOOL	execDirectMode = 0;
static SQLCHAR	saveQuery[128];

static void
executeQuery(HSTMT hstmt)
{
	SQLRETURN	rc;

	if (execDirectMode)
		rc = SQLExecDirect(hstmt, saveQuery, SQL_NTS);
	else
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

	if (execDirectMode)
		strcpy(saveQuery, str);
	else
	{
		rc = SQLPrepare(hstmt, (SQLCHAR *) str, SQL_NTS);
		CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	}
	printf("\nQuery: %s\n", str);
}

static void	escape_test(HSTMT hstmt)
{
	char	outbuf1[64], outbuf3[64], outbuf5[64];
	BOOL	variadic_test_success = 1;

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

	prepareQuery(hstmt, "{ ? = call length('foo') }");
	memset(outbuf1, 0, sizeof(outbuf1));
	bindOutParamString(hstmt, 1, outbuf1, sizeof(outbuf1) - 1, 0);
	executeQuery(hstmt);
	printf("OUT param: %s\n", outbuf1);

	/* It's preferable to cast VARIADIC any fields */
	prepareQuery(hstmt, "{ ? = call concat(?::text, ?::text) }");
	memset(outbuf1, 0, sizeof(outbuf1));
	bindOutParamString(hstmt, 1, outbuf1, sizeof(outbuf1) - 1, 0);
	bindParamString(hstmt, 2, "foo");
	bindParamString(hstmt, 3, "bar");
	if (variadic_test_success)
		executeQuery(hstmt);
	else
		printf("skip this test because it fails\n");
	printf("OUT param: %s\n", outbuf1);

	/**** Date, Time, and Timestamp literals ****/

	prepareQuery(hstmt, "SELECT {d '2014-12-21' } + '1 day'::interval");
	executeQuery(hstmt);

	prepareQuery(hstmt, "SELECT {t '20:30:40' } + '1 hour 1 minute 1 second'::interval");
	executeQuery(hstmt);

	prepareQuery(hstmt, "SELECT {ts '2014-12-21 20:30:40' } + '1 day 1 hour 1 minute 1 second'::interval");
	executeQuery(hstmt);

	/**** call procedure with out and i-o parameters ****/
	prepareQuery(hstmt, "{call a_b_c_d_e(?, ?, ?, ?, ?)}");
	memset(outbuf1, 0, sizeof(outbuf1));
	bindOutParamString(hstmt, 1, outbuf1, sizeof(outbuf1) - 1, 0);
	bindParamString(hstmt, 2, "2017-02-23 11:34:46");
	strcpy(outbuf3, "4");
	bindOutParamString(hstmt, 3, outbuf3, sizeof(outbuf3) - 1, 1);
	bindParamString(hstmt, 4, "3.4");
	memset(outbuf5, 0, sizeof(outbuf5));
	bindOutParamString(hstmt, 5, outbuf5, sizeof(outbuf5) - 1, 0);
	executeQuery(hstmt);
	printf("OUT params: %s : %s : %s\n", outbuf1, outbuf3, outbuf5);
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

	rc = SQLExecDirect(hstmt, "create or replace function a_b_c_d_e"
			"(out a float8, in b timestamp, inout c integer, "
			"in d numeric, out e timestamp) returns record as "
			"$function$ \n"
			"DECLARE \n"
			"BEGIN \n"
			"a := 2 * d; \n"
			"e := b + '1 day'::interval; \n"
			"c := c + 3; \n"
			"END; \n"
			"$function$ \n"
			"LANGUAGE plpgsql\n"
			, SQL_NTS);
	CHECK_STMT_RESULT(rc, "create function a_b_c_d_e failed", hstmt);

	execDirectMode = 0;
	printf("\n-- TEST using SQLExecute after SQLPrepare\n");
	escape_test(hstmt);
	execDirectMode = 1;
	printf("\n-- TEST using SQLExecDirect\n");
	escape_test(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
