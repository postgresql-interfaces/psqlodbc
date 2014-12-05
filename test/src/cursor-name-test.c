#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	HSTMT hstmt2 = SQL_NULL_HSTMT;
	char cursornamebuf[20];
	SQLSMALLINT cursornamelen;
	char buf[40];
	SQLLEN ind;

	/**** Test cursor names ****/

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt2);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* first create a test table we can update */
	rc = SQLExecDirect(hstmt,
					   (SQLCHAR *) "create temporary table cursortesttbl as "
					   "SELECT id, 'foo' || id as t FROM generate_series(1,10) id",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLGetCursorName(hstmt, cursornamebuf, sizeof(cursornamebuf), &cursornamelen);
	CHECK_STMT_RESULT(rc, "SQLGetCursorName failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Per the ODBC spec, the cursor name should begin with SQL_CUR */
	cursornamebuf[strlen("SQL_CUR")] = '\0';
	printf("cursor name prefix: %s\n", cursornamebuf);

	/* Use a custom name */
	rc = SQLSetCursorName(hstmt, (SQLCHAR *) "my_test_cursor", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLSetCursorName failed", hstmt);

	/*
	 * Fetch a large result set without cursor (in Declare/fetch mode, it will
	 * be fetched in chunks)
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT * FROM cursortesttbl WHERE id < 9", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Fetch until we find the row with id = 5 */
	do {
		rc = SQLFetch(hstmt);
		CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	} while(strcmp(buf, "5") != 0);

	/* use the cursor's name for update */
	/*
	 * XXX: Disabled, because the driver doesn't support UPDATE ... WHERE
	 * CURRENT OF. It works with some options (Fetch=1), but it's not reported
	 * as supported so there's no guarantees.
	 */
#ifdef NOT_SUPPORTED
	printf("Updating row with id=5...\n");
	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "UPDATE cursortesttbl SET t = 'updated' || id WHERE CURRENT OF my_test_cursor", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt2);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT * FROM cursortesttbl ORDER BY id FOR UPDATE", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);
#endif

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/****
	 * Test a cursor name that requires quoting. The ODBC spec recommends
	 * not to use such a name, but we better handle it gracefully anyway.
	 *
	 * FIXME: the driver doesn't actually handle this.
	 */
#ifdef BROKEN
	rc = SQLSetCursorName(hstmt, (SQLCHAR *) "my nasty \"quoted\" cur'sor", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLSetCursorName failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT * FROM cursortesttbl WHERE id < 4", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);
#endif

	/* Clean up */
	test_disconnect();

	return 0;
}
