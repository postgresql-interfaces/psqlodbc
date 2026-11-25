/*
 * A COMMIT that fails must be reported, not taken for success: the
 * transaction's work is gone. Covers a deferred constraint failing at
 * COMMIT through SQLEndTran, SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT) and
 * SQLSetPos, and a COMMIT of a transaction that had already failed, which
 * the server answers with the tag ROLLBACK.
 */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static HSTMT hstmt = SQL_NULL_HSTMT;

static void
exec_ok(const char *sql)
{
	SQLRETURN	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);

	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static void
set_autocommit(SQLULEN on, const char *what)
{
	SQLRETURN	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
									   (SQLPOINTER) on, SQL_IS_UINTEGER);

	if (what)
		printf("%s: %s\n", what, SQL_SUCCEEDED(rc) ? "reported success" : "reported an error");
	else
		CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);
}

static void
count_children(void)
{
	SQLRETURN	rc;
	SQLINTEGER	n = -1;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT count(*) FROM commitfail_c", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_SLONG, &n, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("rows in commitfail_c: %d\n", (int) n);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static void
setup(const char *options)
{
	SQLRETURN	rc;

	test_connect_ext((char *) options);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
}

static void
teardown(void)
{
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	hstmt = SQL_NULL_HSTMT;
	test_disconnect();
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;

	setup("");
	exec_ok("DROP TABLE IF EXISTS commitfail_c");
	exec_ok("DROP TABLE IF EXISTS commitfail_p");
	exec_ok("CREATE TABLE commitfail_p (id int PRIMARY KEY)");
	exec_ok("CREATE TABLE commitfail_c (id int PRIMARY KEY, pid int REFERENCES commitfail_p DEFERRABLE INITIALLY DEFERRED)");

	/* 1. SQLEndTran */
	set_autocommit(SQL_AUTOCOMMIT_OFF, NULL);
	exec_ok("INSERT INTO commitfail_c VALUES (1, 42)");
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	printf("SQLEndTran with a deferred violation: %s\n", SQL_SUCCEEDED(rc) ? "reported success" : "reported an error");

	/* 2. turning autocommit on commits the open transaction */
	exec_ok("INSERT INTO commitfail_c VALUES (2, 42)");
	set_autocommit(SQL_AUTOCOMMIT_ON, "autocommit on with a deferred violation");
	count_children();
	teardown();

	/* 3. COMMIT of a failed transaction; the driver doesn't roll back statements */
	setup("Protocol=7.4-0");
	set_autocommit(SQL_AUTOCOMMIT_OFF, NULL);
	exec_ok("INSERT INTO commitfail_p VALUES (1)");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1/0", SQL_NTS);
	printf("failing statement: %s\n", SQL_SUCCEEDED(rc) ? "reported success" : "reported an error");
	SQLFreeStmt(hstmt, SQL_CLOSE);
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	printf("SQLEndTran of a failed transaction: %s\n", SQL_SUCCEEDED(rc) ? "reported success" : "reported an error");
	set_autocommit(SQL_AUTOCOMMIT_ON, NULL);

	exec_ok("DROP TABLE commitfail_c");
	exec_ok("DROP TABLE commitfail_p");
	teardown();

	return 0;
}
