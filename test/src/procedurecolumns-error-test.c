/*
 * Test that SQLProcedureColumns fails cleanly when the server rejects the
 * driver's catalog query, instead of freeing the query's result twice.  A
 * byte that is invalid in the client encoding makes the server reject it.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static int
count_rows(HSTMT hstmt)
{
	int			rows = 0;

	while (SQL_SUCCEEDED(SQLFetch(hstmt)))
		rows++;
	SQLFreeStmt(hstmt, SQL_CLOSE);
	return rows;
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLProcedureColumns(hstmt, NULL, 0, (SQLCHAR *) "public", SQL_NTS,
							 (SQLCHAR *) "simple_add", SQL_NTS, NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLProcedureColumns failed", hstmt);
	printf("valid name: %d columns\n", count_rows(hstmt));

	rc = SQLProcedureColumns(hstmt, NULL, 0, (SQLCHAR *) "public", SQL_NTS,
							 (SQLCHAR *) "simple_add\377", SQL_NTS, NULL, 0);
	/* A SQL_ASCII database accepts the byte, so the query succeeds there */
	if (SQL_SUCCEEDED(rc))
		printf("invalid byte in name: accepted by the server\n");
	else
		printf("invalid byte in name: failed as expected\n");
	SQLFreeStmt(hstmt, SQL_CLOSE);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SELECT 1 failed", hstmt);
	printf("still connected: %d row\n", count_rows(hstmt));

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
