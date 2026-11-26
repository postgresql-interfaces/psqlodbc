/*
 * Test that SQL_ATTR_CONNECTION_DEAD reports SQL_CD_TRUE once the server
 * has dropped the connection, and keeps doing so on later calls.
 * System.Data.Odbc only closes an OdbcConnection when this attribute
 * says the connection is dead (issue #224).
 *
 * The backend of the test connection is terminated from a second
 * connection, so the driver only learns about it on the next query.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static SQLUINTEGER
get_connection_dead(void)
{
	SQLRETURN	rc;
	SQLUINTEGER	dead = 99;

	rc = SQLGetConnectAttr(conn, SQL_ATTR_CONNECTION_DEAD, &dead, 0, NULL);
	CHECK_CONN_RESULT(rc, "SQLGetConnectAttr(SQL_ATTR_CONNECTION_DEAD) failed", conn);
	return dead;
}

static void
run_query_expect_failure(const char *label)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
	{
		printf("FAIL: %s query succeeded on a terminated connection\n", label);
		exit(1);
	}
	printf("%s query failed as expected\n", label);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHENV		env2;
	SQLHDBC		conn2;
	HSTMT		hstmt2 = SQL_NULL_HSTMT;
	SQLINTEGER	pid;
	SQLINTEGER	remaining;
	char		dsn[1024];
	char	   *envvar;
	char		sql[128];
	int			i;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT pg_backend_pid()", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SELECT pg_backend_pid() failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_SLONG, &pid, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

	printf("connection dead before termination: %u\n", (unsigned int) get_connection_dead());

	/* Terminate the first connection's backend from a second connection */
	envvar = getenv("COMMON_CONNECTION_STRING_FOR_REGRESSION_TEST");
	snprintf(dsn, sizeof(dsn), "DSN=%s;%s", get_test_dsn(), envvar ? envvar : "");
	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env2);
	SQLSetEnvAttr(env2, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
	SQLAllocHandle(SQL_HANDLE_DBC, env2, &conn2);
	rc = SQLDriverConnect(conn2, NULL, (SQLCHAR *) dsn, SQL_NTS,
						  NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	CHECK_CONN_RESULT(rc, "second SQLDriverConnect failed", conn2);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn2, &hstmt2);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn2);

	snprintf(sql, sizeof(sql), "SELECT pg_terminate_backend(%d)", (int) pid);
	rc = SQLExecDirect(hstmt2, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "pg_terminate_backend failed", hstmt2);
	SQLFreeStmt(hstmt2, SQL_CLOSE);

	/* pg_terminate_backend() only signals; wait until the backend is gone */
	snprintf(sql, sizeof(sql), "SELECT count(*) FROM pg_stat_activity WHERE pid = %d", (int) pid);
	for (i = 0; i < 100; i++)
	{
		rc = SQLExecDirect(hstmt2, (SQLCHAR *) sql, SQL_NTS);
		CHECK_STMT_RESULT(rc, "pg_stat_activity query failed", hstmt2);
		rc = SQLFetch(hstmt2);
		CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt2);
		rc = SQLGetData(hstmt2, 1, SQL_C_SLONG, &remaining, 0, NULL);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt2);
		SQLFreeStmt(hstmt2, SQL_CLOSE);
		if (0 == remaining)
			break;
		rc = SQLExecDirect(hstmt2, (SQLCHAR *) "SELECT pg_sleep(0.1)", SQL_NTS);
		CHECK_STMT_RESULT(rc, "pg_sleep failed", hstmt2);
		SQLFreeStmt(hstmt2, SQL_CLOSE);
	}
	if (0 != remaining)
	{
		printf("FAIL: backend %d was not terminated\n", (int) pid);
		exit(1);
	}

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
	SQLDisconnect(conn2);
	SQLFreeHandle(SQL_HANDLE_DBC, conn2);
	SQLFreeHandle(SQL_HANDLE_ENV, env2);

	run_query_expect_failure("first");
	printf("connection dead after first failure: %u\n", (unsigned int) get_connection_dead());

	run_query_expect_failure("second");
	printf("connection dead after second failure: %u\n", (unsigned int) get_connection_dead());

	test_disconnect();

	return 0;
}
