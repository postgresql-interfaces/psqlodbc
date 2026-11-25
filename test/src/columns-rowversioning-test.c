/*
 * Test that SQLColumns on a table that does not exist returns no row, also
 * with RowVersioning=1, where the driver appends an "xmin" row to the
 * columns of every table it finds.
 *
 * The table that does exist is listed first, in a schema of its own, so that
 * its name is on the stack when the missing table is looked up.
 *
 * Also test that SQLColumns returns the "xmin" row when it is asked for by
 * name. The column name is a search pattern here, and "xmin" is not an
 * ordinary column of the table, so the search by itself finds no row to
 * take the table's name from. The table is listed in a statement of its
 * own first, so the row is not built from values an earlier call left.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
list_columns(char *schema, char *table, char *column)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	/* TABLE_SCHEM, TABLE_NAME, COLUMN_NAME */
	SQLSMALLINT	colids[] = {2, 3, 4};

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	if (column)
		printf("SQLColumns %s.%s, column %s\n", schema, table, column);
	else
		printf("SQLColumns %s.%s\n", schema, table);
	rc = SQLColumns(hstmt, NULL, 0,
					(SQLCHAR *) schema, SQL_NTS,
					(SQLCHAR *) table, SQL_NTS,
					(SQLCHAR *) column, column ? SQL_NTS : 0);
	CHECK_STMT_RESULT(rc, "SQLColumns failed", hstmt);
	print_result_series(hstmt, colids, 3, -1, FALSE);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

static void
create_objects(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SET client_min_messages = warning; "
		"DROP SCHEMA IF EXISTS hr_priv CASCADE; "
		"CREATE SCHEMA hr_priv; "
		"CREATE TABLE hr_priv.payroll (employee int, salary int)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int main(int argc, char **argv)
{
	test_connect();
	create_objects();
	list_columns("hr_priv", "payroll", NULL);
	list_columns("public", "no_such_table", NULL);
	list_columns("hr_priv", "payroll", "xmin");
	test_disconnect();

	test_connect_ext("RowVersioning=1");
	list_columns("hr_priv", "payroll", NULL);
	list_columns("public", "no_such_table", NULL);
	test_disconnect();

	/* a new connection, so that an earlier call left no table name */
	test_connect_ext("RowVersioning=1");
	list_columns("hr_priv", "payroll", "xmin");
	list_columns("public", "no_such_table", "xmin");
	list_columns("hr_priv", "payroll", "salary");
	test_disconnect();

	return 0;
}
