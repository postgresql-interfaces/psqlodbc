/*
 * Test SQLSetPos(SQL_UPDATE) on a row whose fetch truncated a column.
 *
 * The length buffer of such a column holds the length of the whole value,
 * which is more than the bound buffer holds. The driver must refuse the
 * update rather than write the part it has: that would replace the stored
 * value with a truncated one and report success. That includes a value
 * exactly as long as the buffer, which the fetch cuts by one byte to fit
 * the terminator. Marking the column SQL_COLUMN_IGNORE updates the other
 * columns and leaves it alone.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	id;
	char		entry[8];
	SQLLEN		id_ind, entry_ind;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE setpos_truncated(id int4 primary key, entry text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO setpos_truncated VALUES"
		" (1, 'a value longer than the buffer'), (10, 'exactly8')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT id, entry FROM setpos_truncated ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &id, 0, &id_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, entry, sizeof(entry), &entry_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	printf("fetched: %d \"%s\", length %ld\n", (int) id, entry, (long) entry_ind);

	/* entry holds 7 of its 30 bytes */
	id = 2;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	if (SQL_SUCCEEDED(rc))
		printf("SQLSetPos UPDATE with the truncated column succeeded\n");
	else
		print_diag("SQLSetPos UPDATE with the truncated column failed", SQL_HANDLE_STMT, hstmt);

	id = 3;
	entry_ind = SQL_COLUMN_IGNORE;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos UPDATE with the column ignored failed", hstmt);
	printf("SQLSetPos UPDATE with the column ignored succeeded\n");

	/*
	 * A value exactly as long as the buffer loses its last byte to the
	 * terminator, and its length is the buffer's length.
	 */
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	printf("fetched: %d \"%s\", length %ld\n", (int) id, entry, (long) entry_ind);
	id = 11;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	if (SQL_SUCCEEDED(rc))
		printf("SQLSetPos UPDATE with the exactly full column succeeded\n");
	else
		print_diag("SQLSetPos UPDATE with the exactly full column failed",
				   SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_UNBIND);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT id, entry FROM setpos_truncated ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
