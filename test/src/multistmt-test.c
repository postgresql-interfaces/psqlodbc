#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void print_all_results(HSTMT hstmt)
{
	int i;
	int rc = SQL_SUCCESS;
	for (i = 1; rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO; i++)
	{
		printf("--%d ", i);
		print_result(hstmt);

		rc = SQLMoreResults(hstmt);
	}
	if (rc != SQL_NO_DATA)
		CHECK_STMT_RESULT(rc, "SQLMoreResults failed", hstmt);
}

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char *param1;
	SQLLEN cbParam1;
	char *param2;
	SQLLEN cbParam2;
	SQLSMALLINT colcount;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*** Simple multi-statement with two queries ***/

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1; SELECT 2", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_all_results(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** More queries ***/

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1; SELECT 'foo', 'bar'; SELECT 3; SELECT 4", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_all_results(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** Spurious semicolons ***/

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'foo', 'bar';;; SELECT 'foobar'; ", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_all_results(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** Prepare/Execute a multi-statement with parameters ***/

	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT 'first result set', id, t FROM testtab1 WHERE t = ?; SELECT 'second result set', t FROM testtab1 WHERE t = ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* bind params */
	param1 = "foo";
	cbParam1 = 8;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,	/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	param2 = "bar";
	cbParam2 = 8;
	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,	/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  param2,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam2		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Test SQLNumResultCols, called before SQLExecute() */
	rc = SQLNumResultCols(hstmt, &colcount);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);
	printf("# of result cols: %d\n", colcount);

	/* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);
	print_all_results(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
