/*
 * Test SQLColAttribute
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
runtest(char *extra_conn_options)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLUSMALLINT i;

	printf("Running tests with %s...\n", extra_conn_options);

	/* The behavior of these tests depend on the UnknownSizes parameter */
	test_connect_ext(extra_conn_options);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Get column attributes of a simple query.
	 */
	printf("Testing SQLColAttribute...\n");
	rc = SQLExecDirect(hstmt,
			(SQLCHAR *) "SELECT '1'::int AS intcol, 'foobar'::text AS textcol, 'unknown string' AS unknowncol, 'varchar string' as varcharcol, '' as empty_varchar_col",
			SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	for (i = 1 ; i <= 5; i++)
	{
		char buffer[64];
		SQLLEN number;

		rc = SQLColAttribute(hstmt, i, SQL_DESC_LABEL, buffer, 64, NULL, NULL);
		CHECK_STMT_RESULT(rc, "SQLColAttribute failed", hstmt);
		printf("\n-- Column %d: %s --\n", i, buffer);

		rc = SQLColAttribute(hstmt, i, SQL_DESC_OCTET_LENGTH, NULL, SQL_IS_INTEGER, NULL, &number);
		CHECK_STMT_RESULT(rc, "SQLColAttribute failed", hstmt);
		printf("SQL_DESC_OCTET_LENGTH: %d\n", (int) number);
	}

	/* Clean up */
	test_disconnect();
}

int main(int argc, char **argv)
{

	/*
	 * The output of these tests depend on the UnknownSizes and
	 * MaxVarcharSize parameters
	 */
	runtest("UnknownSizes=-1;MaxVarcharSize=100");
	runtest("UnknownSizes=0;MaxVarcharSize=100");
	runtest("UnknownSizes=1;MaxVarcharSize=100");
	runtest("UnknownSizes=2;MaxVarcharSize=100");
	runtest("UnknownSizes=100;MaxVarcharSize=100");

	return 0;
}
