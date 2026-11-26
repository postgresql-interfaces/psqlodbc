/*
 * Test scrolling backward over a server-side cursor (UseDeclareFetch=1)
 * when the MOVE BACKWARD the driver sends moves the cursor fewer rows
 * than asked.
 *
 * A view with a volatile filter makes an honest server do that: the
 * filter is evaluated again on the backward scan and rejects every row,
 * so "move backward n" answers "MOVE 0". The driver must report an error
 * instead of indexing its row cache with a negative offset.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define	NROWS	300

static void
scroll_to_last(const char *label, const char *relation)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		sql[200];
	SQLINTEGER	id;
	SQLLEN		cbLen;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &id, 0, &cbLen);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	snprintf(sql, sizeof(sql), "select id, note from %s", relation);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll NEXT failed", hstmt);
	printf("%s: first row %d\n", label, (int) id);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_LAST, 0);
	if (SQL_SUCCEEDED(rc))
		printf("%s: last row %d\n", label, (int) id);
	else
	{
		printf("%s: SQLFetchScroll LAST failed\n", label);
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		sql[300];

	test_connect_ext("UseDeclareFetch=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	snprintf(sql, sizeof(sql),
			 "create temporary table move_tbl as "
			 "select g as id, 'item ' || g as note from generate_series(1, %d) g",
			 NROWS);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "create table failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "create temporary sequence move_seq", SQL_NTS);
	CHECK_STMT_RESULT(rc, "create sequence failed", hstmt);
	snprintf(sql, sizeof(sql),
			 "create temporary view move_view as "
			 "select id, note from move_tbl where nextval('move_seq') <= %d",
			 NROWS);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "create view failed", hstmt);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	scroll_to_last("table", "move_tbl");
	scroll_to_last("volatile view", "move_view");

	test_disconnect();

	return 0;
}
