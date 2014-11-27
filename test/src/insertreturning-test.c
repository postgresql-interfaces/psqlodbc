/*
 * INSERT RETURNING tests.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void run_test();

int main(int argc, char **argv)
{
	/*
	 * SQLNumResultCols(), when run after SQLPrepare but before SQLExecute,
	 * return 0 in UseServerSidePrepare=0 mode. Run the test in both modes,
	 * mainly to make the output predictable regardless of the default
	 * UseServerSidePrepare mode in effect.
	 */
	printf("Testing with UseServerSidePrepare=1\n");
	test_connect_ext("UseServerSidePrepare=1");
	run_test();
	test_disconnect();

	printf("Testing with UseServerSidePrepare=0\n");
	test_connect_ext("UseServerSidePrepare=0");
	run_test();
	test_disconnect();

	return 0;
}

static void
run_test()
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char param1[100];
	SQLLEN cbParam1;
	SQLSMALLINT colcount1, colcount2;
	SQLCHAR *sql;
	int i;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	sql = "CREATE TEMPORARY TABLE tmptable (i int4, t text)";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while creating temp table", hstmt);

	/*
	 * We used to have a memory leak when SQLNumResultCols() was called on an
	 * INSERT statement. It's been fixed, but this test case was useful to find
	 * it, when you crank up the number of iterations.
	 */
	for (i = 0; i < 100; i++)
	{
		/* Prepare a statement */
		rc = SQLPrepare(hstmt, (SQLCHAR *) "INSERT INTO tmptable VALUES (1, ?) RETURNING (t)", SQL_NTS);
		CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

		/* bind param  */
		snprintf(param1, sizeof(param1), "foobar %d", i);
		cbParam1 = SQL_NTS;
		rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
							  SQL_C_CHAR,	/* value type */
							  SQL_CHAR,	/* param type */
							  20,		/* column size */
							  0,		/* dec digits */
							  param1,		/* param value ptr */
							  0,		/* buffer len */
							  &cbParam1	/* StrLen_or_IndPtr */);
		CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

		/* Test SQLNumResultCols, called before SQLExecute() */
		rc = SQLNumResultCols(hstmt, &colcount1);
		CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);

		/* Execute */
		rc = SQLExecute(hstmt);
		CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

		/* Call SQLNumResultCols again, after SQLExecute() */
		rc = SQLNumResultCols(hstmt, &colcount2);
		CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);

		printf("# of result cols before SQLExecute: %d, after: %d\n",
			   colcount1, colcount2);

		/* Fetch result */
		print_result(hstmt);

		rc = SQLFreeStmt(hstmt, SQL_CLOSE);
		CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeHandle failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}
}
