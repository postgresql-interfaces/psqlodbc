/*
 * Savepoint commands on one statement handle while another handle of the
 * same connection holds a declare/fetch cursor that was read to its end.
 * Autocommit is on (the default).
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
exec_beside_finished_cursor(char *sql)
{
	int			rc;
	HSTMT		hstmt_cursor = SQL_NULL_HSTMT;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt_cursor);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* 3 rows < Fetch: the first fetch reaches the end, the cursor stays open */
	rc = SQLExecDirect(hstmt_cursor, (SQLCHAR *) "SELECT g FROM generate_series(1, 3) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt_cursor);
	print_result(hstmt_cursor);

	printf("%s\n", sql);
	fflush(stdout);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	printf("ok\n");

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt_cursor);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt_cursor);
}

int main(int argc, char **argv)
{
	test_connect_ext("UseDeclareFetch=1;Fetch=100");

	exec_beside_finished_cursor("SAVEPOINT a; RELEASE a");
	exec_beside_finished_cursor("SAVEPOINT a; ROLLBACK TO a");

	test_disconnect();

	return 0;
}
