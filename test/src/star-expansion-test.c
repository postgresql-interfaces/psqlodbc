/*
 * Test that a SELECT whose result would have more than SHRT_MAX columns is
 * rejected by the driver's parser instead of overflowing its field array.
 *
 * An updatable cursor makes the driver parse the statement and expand "*"
 * to the columns of every table in the FROM list before sending it. A
 * column total summed in a 16-bit integer wraps negative for more than
 * 32768 columns, and the expansion then writes past the array. A 16-bit
 * slot count for the field array does the same for an explicit column list
 * long enough to need more than 32767 slots.
 *
 * The server rejects all of these statements (a target list can have at
 * most 1664 entries); the driver must get that far without writing outside
 * its own buffers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* the test table has this many columns */
#define NCOLS		100
/* SELECT * over this many references to it gives 33000 columns */
#define NSTARREFS	330
/* an explicit column list longer than the field array can hold */
#define NLISTCOLS	32770

static void
run_query(HSTMT hstmt, const char *label, const char *sql)
{
	SQLRETURN	rc;
	int			rows = 0;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		printf("%s: execute failed\n", label);
		print_diag("", SQL_HANDLE_STMT, hstmt);
		SQLFreeStmt(hstmt, SQL_CLOSE);
		return;
	}
	while (SQL_SUCCEEDED(rc = SQLFetch(hstmt)))
		rows++;
	if (SQL_NO_DATA == rc)
		printf("%s: fetched %d rows\n", label, rows);
	else
	{
		printf("%s: fetch failed after %d rows\n", label, rows);
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static HSTMT
updatable_stmt(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	return hstmt;
}

static void
create_table(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		sql[64 + NCOLS * 16];
	char	   *p;
	int			i;

	/* CREATE TEMPORARY TABLE star_expansion_tab (c1 int, c2 int, ...) */
	p = sql + sprintf(sql, "CREATE TEMPORARY TABLE star_expansion_tab (c1 int");
	for (i = 2; i <= NCOLS; i++)
		p += sprintf(p, ", c%d int", i);
	strcpy(p, ")");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO star_expansion_tab (c1) VALUES (1), (2)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int main(int argc, char **argv)
{
	HSTMT		hstmt;
	char	   *sql;
	char	   *p;
	int			i;

	test_connect();
	create_table();
	hstmt = updatable_stmt();

	/* The expansion of "*" is fine while the result fits */
	run_query(hstmt, "two tables",
			  "SELECT * FROM star_expansion_tab t1, star_expansion_tab t2");

	/* SELECT * FROM star_expansion_tab t1, ..., star_expansion_tab t330 */
	sql = malloc(64 + NSTARREFS * 32);
	p = sql + sprintf(sql, "SELECT * FROM star_expansion_tab t1");
	for (i = 2; i <= NSTARREFS; i++)
		p += sprintf(p, ", star_expansion_tab t%d", i);
	run_query(hstmt, "star over 33000 columns", sql);
	free(sql);

	/* SELECT c1, c1, ..., c1 FROM star_expansion_tab */
	sql = malloc(64 + NLISTCOLS * 4);
	p = sql + sprintf(sql, "SELECT c1");
	for (i = 2; i <= NLISTCOLS; i++)
		p += sprintf(p, ",c1");
	strcpy(p, " FROM star_expansion_tab");
	run_query(hstmt, "32770 listed columns", sql);
	free(sql);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	return 0;
}
