/*
 * Test SQLBulkOperations(SQL_ADD) on a static updatable cursor, once after a
 * fetch and once before the first fetch. The added row is deleted, a failing
 * statement rolls the delete back, and the row is fetched again by bookmark.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* The driver allocates its row cache for 100, 200, 400... rows: with 100 it is full */
#define NUM_ROWS	100

static void
create_table(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		sql[200];

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS bulkadd_notes", SQL_NTS);
	CHECK_STMT_RESULT(rc, "DROP TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE bulkadd_notes (id serial PRIMARY KEY, note text UNIQUE)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	snprintf(sql, sizeof(sql),
			 "INSERT INTO bulkadd_notes (note) SELECT 'note ' || g FROM generate_series(1, %d) g",
			 NUM_ROWS);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
}

static void
add_delete_rollback_refetch(const char *label, BOOL fetch_first)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		bookmark[16];
	SQLLEN		bookmark_ind;
	SQLINTEGER	id;
	SQLLEN		id_ind;
	char		note[40];
	SQLLEN		note_ind;

	printf("-- %s\n", label);
	create_table();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER) SQL_UB_VARIABLE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr USE_BOOKMARKS failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id, note FROM bulkadd_notes", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, bookmark, sizeof(bookmark), &bookmark_ind);
	SQLBindCol(hstmt, 1, SQL_C_SLONG, &id, 0, &id_ind);
	SQLBindCol(hstmt, 2, SQL_C_CHAR, note, sizeof(note), &note_ind);

	if (fetch_first)
	{
		rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);
		CHECK_STMT_RESULT(rc, "SQLFetchScroll FIRST failed", hstmt);
		printf("fetched: %d %s\n", (int) id, note);
	}

	id_ind = SQL_IGNORE;
	strcpy(note, "buy milk");
	note_ind = SQL_NTS;
	rc = SQLBulkOperations(hstmt, SQL_ADD);
	CHECK_STMT_RESULT(rc, "SQLBulkOperations SQL_ADD failed", hstmt);
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran COMMIT failed", conn);
	printf("added and committed: buy milk\n");

	rc = SQLBulkOperations(hstmt, SQL_DELETE_BY_BOOKMARK);
	CHECK_STMT_RESULT(rc, "SQLBulkOperations SQL_DELETE_BY_BOOKMARK failed", hstmt);
	printf("deleted: buy milk\n");

	/* The text is in the table already: the server refuses it, the driver rolls back */
	strcpy(note, "note 7");
	rc = SQLBulkOperations(hstmt, SQL_ADD);
	if (SQL_SUCCEEDED(rc))
	{
		printf("adding a duplicate note succeeded\n");
		exit(1);
	}
	print_diag("adding note 7 failed", SQL_HANDLE_STMT, hstmt);
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran ROLLBACK failed", conn);
	printf("rolled back\n");

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_FETCH_BOOKMARK_PTR, bookmark, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr FETCH_BOOKMARK_PTR failed", hstmt);
	rc = SQLFetchScroll(hstmt, SQL_FETCH_BOOKMARK, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll BOOKMARK failed", hstmt);
	printf("fetched by bookmark: %d %s\n", (int) id, note);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;

	/*
	 * The row cache this tests exists only without declare/fetch; with
	 * it the server-side cursor would not survive the ROLLBACK.
	 */
	test_connect_ext("UseDeclareFetch=0");
	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr AUTOCOMMIT failed", conn);

	add_delete_rollback_refetch("fetch, then add", TRUE);
	add_delete_rollback_refetch("add before the first fetch", FALSE);

	test_disconnect();
	return 0;
}
