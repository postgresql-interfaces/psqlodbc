/*
 * Test a keyset-driven cursor on a table with an inheritance child.
 *
 * Each table in an inheritance tree has its own ctids, so the key of a
 * row is (ctid, tableoid). When the driver re-reads, deletes or fetches a
 * row by its key, it must not take the child's row at the same ctid for
 * the parent's row.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define	BOOKMARK_SIZE	14

static SQLINTEGER	id;
static char			t[32];
static SQLLEN		id_ind, t_ind;
static char			bookmark[BOOKMARK_SIZE];
static SQLLEN		bookmark_ind;
static SQLUSMALLINT	row_status;

static void
exec_sql(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

static void
exec_sql_in_other_session(const char *sql)
{
	SQLRETURN	rc;
	SQLHDBC		other_conn = SQL_NULL_HDBC;
	HSTMT		other_hstmt = SQL_NULL_HSTMT;
	char		connstr[256];

	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &other_conn);
	CHECK_CONN_RESULT(rc, "failed to allocate conn handle", other_conn);
	snprintf(connstr, sizeof(connstr), "DSN=%s", get_test_dsn());
	rc = SQLDriverConnect(other_conn, NULL, (SQLCHAR *) connstr, SQL_NTS,
						  NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	CHECK_CONN_RESULT(rc, "SQLDriverConnect failed", other_conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, other_conn, &other_hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", other_conn);
	exec_sql(other_hstmt, sql);
	SQLFreeHandle(SQL_HANDLE_STMT, other_hstmt);
	SQLDisconnect(other_conn);
	SQLFreeHandle(SQL_HANDLE_DBC, other_conn);
}

static const char *
row_status_str(SQLUSMALLINT status)
{
	switch (status)
	{
		case SQL_ROW_SUCCESS:
			return "";
		case SQL_ROW_UPDATED:
			return " (updated)";
		case SQL_ROW_DELETED:
			return " (deleted)";
		default:
			return " (other status)";
	}
}

static void
print_row(const char *label, SQLUSMALLINT status, SQLINTEGER rid, const char *rt)
{
	if (status == SQL_ROW_DELETED)
		printf("%s: deleted\n", label);
	else
		printf("%s: %d\t%s%s\n", label, (int) rid, rt, row_status_str(status));
}

static void
fetch_row(HSTMT hstmt, int rownum)
{
	SQLRETURN	rc;
	char		label[16];

	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, rownum);
	if (rc == SQL_NO_DATA)
	{
		printf("row %d: no data\n", rownum);
		return;
	}
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
	snprintf(label, sizeof(label), "row %d", rownum);
	print_row(label, row_status, id, t);
}

static void
print_tables(HSTMT hstmt)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT tableoid::regclass, ctid, id, t FROM inh_parent ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		saved_bookmarks[2][BOOKMARK_SIZE];
	SQLLEN		saved_bookmark_inds[2];
	SQLINTEGER	ids[2];
	char		ts[2][32];
	SQLLEN		id_inds[2], t_inds[2];
	SQLUSMALLINT	row_statuses[2];
	int			i;

	/* the default; with UseDeclareFetch=1 a fetch does not re-read by key */
	test_connect_ext("UseDeclareFetch=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/*
	 * The parent's rows are at ctids (0,1) to (0,3) of inh_parent, the
	 * child's rows at ctids (0,1) to (0,4) of inh_child.
	 */
	exec_sql(hstmt, "DROP TABLE IF EXISTS inh_child, inh_parent");
	exec_sql(hstmt, "CREATE TABLE inh_parent (id int PRIMARY KEY, t text)");
	exec_sql(hstmt, "CREATE TABLE inh_child () INHERITS (inh_parent)");
	exec_sql(hstmt, "INSERT INTO inh_parent VALUES (1, 'p1'), (2, 'p2'), (3, 'p3')");
	exec_sql(hstmt, "INSERT INTO inh_child VALUES (11, 'c1'), (12, 'c2'), (13, 'c3'), (14, 'c4')");

	printf("Opening a keyset-driven cursor on the parent's rows\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER) SQL_UB_VARIABLE, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_STATUS_PTR, &row_status, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, bookmark, sizeof(bookmark), &bookmark_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &id, 0, &id_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, t, sizeof(t), &t_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id, t FROM inh_parent WHERE id < 10 ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	for (i = 1; i <= 3; i++)
	{
		fetch_row(hstmt, i);
		if (i <= 2)
		{
			memcpy(saved_bookmarks[i - 1], bookmark, bookmark_ind);
			saved_bookmark_inds[i - 1] = bookmark_ind;
		}
	}

	/*
	 * Row 1 moves to (0,4) of inh_parent, where inh_child also has a row.
	 * Row 2 is gone but inh_child still has a row at (0,2).
	 */
	printf("\nAnother session updates row 1 and deletes row 2\n");
	exec_sql_in_other_session("UPDATE inh_parent SET t = 'p1 updated' WHERE id = 1");
	exec_sql_in_other_session("DELETE FROM inh_parent WHERE id = 2");

	printf("\nRe-fetching the rows of the cursor\n");
	fetch_row(hstmt, 3);
	fetch_row(hstmt, 2);
	fetch_row(hstmt, 1);

	/* inh_child's row at (0,3) must survive the positioned delete of row 3 */
	printf("\nDeleting row 3 through the cursor\n");
	fetch_row(hstmt, 3);
	rc = SQLSetPos(hstmt, 1, SQL_DELETE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos DELETE failed", hstmt);
	fetch_row(hstmt, 3);

	printf("\nFetching rows 1 and 2 by bookmark\n");
	rc = SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, saved_bookmarks, BOOKMARK_SIZE, saved_bookmark_inds);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, ids, 0, id_inds);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, ts, sizeof(ts[0]), t_inds);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_STATUS_PTR, row_statuses, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) 2, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLBulkOperations(hstmt, SQL_FETCH_BY_BOOKMARK);
	CHECK_STMT_RESULT(rc, "SQLBulkOperations failed", hstmt);
	/* a row that is gone comes back as NULLs */
	for (i = 0; i < 2; i++)
	{
		printf("row %d: ", i + 1);
		if (id_inds[i] == SQL_NULL_DATA)
			printf("NULL\t");
		else
			printf("%d\t", (int) ids[i]);
		if (t_inds[i] == SQL_NULL_DATA)
			printf("NULL\n");
		else
			printf("%s\n", ts[i]);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_UNBIND);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_STATUS_PTR, NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) 1, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER) SQL_UB_OFF, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_READ_ONLY, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_FORWARD_ONLY, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	printf("\nQuerying the tables again\n");
	print_tables(hstmt);

	exec_sql(hstmt, "DROP TABLE inh_child, inh_parent");

	test_disconnect();

	return 0;
}
