/*
 * Bookmarks of an updatable block cursor in declare/fetch mode.
 *
 * The table has no oid and no key column, so the oid part of every
 * SQL_C_VARBOOKMARK the driver hands out must be zero, whatever the
 * rowset size.
 */
#include <stdio.h>
#include <string.h>

#include "common.h"

#define MAX_ROWSET		10
#define BOOKMARK_LEN	16	/* sizeof(PG_BM): the shortest buffer that gets the oid */
#define OID_OFFSET		12	/* row number (4), status (2), offset (2), blocknum (4) */

static void
fetch_bookmarks(SQLULEN rowset_size)
{
	HSTMT		hstmt = SQL_NULL_HSTMT;
	unsigned char bookmark[MAX_ROWSET][BOOKMARK_LEN];
	SQLLEN		bookmark_ind[MAX_ROWSET];
	SQLINTEGER	id[MAX_ROWSET];
	SQLLEN		id_ind[MAX_ROWSET];
	SQLULEN		rows_fetched;
	SQLULEN		row;
	int			total = 0;
	int			rc;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER) SQL_UB_VARIABLE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr USE_BOOKMARKS failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) rowset_size, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROW_ARRAY_SIZE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR, &rows_fetched, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROWS_FETCHED_PTR failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id FROM bookmark_oid_tab", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, bookmark, BOOKMARK_LEN, bookmark_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol 0 failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, id, 0, id_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol 1 failed", hstmt);

	printf("rowset size %d\n", (int) rowset_size);
	while ((rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0)) != SQL_NO_DATA)
	{
		CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
		for (row = 0; row < rows_fetched; row++)
		{
			unsigned int oid_part = 0;

			if (bookmark_ind[row] == BOOKMARK_LEN)
				memcpy(&oid_part, bookmark[row] + OID_OFFSET, sizeof(oid_part));
			if (oid_part != 0)
				printf("id %d: the oid part of the bookmark is %08x\n", (int) id[row], oid_part);
		}
		total += (int) rows_fetched;
	}
	printf("fetched %d rows\n", total);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int
main(int argc, char **argv)
{
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int			rc;

	/* Fetch keeps its default: 100 rows per FETCH */
	test_connect_ext("UseDeclareFetch=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
					   "CREATE TEMPORARY TABLE bookmark_oid_tab AS "
					   "SELECT g AS id FROM generate_series(1, 300) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	/* 10 divides 100: every FETCH ends where a rowset ends */
	fetch_bookmarks(10);
	/* the rowset of rows 99-105 needs 5 rows from the next FETCH */
	fetch_bookmarks(7);

	test_disconnect();
	return 0;
}
