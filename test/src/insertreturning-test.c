/*
 * INSERT RETURNING tests.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char param1[100];
	SQLLEN cbParam1;
	SQLSMALLINT colcount1, colcount2;
	SQLCHAR *sql;
	int i;

	test_connect();

	rc = SQLAllocStmt(conn, &hstmt);
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

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeStmt failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	/* Clean up */
	test_disconnect();

	return 0;
}
