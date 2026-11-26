/*
 * With statement-level rollback (Protocol 7.4-2), a statement that fails is
 * rolled back alone: a row deleted earlier in the same transaction with
 * SQLSetPos(SQL_DELETE) stays deleted, on a partitioned table as on a plain
 * one.
 */
#include <stdio.h>

#include "common.h"

static HSTMT hstmt = SQL_NULL_HSTMT;

static void
exec_ok(const char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static void
commit(void)
{
	SQLRETURN	rc;

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
}

static void
reset_orders(const char *orders_table)
{
	char		sql[100];

	snprintf(sql, sizeof(sql), "TRUNCATE %s", orders_table);
	exec_ok(sql);
	snprintf(sql, sizeof(sql), "INSERT INTO %s VALUES (1), (2)", orders_table);
	exec_ok(sql);
	commit();
}

static HSTMT
open_cursor_on_order_1(const char *orders_table)
{
	SQLRETURN	rc;
	HSTMT		cursor = SQL_NULL_HSTMT;
	char		sql[100];

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &cursor);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(cursor, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", cursor);
	rc = SQLSetStmtAttr(cursor, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", cursor);

	snprintf(sql, sizeof(sql), "SELECT id FROM %s ORDER BY id", orders_table);
	rc = SQLExecDirect(cursor, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", cursor);
	rc = SQLFetchScroll(cursor, SQL_FETCH_FIRST, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", cursor);

	return cursor;
}

/* a refused note is reported, and the transaction goes on */
static void
insert_note(SQLINTEGER note_id)
{
	SQLRETURN	rc;
	SQLLEN		ind = 0;
	SQLCHAR		sqlstate[6] = "";

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
						  0, 0, &note_id, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO posdel_notes VALUES (?)", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("  note %d inserted\n", (int) note_id);
	else
	{
		SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, NULL, NULL, 0, NULL);
		printf("  note %d refused, SQLSTATE=%s\n", (int) note_id, sqlstate);
	}
	SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static void
print_orders(const char *orders_table)
{
	SQLRETURN	rc;
	SQLINTEGER	order_id;
	SQLLEN		ind;
	char		sql[100];

	snprintf(sql, sizeof(sql), "SELECT id FROM %s ORDER BY id", orders_table);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &order_id, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	printf("  orders left after commit:");
	while (SQL_SUCCEEDED(SQLFetch(hstmt)))
		printf(" %d", (int) order_id);
	printf("\n");
	SQLFreeStmt(hstmt, SQL_UNBIND);
	SQLFreeStmt(hstmt, SQL_CLOSE);
	commit();
}

static void
delete_order_1_then_insert_note(const char *orders_table, SQLINTEGER note_id)
{
	SQLRETURN	rc;
	HSTMT		cursor;

	reset_orders(orders_table);

	cursor = open_cursor_on_order_1(orders_table);
	rc = SQLSetPos(cursor, 1, SQL_DELETE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos(SQL_DELETE) failed", cursor);
	printf("  order 1 deleted with SQLSetPos\n");

	/* must be the very next call after SQLSetPos */
	insert_note(note_id);

	SQLFreeHandle(SQL_HANDLE_STMT, cursor);
	commit();

	print_orders(orders_table);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;

	/* 7.4-2 is the default; it is spelled out for "make installcheck-all" */
	test_connect_ext("Protocol=7.4-2");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	exec_ok("CREATE TEMPORARY TABLE posdel_notes (id int4 PRIMARY KEY)");
	exec_ok("INSERT INTO posdel_notes VALUES (7)");
	exec_ok("CREATE TEMPORARY TABLE posdel_plain (id int4 PRIMARY KEY)");
	exec_ok("CREATE TEMPORARY TABLE posdel_part (id int4 PRIMARY KEY) PARTITION BY RANGE (id)");
	exec_ok("CREATE TEMPORARY TABLE posdel_part_1 PARTITION OF posdel_part FOR VALUES FROM (0) TO (1000)");

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);

	printf("Case 1: partitioned table, the INSERT after the delete succeeds\n");
	delete_order_1_then_insert_note("posdel_part", 8);

	printf("Case 2: plain table, the INSERT after the delete fails\n");
	delete_order_1_then_insert_note("posdel_plain", 7);

	printf("Case 3: partitioned table, the INSERT after the delete fails\n");
	delete_order_1_then_insert_note("posdel_part", 7);

	test_disconnect();

	return 0;
}
