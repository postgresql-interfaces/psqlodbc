/*
 * Test a positioned insert into a table with child tables whose
 * INSERT ... RETURNING reply reports tableoid 0.
 *
 * For such a table the driver asks for "tableoid" as the second key column
 * and uses it to find the child table that took the new row. The driver
 * caches what it knows about the table per connection, so when another
 * session replaces the table with an updatable view that has columns of
 * its own named ctid and tableoid, the cached entry is still used and the
 * view's tableoid value, 0, comes back. The driver must not dereference
 * a NULL pointer for it.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
exec_cmd(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

/*
 * Open an updatable cursor on the table, fetch the first row and add a
 * new one with SQLSetPos(SQL_ADD), leaving the serial column to the
 * database.
 */
static void
positioned_add(const char *label, const char *item)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	id;
	SQLLEN		id_len;
	char		itembuf[64];
	SQLLEN		item_len;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT id, item FROM subclass_tableoid_tab", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &id, 0, &id_len);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, itembuf, sizeof(itembuf), &item_len);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);

	id_len = SQL_IGNORE;
	snprintf(itembuf, sizeof(itembuf), "%s", item);
	item_len = SQL_NTS;
	rc = SQLSetPos(hstmt, 1, SQL_ADD, SQL_LOCK_NO_CHANGE);
	if (SQL_SUCCEEDED(rc))
		printf("%s: SQLSetPos(SQL_ADD) succeeded\n", label);
	else
	{
		printf("%s: SQLSetPos(SQL_ADD) failed\n", label);
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

/* Run DDL from a second connection, so the first one keeps its cache */
static void
other_session(const char **cmds)
{
	SQLRETURN	rc;
	SQLHDBC		conn2;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLCHAR		dsn[1024];
	SQLCHAR		str[1024];
	SQLSMALLINT	strl;

	snprintf(dsn, sizeof(dsn), "DSN=%s;", get_test_dsn());
	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn2);
	CHECK_CONN_RESULT(rc, "failed to allocate dbc handle", conn2);
	rc = SQLDriverConnect(conn2, NULL, dsn, SQL_NTS,
						  str, sizeof(str), &strl, SQL_DRIVER_COMPLETE);
	CHECK_CONN_RESULT(rc, "SQLDriverConnect failed", conn2);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn2, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn2);
	for (; *cmds; cmds++)
		exec_cmd(hstmt, *cmds);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	SQLDisconnect(conn2);
	SQLFreeHandle(SQL_HANDLE_DBC, conn2);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	static const char *swap[] = {
		"DROP TABLE subclass_tableoid_tab CASCADE",
		"CREATE TABLE subclass_tableoid_data (id serial PRIMARY KEY, item text)",
		"INSERT INTO subclass_tableoid_data (item) VALUES ('sample')",
		/* same name, automatically updatable, its own ctid and tableoid */
		"CREATE VIEW subclass_tableoid_tab AS "
		"SELECT id, item, ctid, 0::oid AS tableoid FROM subclass_tableoid_data",
		NULL
	};

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	exec_cmd(hstmt, "DROP TABLE IF EXISTS subclass_tableoid_tab CASCADE");
	exec_cmd(hstmt, "DROP VIEW IF EXISTS subclass_tableoid_tab");
	exec_cmd(hstmt, "DROP TABLE IF EXISTS subclass_tableoid_data");
	exec_cmd(hstmt, "CREATE TABLE subclass_tableoid_tab (id serial PRIMARY KEY, item text)");
	/* the child table sets relhassubclass on subclass_tableoid_tab */
	exec_cmd(hstmt, "CREATE TABLE subclass_tableoid_child () INHERITS (subclass_tableoid_tab)");
	exec_cmd(hstmt, "INSERT INTO subclass_tableoid_tab (item) VALUES ('sample')");

	/* fills the connection's table cache */
	positioned_add("table with child", "first");

	other_session(swap);

	/* served from the cache: tableoid comes back as 0 */
	positioned_add("view after swap", "second");

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT item FROM subclass_tableoid_data ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	exec_cmd(hstmt, "DROP VIEW subclass_tableoid_tab");
	exec_cmd(hstmt, "DROP TABLE subclass_tableoid_data");
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

	test_disconnect();

	return 0;
}
