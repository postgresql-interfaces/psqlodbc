/*
 * Add many rows through an updatable cursor in one transaction.
 *
 * Until a transaction ends, the driver keeps one log entry per row added,
 * updated or deleted through a cursor.  40960 rows fill the log exactly, so
 * the 40961st row makes it grow.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static SQLINTEGER id;
static SQLLEN id_ind;

static void
add_rows_in_one_transaction(HSTMT hstmt, int nrows)
{
	SQLRETURN	rc;

	for (id = 1; id <= nrows; id++)
	{
		rc = SQLBulkOperations(hstmt, SQL_ADD);
		CHECK_STMT_RESULT(rc, "SQLBulkOperations failed", hstmt);
	}
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	printf("added %d rows in one transaction\n", nrows);
	fflush(stdout);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE many_bulk_adds (id int4)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id FROM many_bulk_adds", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &id, 0, &id_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	/* fills the log exactly */
	add_rows_in_one_transaction(hstmt, 40960);

	/* the log grows at the 40961st row */
	add_rows_in_one_transaction(hstmt, 40964);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
