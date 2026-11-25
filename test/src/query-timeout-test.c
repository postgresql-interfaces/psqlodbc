/*
 * Test SQL_ATTR_QUERY_TIMEOUT.
 *
 * The driver implements the timeout with "SET statement_timeout", which is
 * transactional: a ROLLBACK undoes it and a COMMIT keeps it. The driver
 * must notice both. Believing the session has a timeout a rollback undid,
 * or that a committed timeout is gone, makes it skip the SET a statement
 * needs.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

/*
 * Execute a statement that takes longer than the timeout, and print
 * whether it was cancelled.
 */
static void
run_slow_query(HSTMT hstmt, const char *label)
{
	SQLRETURN	rc;
	char		sqlstate[32];
	char		message[1000];
	SQLINTEGER	nativeerror;
	SQLSMALLINT	textlen;

	printf("%s: ", label);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT pg_sleep(2)", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("not cancelled\n");
	else
	{
		rc = SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, &nativeerror,
						   message, sizeof(message), &textlen);
		if (SQL_SUCCEEDED(rc))
			printf("cancelled with SQLSTATE %s\n", sqlstate);
		else
			printf("failed, no error information\n");
	}
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

static void
exec_and_close(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

/*
 * Report what the session has for statement_timeout.
 *
 * A cancelled statement leaves the transaction aborted, and the driver
 * recovers it only when it is set to roll back on an error. With
 * Protocol=7.4-0 it is not, so the session cannot be read until the
 * transaction ends. Report that and end the transaction, rather than give
 * up the test: the rest of it still says something under that setting.
 */
static void
show_statement_timeout(HSTMT hstmt)
{
	SQLRETURN	rc;
	char		sqlstate[32];
	char		message[1000];
	SQLINTEGER	nativeerror;
	SQLSMALLINT	textlen;

	printf("SHOW statement_timeout: ");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SHOW statement_timeout", SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		if (SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate,
										&nativeerror, message,
										sizeof(message), &textlen)))
			printf("not readable, SQLSTATE %s\n", sqlstate);
		else
			printf("failed, no error information\n");
		rc = SQLFreeStmt(hstmt, SQL_CLOSE);
		CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
		rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
		CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
		return;
	}
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt_notimeout = SQL_NULL_HSTMT;
	HSTMT		hstmt_timeout = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt_notimeout);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt_timeout);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}
	rc = SQLSetStmtAttr(hstmt_timeout, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER) 1, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt_timeout);

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);

	/*
	 * Open the transaction with the handle without a timeout, so that the
	 * SET statement_timeout for the other handle is issued inside it.
	 * The ROLLBACK then undoes the SET on the server.
	 */
	printf("-- SET statement_timeout = 1s inside a transaction, then ROLLBACK\n");
	exec_and_close(hstmt_notimeout, "SELECT 1");
	run_slow_query(hstmt_timeout, "timeout 1 before rollback");

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	run_slow_query(hstmt_timeout, "timeout 1 after rollback");
	show_statement_timeout(hstmt_timeout);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	/*
	 * The opposite direction: the 1 second timeout is now committed on the
	 * session. "SET statement_timeout = 0" for the other handle is rolled
	 * back, and the session is back at 1 second.
	 */
	printf("-- SET statement_timeout = 0 inside a transaction, then ROLLBACK\n");
	exec_and_close(hstmt_timeout, "SELECT 1");
	exec_and_close(hstmt_notimeout, "SELECT 1");

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	run_slow_query(hstmt_notimeout, "no timeout after rollback");
	show_statement_timeout(hstmt_notimeout);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	/*
	 * The application's own ROLLBACK TO SAVEPOINT, when the savepoint is
	 * older than the SET, undoes it too, without ending the transaction.
	 */
	printf("-- SET statement_timeout = 1s after a SAVEPOINT, then ROLLBACK TO it\n");
	exec_and_close(hstmt_notimeout, "SAVEPOINT foo");
	exec_and_close(hstmt_timeout, "SELECT 1");
	show_statement_timeout(hstmt_timeout);
	exec_and_close(hstmt_timeout, "ROLLBACK TO SAVEPOINT foo");
	run_slow_query(hstmt_timeout, "timeout 1 after rollback to savepoint");
	show_statement_timeout(hstmt_timeout);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	/*
	 * A COMMIT keeps the SET issued inside the transaction. The next
	 * statement without a timeout must get a "SET statement_timeout = 0"
	 * instead of running under the committed 1 second.
	 */
	printf("-- SET statement_timeout = 1s inside a transaction, then COMMIT\n");
	exec_and_close(hstmt_timeout, "SELECT 1");
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
	run_slow_query(hstmt_notimeout, "no timeout after commit");
	show_statement_timeout(hstmt_notimeout);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	/* Clean up */
	test_disconnect();

	return 0;
}
