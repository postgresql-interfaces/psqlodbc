/*
 * Test SQLColAttribute on an updatable cursor over a table that another
 * session altered, after a DROP TABLE on the same connection.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
exec_direct(HSTMT hstmt, char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, sql, hstmt);
}

static void
print_base_column_name(HSTMT hstmt, SQLUSMALLINT colno)
{
	SQLRETURN	rc;
	char		buffer[64];

	rc = SQLColAttribute(hstmt, colno, SQL_DESC_BASE_COLUMN_NAME, buffer, sizeof(buffer), NULL, NULL);
	CHECK_STMT_RESULT(rc, "SQLColAttribute failed", hstmt);
	printf("SQL_DESC_BASE_COLUMN_NAME of column %d: %s\n", colno, buffer);
}

static void
add_column_from_another_session(void)
{
	SQLRETURN	rc;
	HDBC		other_conn = SQL_NULL_HDBC;
	HSTMT		other_stmt = SQL_NULL_HSTMT;
	char		connstr[256];

	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &other_conn);
	CHECK_CONN_RESULT(rc, "failed to allocate dbc handle", conn);
	snprintf(connstr, sizeof(connstr), "DSN=%s", get_test_dsn());
	rc = SQLDriverConnect(other_conn, NULL, (SQLCHAR *) connstr, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	CHECK_CONN_RESULT(rc, "SQLDriverConnect failed", other_conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, other_conn, &other_stmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", other_conn);

	exec_direct(other_stmt, "ALTER TABLE colatt_stale ADD COLUMN added int");

	SQLFreeHandle(SQL_HANDLE_STMT, other_stmt);
	SQLDisconnect(other_conn);
	SQLFreeHandle(SQL_HANDLE_DBC, other_conn);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	HSTMT		hstmt2 = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt2);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	exec_direct(hstmt2, "DROP TABLE IF EXISTS colatt_stale, colatt_scratch");
	exec_direct(hstmt2, "CREATE TABLE colatt_stale (id int PRIMARY KEY, t text)");
	exec_direct(hstmt2, "CREATE TABLE colatt_scratch (i int)");

	/* The connection caches the column list of colatt_stale. */
	exec_direct(hstmt, "SELECT id, t FROM colatt_stale");
	print_base_column_name(hstmt, 1);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* This connection does not see the ALTER TABLE and keeps its cached list. */
	add_column_from_another_session();

	/*
	 * An updatable cursor makes the driver parse the statement when it is
	 * executed, against the cached list, which has no column "added".
	 */
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	exec_direct(hstmt, "SELECT id, added FROM colatt_stale");

	/* A DROP TABLE on this connection empties its column cache. */
	exec_direct(hstmt2, "DROP TABLE colatt_scratch");

	/* Describing "added" makes the driver fetch the column list again. */
	print_base_column_name(hstmt, 2);
	print_base_column_name(hstmt, 1);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	exec_direct(hstmt2, "DROP TABLE colatt_stale");

	test_disconnect();

	return 0;
}
