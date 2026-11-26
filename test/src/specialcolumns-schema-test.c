/*
 * Test that SQLSpecialColumns(SQL_BEST_ROWID) called without a schema name
 * only looks at the table in the current schema.
 *
 * A same-named table in another schema, with a primary key of its own, must
 * not contribute its key columns to the result.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
special_columns(HSTMT hstmt, const char *schema, const char *table)
{
	SQLRETURN	rc;

	rc = SQLSpecialColumns(hstmt, SQL_BEST_ROWID,
						   NULL, 0,
						   (SQLCHAR *) schema, schema ? SQL_NTS : 0,
						   (SQLCHAR *) table, SQL_NTS,
						   SQL_SCOPE_SESSION,
						   SQL_NO_NULLS);
	CHECK_STMT_RESULT(rc, "SQLSpecialColumns failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/*
	 * Two tables named like the ones in the public schema, in a schema of
	 * their own, each with a primary key that the public table lacks.
	 */
	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "DROP SCHEMA IF EXISTS specialcolumns_other CASCADE", SQL_NTS);
	CHECK_STMT_RESULT(rc, "DROP SCHEMA failed", hstmt);

	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "CREATE SCHEMA specialcolumns_other", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE SCHEMA failed", hstmt);

	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "CREATE TABLE specialcolumns_other.test_special_primary"
		" (uniq integer PRIMARY KEY)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);

	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "CREATE TABLE specialcolumns_other.test_special"
		" (ival integer PRIMARY KEY)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Only the primary key of public.test_special_primary */
	printf("Check for SQLSpecialColumns primary key no schema\n");
	special_columns(hstmt, NULL, "test_special_primary");

	/* The unique indexes of public.test_special, not the other primary key */
	printf("Check for SQLSpecialColumns unique index no schema\n");
	special_columns(hstmt, NULL, "test_special");

	/* With the schema given, the other table is still reachable */
	printf("Check for SQLSpecialColumns primary key other schema\n");
	special_columns(hstmt, "specialcolumns_other", "test_special");

	/* Clean up */
	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "DROP SCHEMA specialcolumns_other CASCADE", SQL_NTS);
	CHECK_STMT_RESULT(rc, "DROP SCHEMA failed", hstmt);

	test_disconnect();

	return 0;
}
