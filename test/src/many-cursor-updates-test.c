/*
 * Update a row many times through a cursor that is still open on the
 * server, in one transaction.
 *
 * With UseDeclareFetch=1 the driver keeps a list of the rows updated
 * through the cursor until the cursor is used up. 20480 updates fill the
 * list as far as its size fits a 16-bit number, so the next update makes
 * it grow past that. A 16-bit size wraps to 0 there, and the loop that
 * doubles it never ends.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define UPDATES 20500

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	id;
	char		t[32];
	SQLLEN		id_ind, t_ind;
	int			i;

	test_connect_ext("UseDeclareFetch=1;Fetch=10");
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE many_updates (id int4 PRIMARY KEY, t text)",
		SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	/* more rows than one block, so that the cursor stays open */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO many_updates "
		"SELECT g, 'row ' || g FROM generate_series(1, 100) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT id, t FROM many_updates ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &id, 0, &id_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, t, sizeof(t), &t_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);

	for (i = 0; i < UPDATES; i++)
	{
		strcpy(t, "updated");
		t_ind = SQL_NTS;
		rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
		if (!SQL_SUCCEEDED(rc))
		{
			printf("update %d failed\n", i + 1);
			print_diag("", SQL_HANDLE_STMT, hstmt);
			break;
		}
	}
	printf("%d updates done\n", i);

	SQLFreeStmt(hstmt, SQL_CLOSE);
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
