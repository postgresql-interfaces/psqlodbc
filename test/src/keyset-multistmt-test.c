/*
 * Test a keyset driven cursor on a query that returns more results than
 * the two the driver sends for it.
 *
 * For an updatable keyset driven cursor the driver splits the query at
 * its WHERE clause into one statement for the columns info and one for
 * the keyset. A second statement in the application's text follows the
 * keyset statement, so the server answers with three results instead of
 * two. The driver must not link its result chain into a cycle on the
 * third one, or it frees a result it goes on using.
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
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	/* The int4 serial column is needed, or the cursor becomes static */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE keyset_multistmt_tab "
		"(id serial PRIMARY KEY, t text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO keyset_multistmt_tab (t) "
		"SELECT 'row ' || g FROM generate_series(1, 5) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);

	run_query(hstmt, "single statement",
			  "SELECT id FROM keyset_multistmt_tab WHERE id > 0");
	run_query(hstmt, "select and update",
			  "SELECT id FROM keyset_multistmt_tab WHERE id > 0; "
			  "UPDATE keyset_multistmt_tab SET t = 'seen' WHERE id > 0");
	run_query(hstmt, "after the update",
			  "SELECT id FROM keyset_multistmt_tab WHERE t = 'seen'");

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	return 0;
}
