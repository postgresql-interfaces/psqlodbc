/*
 * Test behavior when errors happen. Should get a proper error message, and
 * the connection should stay in a sane state.
 *
 * FIXME: With some combinations of settings, an error can leave a
 * transaction open, but in aborted state. That's a bug in the driver, but
 * because I'm not planning to fix it right now, there is an expected output
 * file (errors_2.out) for that. The bug should be fixed, and the expected
 * output removed.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		param1[20] = "foo";
	SQLLEN		cbParam1;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Test a simple parse-time error */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT doesnotexist", SQL_NTS);
	/* Print error, it is expected */
	if (!SQL_SUCCEEDED(rc))
		print_diag("", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* The same, but with a bind parameter */

	/* bind param  */
	cbParam1 = SQL_NTS;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,	/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Test a simple parse-time error */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT doesnotexist2 WHERE bogus = ?", SQL_NTS);
	/* Print error, it is expected */
	if (!SQL_SUCCEEDED(rc))
		print_diag("", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** Test syntax error with SQLPrepare ***/

	/* Test a simple parse-time error */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT doesnotexist3 WHERE bogus = ?", SQL_NTS);

	/* bind param  */
	cbParam1 = SQL_NTS;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,	/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Test a simple parse-time error */
	rc = SQLExecute(hstmt);
	/* Print error, it is expected */
	if (!SQL_SUCCEEDED(rc))
		print_diag("", SQL_HANDLE_STMT, hstmt);


	test_disconnect();

	return 0;
}
