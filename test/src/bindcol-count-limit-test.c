/*
 * Test the limits on column numbers and the ARD's SQL_DESC_COUNT.  The
 * driver keeps column binding counts in SQLSMALLINT, and 32767 is the
 * largest value SQLBindCol and SQL_DESC_COUNT accept.  Unbinding the
 * columns with SQLFreeStmt(SQL_UNBIND) or freeing the statement must end
 * at that count: a loop counter of the same 16-bit type never exceeds it,
 * and the loop then never ends.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define MAX_COLUMNS	32767

static void
print_state(const char *what, SQLRETURN rc, SQLSMALLINT htype, SQLHANDLE h)
{
	SQLCHAR		state[6] = "";
	SQLINTEGER	native;
	SQLCHAR		msg[256];
	SQLSMALLINT	len;

	printf("%s: ", what);
	if (SQL_SUCCEEDED(rc))
	{
		printf("ok\n");
		return;
	}
	SQLGetDiagRec(htype, h, 1, state, &native, msg, sizeof(msg), &len);
	printf("error SQLSTATE=%s\n", (char *) state);
}

/*
 * Bind column 'column' before execution, which PGAPI_BindCol supports,
 * run a one-column query, then unbind and free the statement.
 */
static void
bind_column(int column)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		buf[64];
	SQLLEN		ind;
	char		what[64];

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLBindCol(hstmt, (SQLUSMALLINT) column, SQL_C_CHAR, buf, sizeof(buf), &ind);
	snprintf(what, sizeof(what), "bind column %d", column);
	print_state(what, rc, SQL_HANDLE_STMT, hstmt);

	if (SQL_SUCCEEDED(rc))
	{
		rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'foo'", SQL_NTS);
		CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
		rc = SQLFetch(hstmt);
		CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

		rc = SQLFreeStmt(hstmt, SQL_UNBIND);
		snprintf(what, sizeof(what), "bind column %d, unbind", column);
		print_state(what, rc, SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	snprintf(what, sizeof(what), "bind column %d, free statement", column);
	print_state(what, rc, SQL_HANDLE_STMT, hstmt);
}

/* Set SQL_DESC_COUNT of the statement's ARD to 'count', then unbind */
static void
ard_desc_count(int count)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDESC	hdesc;
	char		what[64];

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &hdesc, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);

	rc = SQLSetDescField(hdesc, 0, SQL_DESC_COUNT, (SQLPOINTER) (SQLLEN) count, 0);
	snprintf(what, sizeof(what), "ARD SQL_DESC_COUNT=%d", count);
	print_state(what, rc, SQL_HANDLE_DESC, hdesc);

	rc = SQLFreeStmt(hstmt, SQL_UNBIND);
	snprintf(what, sizeof(what), "ARD SQL_DESC_COUNT=%d, unbind", count);
	print_state(what, rc, SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	snprintf(what, sizeof(what), "ARD SQL_DESC_COUNT=%d, free statement", count);
	print_state(what, rc, SQL_HANDLE_STMT, hstmt);
}

/*
 * Bind column 'column', then lower the ARD count back to 1.  The GetData
 * info keeps its larger count, so only its unbind loop sees the limit.
 */
static void
bind_column_then_shrink(int column)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDESC	hdesc;
	char		buf[64];
	SQLLEN		ind;
	char		what[64];

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLBindCol(hstmt, (SQLUSMALLINT) column, SQL_C_CHAR, buf, sizeof(buf), &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &hdesc, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	rc = SQLSetDescField(hdesc, 0, SQL_DESC_COUNT, (SQLPOINTER) (SQLLEN) 1, 0);
	CHECK_STMT_RESULT(rc, "SQLSetDescField failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_UNBIND);
	snprintf(what, sizeof(what), "bind column %d, ARD SQL_DESC_COUNT=1, unbind", column);
	print_state(what, rc, SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	snprintf(what, sizeof(what), "bind column %d, ARD SQL_DESC_COUNT=1, free statement", column);
	print_state(what, rc, SQL_HANDLE_STMT, hstmt);
}

int
main(int argc, char **argv)
{
	test_connect();

	bind_column(MAX_COLUMNS - 1);
	bind_column(MAX_COLUMNS);
	bind_column(MAX_COLUMNS + 1);

	ard_desc_count(MAX_COLUMNS - 1);
	ard_desc_count(MAX_COLUMNS);

	bind_column_then_shrink(MAX_COLUMNS);

	test_disconnect();

	return 0;
}
