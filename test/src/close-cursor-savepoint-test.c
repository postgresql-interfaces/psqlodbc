/*
 * Test closing a cursor while the transaction is in failed state, then
 * recovering with ROLLBACK TO SAVEPOINT. With UseDeclareFetch=1 and
 * rollback-on-error level 0 the driver cannot close the server-side cursor
 * at once and closes it after the rollback.
 */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	HSTMT		hstmt2 = SQL_NULL_HSTMT;

	test_connect_ext("UseDeclareFetch=1;Protocol=7.4-0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt2);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr AUTOCOMMIT failed", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "select generate_series(1, 5)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "select failed", hstmt);
	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "savepoint before_failure", SQL_NTS);
	CHECK_STMT_RESULT(rc, "savepoint failed", hstmt2);

	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "select 1/0", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
	{
		printf("division by zero succeeded unexpectedly\n");
		exit(1);
	}
	print_diag("Failed to execute statement", SQL_HANDLE_STMT, hstmt2);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	printf("closed the cursor\n");

	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "rollback to before_failure", SQL_NTS);
	CHECK_STMT_RESULT(rc, "rollback to savepoint failed", hstmt2);
	printf("rolled back to the savepoint\n");

	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "select 'the transaction is usable again'", SQL_NTS);
	CHECK_STMT_RESULT(rc, "select failed", hstmt2);
	print_result(hstmt2);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt2);

	/* Clean up */
	test_disconnect();

	return 0;
}
