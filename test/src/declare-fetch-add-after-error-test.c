/*
 * Test adding a row through an updatable cursor read with UseDeclareFetch=1
 * after another statement on the same connection has failed.
 *
 * The failed statement makes the driver roll back the transaction the
 * server-side cursor lives in, while the application goes on using the
 * result set. Storing the key of the added row in SC_pos_newload() would
 * then write past the end of the keyset, so the driver must refuse to use
 * the cursor once the rollback has closed it.
 *
 * argv[1] is the tag the user files: 1, the default, is already taken and
 * the INSERT fails; any other number is free.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt_cursor = SQL_NULL_HSTMT;
	HSTMT		hstmt_cmd = SQL_NULL_HSTMT;
	SQLINTEGER	tag = (argc > 1) ? atoi(argv[1]) : 1;
	SQLINTEGER	id;
	SQLCHAR		name[64];
	SQLLEN		id_ind, name_ind;
	int			rows_read;

	test_connect_ext("UseDeclareFetch=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt_cursor);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt_cmd);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt_cmd, (SQLCHAR *)
		"CREATE TEMPORARY TABLE addtest_items (id serial PRIMARY KEY, name text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt_cmd);
	rc = SQLExecDirect(hstmt_cmd, (SQLCHAR *)
		"INSERT INTO addtest_items (name) SELECT 'item ' || g FROM generate_series(1, 500) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt_cmd);
	rc = SQLExecDirect(hstmt_cmd, (SQLCHAR *)
		"CREATE TEMPORARY TABLE addtest_tags (tag int PRIMARY KEY)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt_cmd);
	rc = SQLExecDirect(hstmt_cmd, (SQLCHAR *)
		"INSERT INTO addtest_tags VALUES (1)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt_cmd);

	rc = SQLSetStmtAttr(hstmt_cursor, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt_cursor);
	rc = SQLSetStmtAttr(hstmt_cursor, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt_cursor);
	rc = SQLExecDirect(hstmt_cursor, (SQLCHAR *)
		"SELECT id, name FROM addtest_items ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt_cursor);
	rc = SQLBindCol(hstmt_cursor, 1, SQL_C_LONG, &id, 0, &id_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt_cursor);
	rc = SQLBindCol(hstmt_cursor, 2, SQL_C_CHAR, name, sizeof(name), &name_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt_cursor);

	/* More than one block of Fetch=100 rows, so the second block is read */
	for (rows_read = 0; rows_read < 150; rows_read++)
	{
		rc = SQLFetchScroll(hstmt_cursor, SQL_FETCH_NEXT, 0);
		CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt_cursor);
	}
	printf("read %d rows, now on '%s'\n", rows_read, name);

	/* A failure here is reported to the user, not fatal */
	rc = SQLBindParameter(hstmt_cmd, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
						  0, 0, &tag, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt_cmd);
	rc = SQLExecDirect(hstmt_cmd, (SQLCHAR *)
		"INSERT INTO addtest_tags VALUES (?)", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("tag filed\n");
	else
		print_diag("filing the tag failed", SQL_HANDLE_STMT, hstmt_cmd);

	/*
	 * The rollback closed the cursor before all its rows were read, so
	 * the driver refuses to go on with it (SQLSTATE 07005).
	 */
	rc = SQLFetchScroll(hstmt_cursor, SQL_FETCH_NEXT, 0);
	if (SQL_SUCCEEDED(rc) || rc == SQL_NO_DATA)
		printf("next row: fetched from a closed cursor\n");
	else
		print_diag("next row", SQL_HANDLE_STMT, hstmt_cursor);

	id_ind = SQL_IGNORE;		/* serial column */
	strcpy((char *) name, "added");
	name_ind = SQL_NTS;
	rc = SQLBulkOperations(hstmt_cursor, SQL_ADD);
	if (SQL_SUCCEEDED(rc))
		printf("row added through a closed cursor\n");
	else
		print_diag("adding a row", SQL_HANDLE_STMT, hstmt_cursor);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt_cursor);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt_cmd);
	test_disconnect();

	return 0;
}
