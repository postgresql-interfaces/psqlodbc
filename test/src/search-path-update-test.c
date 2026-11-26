#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
exec_sql(char *sql)
{
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int			rc;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, sql, hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

static void
create_tenants(void)
{
	exec_sql("DROP SCHEMA IF EXISTS tenant_a, tenant_b CASCADE");
	exec_sql("CREATE SCHEMA tenant_a");
	exec_sql("CREATE SCHEMA tenant_b");
	exec_sql("CREATE TABLE tenant_a.orders (id int PRIMARY KEY, note text)");
	exec_sql("CREATE TABLE tenant_b.orders (id int PRIMARY KEY, note text)");
	exec_sql("INSERT INTO tenant_a.orders VALUES (1, 'order of tenant_a')");
	exec_sql("INSERT INTO tenant_b.orders VALUES (1, 'order of tenant_b')");
}

static void
switch_tenant_with_set(char *tenant)
{
	char		sql[100];

	snprintf(sql, sizeof(sql), "SET search_path TO %s", tenant);
	exec_sql(sql);
}

static void
switch_tenant_with_set_config(char *tenant)
{
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLLEN		cbTenant = SQL_NTS;
	int			rc;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
						  63, 0, tenant, 0, &cbTenant);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT set_config('search_path', ?, false)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "set_config failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

/*
 * A request of one user: show the order through an updatable cursor on the
 * unqualified table "orders" and, if new_note is given, store it there.
 */
static void
edit_order(char *tenant, char *new_note)
{
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	id;
	char		note[100];
	SQLLEN		indId, indNote;
	int			rc;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &id, 0, &indId);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, note, sizeof(note), &indNote);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id, note FROM orders", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	printf("user of %s sees order %d: %s\n", tenant, (int) id, note);

	if (new_note)
	{
		strcpy(note, new_note);
		indNote = SQL_NTS;
		rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
		CHECK_STMT_RESULT(rc, "SQLSetPos UPDATE failed", hstmt);
		printf("user of %s sets its note to: %s (SQLSetPos returned %s)\n", tenant, new_note,
			   rc == SQL_SUCCESS ? "SQL_SUCCESS" : "SQL_SUCCESS_WITH_INFO");
		if (rc == SQL_SUCCESS_WITH_INFO)
			print_diag(NULL, SQL_HANDLE_STMT, hstmt);
	}
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

static void
print_orders(void)
{
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int			rc;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
					   "SELECT 'tenant_a.orders', id, note FROM tenant_a.orders"
					   " UNION ALL "
					   "SELECT 'tenant_b.orders', id, note FROM tenant_b.orders"
					   " ORDER BY 1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

/*
 * The application serves the users of every tenant and points search_path
 * at the user's schema before each request. The note set by the user of
 * tenant_b must end up in tenant_b.orders, the table its cursor showed.
 */
int main(int argc, char **argv)
{
	printf("\nThe application switches search_path with SET\n");
	test_connect();
	create_tenants();
	switch_tenant_with_set("tenant_a");
	edit_order("tenant_a", NULL);
	switch_tenant_with_set("tenant_b");
	edit_order("tenant_b", "set by tenant_b after SET");
	print_orders();
	test_disconnect();

	/* a new connection and new tables, without state from the earlier case */
	printf("\nThe application switches search_path with set_config()\n");
	test_connect();
	create_tenants();
	switch_tenant_with_set_config("tenant_a");
	edit_order("tenant_a", NULL);
	switch_tenant_with_set_config("tenant_b");
	edit_order("tenant_b", "set by tenant_b after set_config()");
	print_orders();

	exec_sql("DROP SCHEMA tenant_a, tenant_b CASCADE");
	test_disconnect();
	return 0;
}
