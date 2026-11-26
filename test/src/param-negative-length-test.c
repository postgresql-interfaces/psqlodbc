/*
 * Test that a negative StrLen_or_IndPtr value of a bound parameter that is
 * none of the special indicator values (SQL_NULL_DATA, SQL_NTS,
 * SQL_DEFAULT_PARAM, SQL_DATA_AT_EXEC, SQL_LEN_DATA_AT_EXEC(n)) is rejected
 * instead of being used as the parameter's byte count.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
select_with_length(HSTMT hstmt, SQLSMALLINT ctype, SQLSMALLINT sqltype,
				   const char *sql, char *value, SQLLEN buflen, SQLLEN length)
{
	SQLRETURN	rc;
	SQLLEN		cbValue = length;
	SQLLEN		ind;
	char		result[100];

	printf("length %ld: ", (long) length);
	/* the line must be out before the driver is called, it may never return */
	fflush(stdout);

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  ctype,		/* value type */
						  sqltype,		/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  value,		/* param value ptr */
						  buflen,		/* buffer len */
						  &cbValue		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);
		return;
	}
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, result, sizeof(result), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("%s\n", ind == SQL_NULL_DATA ? "NULL" : result);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		digits[] = "123";
	char		bytes[] = "\x01\x02\x03";

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* SQL_C_CHAR -> integer, substituted into the statement without quotes */
	printf("SQL_C_CHAR -> SQL_INTEGER\n");
	select_with_length(hstmt, SQL_C_CHAR, SQL_INTEGER,
					   "SELECT CAST(? AS integer)",
					   digits, sizeof(digits), 3);
	select_with_length(hstmt, SQL_C_CHAR, SQL_INTEGER,
					   "SELECT CAST(? AS integer)",
					   digits, sizeof(digits), SQL_NTS);
	/*
	 * Not SQL_NULL_DATA (-1), SQL_DATA_AT_EXEC (-2), SQL_NTS (-3),
	 * SQL_DEFAULT_PARAM (-5) or SQL_LEN_DATA_AT_EXEC (<= -100).
	 */
	select_with_length(hstmt, SQL_C_CHAR, SQL_INTEGER,
					   "SELECT CAST(? AS integer)",
					   digits, sizeof(digits), -10);

	/* SQL_C_BINARY -> bytea, substituted as a quoted hex literal */
	printf("SQL_C_BINARY -> SQL_VARBINARY\n");
	select_with_length(hstmt, SQL_C_BINARY, SQL_VARBINARY,
					   "SELECT encode(CAST(? AS bytea), 'hex')",
					   bytes, sizeof(bytes), 3);
	select_with_length(hstmt, SQL_C_BINARY, SQL_VARBINARY,
					   "SELECT encode(CAST(? AS bytea), 'hex')",
					   bytes, sizeof(bytes), SQL_NULL_DATA);
	select_with_length(hstmt, SQL_C_BINARY, SQL_VARBINARY,
					   "SELECT encode(CAST(? AS bytea), 'hex')",
					   bytes, sizeof(bytes), -4);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
