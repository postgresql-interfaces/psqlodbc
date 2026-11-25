/*
 * Test SQLSetPos(SQL_REFRESH) on a keyset-driven cursor over a table whose
 * name contains quote characters.
 *
 * To re-read the latest version of a row the driver sends
 * "... where ctid = (select currtid2('<table>', '<tid>'))". The table name
 * is quoted as an identifier but placed inside a string literal, so a
 * single quote in the name must be escaped for the literal as well. The
 * name comes either from the application's statement (table with an
 * auto-increment column) or from the server (child table of an inherited
 * table, looked up by tableoid).
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
refresh_first_row(const char *label, const char *sql)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLUINTEGER	cursor_type;
	SQLINTEGER	id;
	char		t[40];
	SQLLEN		ind_id, ind_t;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);

	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &id, 0, &ind_id);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, t, sizeof(t), &ind_t);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) &cursor_type, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	printf("%s: cursor_type=%d\n", label, (int) cursor_type);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	printf("%s: fetched %d %s\n", label, (int) id, t);

	rc = SQLSetPos(hstmt, 1, SQL_REFRESH, SQL_LOCK_NO_CHANGE);
	if (SQL_SUCCEEDED(rc))
		printf("%s: refreshed\n", label);
	else
	{
		printf("%s: SQLSetPos(SQL_REFRESH) failed\n", label);
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	/*
	 * The cursor types printed below are those of UseDeclareFetch=0;
	 * with UseDeclareFetch=1 the driver uses a static cursor instead.
	 */
	test_connect_ext("UseDeclareFetch=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* the auto-increment column makes the cursor keyset-driven */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE \"refresh_q'tab\" (id serial, t text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO \"refresh_q'tab\" (t) VALUES ('one'), ('two')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);

	/* the rows are read through a child table, whose name the server supplies */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE refresh_q_parent (id int, t text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE \"refresh_q'child\" () INHERITS (refresh_q_parent)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO \"refresh_q'child\" VALUES (1, 'one'), (2, 'two')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	refresh_first_row("quoted table", "SELECT id, t FROM \"refresh_q'tab\" ORDER BY id");
	refresh_first_row("quoted child table", "SELECT id, t FROM refresh_q_parent ORDER BY id");

	test_disconnect();

	return 0;
}
