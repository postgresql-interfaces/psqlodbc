/*
 * Test that the row array size (SQL_ATTR_ROW_ARRAY_SIZE / SQL_DESC_ARRAY_SIZE
 * of the ARD) is never below one row. A smaller value stored as is makes
 * SQLSetPos(0, SQL_REFRESH) write the rest of the result set past the end
 * of the bound arrays.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define ARRAY_ROWS 3

static void
print_row_array_size(HSTMT hstmt, const char *label)
{
	SQLRETURN	rc;
	SQLULEN		size = 12345;

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	printf("%s: row array size %ld\n", label, (long) size);
}

static void
set_row_array_size(HSTMT hstmt, SQLLEN value)
{
	SQLRETURN	rc;

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) value, 0);
	printf("SQLSetStmtAttr(SQL_ATTR_ROW_ARRAY_SIZE, %ld) returned %d\n", (long) value, rc);
	if (SQL_SUCCESS_WITH_INFO == rc)
		print_diag("warning:", SQL_HANDLE_STMT, hstmt);
}

static void
set_ard_array_size(HSTMT hstmt, SQLLEN value)
{
	SQLRETURN	rc;
	SQLHDESC	ard;

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	rc = SQLSetDescField(ard, 0, SQL_DESC_ARRAY_SIZE, (SQLPOINTER) value, 0);
	printf("SQLSetDescField(SQL_DESC_ARRAY_SIZE, %ld) returned %d\n", (long) value, rc);
	if (SQL_SUCCESS_WITH_INFO == rc)
		print_diag("warning:", SQL_HANDLE_DESC, ard);
}

/*
 * Fetch one rowset and refresh it; the driver must not write past the row
 * array size. That size is read back rather than assumed: the Windows
 * driver manager refuses a size below one itself and keeps the old one.
 */
static void
fetch_and_refresh(HSTMT hstmt)
{
	SQLRETURN	rc;
	SQLINTEGER	id[ARRAY_ROWS];
	SQLLEN		ind[ARRAY_ROWS];
	SQLUSMALLINT status[ARRAY_ROWS];
	SQLULEN		fetched = 0;
	SQLULEN		size = 0;
	int			i;

	for (i = 0; i < ARRAY_ROWS; i++)
	{
		id[i] = -1;
		ind[i] = -1;
		status[i] = 0xFFFF;
	}
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_STATUS_PTR, status, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROW_STATUS_PTR failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR, &fetched, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROWS_FETCHED_PTR failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT g FROM generate_series(1, 10) g ORDER BY g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, id, 0, ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
	printf("fetched %lu row(s)\n", (unsigned long) fetched);
	rc = SQLSetPos(hstmt, 0, SQL_REFRESH, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos failed", hstmt);
	printf("row 0: status %d\n", (int) status[0]);
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	/*
	 * A driver that keeps the size it was given reports (SQLULEN) -1 here,
	 * which is not the index of a bound row. Check from the second row on
	 * in that case: the first row is the one the driver is entitled to
	 * write, and reading before the arrays is a defect of its own.
	 */
	if (size < 1 || size > (SQLULEN) ARRAY_ROWS)
		i = 1;
	else
		i = (int) size;
	for (; i < ARRAY_ROWS; i++)
	{
		if (id[i] != -1 || ind[i] != -1 || status[i] != 0xFFFF)
			printf("row %d: overwritten\n", i);
	}

	SQLFreeStmt(hstmt, SQL_CLOSE);
	SQLFreeStmt(hstmt, SQL_UNBIND);
	SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_STATUS_PTR, NULL, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR, NULL, 0);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDESC	desc = SQL_NULL_HDESC;

	/*
	 * With UseDeclareFetch=1, SQL_REFRESH cannot re-read the rows of a
	 * generate_series() result and reports SQL_ROW_ERROR for them.
	 */
	test_connect_ext("UseDeclareFetch=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* statement attribute */
	set_row_array_size(hstmt, 0);
	print_row_array_size(hstmt, "after 0");
	fetch_and_refresh(hstmt);
	set_row_array_size(hstmt, 2);
	print_row_array_size(hstmt, "after 2");
	set_row_array_size(hstmt, -1);
	print_row_array_size(hstmt, "after -1");
	fetch_and_refresh(hstmt);

	/* descriptor field of the implicit ARD */
	set_ard_array_size(hstmt, 2);
	print_row_array_size(hstmt, "after ARD 2");
	set_ard_array_size(hstmt, 0);
	print_row_array_size(hstmt, "after ARD 0");
	fetch_and_refresh(hstmt);

	/* an explicitly allocated descriptor used as the ARD */
	rc = SQLAllocHandle(SQL_HANDLE_DESC, conn, &desc);
	CHECK_CONN_RESULT(rc, "failed to allocate desc handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, desc, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr APP_ROW_DESC failed", hstmt);
	print_row_array_size(hstmt, "explicit ARD");
	fetch_and_refresh(hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, SQL_NULL_HDESC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr APP_ROW_DESC failed", hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_DESC, desc);

	test_disconnect();

	return 0;
}
