/*
 * Test DML (INSERT, UPDATE, DELETE) commands
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
	SQLCHAR *sql;
	int i;
	SQLLEN rowcount;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Prepare a test table */
	sql = "CREATE TEMPORARY TABLE tmptable (i int4, t text)";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while creating temp table", hstmt);

	/* Insert some rows, using a prepared statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "INSERT INTO tmptable VALUES (?, 'foobar')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	for (i = 1; i <= 5; i++)
	{
		/* bind params  */
		snprintf(param1, sizeof(param1), "%d", i);
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

		/* Execute */
		rc = SQLExecute(hstmt);
		CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

		/* Test SQLRowCount() */
		rc = SQLRowCount(hstmt, &rowcount);
		CHECK_STMT_RESULT(rc, "SQLRowCount failed", hstmt);

		printf("# of rows inserted: %d\n", (int) rowcount);

		rc = SQLFreeStmt(hstmt, SQL_CLOSE);
		CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	}

	/* Update some rows */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "UPDATE tmptable SET t = 'updated ' || i WHERE i <= 3" , SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLRowCount(hstmt, &rowcount);
	CHECK_STMT_RESULT(rc, "SQLRowCount failed", hstmt);
	printf("# of rows updated: %d\n", (int) rowcount);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* and delete some rows */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "DELETE FROM tmptable WHERE i = 4 OR i = 2" , SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLRowCount(hstmt, &rowcount);
	CHECK_STMT_RESULT(rc, "SQLRowCount failed", hstmt);
	printf("# of rows deleted: %d\n", (int) rowcount);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Print the contents of the table after all the updates to verify */

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT * FROM tmptable", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
