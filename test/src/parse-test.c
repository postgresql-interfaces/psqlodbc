/*
 * Test driver's query parsing functionality.
 *
 * (People should be using server-side parsing nowadays, though...)
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;

	test_connect_ext("Parse=1;DisallowPremature=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*** Simple case ***/
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id, t FROM testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result_meta(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** Test quoting ***/
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT \"id\", \"t\" FROM \"testtab1\"", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result_meta(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** Test quoting ***/
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT \"id\", \"t\", 'foo''bar' AS \"con\"\"stant\" FROM \"testtab1\"", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result_meta(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
