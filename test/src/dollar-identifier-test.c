/*
 * Test that a positioned update through a cursor on a table whose name
 * contains a '$' goes to that table, and not to a table named by the
 * part of the name in front of the '$'.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	colvalue;
	SQLLEN		indColvalue;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Two tables with the same columns: "acct" and "acct$items". A cursor
	 * on the latter must not write to the former.
	 */
	printf("Creating test tables acct and acct$items\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE acct(i int4, note text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO acct VALUES (1, 'acct')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE acct$items(i int4, note text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO acct$items VALUES (1, 'acct$items')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	printf("Opening a cursor on acct$items for update\n");

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &colvalue, 0, &indColvalue);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT i, note FROM acct$items", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	printf("Updating the row through the cursor\n");
	colvalue += 100;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos UPDATE failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* The update must have gone to acct$items, and acct must be untouched */
	printf("\nQuerying acct$items\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT i, note FROM acct$items", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	printf("\nQuerying acct\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT i, note FROM acct", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
