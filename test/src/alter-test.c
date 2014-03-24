#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Create a table to test with */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE testtbl(t varchar(40))", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/**** A simple query against the table, fetch column info ****/

	rc = SQLExecDirect(hstmt, "SELECT * FROM testtbl", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Get column metadata */
	print_result_meta(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Alter the table */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "ALTER TABLE testtbl ALTER COLUMN t TYPE varchar(80)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Run the query again, check if the metadata was updated */

	rc = SQLExecDirect(hstmt, "SELECT * FROM testtbl", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Get column metadata */
	print_result_meta(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
