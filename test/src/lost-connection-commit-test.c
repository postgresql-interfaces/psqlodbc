/*
 * Test that a commit is not reported as successful after the connection
 * was lost inside a manual-commit transaction.
 *
 * When the driver notices that the connection is gone, it clears its
 * in-transaction flag.  If that happens in a call that discards the error
 * (here: SQLFreeHandle of a prepared statement, which sends DEALLOCATE),
 * SQLEndTran(SQL_COMMIT) used to find no open transaction and return
 * SQL_SUCCESS, although the server had rolled the transaction back.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	HSTMT		hstmt2 = SQL_NULL_HSTMT;
	SQLHDBC		conn2 = SQL_NULL_HDBC;
	SQLCHAR		dsn[1024];
	SQLCHAR		str[1024];
	SQLSMALLINT	strl;
	SQLINTEGER	pid;
	SQLLEN		ind;
	char		sql[100];

	test_connect();

	/* A second connection, used to terminate the first one's backend. */
	snprintf(dsn, sizeof(dsn), "DSN=%s", get_test_dsn());
	SQLAllocHandle(SQL_HANDLE_DBC, env, &conn2);
	rc = SQLDriverConnect(conn2, NULL, dsn, SQL_NTS,
						  str, sizeof(str), &strl, SQL_DRIVER_COMPLETE);
	CHECK_CONN_RESULT(rc, "SQLDriverConnect failed", conn2);

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT pg_backend_pid()", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &pid, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Do some work in the transaction, with a prepared statement. */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMP TABLE lost_conn_tab (id int4)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLPrepare(hstmt, (SQLCHAR *) "INSERT INTO lost_conn_tab VALUES (1)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* Kill the first connection while the transaction is open. */
	printf("killing connection...\n");
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn2, &hstmt2);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn2);
	snprintf(sql, sizeof(sql), "SELECT pg_terminate_backend(%d)", (int) pid);
	rc = SQLExecDirect(hstmt2, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt2);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt2);
	rc = SQLDisconnect(conn2);
	CHECK_CONN_RESULT(rc, "SQLDisconnect failed", conn2);
	SQLFreeHandle(SQL_HANDLE_DBC, conn2);

	/*
	 * Freeing the prepared statement sends DEALLOCATE, which is where the
	 * driver notices that the connection is gone. The error is not
	 * reported to the application.
	 */
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	printf("SQLFreeHandle: %s\n", SQL_SUCCEEDED(rc) ? "succeeded" : "failed");

	/* The commit must fail. */
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	if (SQL_SUCCEEDED(rc))
		printf("SQLEndTran(SQL_COMMIT) succeeded on a lost connection\n");
	else
		print_diag("SQLEndTran(SQL_COMMIT) failed", SQL_HANDLE_DBC, conn);

	/* So must turning autocommit on, which commits the transaction. */
	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_ON, 0);
	if (SQL_SUCCEEDED(rc))
		printf("SQLSetConnectAttr(SQL_AUTOCOMMIT_ON) succeeded on a lost connection\n");
	else
		print_diag("SQLSetConnectAttr(SQL_AUTOCOMMIT_ON) failed", SQL_HANDLE_DBC, conn);

	/* A rollback is fine; the server has already done that. */
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	if (SQL_SUCCEEDED(rc))
		printf("SQLEndTran(SQL_ROLLBACK) succeeded\n");
	else
		print_diag("SQLEndTran(SQL_ROLLBACK) failed", SQL_HANDLE_DBC, conn);

	test_disconnect();

	return 0;
}
