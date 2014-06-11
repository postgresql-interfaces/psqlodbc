#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQL_INTERVAL_STRUCT intervalval;
	char *sql;

	char buf[40];
	SQLINTEGER ld;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * The interval stuff requires intervalstyle=postgres at the momemnt.
	 * Someone should fix the driver to understand other formats,
	 * postgres_verbose in particular...
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SET intervalstyle=postgres", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Run a query with a result set with all kinds of values */
	sql = "SELECT "
		"'foo'::varchar(10) AS varcharcol,\n"
		"123::integer as integercol,\n"
		"'10 years'::interval AS intervalyears,\n"
		"'11 months'::interval AS intervalmonths,\n"
		"'12 days'::interval AS intervaldays,\n"
		"'1 ' || repeat('evil_long_string',100) || ' 2 still_evil'::text AS evil_interval\n";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Fetch result */

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("varcharcol: %s\n", buf);

	rc = SQLGetData(hstmt, 2, SQL_C_LONG, &ld, sizeof(ld), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("integercol: %ld\n", (long) ld);

	rc = SQLGetData(hstmt, 3, SQL_C_INTERVAL_YEAR, &intervalval, sizeof(intervalval), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("intervalyears: %ld\n", (long) intervalval.intval.year_month.year);

	rc = SQLGetData(hstmt, 4, SQL_C_INTERVAL_MONTH, &intervalval, sizeof(intervalval), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("intervalmonths: %ld\n", (long) intervalval.intval.year_month.month);

	rc = SQLGetData(hstmt, 5, SQL_C_INTERVAL_DAY, &intervalval, sizeof(intervalval), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("intervaldays: %ld\n", (long) intervalval.intval.day_second.day);

	rc = SQLGetData(hstmt, 6, SQL_C_INTERVAL_DAY, &intervalval, sizeof(intervalval), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("bogus long string as interval: %ld\n", (long) intervalval.intval.day_second.day);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
