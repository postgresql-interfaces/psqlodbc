/*
 * Test conversion of parameter values from C to SQL datatypes.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define TEST_CONVERT(sql, c_type, sql_type, value) \
	test_convert(sql, c_type, #c_type, sql_type, #sql_type, value)

static void test_convert(const char *sql,
						 SQLSMALLINT c_type, const char *c_type_str,
						 SQLSMALLINT sql_type, const char *sql_type_str,
						 SQLPOINTER value);

static HSTMT hstmt = SQL_NULL_HSTMT;

int main(int argc, char **argv)
{
	SQLRETURN rc;
	SQLINTEGER intparam;
	char	byteaparam[] = { 'f', 'o', 'o', '\n', '\\', 'b', 'a', 'r', '\0' };

	/*
	 * let's not confuse the output with LF conversions. There's a separate
	 * regression test for that.
	 */
	test_connect_ext("LFConversion=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*** Test proper escaping of integer parameters  ***/
	printf("\nTesting conversions...\n");

	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_INTEGER, "2");
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_INTEGER, "-2");
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_SMALLINT, "2");
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_SMALLINT, "-2");
	TEST_CONVERT("SELECT 2.2 > ?", SQL_C_CHAR, SQL_FLOAT, "2.3");
	TEST_CONVERT("SELECT 3.3 > ?", SQL_C_CHAR, SQL_DOUBLE, "3.01");
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_CHAR, "5 escapes: \\ and '");

	/* test boundary cases */
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_SMALLINT, "32767");
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_SMALLINT, "-32768");

	/*
	 * The result of these depend on whether the server treats the parameters
	 * as a string or an integer.
	 */
	printf("\nTesting conversions whose result depend on whether the\n");
	printf("parameter is treated as a string or an integer...\n");
	TEST_CONVERT("SELECT '555' > ?", SQL_C_CHAR, SQL_INTEGER, "6");
	TEST_CONVERT("SELECT '555' > ?", SQL_C_CHAR, SQL_SMALLINT, "6");
	TEST_CONVERT("SELECT '555' > ?", SQL_C_CHAR, SQL_CHAR, "6");

	/*
	 * The result of this test depends on what datatype the server thinks
	 * it's dealing with. If the driver sends it as a naked literal, the
	 * server will treat it as a numeric because it doesn't fit in an int4.
	 * But if the driver tells the server what the datatype is, int4, the
	 * server will throw an error. In either case, this isn't something that
	 * a correct application should be doing, because it's clearly not a
	 * valid value for an SQL_INTEGER. But it's an interesting edge case to
	 * test.
	 */
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_INTEGER, "99999999999999999999999");

	printf("\nTesting conversions with invalid values...\n");

	TEST_CONVERT("SELECT 2 > ?", SQL_C_CHAR, SQL_INTEGER, "2, 'injected, BAD!'");
	TEST_CONVERT("SELECT 2 > ?", SQL_C_CHAR, SQL_SMALLINT, "2, 'injected, BAD!'");
	TEST_CONVERT("SELECT 1.3 > ?", SQL_C_CHAR, SQL_FLOAT, "3', 'injected, BAD!', '1");
	TEST_CONVERT("SELECT 1.4 > ?", SQL_C_CHAR, SQL_FLOAT, "4 \\'bad', '1");
	TEST_CONVERT("SELECT 1-?", SQL_C_CHAR, SQL_INTEGER, "-1");
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_INTEGER, "-");
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_INTEGER, "");
	TEST_CONVERT("SELECT 1-?", SQL_C_CHAR, SQL_SMALLINT, "-1");
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_SMALLINT, "-");
	TEST_CONVERT("SELECT 1 > ?", SQL_C_CHAR, SQL_SMALLINT, "");

	intparam = 1234;
	TEST_CONVERT("SELECT 0-?", SQL_C_SLONG, SQL_INTEGER, &intparam);
	intparam = -1234;
	TEST_CONVERT("SELECT 0-?", SQL_C_SLONG, SQL_INTEGER, &intparam);

	intparam = 1234;
	TEST_CONVERT("SELECT 0-?", SQL_C_SLONG, SQL_SMALLINT, &intparam);
	intparam = -1234;
	TEST_CONVERT("SELECT 0-?", SQL_C_SLONG, SQL_SMALLINT, &intparam);

	printf("\nTesting bytea conversions\n");
	TEST_CONVERT("SELECT ?", SQL_C_BINARY, SQL_BINARY, byteaparam);
	TEST_CONVERT("SELECT ?", SQL_C_CHAR, SQL_BINARY, "666f6f0001");
	TEST_CONVERT("SELECT ?::text", SQL_C_BINARY, SQL_CHAR, byteaparam);

	printf("\nTesting datetime conversions\n");
	TEST_CONVERT("SELECT ?", SQL_C_CHAR, SQL_TIMESTAMP, "04-22-2011 01:23:45");
	TEST_CONVERT("SELECT ?", SQL_C_CHAR, SQL_TIMESTAMP, "{ts '2011-04-22 01:23:45'}");
	TEST_CONVERT("SELECT ?", SQL_C_CHAR, SQL_TIME, "{t '01:23:45'}");
	TEST_CONVERT("SELECT ?", SQL_C_CHAR, SQL_DATE, "{d '2011-04-22'}");

	/* Clean up */
	test_disconnect();

	return 0;
}

/*
 * Execute a query with one parameter, with given C and SQL types. Print
 * error or result.
 */
static void
test_convert(const char *sql,
			 SQLSMALLINT c_type, const char *c_type_str,
			 SQLSMALLINT sql_type, const char *sql_type_str,
			 SQLPOINTER value)
{
	SQLRETURN	rc;
	SQLLEN		cbParam = SQL_NTS;
	int			failed = 0;

	/* Print what we're doing */
	switch (c_type)
	{
		case SQL_C_SLONG:
			printf("Testing \"%s\" with %s -> %s param %d...\n",
				   sql, c_type_str, sql_type_str, *((SQLINTEGER *) value));
			break;

		case SQL_C_CHAR:
			printf("Testing \"%s\" with %s -> %s param \"%s\"...\n",
				   sql, c_type_str, sql_type_str, (char *) value);
			break;

		default:
			printf("Testing \"%s\" with %s -> %s param...\n",
				   sql, c_type_str, sql_type_str);
			break;
	}

	if (c_type == SQL_BINARY)
		cbParam = strlen(value) + 1;
	else
		cbParam = SQL_NTS; /* ignored for non-character data */

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

	printf("\n");
}

