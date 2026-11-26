/*
 * Test SQLBulkOperations() by bookmark in declare/fetch mode, on a row that
 * is no longer in the row cache, after another row was updated through the
 * same cursor.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define BOOKMARKED_ROW	301
#define BOOKMARK_SIZE	16

/* Print the rows of the table that differ from what was inserted. */
static void
printChangedRows(void)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		changed[200];
	SQLLEN		ind;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
					   "SELECT string_agg(g || '=' || coalesce(note, 'DELETED'), ', ' ORDER BY g) "
					   "FROM generate_series(1, 400) g LEFT JOIN bulkbookmark_test ON id = g "
					   "WHERE note IS DISTINCT FROM 'original'", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, changed, sizeof(changed), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("changed rows: %s\n", changed);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

/*
 * Update row_to_edit with SQLSetPos(SQL_UPDATE), save the bookmark of
 * BOOKMARKED_ROW, scroll away from it, then pass the saved bookmark to
 * SQLBulkOperations(operation).
 */
static void
editThenBulkOperation(int row_to_edit, SQLSMALLINT operation)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	id;
	SQLLEN		id_ind;
	char		note[40];
	SQLLEN		note_ind;
	char		bookmark[BOOKMARK_SIZE];
	SQLLEN		bookmark_ind;
	char		saved_bookmark[BOOKMARK_SIZE];
	SQLLEN		saved_bookmark_ind;
	SQLINTEGER	saved_bookmark_id;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS bulkbookmark_test", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE bulkbookmark_test(id int4 primary key, note text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO bulkbookmark_test SELECT g, 'original' FROM generate_series(1, 400) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER) SQL_UB_VARIABLE, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	rc = SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, bookmark, sizeof(bookmark), &bookmark_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &id, 0, &id_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, note, sizeof(note), &note_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id, note FROM bulkbookmark_test ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, row_to_edit);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
	strcpy(note, "edited");
	note_ind = SQL_NTS;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos failed", hstmt);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, BOOKMARKED_ROW);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
	memcpy(saved_bookmark, bookmark, sizeof(bookmark));
	saved_bookmark_ind = bookmark_ind;
	saved_bookmark_id = id;

	/* 150 rows back: more than Fetch, so the bookmarked row leaves the cache */
	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, BOOKMARKED_ROW - 150);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);

	/* One-row bookmark array; for the update, only note gets a new value */
	memcpy(bookmark, saved_bookmark, sizeof(bookmark));
	bookmark_ind = saved_bookmark_ind;
	id_ind = SQL_COLUMN_IGNORE;
	strcpy(note, "updated by bookmark");
	note_ind = SQL_NTS;
	rc = SQLBulkOperations(hstmt, operation);
	CHECK_STMT_RESULT(rc, "SQLBulkOperations failed", hstmt);
	printf("edit row %d, then %s by the bookmark of row %d (id %d): %s, ",
		   row_to_edit,
		   operation == SQL_DELETE_BY_BOOKMARK ? "delete" : "update",
		   BOOKMARKED_ROW, (int) saved_bookmark_id,
		   rc == SQL_SUCCESS ? "SQL_SUCCESS" : "SQL_SUCCESS_WITH_INFO");

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	printChangedRows();
}

int main(int argc, char **argv)
{
	/* Fetch=100 is the default: the driver caches 100 rows at a time */
	test_connect_ext("UseDeclareFetch=1;Fetch=100");

	editThenBulkOperation(2, SQL_DELETE_BY_BOOKMARK);
	editThenBulkOperation(1, SQL_DELETE_BY_BOOKMARK);
	editThenBulkOperation(2, SQL_UPDATE_BY_BOOKMARK);
	editThenBulkOperation(1, SQL_UPDATE_BY_BOOKMARK);

	test_disconnect();

	return 0;
}
