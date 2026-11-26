/*
 * With Parse=1 every table reference of a parsed statement takes a reference
 * on the connection's cached column info of that table.  One statement naming
 * testtab1 32767 times plus a second statement on testtab1 must not make the
 * reference count wrap.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define MAX_TABLE_REFS	32767	/* the most one statement can hold */

/* "SELECT 1 FROM testtab1,testtab1,...,testtab1" with nrefs references */
static char *
select_from_testtab1_repeated(int nrefs)
{
	char	   *sql = malloc(32 + nrefs * strlen(",testtab1"));
	char	   *p;
	int			i;

	p = sql + sprintf(sql, "SELECT 1 FROM testtab1");
	for (i = 1; i < nrefs; i++)
		p += sprintf(p, ",testtab1");
	return sql;
}

static HSTMT
prepare_and_count_columns(const char *name, const char *sql)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLSMALLINT	ncols;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);
	rc = SQLPrepare(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	rc = SQLNumResultCols(hstmt, &ncols);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);
	printf("%s: %d result columns\n", name, (int) ncols);
	return hstmt;
}

int
main(int argc, char **argv)
{
	char	   *big_sql = select_from_testtab1_repeated(MAX_TABLE_REFS);
	HSTMT		big_stmt, small_stmt;

	test_connect_ext("Parse=1");

	big_stmt = prepare_and_count_columns("statement 1 (testtab1 named 32767 times)", big_sql);
	small_stmt = prepare_and_count_columns("statement 2 (SELECT id FROM testtab1)", "SELECT id FROM testtab1");

	SQLFreeHandle(SQL_HANDLE_STMT, small_stmt);
	printf("freed statement 2\n");
	SQLFreeHandle(SQL_HANDLE_STMT, big_stmt);
	printf("freed statement 1\n");

	test_disconnect();
	free(big_sql);

	return 0;
}
