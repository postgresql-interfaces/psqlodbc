/*
 * Test {fn ...} escapes whose closing brace is missing.
 *
 * processParameters() must return positioned at the closing brace of the
 * escape, also for an unclosed nested escape. Returning at the end of the
 * statement instead makes the enclosing escape read past the driver's copy
 * of the statement text.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static BOOL	execDirectMode = 0;

static void
runQuery(HSTMT hstmt, const char *str)
{
	SQLRETURN	rc;

	printf("\nQuery: %s\n", str);
	if (execDirectMode)
		rc = SQLExecDirect(hstmt, (SQLCHAR *) str, SQL_NTS);
	else
	{
		rc = SQLPrepare(hstmt, (SQLCHAR *) str, SQL_NTS);
		CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
		rc = SQLExecute(hstmt);
	}
	if (SQL_SUCCEEDED(rc))
		print_result(hstmt);
	else
		print_diag("Execute failed", SQL_HANDLE_STMT, hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

static void
escape_test(HSTMT hstmt)
{
	/* properly nested and closed escapes */
	runQuery(hstmt, "SELECT {fn UCASE({fn LCASE({fn UCASE('a')})})}");
	runQuery(hstmt, "SELECT {fn UCASE('a')}");

	/* a missing closing brace is an error, like for {d ...} */
	runQuery(hstmt, "SELECT {fn UCASE('a')");
	runQuery(hstmt, "SELECT {fn UCASE({fn LCASE('a')}");
	runQuery(hstmt, "SELECT {fn a() {fn b() {fn c()");
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
