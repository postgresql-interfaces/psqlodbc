/*
 * Test ODBC escape syntax
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"


static void
setParamName(HSTMT hstmt, int paramno, const char *paramname)
{
	SQLHDESC 	hIpd = NULL;

	SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, &hIpd, 0, 0);
	SQLSetDescField(hIpd, paramno, SQL_DESC_NAME, (SQLPOINTER) paramname, SQL_NTS);
}

/* bind string param as CHAR  */
static void
bindParamString(HSTMT hstmt, int paramno, const char *paramname, char *str)
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
	setParamName(hstmt, paramno, paramname);

	if (paramname)
		printf("Param %d (%s): %s\n", paramno, paramname, str);
	else
		printf("Param %d: %s\n", paramno, str);
}

static void
bindOutParamString(HSTMT hstmt, int paramno, const char *paramname, char *outbuf, int outbuflen, BOOL inout)
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
	setParamName(hstmt, paramno, paramname);

	if (paramname)
		printf("Param %d (%s) is an %s parameter\n", paramno, paramname, inout ? "I-O": "OUT");
	else
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
	bindParamString(hstmt, 1, NULL, "foo");
	bindParamString(hstmt, 2, NULL, "bar");
	executeQuery(hstmt);

	/* LOCATE */
	prepareQuery(hstmt, "SELECT {fn LOCATE(?, ?, 2) }");
	bindParamString(hstmt, 1, NULL, "needle");
	bindParamString(hstmt, 2, NULL, "this is a needle in an ol' haystack");
	executeQuery(hstmt);

	/* LOCATE(SUBSTRING, SUBSTRING) */
	prepareQuery(hstmt, "SELECT {fn LOCATE({fn SUBSTRING(?, 2, 4)}, {fn SUBSTRING(?, 3)}, 3) }");
	/* using the same parameters */
	bindParamString(hstmt, 1, NULL, "needle");
	bindParamString(hstmt, 2, NULL, "this is a needle in an ol' haystack");
	executeQuery(hstmt);

	/* SPACE */
	prepareQuery(hstmt, "SELECT 'x' || {fn SPACE(10) } || 'x'");
	executeQuery(hstmt);

	/**** CALL escapes ****/

	prepareQuery(hstmt, "{ call length(?) }");
	bindParamString(hstmt, 1, NULL, "foobar");
	executeQuery(hstmt);

	prepareQuery(hstmt, "{ call right(?, ?) }");
	bindParamString(hstmt, 1, NULL, "foobar");
	bindParamString(hstmt, 2, NULL, "3");
	executeQuery(hstmt);

	prepareQuery(hstmt, "{ ? = call length('foo') }");
	pg_memset(outbuf1, 0, sizeof(outbuf1));
	bindOutParamString(hstmt, 1, NULL, outbuf1, sizeof(outbuf1) - 1, 0);
	executeQuery(hstmt);
	printf("OUT param: %s\n", outbuf1);

	/* It's preferable to cast VARIADIC any fields */
	prepareQuery(hstmt, "{ ? = call concat(?::text, ?::text) }");
	pg_memset(outbuf1, 0, sizeof(outbuf1));
	bindOutParamString(hstmt, 1, NULL, outbuf1, sizeof(outbuf1) - 1, 0);
	bindParamString(hstmt, 2, NULL, "foo");
	bindParamString(hstmt, 3, NULL, "bar");
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
	pg_memset(outbuf1, 0, sizeof(outbuf1));
	bindOutParamString(hstmt, 1, NULL, outbuf1, sizeof(outbuf1) - 1, 0);
	bindParamString(hstmt, 2, NULL, "2017-02-23 11:34:46");
	strcpy(outbuf3, "4");
	bindOutParamString(hstmt, 3, NULL, outbuf3, sizeof(outbuf3) - 1, 1);
	bindParamString(hstmt, 4, NULL, "3.4");
	pg_memset(outbuf5, 0, sizeof(outbuf5));
	bindOutParamString(hstmt, 5, NULL, outbuf5, sizeof(outbuf5) - 1, 0);
	executeQuery(hstmt);
	printf("OUT params: %s : %s : %s\n", outbuf1, outbuf3, outbuf5);

	/**** call procedure parameters by name (e,a,b,c,d) ****/
	prepareQuery(hstmt, "{call a_b_c_d_e(?, ?, ?, ?, ?)}");
	pg_memset(outbuf5, 0, sizeof(outbuf5));
	bindOutParamString(hstmt, 1, "e", outbuf5, sizeof(outbuf5) - 1, 0);
	pg_memset(outbuf1, 0, sizeof(outbuf1));
	bindOutParamString(hstmt, 2, "a", outbuf1, sizeof(outbuf1) - 1, 0);
	bindParamString(hstmt, 3, "b", "2017-02-23 11:34:46");
	strcpy(outbuf3, "4");
	bindOutParamString(hstmt, 4, "c", outbuf3, sizeof(outbuf3) - 1, 1);
	bindParamString(hstmt, 5, "d", "3.4");
	executeQuery(hstmt);
	printf("OUT params: %s : %s : %s\n", outbuf1, outbuf3, outbuf5);

	/**** call procedure parameters by name (b,c,d,e,a) ****/
	prepareQuery(hstmt, "{call a_b_c_d_e(?, ?, ?, ?, ?)}");
	bindParamString(hstmt, 1, "b", "2017-02-23 11:34:46");
	strcpy(outbuf3, "4");
	bindOutParamString(hstmt, 2, "c", outbuf3, sizeof(outbuf3) - 1, 1);
	bindParamString(hstmt, 3, "d", "3.4");
	pg_memset(outbuf5, 0, sizeof(outbuf5));
	bindOutParamString(hstmt, 4, "e", outbuf5, sizeof(outbuf5) - 1, 0);
	pg_memset(outbuf1, 0, sizeof(outbuf1));
	bindOutParamString(hstmt, 5, "a", outbuf1, sizeof(outbuf1) - 1, 0);
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
