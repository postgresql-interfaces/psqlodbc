/*
 * Test fetching a rowset that overlaps the beginning of the result set
 * with an updatable block cursor, after a row has been deleted through
 * the cursor.
 *
 * PGAPI_ExtendedFetch() used to scan the keyset from row -1 in this case.
 * The rows returned are the same either way, so the defect only shows up
 * under valgrind or AddressSanitizer.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define ROWSET_SIZE 3

static void
print_rowset(const char *call, SQLRETURN rc, HSTMT hstmt, SQLINTEGER *id, SQLULEN rows_fetched)
{
	SQLULEN		i;

	printf("%s: ", call);
	if (rc == SQL_NO_DATA)
		printf("no data");
	else
	{
		CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
		printf("ids");
		for (i = 0; i < rows_fetched; i++)
			printf(" %d", (int) id[i]);
	}
	printf("\n");
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	id[ROWSET_SIZE];
	SQLULEN		rows_fetched = 0;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE items AS "
		"SELECT g AS id FROM generate_series(1, 8) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) ROWSET_SIZE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROW_ARRAY_SIZE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR, &rows_fetched, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROWS_FETCHED_PTR failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id FROM items ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SELECT failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, id, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, 1);
	print_rowset("SQLFetchScroll(SQL_FETCH_ABSOLUTE, 1)", rc, hstmt, id, rows_fetched);

	rc = SQLSetPos(hstmt, 1, SQL_DELETE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos SQL_DELETE failed", hstmt);
	printf("SQLSetPos(1, SQL_DELETE): ok\n");

	/* Position before the first row, then refresh the rowset */
	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, 0);
	print_rowset("SQLFetchScroll(SQL_FETCH_ABSOLUTE, 0)", rc, hstmt, id, rows_fetched);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, 0);
	print_rowset("SQLFetchScroll(SQL_FETCH_RELATIVE, 0)", rc, hstmt, id, rows_fetched);

	/* The same from the first rowset, scrolling back past the beginning */
	rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);
	print_rowset("SQLFetchScroll(SQL_FETCH_FIRST, 0)", rc, hstmt, id, rows_fetched);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, -ROWSET_SIZE);
	print_rowset("SQLFetchScroll(SQL_FETCH_RELATIVE, -3)", rc, hstmt, id, rows_fetched);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, 0);
	print_rowset("SQLFetchScroll(SQL_FETCH_RELATIVE, 0)", rc, hstmt, id, rows_fetched);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
