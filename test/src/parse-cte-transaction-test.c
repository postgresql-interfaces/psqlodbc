/*
 * Test that parsing a statement for its column information does not abort
 * the application's transaction.
 *
 * With Parse=1 the driver asks the server which schema each unqualified
 * table name is in. A name that is not a relation the role can see, a
 * misspelling included, makes that question fail. Without a savepoint of
 * its own that error aborts the transaction the application has open, and
 * SQLDescribeCol then reports 25P02 rather than the real reason.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLCHAR		colname[64];
	SQLSMALLINT	namelen, datatype, decdigits, nullable;
	SQLULEN		colsize;

	test_connect_ext("Parse=1");

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'in transaction'", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SELECT failed", hstmt);
	print_result(hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	/*
	 * A FROM item that is not a relation this role can see. The driver
	 * asks the server which schema the name is in, and that question
	 * must not abort the transaction the application has open.
	 */
	rc = SQLPrepare(hstmt, (SQLCHAR *)
		"SELECT * FROM nosuchtable_parse_cte", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	rc = SQLDescribeCol(hstmt, 1, colname, sizeof(colname), &namelen,
						&datatype, &colsize, &decdigits, &nullable);
	if (SQL_SUCCEEDED(rc))
		printf("column 1: %s\n", colname);
	else
		print_diag("SQLDescribeCol failed", SQL_HANDLE_STMT, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	/*
	 * Only the describe is checked. Executing the statement fails on its
	 * own, and whether the transaction survives that depends on the
	 * rollback-on-error setting, so it would need an expected file per
	 * combination and would not say anything about the probe. End the
	 * transaction here and confirm the connection is still good.
	 */
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	/* the connection must still be usable */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'still usable'", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		print_result(hstmt);
	else
		print_diag("next statement failed", SQL_HANDLE_STMT, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
