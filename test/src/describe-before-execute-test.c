/*
 * Test describing a prepared statement before SQLExecute when the driver
 * substitutes the parameters itself (UseServerSidePrepare=0). The describe
 * runs the statement with NULL in place of each parameter, and the NULL
 * must be cast to the type of the literal SQLExecute writes: a fetch of a
 * column bound as SQL_C_DEFAULT is refused with 07006 when the column's
 * type at fetch time is not the described one, because the application
 * sized its buffer for the described type.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static HSTMT
prepare_select_param(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	return hstmt;
}

/*
 * Describe the single result column before execution and print the
 * reported SQL type.
 */
static void
describe_column(HSTMT hstmt, const char *label)
{
	SQLRETURN	rc;
	SQLCHAR		colname[64];
	SQLSMALLINT	namelen;
	SQLSMALLINT	datatype;
	SQLULEN		colsize;
	SQLSMALLINT	decdigits;
	SQLSMALLINT	nullable;

	rc = SQLDescribeCol(hstmt, 1, colname, sizeof(colname), &namelen,
						&datatype, &colsize, &decdigits, &nullable);
	CHECK_STMT_RESULT(rc, "SQLDescribeCol failed", hstmt);
	printf("%s: described type %d\n", label, (int) datatype);
}

static void
execute_and_fetch(HSTMT hstmt)
{
	SQLRETURN	rc;

	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
}

static void
free_stmt(HSTMT hstmt)
{
	SQLRETURN	rc;

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt;
	SQLSMALLINT	short_param;
	double		double_param;
	SQLBIGINT	bigint_param;
	SQLINTEGER	int_value;
	double		double_value;
	char		char_value[64];
	SQLLEN		ind;

	test_connect_ext("UseServerSidePrepare=0");

	/*
	 * A valid SQL_SMALLINT value is written without quotes or a cast, and
	 * the server reads it as int4, so the describe must report SQL_INTEGER.
	 */
	hstmt = prepare_select_param();
	short_param = 5;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SSHORT,
						  SQL_SMALLINT, 0, 0, &short_param, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	describe_column(hstmt, "smallint param");
	execute_and_fetch(hstmt);
	int_value = 0;
	rc = SQLGetData(hstmt, 1, SQL_C_DEFAULT, &int_value, sizeof(int_value), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("smallint param: default fetch gives %ld\n", (long) int_value);
	free_stmt(hstmt);

	/* An SQL_DOUBLE value is written with a ::float8 cast. */
	hstmt = prepare_select_param();
	double_param = 1.25;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
						  SQL_DOUBLE, 0, 0, &double_param, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	describe_column(hstmt, "double param");
	execute_and_fetch(hstmt);
	double_value = 0;
	rc = SQLGetData(hstmt, 1, SQL_C_DEFAULT, &double_value, sizeof(double_value), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("double param: default fetch gives %g\n", double_value);
	free_stmt(hstmt);

	/*
	 * An SQL_BIGINT value is written as a quoted literal without a cast,
	 * so the column comes back as text, in the describe and in the
	 * execution both. The reported type of text depends on the driver
	 * options, so only the fetched value is printed.
	 */
	hstmt = prepare_select_param();
	bigint_param = (SQLBIGINT) 5000000000LL;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
						  SQL_BIGINT, 0, 0, &bigint_param, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	execute_and_fetch(hstmt);
	char_value[0] = '\0';
	rc = SQLGetData(hstmt, 1, SQL_C_DEFAULT, char_value, sizeof(char_value), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("bigint param: default fetch gives %s\n", char_value);
	free_stmt(hstmt);

	test_disconnect();

	return 0;
}
