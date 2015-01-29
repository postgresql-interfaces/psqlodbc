/*
 * This test case tests the case that a query needs to be described before
 * executing it. That happens when an application calls e.h. SQLDescribeCol()
 * or SQLNumResultCols() after preparing a query with SQLPrepare(), but
 * before calling SQLExecute().
 *
 * The driver used to have an option to do so-called "premature execution",
 * where it simply executed the driver prematurely, before the SQLExecute()
 * call. That was of course quite dangerous, if the query was not read-only,
 * and the application didn't follow through with the SQLExecute(). It could
 * be disabled with the DisallowPremature setting. The driver doesn't do
 * that anymore, but it's a good case to test.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
runtest(const char *query, char *bind_before, char *bind_after, int execute)
{
	SQLRETURN	rc;
	HSTMT		hstmt;
	SQLLEN		cbParam1;
	SQLSMALLINT colcount;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLPrepare(hstmt, (SQLCHAR *) query, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	if (bind_before)
	{
		cbParam1 = SQL_NTS;
		rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,		/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  bind_before,	/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
		CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	}

	/* Test SQLNumResultCols, called before SQLExecute() */
	rc = SQLNumResultCols(hstmt, &colcount);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);
	printf("# of result cols: %d\n", colcount);

	if (bind_after)
	{
		cbParam1 = SQL_NTS;
		rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,		/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  bind_after,	/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
		CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	}
	
	/* Don't execute the statement! The row should not be inserted. */

	/* And execute */
	if (execute)
	{
		rc = SQLExecute(hstmt);
		CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);
	}

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	/**** Set up a test table and function ****/

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE premature_test (t text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE OR REPLACE FUNCTION insertfunc (t text) RETURNS text AS "
		"$$ INSERT INTO premature_test VALUES ($1) RETURNING 'func insert'::text $$ "
		"LANGUAGE sql",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);


	/****
	 * First, try with a plain INSERT. Prepare the statement, run
	 * SQLNumResultCols() which forces the statement to be sent and parsed
	 * in the backend.
	 */
	printf("\nPreparing an INSERT statement\n");
	runtest("INSERT INTO premature_test VALUES (?) RETURNING 'plain insert'::text", "plain insert", NULL, 0);
	/* same with no parameter bound */
	runtest("INSERT INTO premature_test VALUES (?) RETURNING 'plain insert'::text", NULL, NULL, 0);

	/*** Now, do the same with the function ***/
	printf("\nPreparing an insert using a function\n");
	runtest("SELECT insertfunc(?)", "function insert", NULL, 0);
	runtest("SELECT insertfunc(?)", NULL, NULL, 0);

	/*** Same with the function, used in a multi-statement ***/

	printf("\nPreparing a multi-statement\n");
	runtest("SELECT 'foo', 2, 3; SELECT insertfunc(?), 2; SELECT 'bar'",
			"function insert in multi-statement", NULL, 0);
	runtest("SELECT 'foo', 2, 3; SELECT insertfunc(?), 2; SELECT 'bar'",
			NULL, NULL, 0);

	/*** Again with the function, but this time execute it too. With a
	 * twist: we rebind a different parameter after the SQLNumResultCols
	 * call.
	 */
	printf("\nPrepare with function, but execute with different param\n");
	runtest("SELECT insertfunc(?)", "function insert wrong",
			"function insert right", 1);

	/*** Now check that the table contains only the last insertion  ***/

	printf("\nChecking table contents. Should contain only one row.\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT * FROM premature_test", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
