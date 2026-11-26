/*
 * Test that a statement handle with a keyset-driven cursor can be used again
 * after the first statement executed on it failed, whether the server or the
 * driver refused that statement.
 *
 * The driver parses a SELECT (position of FROM, table, key column) before it
 * rewrites it for the keyset-driven cursor. That information must not survive
 * the replacement of the statement text, or it is applied to the next, possibly
 * shorter, statement executed on the same handle.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
run_query(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc;
	int			rows = 0;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
	{
		while (SQL_SUCCEEDED(SQLFetch(hstmt)))
			rows++;
		printf("%s: fetched %d rows\n", sql, rows);
	}
	else
	{
		printf("%s: execute failed\n", sql);
		print_diag(NULL, SQL_HANDLE_STMT, hstmt);
	}
	fflush(stdout);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static void
prepare_and_run_query(HSTMT hstmt, const char *sql, BOOL describe)
{
	SQLRETURN	rc;
	SQLSMALLINT	ncols;
	int			rows = 0;

	rc = SQLPrepare(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
	{
		/* a describe the server refuses leaves the statement unparsed */
		if (describe)
			rc = SQLNumResultCols(hstmt, &ncols);
		if (!SQL_SUCCEEDED(rc))
		{
			printf("%s (prepared): describe failed\n", sql);
			print_diag(NULL, SQL_HANDLE_STMT, hstmt);
		}
		rc = SQLExecute(hstmt);
	}
	if (SQL_SUCCEEDED(rc))
	{
		while (SQL_SUCCEEDED(SQLFetch(hstmt)))
			rows++;
		printf("%s (prepared): fetched %d rows\n", sql, rows);
		SQLFreeStmt(hstmt, SQL_CLOSE);
	}
	else
	{
		/* no SQLFreeStmt(SQL_CLOSE): the next SQLPrepare replaces the text */
		printf("%s (prepared): execute failed\n", sql);
		print_diag(NULL, SQL_HANDLE_STMT, hstmt);
	}
	fflush(stdout);
}

static HSTMT
keyset_driven_stmt(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	return hstmt;
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt;

	test_connect();

	/* Without a key column such as serial the cursor falls back to static */
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE stale_parse_tab (id serial PRIMARY KEY, d date)",
		SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

	/* The server refuses the first statement */
	hstmt = keyset_driven_stmt();
	run_query(hstmt, "SELECT id, d FROM stale_parse_tab WHERE nosuchcolumn = 1");
	run_query(hstmt, "SELECT 1");
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

	/* The driver refuses the first statement: {dt ...} is not an ODBC escape */
	hstmt = keyset_driven_stmt();
	run_query(hstmt, "SELECT id, d FROM stale_parse_tab WHERE d > {dt '2026-01-01'}");
	run_query(hstmt, "SELECT 1");
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

	/*
	 * The same through SQLPrepare + SQLExecute. The second statement is one
	 * the server refuses to describe, so that it is not parsed again.
	 */
	hstmt = keyset_driven_stmt();
	prepare_and_run_query(hstmt, "SELECT id, d FROM stale_parse_tab WHERE d > {dt '2026-01-01'}", FALSE);
	prepare_and_run_query(hstmt, "SELECT nosuch", TRUE);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

	test_disconnect();
	return 0;
}
