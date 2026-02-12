/*
 * Test SQL_ATTR_ASYNC_ENABLE handling.
 *
 * Verifies that setting SQL_ATTR_ASYNC_ENABLE to SQL_ASYNC_ENABLE_ON
 * returns SQL_ERROR (since async is not supported), and that setting
 * SQL_ASYNC_ENABLE_OFF succeeds.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	async_enable;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Test 1: Setting SQL_ASYNC_ENABLE_ON should fail with SQL_ERROR
	 */
	printf("Setting SQL_ATTR_ASYNC_ENABLE = SQL_ASYNC_ENABLE_ON\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ASYNC_ENABLE,
						(SQLPOINTER) SQL_ASYNC_ENABLE_ON, SQL_IS_UINTEGER);
	if (rc == SQL_ERROR)
		printf("ok, got SQL_ERROR as expected\n");
	else
		printf("unexpected result: %d\n", rc);

	/*
	 * Test 2: Setting SQL_ASYNC_ENABLE_OFF should succeed
	 */
	printf("Setting SQL_ATTR_ASYNC_ENABLE = SQL_ASYNC_ENABLE_OFF\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ASYNC_ENABLE,
						(SQLPOINTER) SQL_ASYNC_ENABLE_OFF, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr SQL_ASYNC_ENABLE_OFF failed", hstmt);
	printf("ok\n");

	/*
	 * Test 3: Getting SQL_ATTR_ASYNC_ENABLE should return SQL_ASYNC_ENABLE_OFF
	 */
	printf("Getting SQL_ATTR_ASYNC_ENABLE\n");
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_ASYNC_ENABLE,
						&async_enable, SQL_IS_UINTEGER, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr SQL_ATTR_ASYNC_ENABLE failed", hstmt);
	printf("async_enable: %s\n",
		   async_enable == SQL_ASYNC_ENABLE_OFF ? "SQL_ASYNC_ENABLE_OFF" :
		   async_enable == SQL_ASYNC_ENABLE_ON ? "SQL_ASYNC_ENABLE_ON" :
		   "other");

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
