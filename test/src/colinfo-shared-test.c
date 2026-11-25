/*
 * Two cursors parsed against the same table share the connection's cached
 * column information.  A "DROP TABLE" command tag makes the connection drop
 * that cache.  Freeing one cursor afterwards must not take the column
 * information away from the other.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

/* keyset-driven and updatable, so the driver parses the query against the table */
static HSTMT
open_updatable_cursor(const char *query)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr(SQL_ATTR_CURSOR_TYPE) failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr(SQL_ATTR_CONCURRENCY) failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) query, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	return hstmt;
}

static void
describe_second_cursor_after(const char *statement)
{
	SQLRETURN	rc;
	HSTMT		first;
	HSTMT		second;
	HSTMT		other = SQL_NULL_HSTMT;

	first = open_updatable_cursor("SELECT id, t FROM testtab1");
	/*
	 * The driver's parser does not resolve the cast, but the server still
	 * reports the column as testtab1.t, so describing it looks it up in the
	 * shared column information.
	 */
	second = open_updatable_cursor("SELECT id, t::varchar(20) FROM testtab1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &other);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(other, (SQLCHAR *) statement, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", other);
	printf("\nexecuted %s\n", statement);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, first);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", first);
	printf("freed the first cursor\n");

	print_result_meta(second);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, second);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", second);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, other);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", other);
}

int
main(int argc, char **argv)
{

	test_connect();

	/* command tag "DROP VIEW": the connection keeps its column cache */
	describe_second_cursor_after("DROP VIEW IF EXISTS no_such_relation");

	/* command tag "DROP TABLE": the connection drops its column cache */
	describe_second_cursor_after("DROP TABLE IF EXISTS no_such_relation");

	test_disconnect();

	return 0;
}
