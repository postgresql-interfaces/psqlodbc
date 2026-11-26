/*
 * Test a bookmark column (column 0) bound as SQL_C_VARBOOKMARK with a
 * buffer shorter than the 4-byte row number bookmark.
 *
 * The driver must not write more than BufferLength bytes into the buffer.
 * A bookmark that does not fit is truncated: the fetch returns 01004 and
 * the indicator holds the length of the whole bookmark.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define BUF_SIZE 24
#define GUARD 0x5a

static void
fetch_with_buffer_length(HSTMT hstmt, SQLLEN buflen)
{
	SQLRETURN	rc;
	unsigned char buf[BUF_SIZE];
	SQLLEN		ind = 0;
	SQLLEN		i;
	int			touched = 0;

	memset(buf, GUARD, sizeof(buf));
	rc = SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, buf, buflen, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT g FROM generate_series(1, 3) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	printf("BufferLength %d: ", (int) buflen);
	if (SQL_SUCCESS == rc)
		printf("SQL_SUCCESS");
	else if (SQL_SUCCESS_WITH_INFO == rc)
	{
		SQLCHAR		state[6] = "";
		SQLINTEGER	native;
		SQLCHAR		msg[256];
		SQLSMALLINT	len;

		SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, state, &native,
					  msg, sizeof(msg), &len);
		printf("SQL_SUCCESS_WITH_INFO %s", (char *) state);
	}
	else
	{
		print_diag("SQLFetch failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	for (i = buflen; i < BUF_SIZE; i++)
		if (buf[i] != GUARD)
			touched = 1;
	printf(", indicator %d, bytes after the buffer %s\n", (int) ind,
		   touched ? "overwritten" : "untouched");

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_UNBIND);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_USE_BOOKMARKS,
						(SQLPOINTER) SQL_UB_VARIABLE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	/* room for the whole bookmark */
	fetch_with_buffer_length(hstmt, 16);
	/* shorter than the 4-byte row number bookmark */
	fetch_with_buffer_length(hstmt, 2);
	fetch_with_buffer_length(hstmt, 0);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
