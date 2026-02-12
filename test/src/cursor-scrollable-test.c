/*
 * Test SQL_ATTR_CURSOR_SCROLLABLE setter support.
 *
 * Verifies that setting SQL_ATTR_CURSOR_SCROLLABLE to SQL_SCROLLABLE
 * or SQL_NONSCROLLABLE properly sets the cursor type and allows
 * scrollable operations.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLUINTEGER	scrollable;
	SQLUINTEGER	cursor_type;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Test 1: Set SQL_ATTR_CURSOR_SCROLLABLE to SQL_SCROLLABLE
	 */
	printf("Setting SQL_ATTR_CURSOR_SCROLLABLE = SQL_SCROLLABLE\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_SCROLLABLE,
						(SQLPOINTER) SQL_SCROLLABLE, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr SQL_SCROLLABLE failed", hstmt);
	printf("ok\n");

	/* Verify cursor type is now SQL_CURSOR_STATIC */
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						&cursor_type, SQL_IS_UINTEGER, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr SQL_ATTR_CURSOR_TYPE failed", hstmt);
	printf("cursor_type after SQL_SCROLLABLE: %s\n",
		   cursor_type == SQL_CURSOR_STATIC ? "SQL_CURSOR_STATIC" :
		   cursor_type == SQL_CURSOR_FORWARD_ONLY ? "SQL_CURSOR_FORWARD_ONLY" :
		   "other");

	/* Verify SQL_ATTR_CURSOR_SCROLLABLE reads back correctly */
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_SCROLLABLE,
						&scrollable, SQL_IS_UINTEGER, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr SQL_ATTR_CURSOR_SCROLLABLE failed", hstmt);
	printf("scrollable: %s\n",
		   scrollable == SQL_SCROLLABLE ? "SQL_SCROLLABLE" :
		   scrollable == SQL_NONSCROLLABLE ? "SQL_NONSCROLLABLE" :
		   "other");

	/*
	 * Test 2: Set SQL_ATTR_CURSOR_SCROLLABLE to SQL_NONSCROLLABLE
	 */
	printf("Setting SQL_ATTR_CURSOR_SCROLLABLE = SQL_NONSCROLLABLE\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_SCROLLABLE,
						(SQLPOINTER) SQL_NONSCROLLABLE, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr SQL_NONSCROLLABLE failed", hstmt);
	printf("ok\n");

	/* Verify cursor type is now SQL_CURSOR_FORWARD_ONLY */
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						&cursor_type, SQL_IS_UINTEGER, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr SQL_ATTR_CURSOR_TYPE failed", hstmt);
	printf("cursor_type after SQL_NONSCROLLABLE: %s\n",
		   cursor_type == SQL_CURSOR_STATIC ? "SQL_CURSOR_STATIC" :
		   cursor_type == SQL_CURSOR_FORWARD_ONLY ? "SQL_CURSOR_FORWARD_ONLY" :
		   "other");

	/* Verify SQL_ATTR_CURSOR_SCROLLABLE reads back correctly */
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_SCROLLABLE,
						&scrollable, SQL_IS_UINTEGER, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr SQL_ATTR_CURSOR_SCROLLABLE failed", hstmt);
	printf("scrollable: %s\n",
		   scrollable == SQL_SCROLLABLE ? "SQL_SCROLLABLE" :
		   scrollable == SQL_NONSCROLLABLE ? "SQL_NONSCROLLABLE" :
		   "other");

	/*
	 * Test 3: Verify scrollable cursor actually works by doing a
	 * SQL_FETCH_FIRST after some fetches
	 */
	printf("Testing scrollable cursor with SQL_FETCH_FIRST\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_SCROLLABLE,
						(SQLPOINTER) SQL_SCROLLABLE, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr SQL_SCROLLABLE failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'row' || g FROM generate_series(1, 5) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Fetch first two rows */
	{
		char buf[40];
		SQLLEN ind;

		rc = SQLFetch(hstmt);
		CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("row: %s\n", buf);

		rc = SQLFetch(hstmt);
		CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("row: %s\n", buf);

		/* Now scroll back to first */
		rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);
		CHECK_STMT_RESULT(rc, "SQLFetchScroll SQL_FETCH_FIRST failed", hstmt);
		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("after SQL_FETCH_FIRST: %s\n", buf);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
