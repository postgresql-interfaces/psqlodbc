/*
 * Test that a SELECT whose last word is FROM reaches the server, and gets
 * the server's syntax error, when it is opened on a keyset-driven updatable
 * cursor.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
run_query(HSTMT hstmt, const char *label, const char *sql)
{
	SQLRETURN	rc;

	printf("%s: \"%s\"\n", label, sql);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("execute succeeded\n");
	else
		print_diag("execute failed", SQL_HANDLE_STMT, hstmt);
	/* keep the output if a later call kills the process */
	fflush(stdout);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	/* a serial column is a key the driver accepts for a keyset-driven cursor */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE keyset_from_tab (id serial)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);

	/*
	 * The parser takes the table from the first FROM but, because of the
	 * comma, records the position of the last one. The two statements
	 * differ only in the blank at the end.
	 */
	run_query(hstmt, "blank after the last FROM",
			  "select id, from keyset_from_tab select id from ");
	run_query(hstmt, "FROM is the last word",
			  "select id, from keyset_from_tab select id from");
	/* the same, with the recorded WHERE in front of the recorded FROM */
	run_query(hstmt, "WHERE before the last FROM",
			  "select id, from keyset_from_tab where 1=1 select id from x");

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	return 0;
}
