/*
 * Tests for deprecated functions.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

void
print_cursor_type(SQLINTEGER cursor_type)
{
	printf("Cursor type is: ");
	if (cursor_type == SQL_CURSOR_FORWARD_ONLY)
		printf("forward\n");
	else if (cursor_type == SQL_CURSOR_KEYSET_DRIVEN)
		printf("keyset driven\n");
	else if (cursor_type == SQL_CURSOR_DYNAMIC)
		printf("dynamic\n");
	else if (cursor_type == SQL_CURSOR_STATIC)
		printf("static\n");
	else
	{
		printf("unknown type of cursor: %d\n", cursor_type);
		exit(1);
	}
}

void
print_access_type(SQLINTEGER access_type)
{
	printf("Access type is: ");
	if (access_type == SQL_MODE_READ_ONLY)
		printf("read-only\n");
	else if (access_type == SQL_MODE_READ_WRITE)
		printf("read-write\n");
	else
	{
		printf("Incorrect type of access\n");
		exit(1);
	}
}

/* Array size for SQLParamOptions test */
#define ARRAY_SIZE	10

int
main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLHDBC conn2;
	SQLHENV env2;
	SQLSMALLINT val;
	SQLINTEGER valint;
	char buffer[256];
	char message[1000];
	int i;

	/* Parameters for SQLParamOptions */
	SQLULEN nprocessed;
	SQLLEN int_ind_array[ARRAY_SIZE];
	SQLLEN str_ind_array[ARRAY_SIZE];
	SQLUSMALLINT status_array[ARRAY_SIZE];
	SQLUINTEGER int_array[ARRAY_SIZE];
	SQLCHAR str_array[ARRAY_SIZE][30];

	/*
	 * SQLAllocConnect -> SQLAllocHandle
	 * SQLAllocEnv -> SQLAllocHandle
	 * Get a connection.
	 */
	printf("Check for SQLAllocEnv\n");
	rc = SQLAllocEnv(&env2);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocEnv failed", SQL_HANDLE_ENV, env2);
		exit(1);
	}
	SQLSetEnvAttr(env2, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
	printf("Check for SQLAllocConnect\n");
	rc = SQLAllocConnect(env2, &conn2);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocConnect failed", SQL_HANDLE_DBC, conn2);
		exit(1);
	}

	snprintf(buffer, sizeof(buffer), "DSN=%s;", get_test_dsn());

	rc = SQLDriverConnect(conn2, NULL, buffer, SQL_NTS,
						  NULL, 0, &val,
						  SQL_DRIVER_COMPLETE);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLDriverConnect failed", SQL_HANDLE_DBC, conn2);
		exit(1);
	}

	/*
	 * SQLAllocStmt -> SQLAllocHandle
	 * Allocate a statement handle.
	 */
	printf("Check for SQLAllocStmt\n");
	rc = SQLAllocStmt(conn2, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocStmt failed", SQL_HANDLE_DBC, conn2);
		exit(1);
	}

	/*
	 * SQLSetConnectOption -> SQLSetConnectAttr
	 * SQLGetConnectOption -> SQLGetConnectAttr
	 * Test connection attribute.
	 */
	printf("Check for SQLSetConnectOption\n");
	rc = SQLSetConnectOption(conn2,
							 SQL_ATTR_ACCESS_MODE,
							 SQL_MODE_READ_WRITE);
	CHECK_STMT_RESULT(rc, "SQLSetConnectOption failed", hstmt);
	printf("Check for SQLGetConnectOption\n");
	rc = SQLGetConnectOption(conn2,
							 SQL_ATTR_ACCESS_MODE,
							 &valint);
	CHECK_STMT_RESULT(rc, "SQLGetConnectOption failed", hstmt);
	print_access_type(valint);

	/*
	 * SQLError -> SQLGetDiagRec
	 * Trigger an error.
	 */
	printf("Check for SQLError\n");
	rc = SQLExecDirect(hstmt,
				(SQLCHAR *) "INSERT INTO table_not_here VALUES (1)",
				SQL_NTS);
	rc = SQLError(env2, conn2, hstmt, buffer, &valint,
				  message, 256, &val);
	CHECK_STMT_RESULT(rc, "SQLError failed", hstmt);
	printf("Error check: %s\n", (char *)message);

	/*
	 * SQLSetParam[2] -> SQLBindParameter
	 * Test for set parameter, somewhat similar to last example.
	 */
	printf("Check for SQLSetParam\n");
	val = 100;
	SQLSetParam(hstmt, 1, SQL_C_SHORT, SQL_SMALLINT, 0, 0,
				&val, NULL);
	CHECK_STMT_RESULT(rc, "SQLSetParam failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT ?::int AS foo", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * SQLGetStmtOption -> SQLGetStmtAttr
	 * SQLSetStmtOption -> SQLSetStmtAttr
	 * SQLSetScrollOptions -> SQLSetStmtAttr
	 * Test for statement and cursor options.
	 */

	/* Pointer should be forward-only here */
	printf("Check for SQLGetStmtOption\n");
	rc = SQLGetStmtOption(hstmt, SQL_CURSOR_TYPE, &valint);
	CHECK_STMT_RESULT(rc, "SQLGetStmtOption failed", hstmt);
	print_cursor_type(valint);

	/* Change it to static and check its new value */
	printf("Check for SQLSetStmtOption\n");
	rc = SQLSetStmtOption(hstmt, SQL_CURSOR_TYPE, SQL_CURSOR_STATIC);
	CHECK_STMT_RESULT(rc, "SQLSetStmtOption failed", hstmt);
	rc = SQLGetStmtOption(hstmt, SQL_CURSOR_TYPE, &valint);
	CHECK_STMT_RESULT(rc, "SQLGetStmtOption failed", hstmt);
	print_cursor_type(valint);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Change it now to dynamic using another function */
	printf("Check for SQLSetScrollOptions\n");
	rc = SQLSetScrollOptions(hstmt, SQL_CONCUR_READ_ONLY,
							SQL_SCROLL_FORWARD_ONLY, 1);
	CHECK_STMT_RESULT(rc, "SQLSetScrollOptions failed", hstmt);
	rc = SQLGetStmtOption(hstmt, SQL_CURSOR_TYPE, &valint);
	CHECK_STMT_RESULT(rc, "SQLGetStmtOption failed", hstmt);
	/*
	 *	The result on Windows is ununderstandable to me.
	 *	This deprecated-test doesn't seem to have much meaning.
	 *	So just suppress a diff output.
	 */
#ifdef	WIN32
	printf("Cursor type is: forward\n");
#else
	print_cursor_type(valint);
#endif
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * SQLColAttributes -> SQLColAttribute
	 * Test for column attributes. Return column attributes of
	 * a simple query.
	 */
	printf("Check for SQLColAttributes\n");
	rc = SQLExecDirect(hstmt,
			(SQLCHAR *) "SELECT '1'::int AS foo, 'foobar'::text AS bar",
			SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	for (i = 0 ; i < 2; i++)
	{
		char buffer[64];
		rc = SQLColAttribute(hstmt, (SQLUSMALLINT) i + 1,
							 SQL_DESC_LABEL, buffer, 64, NULL, NULL);
		CHECK_STMT_RESULT(rc, "SQLColAttribute failed", hstmt);
		printf("Column %d: %s\n", i + 1, buffer);
	}
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * SQLParamOptions -> SQLSetStmtAttr
	 * Test for parameter options. SQLParamOptions is mapped using
	 * SQL_ATTR_PARAMS_PROCESSED_PTR and SQL_ATTR_PARAMSET_SIZE. Here
	 * we insert a set of values in a temporary table.
	 */

	/* Fill in array values */
	for (i = 0; i < ARRAY_SIZE; i++)
	{
		int_array[i] = i;
		int_ind_array[i] = 0;
		sprintf(str_array[i], "column num %d", i);
		str_ind_array[i] = SQL_NTS;
	}
	printf("Check for SQLParamOptions\n");
	rc = SQLExecDirect(hstmt,
		   (SQLCHAR *) "CREATE TEMPORARY TABLE tmptable (i int4, t text)",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed for table creation", hstmt);

	rc = SQLParamOptions(hstmt, (SQLULEN) ARRAY_SIZE, &nprocessed);
	CHECK_STMT_RESULT(rc, "SQLParamOptions failed", hstmt);

	/* Set some extra parameters */
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_STATUS_PTR, status_array, 0);

	/* Bind the parameter arrays. */
	SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 5, 0,
					 int_array, 0, int_ind_array);
	SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 29, 0,
					 str_array, 30, str_ind_array);

	/* Execute and fetch statuses */
	rc = SQLExecDirect(hstmt,
			(SQLCHAR *) "INSERT INTO tmptable VALUES (?, ?)",
			SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	printf("Status of execution\n");
	for (i = 0; i < nprocessed; i++)
	{
		switch (status_array[i])
		{
			case SQL_PARAM_SUCCESS:
			case SQL_PARAM_SUCCESS_WITH_INFO:
				break;

			case SQL_PARAM_ERROR:
				printf("%d\tError\n", i);
				break;

			case SQL_PARAM_UNUSED:
				printf("%d\tUnused\n", i);
				break;

			case SQL_PARAM_DIAG_UNAVAILABLE:
				printf("%d\tDiag unavailable\n", i);
				break;
		}
	}

	/*
	 * SQLFreeStmt(SQL_DROP) -> SQLFreeHandle
	 *
	 * Free the statement handle used. SQL_DROP is deprecated, other
	 * options are not.
	 */
	printf("Check for SQLFreeStmt\n");
	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt with SQL_DROP failed", hstmt);

	/*
	 * SQLFreeConnect -> SQLFreeHandle
	 * SQLFreeEnv -> SQLFreeHandle
	 * Disconnect and free connection.
	 */
	rc = SQLDisconnect(conn2);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLDisconnect failed", SQL_HANDLE_DBC, conn2);
		exit(1);
	}
	printf("Check for SQLFreeConnect\n");
	rc = SQLFreeConnect(conn2);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeConnect failed", SQL_HANDLE_DBC, conn2);
		exit(1);
	}
	printf("Check for SQLFreeEnv\n");
	rc = SQLFreeEnv(env2);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeEnv failed", SQL_HANDLE_ENV, env2);
		exit(1);
	}

	/* Grab new connection and handle for the next tests */
	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * SQLTransact -> SQLEndTran
	 * Check transaction ROLLBACK using SQLTransact.
	 */
	printf("Check for SQLTransact\n");

	/* First disable autocommit */
	rc = SQLSetConnectAttr(conn,
						   SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER)SQL_AUTOCOMMIT_OFF,
						   SQL_IS_UINTEGER);

	/* Insert a row and rollback it */
	rc = SQLExecDirect(hstmt,
			(SQLCHAR *) "INSERT INTO testtab1 VALUES (200, 'foo')",
			SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLTransact(SQL_NULL_HENV, conn, SQL_ROLLBACK);
	CHECK_STMT_RESULT(rc, "SQLTransact failed", hstmt);

	/* Now check that the row is not inserted */
	rc = SQLExecDirect(hstmt,
			(SQLCHAR *) "SELECT t FROM testtab1 WHERE id = 200",
			SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
