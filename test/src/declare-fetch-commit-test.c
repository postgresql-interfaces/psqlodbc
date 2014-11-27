/*
 * Test what happens when the transaction is committed while fetching from a
 * result set.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int			rows;

	/****
     * Run this test with UseDeclareFetch = 1 and Fetch=1, so that we fetch
     * one row at a time. Protocol=-2 is required so that the cursors survives
	 * the commit.
     */
	test_connect_ext("UseDeclareFetch=1;Fetch=1;Protocol=7.4-2");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Disable autocommit, and execute a dummy UPDATE to start a transaction */
	rc = SQLSetConnectAttr(conn,
						   SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER)SQL_AUTOCOMMIT_OFF,
						   SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetConnectAttr failed", hstmt);

	SQLExecDirect(hstmt, (SQLCHAR *) "update testtbl1 set t = t where id = 123456", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/**** Start a cursor ****/

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT * FROM generate_series(1, 5) g ", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* Fetch result */

	printf("Result set:\n");
	rows = 0;
	while(1)
	{
		rc = SQLFetch(hstmt);
		if (rc == SQL_NO_DATA)
			break;
		if (rc == SQL_SUCCESS)
		{
			char buf[40];
			SQLLEN ind;

			rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
			if (!SQL_SUCCEEDED(rc))
			{
				print_diag("SQLGetData failed", SQL_HANDLE_STMT, hstmt);
				exit(1);
			}
			printf("%s", buf);

			printf("\n");
		}
		else
		{
			print_diag("SQLFetch failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}

		/* In the middle of the result set, COMMIT */
		if (rows == 2)
		{
			rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
			CHECK_STMT_RESULT(rc, "SQLEndTran failed\n", hstmt);
		}
		rows++;
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
