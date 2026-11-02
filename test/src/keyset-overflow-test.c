/*
 * Test that an oversized value in the hidden keyset (ctid) column of an
 * updatable cursor is rejected instead of overflowing the driver's buffer.
 *
 * A view can have a column of its own named ctid. Selecting from it with an
 * updatable cursor makes the driver append "ctid" to the query, so the view
 * column comes back where the driver expects a tuple identifier.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
run_query(HSTMT hstmt, const char *label, const char *sql)
{
	SQLRETURN	rc;
	int			rows = 0;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		printf("%s: execute failed\n", label);
		print_diag("", SQL_HANDLE_STMT, hstmt);
		SQLFreeStmt(hstmt, SQL_CLOSE);
		return;
	}
	while (SQL_SUCCEEDED(rc = SQLFetch(hstmt)))
		rows++;
	if (SQL_NO_DATA == rc)
		printf("%s: fetched %d rows\n", label, rows);
	else
	{
		printf("%s: fetch failed after %d rows\n", label, rows);
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static HSTMT
updatable_stmt(SQLULEN cursor_type)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) cursor_type, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	return hstmt;
}

static void
create_objects(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE keyset_overflow_tab AS "
		"SELECT g AS id FROM generate_series(1, 3) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	/* The second row's ctid does not fit in the keyset buffer */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY VIEW keyset_overflow_view AS "
		"SELECT id, CASE id WHEN 2 THEN repeat('x', 200) ELSE '(0,1)' END AS ctid "
		"FROM keyset_overflow_tab", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE VIEW failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int main(int argc, char **argv)
{
	HSTMT		hstmt;

	test_connect();
	create_objects();
	hstmt = updatable_stmt(SQL_CURSOR_KEYSET_DRIVEN);
	run_query(hstmt, "real ctid", "SELECT id FROM keyset_overflow_tab");
	run_query(hstmt, "oversized ctid", "SELECT id FROM keyset_overflow_view");
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	/* With one row per fetch, the oversized row is read by a later fetch */
	test_connect_ext("UseDeclareFetch=1;Fetch=1");
	create_objects();
	hstmt = updatable_stmt(SQL_CURSOR_STATIC);
	run_query(hstmt, "declare/fetch real ctid", "SELECT id FROM keyset_overflow_tab");
	run_query(hstmt, "declare/fetch oversized ctid", "SELECT id FROM keyset_overflow_view");
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	return 0;
}
