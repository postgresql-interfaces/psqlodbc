/*
 * Test a catalog function name argument whose length does not fit
 * SQLSMALLINT.
 *
 * The Unicode entry points convert each name to UTF-8 and pass the byte
 * count on as a SQLSMALLINT. A character takes up to four bytes there, so
 * the count can exceed SHRT_MAX for an argument length that fits it. A
 * count that wraps negative is read as an absent argument, and SQLTables
 * would answer with every table instead of the one named. No identifier
 * is that long, so the answer must be an empty result.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include <sqlucode.h>

/* 11000 characters of three UTF-8 bytes each: 33000 > SHRT_MAX */
#define NAME_CHARS	11000

static void
count_tables(HSTMT hstmt, const char *label)
{
	SQLRETURN	rc;
	int			rows = 0;

	while (SQL_SUCCEEDED(rc = SQLFetch(hstmt)))
		rows++;
	printf("%s: %s\n", label, rows ? "rows found" : "no rows");
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLWCHAR   *name;
	SQLWCHAR	empty[1] = {0};
	int			i;

	name = malloc((NAME_CHARS + 1) * sizeof(SQLWCHAR));
	if (NULL == name)
	{
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	for (i = 0; i < NAME_CHARS; i++)
		name[i] = 0x4e2d;		/* a three-byte character in UTF-8 */
	name[NAME_CHARS] = 0;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* the length is given, so the driver measures the UTF-8 form */
	rc = SQLTablesW(hstmt, NULL, 0, NULL, 0, name, NAME_CHARS, NULL, 0);
	if (SQL_SUCCEEDED(rc))
		count_tables(hstmt, "SQLTablesW with an over-long table name");
	else
		print_diag("SQLTablesW failed", SQL_HANDLE_STMT, hstmt);

	/*
	 * The same with the name arguments read as identifiers. Identifier
	 * arguments must not be null pointers: the Windows driver manager
	 * refuses a null catalog or schema name with HY009 before the call
	 * reaches the driver, so pass empty ones.
	 */
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER) SQL_TRUE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLTablesW(hstmt, empty, SQL_NTS, empty, SQL_NTS, name, NAME_CHARS, NULL, 0);
	if (SQL_SUCCEEDED(rc))
		count_tables(hstmt, "the same with SQL_ATTR_METADATA_ID on");
	else
		print_diag("SQLTablesW failed", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();
	free(name);

	return 0;
}
