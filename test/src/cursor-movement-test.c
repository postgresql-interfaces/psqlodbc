#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
printFetchResult(HSTMT hstmt, int rc)
{
	if (SQL_SUCCEEDED(rc))
	{
		char buf[40];
		SQLLEN ind;

		if (rc == SQL_SUCCESS_WITH_INFO)
			print_diag("got SUCCESS_WITH_INFO", SQL_HANDLE_STMT, hstmt);

		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("fetched: %s\n", buf);
	}
	else if (rc == SQL_NO_DATA)
		printf("Fetch: no data found\n"); /* expected */
	else
		CHECK_STMT_RESULT(rc, "Fetch failed", hstmt);
}

static void
testLargeResult(HSTMT hstmt)
{
	int rc;
	int i;

	/* Make the cursor scrollable */
	rc = SQLSetStmtAttr(hstmt, SQL_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	/*
	 * Fetch a large result set without cursor (in Declare/fetch mode, it will
	 * be fetched in chunks)
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'foo' || g FROM generate_series(1, 3210) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Fetch the first 10 rows */
	for (i = 0; i < 10; i++)
	{
		rc = SQLFetch(hstmt);
		printFetchResult(hstmt, rc);
	}

	/* Test all the SQL_FETCH_* modes with SQLFetchScroll */
	printf("Testing SQL_FETCH_NEXT...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	printFetchResult(hstmt, rc);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_PRIOR...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_PRIOR, 0);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_ABSOLUTE (5)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, 5);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_RELATIVE (+2)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, 2);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_RELATIVE (-2)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, -2);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_RELATIVE (+1)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, 1);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_RELATIVE (0, no movement)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, 0);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_FIRST...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);
	printFetchResult(hstmt, rc);

	/*** Ok, now test the behavior at "before first row" ***/
	printf("Testing SQL_FETCH_PRIOR before first row...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_PRIOR, 0);
	printFetchResult(hstmt, rc);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_PRIOR, 0);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_NEXT...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	printFetchResult(hstmt, rc);

	/*** Test behavior at the end ***/

	printf("Testing SQL_FETCH_LAST...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_LAST, 0);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_LAST...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_LAST, 0);
	printFetchResult(hstmt, rc);


	printf("Testing SQL_FETCH_NEXT at the end of result set\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_NEXT at the end of result set\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	printFetchResult(hstmt, rc);

	printf("And SQL_FETCH_PRIOR...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_PRIOR, 0);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_RELATIVE (+10)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, 10);
	printFetchResult(hstmt, rc);

	printf("And SQL_FETCH_PRIOR...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_PRIOR, 0);
	printFetchResult(hstmt, rc);

	printf("Testing negative SQL_FETCH_ABSOLUTE (-5)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, -5);
	printFetchResult(hstmt, rc);

	printf("Testing negative SQL_FETCH_ABSOLUTE, before start...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, -10000);
	printFetchResult(hstmt, rc);

	printf("Testing  SQL_FETCH_ABSOLUTE, beoynd end...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, 10000);
	printFetchResult(hstmt, rc);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

static const char *
sql_commit_behavior_str(SQLUINTEGER info)
{
	static char buf[50];

	switch(info)
	{
		case SQL_CB_DELETE:
			return "SQL_CB_DELETE";

		case SQL_CB_CLOSE:
			return "SQL_CB_CLOSE";

		case SQL_CB_PRESERVE:
			return "SQL_CB_PRESERVE";

		default:
			sprintf(buf, "unknown (%u)", (unsigned int) info);
			return buf;
	}
}

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLUSMALLINT info;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Print out the current SQL_CURSOR_COMMIT_BEHAVIOR and
	 * SQL_CURSOR_ROLLBACK settings. The result of this test case depends on
	 * those.
	 */
	rc = SQLGetInfo(conn, SQL_CURSOR_COMMIT_BEHAVIOR, &info, sizeof(info), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetInfo failed", hstmt);
	printf("SQL_CURSOR_COMMIT_BEHAVIOR: %s\n", sql_commit_behavior_str(info));

	rc = SQLGetInfo(conn, SQL_CURSOR_ROLLBACK_BEHAVIOR, &info, sizeof(info), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetInfo failed", hstmt);
	printf("SQL_CURSOR_ROLLBACK_BEHAVIOR: %s\n\n", sql_commit_behavior_str(info));

	/* Run the test */
	testLargeResult(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
