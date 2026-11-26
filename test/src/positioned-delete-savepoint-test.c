/*
 * Test a positioned delete through a keyset-driven cursor that is undone
 * with ROLLBACK TO SAVEPOINT. The cursor must show the row again.
 */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	HSTMT		hstmt2 = SQL_NULL_HSTMT;
	SQLINTEGER	id;
	SQLLEN		ind;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt2);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "create temporary table savepointtab (id serial primary key)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "create table failed", hstmt2);
	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "insert into savepointtab select generate_series(1, 5)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "insert failed", hstmt2);

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr AUTOCOMMIT failed", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &id, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "select id from savepointtab", SQL_NTS);
	CHECK_STMT_RESULT(rc, "select failed", hstmt);

	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "savepoint before_delete", SQL_NTS);
	CHECK_STMT_RESULT(rc, "savepoint failed", hstmt2);
	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
	rc = SQLSetPos(hstmt, 1, SQL_DELETE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos delete failed", hstmt);
	printf("deleted id=%d\n", id);

	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "rollback to before_delete", SQL_NTS);
	CHECK_STMT_RESULT(rc, "rollback to savepoint failed", hstmt2);
	printf("rolled back to the savepoint\n");

	rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
	printf("first row is id=%d\n", id);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt2);

	/* Clean up */
	test_disconnect();

	return 0;
}
