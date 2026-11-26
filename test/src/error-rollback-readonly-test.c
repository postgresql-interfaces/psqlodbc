/*
 * Statement-level rollback (Protocol 7.4-2) when the ODBC call just before the
 * failing statement was read-only, here SQLProcedureColumns.
 *
 * The driver used to skip the SAVEPOINT after a read-only call, and then
 * forgot that a savepoint was available, so a failing statement in that
 * position rolled back the whole transaction instead of just itself, silently
 * discarding earlier work.  It now sets a savepoint there as anywhere else.
 * Only the failing statement may be rolled back.
 *
 * Transactions 3 and 4 follow a fetch on a keyset-driven cursor, another
 * read-only call: a positioned update that succeeds must survive a later
 * failing INSERT, and a failing SQLBulkOperations(SQL_ADD) must not undo the
 * INSERT before it.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static HSTMT hstmt = SQL_NULL_HSTMT;
static HSTMT hcursor = SQL_NULL_HSTMT;
static SQLINTEGER cursor_value;
static SQLLEN cursor_ind;

static void
insert_value(SQLINTEGER value)
{
	SQLRETURN	rc;
	SQLCHAR		sqlstate[6];

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
						  0, 0, &value, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO rbtab VALUES (?)", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("  insert %d: ok\n", (int) value);
	else
	{
		SQLGetDiagField(SQL_HANDLE_STMT, hstmt, 1, SQL_DIAG_SQLSTATE,
						sqlstate, sizeof(sqlstate), NULL);
		printf("  insert %d: SQLSTATE=%s\n", (int) value, sqlstate);
	}
	SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
}

static void
look_up_procedure_columns(void)
{
	SQLRETURN	rc;

	rc = SQLProcedureColumns(hstmt, NULL, 0, NULL, 0,
							 (SQLCHAR *) "abs", SQL_NTS, NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLProcedureColumns failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
	printf("  SQLProcedureColumns: ok\n");
}

/* open a keyset-driven cursor on rbtab and fetch its first row */
static void
fetch_with_cursor(void)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hcursor, (SQLCHAR *) "SELECT i FROM rbtab ORDER BY i", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hcursor);
	rc = SQLFetch(hcursor);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hcursor);
	printf("  fetch: %d\n", (int) cursor_value);
}

static void
setpos_update(SQLINTEGER value)
{
	SQLRETURN	rc;

	cursor_value = value;
	cursor_ind = sizeof(cursor_value);
	rc = SQLSetPos(hcursor, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos failed", hcursor);
	printf("  SQLSetPos update to %d: ok\n", (int) value);
	SQLFreeStmt(hcursor, SQL_CLOSE);
}

static void
bulk_add(SQLINTEGER value)
{
	SQLRETURN	rc;
	SQLCHAR		sqlstate[6];

	cursor_value = value;
	cursor_ind = sizeof(cursor_value);
	rc = SQLBulkOperations(hcursor, SQL_ADD);
	if (SQL_SUCCEEDED(rc))
		printf("  SQLBulkOperations add %d: ok\n", (int) value);
	else
	{
		SQLGetDiagField(SQL_HANDLE_STMT, hcursor, 1, SQL_DIAG_SQLSTATE,
						sqlstate, sizeof(sqlstate), NULL);
		printf("  SQLBulkOperations add %d: SQLSTATE=%s\n", (int) value, sqlstate);
	}
	SQLFreeStmt(hcursor, SQL_CLOSE);
}

static void
commit(void)
{
	SQLRETURN	rc;

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
}

static void
print_committed_rows(void)
{
	SQLRETURN	rc;
	SQLCHAR		rows[32];

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
					   "SELECT coalesce(string_agg(i::text, ' ' ORDER BY i), '') FROM rbtab",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, rows, sizeof(rows), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
	printf("  committed rows: %s\n", rows);
}

static void
run_transaction(const char *label, int variant)
{
	SQLRETURN	rc;

	printf("%s\n", label);
	insert_value(1);
	switch (variant)
	{
		case 1:
			look_up_procedure_columns();
			break;
		case 2:
			fetch_with_cursor();
			setpos_update(5);
			break;
		case 3:
			fetch_with_cursor();
			bulk_add(-1);		/* refused by CHECK (i > 0) */
			break;
	}
	if (variant != 3)
		insert_value(-1);		/* the end user's value, refused by CHECK (i > 0) */
	insert_value(2);
	commit();
	print_committed_rows();

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "DELETE FROM rbtab", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	commit();
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;

	test_connect_ext("Protocol=7.4-2;UpdatableCursors=1");
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
					   "CREATE TEMPORARY TABLE rbtab (i int4 CHECK (i > 0))",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	commit();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hcursor);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hcursor, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hcursor);
	rc = SQLSetStmtAttr(hcursor, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hcursor);
	rc = SQLBindCol(hcursor, 1, SQL_C_SLONG, &cursor_value, 0, &cursor_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hcursor);

	run_transaction("Transaction 1: insert, failing insert, insert", 0);
	run_transaction("Transaction 2: insert, SQLProcedureColumns, failing insert, insert", 1);
	run_transaction("Transaction 3: insert, fetch, SQLSetPos update, failing insert, insert", 2);
	run_transaction("Transaction 4: insert, fetch, failing SQLBulkOperations add, insert", 3);

	test_disconnect();

	return 0;
}
