/*
 * Test conversion of parameter values from C to SQL datatypes.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void test_convert(const char *sql, SQLUSMALLINT c_type,
						 SQLSMALLINT sql_type, char *value);

static HSTMT hstmt = SQL_NULL_HSTMT;

int main(int argc, char **argv)
{
	SQLRETURN rc;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*** Test proper escaping of parameters  ***/
	printf("\nTesting conversions...\n");

	test_convert("SELECT 1 > ?", SQL_C_CHAR, SQL_INTEGER, "2");
	test_convert("SELECT 1 > ?", SQL_C_CHAR, SQL_INTEGER, "-2");
	test_convert("SELECT 2.2 > ?", SQL_C_CHAR, SQL_FLOAT, "2.3");
	test_convert("SELECT 3.3 > ?", SQL_C_CHAR, SQL_DOUBLE, "3.01");
	test_convert("SELECT 1 > ?", SQL_C_CHAR, SQL_CHAR, "5 escapes: \\ and '");

	printf("\nTesting conversions with invalid values...\n");

	test_convert("SELECT 2 > ?", SQL_C_CHAR, SQL_INTEGER, "2, 'injected, BAD!'");
	test_convert("SELECT 1.3 > ?", SQL_C_CHAR, SQL_FLOAT, "3', 'injected, BAD!', '1");
	test_convert("SELECT 1.4 > ?", SQL_C_CHAR, SQL_FLOAT, "4 \\'bad', '1");
	test_convert("SELECT 1 > ?", SQL_C_CHAR, SQL_INTEGER, "99999999999999999999999");

	/* Clean up */
	test_disconnect();

	return 0;
}

/*
 * Execute a query with one parameter, with given C and SQL types. Print
 * error or result.
 */
static void
test_convert(const char *sql, SQLUSMALLINT c_type, SQLSMALLINT sql_type,
			  char *value)
{
	SQLRETURN rc;
	SQLLEN cbParam = SQL_NTS;
	int failed = 0;

	/* a query with an SQL_INTEGER param. */
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  c_type,	/* value type */
						  sql_type,	/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  value,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);
		failed = 1;
	}
	else
		print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * In error_on_rollback=0 mode, we don't currently recover from the error.
	 * I think that's a bug in the driver, but meanwhile, let's just force
	 * a rollback manually
	 */
	if (failed)
	{
		rc = SQLExecDirect(hstmt, (SQLCHAR *) "ROLLBACK /* clean up after failed test */", SQL_NTS);
		CHECK_STMT_RESULT(rc, "SQLExecDirect(ROLLBACK) failed", hstmt);
	}
}

