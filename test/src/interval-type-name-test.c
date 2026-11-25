/*
 * Test SQL_DESC_TYPE_NAME of interval columns.
 *
 * A plain interval column (type modifier -1) used to return SQL_SUCCESS
 * without writing the type name into the application's buffer.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLUSMALLINT i;
	SQLSMALLINT numcols;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLExecDirect(hstmt,
			(SQLCHAR *) "SELECT '1 day'::interval AS plain_interval, '1 day'::interval day AS interval_day, '1 year'::interval year to month AS interval_year_to_month, '1 second'::interval second AS interval_second",
			SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLNumResultCols(hstmt, &numcols);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);

	for (i = 1 ; i <= numcols; i++)
	{
		char label[64];
		char buffer[64];
		SQLSMALLINT len;

		rc = SQLColAttribute(hstmt, i, SQL_DESC_LABEL, label, sizeof(label), NULL, NULL);
		CHECK_STMT_RESULT(rc, "SQLColAttribute failed", hstmt);

		/* a successful call must always write the buffer */
		strcpy(buffer, "(buffer not written)");
		rc = SQLColAttribute(hstmt, i, SQL_DESC_TYPE_NAME, buffer, sizeof(buffer), &len, NULL);
		CHECK_STMT_RESULT(rc, "SQLColAttribute failed", hstmt);
		printf("%s: SQL_DESC_TYPE_NAME: %s (length %d)\n", label, buffer, (int) len);

		strcpy(buffer, "(buffer not written)");
		rc = SQLColAttribute(hstmt, i, SQL_DESC_LOCAL_TYPE_NAME, buffer, sizeof(buffer), &len, NULL);
		CHECK_STMT_RESULT(rc, "SQLColAttribute failed", hstmt);
		printf("%s: SQL_DESC_LOCAL_TYPE_NAME: %s (length %d)\n", label, buffer, (int) len);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
