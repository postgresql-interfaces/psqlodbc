/*
 * Test SQLFetchScroll(SQL_FETCH_BOOKMARK) with a bookmark that is not a
 * row of the result.
 *
 * The bookmark is a value the application passes, and the driver uses it
 * as the row to start from. With a deleted row in the result it reads the
 * state of the rows from there on, so a bookmark far outside the result
 * must be refused before that: the fetch ends with SQL_NO_DATA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
fetch_bookmark(HSTMT hstmt, SQLLEN *bookmark, SQLLEN value, SQLLEN offset)
{
	SQLRETURN	rc;

	*bookmark = value;
	printf("bookmark %ld, offset %ld: ", (long) value, (long) offset);
	/* a crash must not lose the line */
	fflush(stdout);
	rc = SQLFetchScroll(hstmt, SQL_FETCH_BOOKMARK, offset);
	if (SQL_NO_DATA == rc)
		printf("SQL_NO_DATA\n");
	else if (SQL_SUCCEEDED(rc))
		printf("fetched a row\n");
	else
		print_diag("failed", SQL_HANDLE_STMT, hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	id;
	SQLLEN		ind;
	SQLLEN		bookmark = 0;

	test_connect_ext("UseDeclareFetch=0");
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE bookmark_range (id int4 PRIMARY KEY, t text)",
		SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO bookmark_range "
		"SELECT g, 'row ' || g FROM generate_series(1, 5) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_LOCK, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_USE_BOOKMARKS,
						(SQLPOINTER) SQL_UB_VARIABLE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT id, t FROM bookmark_range ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &id, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);

	/* with a deleted row, the driver looks at each row's state */
	rc = SQLSetPos(hstmt, 1, SQL_DELETE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos(SQL_DELETE) failed", hstmt);
	printf("deleted the first row through the cursor\n");

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_FETCH_BOOKMARK_PTR, &bookmark, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	fetch_bookmark(hstmt, &bookmark, 0x7fffffff, -1);
	fetch_bookmark(hstmt, &bookmark, 0x7fffffff, 1);
	fetch_bookmark(hstmt, &bookmark, 1000000, -1);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
