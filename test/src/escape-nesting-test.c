/*
 * Test ODBC scalar function escapes nested inside one another,
 * {fn abs({fn abs( ... )})}. A statement nested too deeply to convert
 * must fail with an error instead of crashing the application.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* SELECT {fn abs({fn abs( ... {fn abs(-1)} ... )})} */
static char *
nested_escapes(int depth)
{
	char	   *sql = malloc(strlen("SELECT -1") + depth * strlen("{fn abs()}") + 1);
	char	   *p = sql;
	int			i;

	p += sprintf(p, "SELECT ");
	for (i = 0; i < depth; i++)
		p += sprintf(p, "{fn abs(");
	p += sprintf(p, "-1");
	for (i = 0; i < depth; i++)
		p += sprintf(p, ")}");
	return sql;
}

/* SELECT count(*) FROM (VALUES ({fn abs(-1)}),({fn abs(-1)}), ...) v */
static char *
side_by_side_escapes(int count)
{
	char	   *sql = malloc(strlen("SELECT count(*) FROM (VALUES ) v") + count * strlen(",({fn abs(-1)})") + 1);
	char	   *p = sql;
	int			i;

	p += sprintf(p, "SELECT count(*) FROM (VALUES ");
	for (i = 0; i < count; i++)
		p += sprintf(p, "%s({fn abs(-1)})", i == 0 ? "" : ",");
	p += sprintf(p, ") v");
	return sql;
}

static void
execute_and_print(HSTMT hstmt, const char *label, char *sql)
{
	SQLRETURN	rc;

	printf("%s\n", label);
	fflush(stdout);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		print_result(hstmt);
	else
		print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	free(sql);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	execute_and_print(hstmt, "10 nested escapes", nested_escapes(10));
	execute_and_print(hstmt, "50000 escapes side by side", side_by_side_escapes(50000));
	execute_and_print(hstmt, "50000 nested escapes", nested_escapes(50000));

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
