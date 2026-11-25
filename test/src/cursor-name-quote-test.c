/*
 * Test that SQLSetCursorName refuses a name with a double quote.
 *
 * With UseDeclareFetch=1 the driver writes the cursor name between double
 * quotes into the DECLARE, FETCH, MOVE and CLOSE statements it sends. A
 * double quote in the name ends the quoted name there, and the rest of
 * the name is read as SQL. The name below would run a COMMIT that the
 * application did not ask for, so the row inserted before it would
 * survive the application's ROLLBACK.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	count = -1;
	SQLLEN		ind;

	test_connect_ext("UseDeclareFetch=1");
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE cursor_name_quote (i int4)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO cursor_name_quote VALUES (1)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	/* the driver manager refuses SQLSetCursorName on an open statement */
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetCursorName(hstmt, (SQLCHAR *)
		"x\"cursor for values(1);commit--", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("SQLSetCursorName accepted the name\n");
	else
		print_diag("SQLSetCursorName refused the name", SQL_HANDLE_STMT, hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT i FROM cursor_name_quote", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("the query ran\n");
	else
		print_diag("the query failed", SQL_HANDLE_STMT, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT count(*) FROM cursor_name_quote", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SELECT count failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_SLONG, &count, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("rows left after the rollback: %d\n", (int) count);
	SQLFreeStmt(hstmt, SQL_CLOSE);
	SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
