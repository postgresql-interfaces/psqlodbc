/*
 * A result browser: shows a query's result in pages of three rows on a
 * static cursor, going forward twice and then back once with
 * SQLExtendedFetch(). Before every call the row count is set to 0, so a
 * call that fails must leave it at 0.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define PAGE_SIZE 3

static void
browse(const char *query)
{
	static const SQLUSMALLINT directions[] = {SQL_FETCH_NEXT, SQL_FETCH_NEXT, SQL_FETCH_PRIOR};
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLRETURN	rc;
	SQLCHAR		ids[PAGE_SIZE][32];
	SQLLEN		id_lengths[PAGE_SIZE];
	SQLUSMALLINT row_status[PAGE_SIZE];
	SQLULEN		rows_in_page;
	SQLULEN		row;
	int			i;

	printf("%s\n", query);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr(SQL_ATTR_CURSOR_TYPE) failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ROWSET_SIZE, (SQLPOINTER) PAGE_SIZE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr(SQL_ROWSET_SIZE) failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, ids, sizeof(ids[0]), id_lengths);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) query, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	for (i = 0; i < 3; i++)
	{
		rows_in_page = 0;
		rc = SQLExtendedFetch(hstmt, directions[i], 0, &rows_in_page, row_status);
		printf("%s: SQLExtendedFetch returned %d, rows in page: %lu\n",
			   directions[i] == SQL_FETCH_PRIOR ? "prev" : "next",
			   rc, (unsigned long) rows_in_page);
		if (SQL_SUCCEEDED(rc))
		{
			for (row = 0; row < rows_in_page; row++)
				printf("    %s\n", ids[row]);
		}
		else
			print_diag(NULL, SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int
main(int argc, char **argv)
{
	test_connect();

	/* keeps its static cursor: "prev" shows rows 1-3 again */
	browse("SELECT id FROM generate_series(1, 10) AS id");

	/*
	 * Not classified as SELECT, so the driver turns the static cursor into a
	 * forward-only one and "prev" fails with HY106.
	 */
	browse("WITH ids AS (SELECT generate_series(1, 10) AS id) SELECT id FROM ids");

	test_disconnect();
	return 0;
}
