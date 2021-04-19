/*
 * INSERT RETURNING tests.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void run_test();

int main(int argc, char **argv)
{
	/*
	 * SQLNumResultCols(), when run after SQLPrepare but before SQLExecute,
	 * return 0 in UseServerSidePrepare=0 mode. Run the test in both modes,
	 * mainly to make the output predictable regardless of the default
	 * UseServerSidePrepare mode in effect.
	 */
	printf("Testing with UseServerSidePrepare=1\n");
	test_connect_ext("UseServerSidePrepare=1");
	run_test();
	test_disconnect();

	printf("Testing with UseServerSidePrepare=0\n");
	test_connect_ext("UseServerSidePrepare=0");
	run_test();
	test_disconnect();

	return 0;
}

static void
run_test()
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char param1[100];
	int param2;
	SQLLEN cbParam1;
	SQLSMALLINT colcount1, colcount2;
	SQLCHAR *sql;
	int i;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	sql = "CREATE TEMPORARY TABLE tmptable (i int4, t text)";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while creating temp table", hstmt);

	/*
	 * We used to have a memory leak when SQLNumResultCols() was called on an
	 * INSERT statement. It's been fixed, but this test case was useful to find
	 * it, when you crank up the number of iterations.
	 */
	for (i = 0; i < 100; i++)
	{
		/* Prepare a statement */
		rc = SQLPrepare(hstmt, (SQLCHAR *) "INSERT INTO tmptable VALUES (1, ?) RETURNING (t)", SQL_NTS);
		CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

		/* bind param  */
		snprintf(param1, sizeof(param1), "foobar %d", i);
		cbParam1 = SQL_NTS;
		rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
							  SQL_C_CHAR,	/* value type */
							  SQL_CHAR,	/* param type */
							  20,		/* column size */
							  0,		/* dec digits */
							  param1,		/* param value ptr */
							  0,		/* buffer len */
							  &cbParam1	/* StrLen_or_IndPtr */);
		CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

		/* Test SQLNumResultCols, called before SQLExecute() */
		rc = SQLNumResultCols(hstmt, &colcount1);
		CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);

		/* Execute */
		rc = SQLExecute(hstmt);
		CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

		/* Call SQLNumResultCols again, after SQLExecute() */
		rc = SQLNumResultCols(hstmt, &colcount2);
		CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);

		printf("# of result cols before SQLExecute: %d, after: %d\n",
			   colcount1, colcount2);

		/* Fetch result */
		print_result(hstmt);

		rc = SQLFreeStmt(hstmt, SQL_CLOSE);
		CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	}

	/* Test for UPDATE returning */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "TRUNCATE TABLE tmptable", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while truncating temp table", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO tmptable values(3,'foo')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while nserting temp table", hstmt);
	/* Prepare a UPDATE statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "UPDATE tmptable set t=? where i=? RETURNING (t)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare for UPDATE failed", hstmt);
	/* bind params  */
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,	/* param type */
						  20,		/* column size */
						  0,		/* dec digits */
						  param1,		/* param value ptr */
						  0,		/* buffer len */
						  &cbParam1	/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter 1 failed", hstmt);
	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT,
						  SQL_C_SLONG,	/* value type */
						  SQL_INTEGER,	/* param type */
						  0,		/* column size */
						  0,		/* dec digits */
						  &param2,	/* param value ptr */
						  0,		/* buffer len */
						  NULL		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter 2 failed", hstmt);
	for (i = 0; i < 4;i++)
	{
		snprintf(param1, sizeof(param1), "foobar %d", i);
		cbParam1 = SQL_NTS;
		param2 = i;
		rc = SQLExecute(hstmt);
		if (SQL_SUCCEEDED(rc))
		{
			rc = SQLFetch(hstmt);
			if (SQL_NO_DATA == rc)
				printf("??? SQLFetch for UPDATE not found key i=%d\n", param2);
			else if (SQL_SUCCEEDED(rc))
				printf("SQLExecute UPDATE key i=%d value t=%s\n", param2, param1);
			else
				CHECK_STMT_RESULT(rc, "SQLFetch for UPDATE failed", hstmt);
			SQLFreeStmt(hstmt, SQL_CLOSE);
		}
		else if (SQL_NO_DATA == rc)
			printf("SQLExecute UPDATE not found key i=%d\n", param2);
		else
			CHECK_STMT_RESULT(rc, "SQLExecute UPDATE failed", hstmt);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeHandle failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}
}
