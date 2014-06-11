#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Test VACUUM */
	printf("Testing VACUUM with SQLExecDirect...\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "vacuum (analyze) testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Same with SQLPrepare/SQLExecute */
	printf("Testing VACUUM with SQLPrepare/SQLExecute...\n");
	rc = SQLPrepare(hstmt, (SQLCHAR *) "VACUUM ANALYZE testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Now the same with autocommit disabled. The driver should recognize
	 * that the commands are VACUUMs, which cannot be run in a transaction
	 * block, and not issue a BEGIN even it normally would in autocommit
	 * mode. In other words, these commands should behave the same with or
	 * without autocommit. But if you issued a normal query, like a SELECT,
	 * first in the same transaction, and then tried to run a VACUUM, it
	 * would fail with "VACUUM cannot run inside a transaction block" error.
	 */
	printf("Disabling autocommit...\n");

	rc = SQLSetConnectAttr(conn,
						   SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER)SQL_AUTOCOMMIT_OFF,
						   SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetConnectAttr failed", hstmt);

	/* Test VACUUM */
	printf("Testing VACUUM with SQLExecDirect...\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "vacuum analyze testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Same with SQLPrepare/SQLExecute */
	printf("Testing VACUUM with SQLPrepare/SQLExecute...\n");
	rc = SQLPrepare(hstmt, (SQLCHAR *) "VACUUM (ANALYZE) testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
