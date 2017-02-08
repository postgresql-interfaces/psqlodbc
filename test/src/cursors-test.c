#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
testLargeResult(HSTMT hstmt, int betweenstmts)
{
	int rc;
	int i;

	rc = SQLSetConnectAttr(conn, SQL_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
	CHECK_CONN_RESULT(rc, "SQL_AUTOCOMMIT off failed", conn);
	/*
	 * Fetch a large result set without cursor (in Declare/fetch mode, it will
	 * be fetched in chunks)
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'foo' || g FROM generate_series(1, 3210) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Fetch the first 10 rows */
	for (i = 0; i < 10; i++)
	{
		char buf[40];
		SQLLEN ind;

		rc = SQLFetch(hstmt);
		if (rc != SQL_SUCCESS)
		{
			CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
		}

		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("%s ", buf);
	}

	/* Now Commit, Rollback or do nothing depending on the argument */
	switch (betweenstmts)
	{
		case 1:
			printf("\nCommit\n");
			rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
			CHECK_STMT_RESULT(rc, "SQLEndTran failed\n", hstmt);
			break;

		case 2:
			printf("\nRollback\n");
			rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
			CHECK_STMT_RESULT(rc, "SQLEndTran failed\n", hstmt);
			break;
		default:
			/* do nothing */
			break;
	}

	/*
	 * Try to fetch the rest of the result set.
	 * (Will fail in SQL_CB_CLOSE mode and succeed in SQL_CB_PRESERVE).
	 */
	for (;; i++)
	{
		char buf[40];
		SQLLEN ind;

		rc = SQLFetch(hstmt);
		if (rc == SQL_NO_DATA)
			break;
		if (rc != SQL_SUCCESS)
		{
			char sqlstate[32] = "";
			SQLINTEGER nativeerror;
			SQLSMALLINT textlen;

			SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, &nativeerror,
						  NULL, 0, &textlen);
			if (strcmp(sqlstate, "HY010") == 0)
			{
				/* This is expected in SQL_CB_CLOSE mode */
				printf("SQLFetch failed with HY010 (which probably means that the cursor was closed at commit/rollback)");
				break;
			}
			else
				CHECK_STMT_RESULT(rc, "SQLGetDiagRec failed", hstmt);
		}

		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		if (i == 20)
			printf("... ");
		else if (20 < i && i <= 3205)
		{
			/* skip printing, to keep the output short */
		}
		else
			printf("%s ", buf);
	}
	printf("\nFetched %d rows altogether\n\n", i);

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

	/* Run three variations of the test */
	testLargeResult(hstmt, 0);
	testLargeResult(hstmt, 1);
	testLargeResult(hstmt, 2);

	/* Clean up */
	test_disconnect();

	return 0;
}
