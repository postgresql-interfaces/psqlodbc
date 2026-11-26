/*
 * Test reading a large object column whose value names no large object.
 *
 * In autocommit mode the driver begins a transaction of its own to read
 * the large object. When lo_open fails, that transaction must be ended
 * by the driver, or the connection stays in the aborted transaction
 * and every later query fails with 25P02.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
read_row(HSTMT hstmt, int id)
{
	SQLRETURN	rc;
	char		sql[100];
	char		buf[100];
	SQLLEN		ind;

	snprintf(sql, sizeof(sql),
			 "SELECT large_data FROM lo_open_error_tab WHERE id = %d", id);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		rc = SQLFetch(hstmt);
	if (SQL_SUCCEEDED(rc))
		rc = SQLGetData(hstmt, 1, SQL_C_BINARY, buf, sizeof(buf), &ind);

	if (SQL_SUCCEEDED(rc))
		printf("row %d: %.*s\n", id, (int) ind, buf);
	else
	{
		printf("row %d: failed\n", id);
		print_diag(NULL, SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

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

	/*
	 * Three rows with a large object each. The large object of the
	 * second row is then unlinked, so the row keeps an OID that names
	 * no large object.
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE lo_open_error_tab (id int4, large_data lo)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO lo_open_error_tab VALUES (1, lo_from_bytea(0, 'first')), (2, lo_from_bytea(0, 'second')), (3, lo_from_bytea(0, 'third'))", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT lo_unlink(large_data) FROM lo_open_error_tab WHERE id = 2", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Read the rows one by one in autocommit mode. Reading row 2 fails,
	 * but the rows after it must still be readable.
	 */
	read_row(hstmt, 1);
	read_row(hstmt, 2);
	read_row(hstmt, 3);

	/* A non-query statement must work too */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "UPDATE lo_open_error_tab SET id = id WHERE id = 3", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	printf("update ok\n");

	/* Clean up */
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
