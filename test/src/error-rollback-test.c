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

/*
 * Helpers for the error-class matrix below.
 *
 * The existing tests above cover just one error class (invalid integer input,
 * 22P02) via SQLExecDirect.  With statement rollback (Protocol=7.4-2) the
 * driver used to bundle "SAVEPOINT ...; <statement>" into a single simple-
 * query string, which fails as a unit at parse time on grammar errors -- so
 * a *syntax error* (42601) would silently roll back the whole transaction
 * instead of just the offending statement (issue #203).  The matrix below
 * exercises three error classes across ExecDirect and Prepare/Execute, and
 * asserts (via a marker row) that earlier work in the transaction survives.
 *
 * Errors always surface at SQLExecute time (not SQLPrepare) for the paths
 * this driver takes, so we only print the SQLSTATE, not where it fired.
 */

/* Extract the first SQLSTATE from a statement handle. */
static void
get_state(HSTMT s, char *out, size_t outlen)
{
	SQLCHAR		state[6] = {0};
	SQLINTEGER	native;
	SQLCHAR		msg[256];
	SQLSMALLINT	len;

	SQLGetDiagRec(SQL_HANDLE_STMT, s, 1, state,
				  &native, msg, sizeof(msg), &len);
	snprintf(out, outlen, "%s", (char *) state);
}

/* Run a statement expected to fail; print its SQLSTATE.  A different
 * SQLSTATE, or unexpected success, is flagged inline so `diff` catches it. */
static void
expect_fail(HSTMT s, int use_prepare, const char *label,
			const char *sql, const char *want_state)
{
	SQLRETURN	rc;
	char		state[8] = {0};

	if (use_prepare)
	{
		rc = SQLPrepare(s, (SQLCHAR *) sql, SQL_NTS);
		if (SQL_SUCCEEDED(rc))
			rc = SQLExecute(s);
	}
	else
		rc = SQLExecDirect(s, (SQLCHAR *) sql, SQL_NTS);

	if (SQL_SUCCEEDED(rc))
	{
		printf("%s: UNEXPECTED SUCCESS\n", label);
		SQLFreeStmt(s, SQL_CLOSE);
		return;
	}
	get_state(s, state, sizeof(state));
	printf("%s: SQLSTATE=%s", label, state);
	if (strcmp(state, want_state) != 0)
		printf(" [MISMATCH want=%s]", want_state);
	SQLFreeStmt(s, SQL_CLOSE);
}

/* After a failed statement, verify the marker row inserted earlier in the
 * same transaction is still visible (statement-level rollback worked) and
 * that the transaction is still usable. */
static void
check_survival(HSTMT s)
{
	SQLRETURN	rc;
	SQLCHAR		buf[32];
	SQLLEN		ind;

	rc = SQLExecDirect(s, (SQLCHAR *)
					   "SELECT count(*) FROM errortab WHERE i=100",
					   SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		printf(", marker=<count query failed>, tx=ABORTED\n");
		SQLFreeStmt(s, SQL_CLOSE);
		return;
	}
	if (SQLFetch(s) != SQL_SUCCESS)
	{
		printf(", marker=<fetch failed>\n");
		SQLFreeStmt(s, SQL_CLOSE);
		return;
	}
	SQLGetData(s, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
	printf(", marker=%s", (char *) buf);
	SQLFreeStmt(s, SQL_CLOSE);

	rc = SQLExecDirect(s, (SQLCHAR *) "SELECT 1", SQL_NTS);
	printf(", tx=%s\n", SQL_SUCCEEDED(rc) ? "alive" : "ABORTED");
	SQLFreeStmt(s, SQL_CLOSE);
}

/* Run one case: insert marker row, fail the given statement, then verify
 * the marker row survives.  Each case rolls back at the end so the next
 * case starts clean. */
static void
run_case(int use_prepare, const char *label,
		 const char *sql, const char *want_state)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
					   "INSERT INTO errortab VALUES (100)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "marker insert failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	expect_fail(hstmt, use_prepare, label, sql, want_state);
	check_survival(hstmt);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_STMT_RESULT(rc, "SQLEndTran (case) failed", hstmt);
}

/* One matrix run: three error classes x two execution paths. */
static void
run_matrix(const char *options, const char *header)
{
	printf("%s\n", header);
	error_rollback_init((char *) options);

	run_case(0, "  ExecDirect syntax    ",
			 "INSERT INTO errortab VALUS (1)", "42601");
	run_case(1, "  Prepare    syntax    ",
			 "INSERT INTO errortab VALUS (1)", "42601");
	run_case(0, "  ExecDirect undef-rel ",
			 "INSERT INTO no_such_tbl VALUES (1)", "42P01");
	run_case(1, "  Prepare    undef-rel ",
			 "INSERT INTO no_such_tbl VALUES (1)", "42P01");
	run_case(0, "  ExecDirect bad-value ",
			 "INSERT INTO errortab VALUES ('nope')", "22P02");
	run_case(1, "  Prepare    bad-value ",
			 "INSERT INTO errortab VALUES ('nope')", "22P02");

	error_rollback_clean();
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

	/*
	 * Error-class matrix under Protocol=7.4-2.
	 *
	 * Each case inserts a marker row, then runs a statement expected to fail
	 * with a particular SQLSTATE, then asserts the marker row is still there
	 * (statement rollback worked) and the transaction is still usable.
	 *
	 * The syntax-error (42601) rows are the ones that broke pre-issue-#203
	 * fix: on ExecDirect+SSP=0, Prepare+SSP=0 and ExecDirect+SSP=1 the whole
	 * transaction was silently aborted, so the marker row disappeared.
	 * The other classes and Prepare+SSP=1 have always worked; they're
	 * included as regression guards.
	 */
	run_matrix("Protocol=7.4-2;UseServerSidePrepare=0",
			   "Test for rollback protocol 2 error-class matrix (SSP=0)");
	run_matrix("Protocol=7.4-2;UseServerSidePrepare=1",
			   "Test for rollback protocol 2 error-class matrix (SSP=1)");

	return 0;
}
