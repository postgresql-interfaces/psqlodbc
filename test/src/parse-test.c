/*
 * Test driver's query parsing functionality.
 *
 * (People should be using server-side parsing nowadays, though...)
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;

	test_connect_ext("Parse=1;DisallowPremature=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*** Simple case ***/
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id, t FROM testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result_meta(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** Test quoting ***/
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT \"id\", \"t\" FROM \"testtab1\"", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result_meta(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** Test quoting ***/
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT \"id\", \"t\", 'foo''bar' AS \"con\"\"stant\" FROM \"testtab1\"", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result_meta(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Test handling of True(MSACCESS)
	 *	("a_boolean_item" = 1)  => ("a_boolean_item"='1')
	 */
	rc = SQLSetConnectAttr(conn, 65549, (SQLPOINTER) 1, 0);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT relname FROM pg_class where (\"relisshared\" = 1)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect for boolean = 1 for MSACCESS failed", hstmt);
	rc = SQLSetConnectAttr(conn, 65549, (SQLPOINTER) 0, 0);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	/*/
	 *	Do the following 2 queries work in declare/fetch mode?
	 */
	/* Do the driver detect *for update* and suppress *Declare .. cursor for* clause? */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id FROM \"testtab1\"for update", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect for SELECT ... FOR UPDATE failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	/* Do the driver detect *select .. into* and suppress *Declare .. cursor for* clause? */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT\"id\"into\"testtab2\"from\"testtab1\"", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect for SELECT .. INTO failed", hstmt);
	/* INSERT INTO ... () values () => INSERT INTO ... DEFAULT VALUES */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "insert into testtab2 () values ()", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect for INSERT INTO ... () failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
