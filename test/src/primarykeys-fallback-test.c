/*
 * Test the SQLPrimaryKeys fallback for tables without a PRIMARY KEY
 * constraint.
 *
 * When a table has no primary key constraint, the driver looks for an
 * old-fashioned primary key, i.e. a unique index named <table>_pkey.
 * That index must belong to the table itself and must be unique; an
 * index of that name on some other table in the schema, or a non-unique
 * index, must not be reported as the table's primary key.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
run_sql(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

static void
check_primary_keys(HSTMT hstmt, const char *table)
{
	SQLRETURN	rc;

	rc = SQLPrimaryKeys(hstmt,
						NULL, 0,
						(SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) table, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrimaryKeys failed", hstmt);
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

	run_sql(hstmt, "DROP TABLE IF EXISTS pk_fallback_test");
	run_sql(hstmt, "DROP TABLE IF EXISTS pk_fallback_other");

	/* A table without a primary key, and another table in the same schema */
	run_sql(hstmt, "CREATE TABLE pk_fallback_test (id int, branch int)");
	run_sql(hstmt, "CREATE TABLE pk_fallback_other (branch int)");

	/* No index at all: no primary key */
	printf("Check SQLPrimaryKeys without any index\n");
	check_primary_keys(hstmt, "pk_fallback_test");

	/* An index named <table>_pkey on a different table must not count */
	run_sql(hstmt, "CREATE INDEX pk_fallback_test_pkey ON pk_fallback_other (branch)");
	printf("Check SQLPrimaryKeys with <table>_pkey index on another table\n");
	check_primary_keys(hstmt, "pk_fallback_test");
	run_sql(hstmt, "DROP INDEX pk_fallback_test_pkey");

	/* A non-unique index named <table>_pkey on the table itself must not count */
	run_sql(hstmt, "CREATE INDEX pk_fallback_test_pkey ON pk_fallback_test (branch)");
	printf("Check SQLPrimaryKeys with non-unique <table>_pkey index\n");
	check_primary_keys(hstmt, "pk_fallback_test");
	run_sql(hstmt, "DROP INDEX pk_fallback_test_pkey");

	/* A unique index named <table>_pkey on the table itself is the old-fashioned primary key */
	run_sql(hstmt, "CREATE UNIQUE INDEX pk_fallback_test_pkey ON pk_fallback_test (id)");
	printf("Check SQLPrimaryKeys with unique <table>_pkey index\n");
	check_primary_keys(hstmt, "pk_fallback_test");

	/* Clean up */
	run_sql(hstmt, "DROP TABLE pk_fallback_test");
	run_sql(hstmt, "DROP TABLE pk_fallback_other");

	test_disconnect();

	return 0;
}
