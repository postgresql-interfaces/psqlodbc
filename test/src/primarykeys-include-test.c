/*
 * Test that SQLPrimaryKeys excludes INCLUDE columns.
 *
 * A PRIMARY KEY with INCLUDE (PG 11+) should only return the actual
 * key columns, not the included columns.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* INCLUDE clause requires PG >= 11 */
	if (server_version_lt(conn, 11, 0))
	{
		printf("Server version < 11, skipping INCLUDE test\n");
		test_disconnect();
		return 0;
	}

	/* Create table with PK that has INCLUDE columns */
	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "DROP TABLE IF EXISTS pk_include_test", SQL_NTS);
	CHECK_STMT_RESULT(rc, "DROP TABLE failed", hstmt);

	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "CREATE TABLE pk_include_test (a int, b int, c int, d int,"
		" CONSTRAINT pk_include_test_pkey PRIMARY KEY (a, b) INCLUDE (c, d))",
		SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Call SQLPrimaryKeys - should return only a and b, not c or d */
	printf("Check SQLPrimaryKeys with INCLUDE columns\n");
	rc = SQLPrimaryKeys(hstmt,
						NULL, 0,
						(SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) "pk_include_test", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrimaryKeys failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "DROP TABLE pk_include_test", SQL_NTS);
	CHECK_STMT_RESULT(rc, "DROP TABLE failed", hstmt);

	test_disconnect();

	return 0;
}
