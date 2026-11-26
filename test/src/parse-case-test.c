/*
 * Test that the driver's query parser (Parse=1) resolves column names the
 * way the server does: an unquoted name is folded to lower case and then
 * compared exactly, a double-quoted name is compared as is.
 *
 * A table with the columns "READING" smallint and reading uuid tells the two
 * apart: SQLDescribeCol must describe the same column that SQLFetch later
 * returns, otherwise an application that sizes its buffer from the
 * description gets a 16-byte uuid written into a 2-byte buffer.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
describe(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc;

	printf("%s\n", sql);
	rc = SQLPrepare(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	print_result_meta(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect_ext("Parse=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* The decoy column comes first so that a loose match finds it */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE parse_case_tab "
		"(\"READING\" smallint, reading uuid)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);

	/* unquoted: the lower case column, whatever the case written */
	describe(hstmt, "SELECT reading FROM parse_case_tab");
	describe(hstmt, "SELECT READING FROM parse_case_tab");
	describe(hstmt, "SELECT parse_case_tab.Reading FROM parse_case_tab");
	describe(hstmt, "SELECT \"parse_case_tab\".READING FROM parse_case_tab");
	describe(hstmt, "SELECT t.reading FROM parse_case_tab t");
	/* double-quoted: exactly that column */
	describe(hstmt, "SELECT \"reading\" FROM parse_case_tab");
	describe(hstmt, "SELECT \"READING\" FROM parse_case_tab");
	describe(hstmt, "SELECT parse_case_tab.\"READING\" FROM parse_case_tab");
	describe(hstmt, "SELECT T.\"READING\" FROM parse_case_tab T");

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
