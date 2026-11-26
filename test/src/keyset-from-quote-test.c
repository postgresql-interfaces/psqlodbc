/*
 * Test a keyset-driven cursor on a statement in which FROM is followed
 * directly by a quoted table name, without a space.
 *
 * For a keyset-driven cursor the driver builds a second query from the
 * text after FROM. That text starts right after the keyword, whatever
 * byte follows it.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
update_first_row(const char *sql, const char *newval)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLUINTEGER	cursor_type;
	SQLINTEGER	id;
	char		t[20];
	SQLLEN		ind_id, ind_t;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &id, 0, &ind_id);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, t, sizeof(t), &ind_t);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	printf("%s\n", sql);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		return;
	}
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) &cursor_type, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	printf("cursor_type=%d\n", (int) cursor_type);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	printf("fetched %d %s\n", (int) id, t);

	/* a positioned update needs the keyset */
	ind_id = SQL_IGNORE;
	snprintf(t, sizeof(t), "%s", newval);
	ind_t = SQL_NTS;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	if (SQL_SUCCEEDED(rc))
		printf("SQLSetPos(SQL_UPDATE) succeeded\n");
	else
		print_diag("SQLSetPos(SQL_UPDATE) failed", SQL_HANDLE_STMT, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
	SQLFreeStmt(hstmt, SQL_UNBIND);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT id, t FROM keyset_from ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect_ext("UseDeclareFetch=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE keyset_from (id serial, t text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO keyset_from (t) VALUES ('one'), ('two')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	update_first_row("SELECT id, t FROM\"keyset_from\"", "updated");
	update_first_row("SELECT id, t FROM\"keyset_from\" WHERE id > 0 ORDER BY id",
					 "updated again");

	test_disconnect();

	return 0;
}
