/*
 * Tests for the existing behaviors of rollback on errors:
 * 0 -> Do nothing and let the application do it
 * 1 -> Rollback the entire transaction
 * 2 -> Rollback only the statement
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

HSTMT hstmt = SQL_NULL_HSTMT;

static void
error_rollback_init(char *options)
{
	SQLRETURN rc;

	/* Error if initialization is already done */
	if (hstmt != SQL_NULL_HSTMT)
	{
		printf("Initialization already done, leaving...\n");
		exit(1);
	}

	test_connect_ext(options);
	rc = SQLAllocStmt(conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Disable autocommit */
	rc = SQLSetConnectAttr(conn,
						   SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER)SQL_AUTOCOMMIT_OFF,
						   SQL_IS_UINTEGER);

	/* Create a table to use */
	rc = SQLExecDirect(hstmt,
			   (SQLCHAR *) "CREATE TEMPORARY TABLE errortab (i int4)",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* And of course commit... */
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_STMT_RESULT(rc, "SQLEndTran failed", hstmt);
}

static void
error_rollback_clean(void)
{
	SQLRETURN rc;

	/* Clean up everything */
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	test_disconnect();
	hstmt = SQL_NULL_HSTMT;
}

static void
error_rollback_exec_success(int arg)
{
	SQLRETURN rc;
	char buf[100];

	printf("Executing query that will succeed\n");

	/* Now execute the query */
	snprintf(buf, sizeof(buf), "INSERT INTO errortab VALUES (%d)", arg);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) buf, SQL_NTS);

	/* Print error if any, but do not exit */
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
}

/* Runs a query that's expected to fail */
static void
error_rollback_exec_failure(int arg)
{
	SQLRETURN rc;
	char buf[100];

	printf("Executing query that will fail\n");

	snprintf(buf, sizeof(buf), "INSERT INTO errortab VALUES ('fail%d')", arg);

	/* Now execute the query */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) buf, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
	{
		printf("SQLExecDirect should have failed but it succeeded\n");
		exit(1);
	}

	/* Print error, it is expected */
	print_diag("Failed to execute statement", SQL_HANDLE_STMT, hstmt);
}

/*
 * Runs another query that's expected to fail.
 *
 * This query uses the ODBC procedure call escape syntax, because such queries
 * go through a slightly different execution path in the driver.
 */
void
error_rollback_exec_proccall_failure(void)
{
	SQLRETURN rc;

	printf("Executing procedure call that will fail\n");

	/* Now execute the query */
	rc = SQLExecDirect(hstmt,
					   (SQLCHAR *) "{ call invalidfunction() }",
					   SQL_NTS);
	if (SQL_SUCCEEDED(rc))
	{
		printf("SQLExecDirect should have failed but it succeeded\n");
		exit(1);
	}

	/* Print error, it is expected */
	print_diag("Failed to execute procedure call", SQL_HANDLE_STMT, hstmt);
}

void
error_rollback_print(void)
{
	SQLRETURN rc;

	/* Create a table to use */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT i FROM errortab", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Show results */
	print_result(hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN rc;

	/*
	 * Test for protocol at 0.
	 * Do nothing when error occurs and let application do necessary
	 * ROLLBACK on error.
	 */
	printf("Test for rollback protocol 0\n");
	error_rollback_init("Protocol=7.4-0");

	/* Insert a row correctly */
	error_rollback_exec_success(1);

	/* Now trigger an error, the row previously inserted will disappear */
	error_rollback_exec_failure(1);

	/*
	 * Now rollback the transaction block, it is the responsibility of
	 * application.
	 */
	printf("Rolling back with SQLEndTran\n");
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_STMT_RESULT(rc, "SQLEndTran failed", hstmt);

	/* Insert row correctly now */
	error_rollback_exec_success(1);

	/* Not yet committed... */
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_STMT_RESULT(rc, "SQLEndTran failed", hstmt);

	/* Print result */
	error_rollback_print();

	/* Clean up */
	error_rollback_clean();

	/*
	 * Test for rollback protocol 1
	 * In case of an error rollback the entire transaction.
	 */
	printf("Test for rollback protocol 1\n");
	error_rollback_init("Protocol=7.4-1");

	/*
	 * Insert a row, trigger an error, and re-insert a row. Only one
	 * row should be visible here.
	 */
	error_rollback_exec_success(1);
	error_rollback_exec_failure(1);
	error_rollback_exec_success(1);
	error_rollback_print();

	/* Clean up */
	error_rollback_clean();

	/*
	 * Test for rollback protocol 2
	 * In the case of an error rollback only the latest statement.
	 */
	printf("Test for rollback protocol 2\n");
	error_rollback_init("Protocol=7.4-2");

	/*
	 * Do a bunch of insertions and failures.
	 */
	error_rollback_exec_success(1);
	error_rollback_exec_success(2);
	error_rollback_exec_failure(-1);
	error_rollback_exec_success(3);
	error_rollback_exec_success(4);
	error_rollback_exec_failure(-1);
	error_rollback_exec_failure(-1);
	error_rollback_exec_success(5);
	error_rollback_exec_proccall_failure();
	error_rollback_exec_success(6);
	error_rollback_exec_success(7);
	error_rollback_print();

	/* Clean up */
	error_rollback_clean();

	return 0;
}
