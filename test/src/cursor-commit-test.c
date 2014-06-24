/*
 * This test case tests for a bug in result set caching, with
 * UseDeclareFetch=1, that was fixed. The bug occurred when a cursor was
 * closed, due to transaction commit, before any rows were fetched from
 * it. That set the "base" of the internal cached rowset incorrectly,
 * off by one.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLCHAR		charval[100];
	SQLLEN		len;
	int			row;

	test_connect();

	/* Start a transaction */
	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	/*
	 * Begin executing a query
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT g FROM generate_series(1,3) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, &charval, sizeof(charval), &len);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	/* Commit. This implicitly closes the cursor in the server. */
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to commit", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll(FIRST) failed", hstmt);

	if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
		printf("first row: %s\n", charval);

	row = 1;
	while (1)
	{
		rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
		if (rc == SQL_NO_DATA)
			break;

		row++;

		if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
			printf("row %d: %s\n", row, charval);
		else
		{
			print_diag("SQLFetchScroll failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
