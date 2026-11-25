/*
 * Boolean parameters passed as counted strings: SQL_C_CHAR with the length in
 * StrLen_or_Ind and no terminating NUL. Only the first "len" bytes are the
 * value.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
insert_flag(HSTMT hstmt, char *value, SQLLEN len)
{
	SQLRETURN	rc;

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_BIT,		/* param type */
						  0,			/* column size */
						  0,			/* dec digits */
						  value,		/* param value ptr */
						  len,			/* buffer len */
						  &len			/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	printf("value \"%.*s\": ", (int) len, value);
	/* keep the line in the output file even if the driver crashes */
	fflush(stdout);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO settings VALUES (?)", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("stored\n");
	else
		print_diag("rejected", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char *dash;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE settings (enabled boolean)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while creating temp table", hstmt);

	/* "-1" is the driver's spelling of true */
	insert_flag(hstmt, "-1", 2);

	/* "-" is not a boolean, whatever byte follows it in memory */
	insert_flag(hstmt, "-1", 1);

	/* "-" in a buffer of its own length */
	dash = malloc(1);
	dash[0] = '-';
	insert_flag(hstmt, dash, 1);
	free(dash);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
