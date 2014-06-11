#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
runTest(HSTMT hstmt)
{
	int			rc;
	SQLINTEGER	intparam;
	SQLLEN		cbParam1;

	/**** A simple WITH-query ****/

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "with recursive cte as (select g, 'foo' || g as foocol from generate_series(1,10) as g) select * from cte;", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/**** Same with SQLPrepare/SQLExecute and an integer param ****/

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "with cte as (select g, 'foo' || g as foocol from generate_series(1,10) as g) select * from cte WHERE g < ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* bind param  */
	intparam = 3;
	cbParam1 = sizeof(intparam);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_INTEGER,	/* value type */
						  SQL_INTEGER,	/* param type */
						  0,			/* column size (ignored for SQL_INTEGER) */
						  0,			/* dec digits */
						  &intparam,	/* param value ptr */
						  sizeof(intparam), /* buffer len (ignored for SQL_INTEGER) */
						  &cbParam1		/* StrLen_or_IndPtr (ignored for SQL_INTEGER) */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* Fetch result */
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	/**** Test WITH-queries in with and without  UseDeclareFetch. ****/
	/*
	 * This used to not work with older versions of the driver, because of
	 * a bug. It was not covered by any of the existing regression tests.
	 */
	test_connect_ext("UseDeclareFetch=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}
	runTest(hstmt);

	/* Clean up */
	test_disconnect();

	/**** And then the same with UseDeclareFetch = 1 ****/
	test_connect_ext("UseDeclareFetch=1;Fetch=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}
	runTest(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
