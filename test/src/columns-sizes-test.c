/*
 * Test that SQLColumns never returns a negative COLUMN_SIZE, BUFFER_LENGTH or
 * CHAR_OCTET_LENGTH.  MSDASQL binds them as SQL_C_ULONG, where a negative
 * value fails with 22003 (issue #228).  CHAR_OCTET_LENGTH is NULL for types
 * that are not character or binary, and SQL_NO_TOTAL is reported as INT_MAX.
 *
 * The sizes of the character columns depend on the platform and the driver,
 * so for those the test only shows whether a value was returned.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
show_size(const char *label, SQLUINTEGER value, SQLLEN ind, int exact)
{
	if (SQL_NULL_DATA == ind)
		printf(" %s=NULL", label);
	else if (!exact)
		printf(" %s=%s", label, value > 0 ? "set" : "zero");
	else if (INT_MAX == value)
		printf(" %s=INT_MAX", label);
	else
		printf(" %s=%lu", label, (unsigned long) value);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLCHAR		name[129];
	SQLLEN		nameInd, sizeInd, lengthInd, octetInd;
	SQLUINTEGER	size, length, octet;
	int			exact;

	/* boolean is reported as a character type with BoolsAsChar=1 */
	test_connect_ext("BoolsAsChar=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS columns_sizes", SQL_NTS);
	CHECK_STMT_RESULT(rc, "DROP TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TABLE columns_sizes "
		"(i integer, v varchar(10), b bytea, f boolean)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	rc = SQLColumns(hstmt, NULL, 0, NULL, 0,
					(SQLCHAR *) "columns_sizes", SQL_NTS, NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLColumns failed", hstmt);

	SQLBindCol(hstmt, 4, SQL_C_CHAR, name, sizeof(name), &nameInd);
	SQLBindCol(hstmt, 7, SQL_C_ULONG, &size, 0, &sizeInd);
	SQLBindCol(hstmt, 8, SQL_C_ULONG, &length, 0, &lengthInd);
	SQLBindCol(hstmt, 16, SQL_C_ULONG, &octet, 0, &octetInd);

	while ((rc = SQLFetch(hstmt)) != SQL_NO_DATA)
	{
		if (!SQL_SUCCEEDED(rc))
		{
			print_diag("SQLFetch failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}
		/* integer and bytea sizes are the same on every platform */
		exact = (0 == strcmp((char *) name, "i") || 0 == strcmp((char *) name, "b"));
		printf("%s:", name);
		show_size("COLUMN_SIZE", size, sizeInd, exact);
		show_size("BUFFER_LENGTH", length, lengthInd, exact);
		show_size("CHAR_OCTET_LENGTH", octet, octetInd, exact);
		printf("\n");
	}

	SQLFreeStmt(hstmt, SQL_CLOSE);
	SQLFreeStmt(hstmt, SQL_UNBIND);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "DROP TABLE columns_sizes", SQL_NTS);
	CHECK_STMT_RESULT(rc, "DROP TABLE failed", hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
