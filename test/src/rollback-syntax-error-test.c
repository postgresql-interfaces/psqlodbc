/*
 * Regression test for issue #203.
 *
 * With statement-level rollback (Protocol 7.4-2), a *syntax* error must roll
 * back only the offending statement, leaving earlier work in the transaction
 * intact -- exactly like an execution-time error already does.
 *
 * The driver used to bundle "SAVEPOINT ...; <user statement>" into a single
 * simple-query string.  A syntax error fails that whole string at parse time,
 * so the SAVEPOINT never executed and the driver fell back to rolling back the
 * entire transaction -- silently discarding earlier work (see the issue: a
 * table lock obtained earlier was lost).  An execution-time error (e.g. a
 * missing relation) did not hit this because the SAVEPOINT executed first.
 *
 * This test inserts a row, triggers each kind of error, and checks that the
 * row is still visible afterwards (statement rolled back, transaction alive).
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static HSTMT hstmt = SQL_NULL_HSTMT;

/* Execute a statement expected to fail; print only its SQLSTATE (stable
 * across server versions, unlike the message text). */
static void
exec_expect_error(const char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
	{
		printf("  ERROR: statement unexpectedly succeeded: %s\n", sql);
		return;
	}
	{
		SQLCHAR		state[6];
		SQLINTEGER	native;
		SQLCHAR		msg[512];
		SQLSMALLINT	len;

		if (SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, state,
										&native, msg, sizeof(msg), &len)))
			printf("  expected error, SQLSTATE=%s\n", state);
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static void
exec_ok(const char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

/* Print how many rows are currently visible in rbtab. */
static void
show_rowcount(const char *label)
{
	SQLRETURN	rc;
	SQLCHAR		buf[32];
	SQLLEN		ind;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT count(*) FROM rbtab", SQL_NTS);
	CHECK_STMT_RESULT(rc, "count query failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "count fetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
	CHECK_STMT_RESULT(rc, "count getdata failed", hstmt);
	printf("  %s: %s\n", label, (char *) buf);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;

	test_connect_ext("Protocol=7.4-2");

	rc = SQLAllocStmt(conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetConnectAttr failed", hstmt);

	/* A committed temp table gives us a clean, session-local slate. */
	exec_ok("CREATE TEMPORARY TABLE rbtab (i int4)");
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_STMT_RESULT(rc, "SQLEndTran (create) failed", hstmt);

	/*
	 * Case 1: syntax error (parse-time).  This is the issue #203 case.
	 * The row inserted before the error must survive.
	 */
	printf("Case 1: syntax error must roll back only the statement\n");
	exec_ok("INSERT INTO rbtab VALUES (100)");
	exec_expect_error("INSERT INTO rbtab VALUS (101)");   /* typo -> 42601 */
	show_rowcount("rows visible after syntax error");

	/*
	 * Case 2: execution-time error (missing relation).  Same expectation;
	 * this path already worked, so it guards against regressions.
	 */
	printf("Case 2: execution-time error rolls back only the statement\n");
	exec_ok("INSERT INTO rbtab VALUES (200)");
	exec_expect_error("INSERT INTO no_such_table_xyz VALUES (201)"); /* 42P01 */
	show_rowcount("rows visible after exec-time error");

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_STMT_RESULT(rc, "SQLEndTran (rollback) failed", hstmt);

	test_disconnect();

	return 0;
}
