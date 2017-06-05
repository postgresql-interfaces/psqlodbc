/*
 * Test deleting tuples all with scrolling block cursors BOF ->
 * EOF -> BOF -> ...
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define	TOTAL	120
#define	BLOCK	4

static HSTMT	hstmte = SQL_NULL_HSTMT;

static SQLRETURN delete_loop(HSTMT hstmt)
{
	SQLRETURN	rc;
	BOOL	use_first_last = 0;
	int	delcnt = 0, delsav, loopcnt = 0;
	SQLSMALLINT	orientation = SQL_FETCH_FIRST;
	
	do {
		printf("\torientation=%d delete count=%d\n", orientation, delcnt);
		delsav = delcnt;
		if (use_first_last)
			orientation = (orientation == SQL_FETCH_NEXT ? SQL_FETCH_FIRST : SQL_FETCH_LAST);
		while (rc = SQLFetchScroll(hstmt, orientation, 0), SQL_SUCCEEDED(rc))
		{
			orientation = (orientation == SQL_FETCH_NEXT ? SQL_FETCH_NEXT : (orientation == SQL_FETCH_FIRST ? SQL_FETCH_NEXT : SQL_FETCH_PRIOR));
			rc = SQLSetPos(hstmt, 1, SQL_DELETE, SQL_LCK_NO_CHANGE);
			CHECK_STMT_RESULT(rc, "SQLSetPos delete failed", hstmt);
			delcnt++;
		}
		if (SQL_NO_DATA != rc)
		{
			CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
		}
		orientation = (orientation == SQL_FETCH_NEXT ? SQL_FETCH_PRIOR : SQL_FETCH_NEXT);
		if (++loopcnt == 4)
			SQLExecDirect(hstmte, (SQLCHAR *) "savepoint miho", SQL_NTS);
	} while (delcnt != delsav);
	printf("delete all count %d\n", delcnt);

	return	SQL_SUCCESS;
}

int main(int argc, char **argv)
{
	int		rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int		i, j, k;
	int		count = TOTAL;	
	char		query[100];
	SQLLEN		rowArraySize = BLOCK;
	SQLULEN		rowsFetched;
	SQLINTEGER	id[BLOCK];
	SQLLEN		cbLen[BLOCK];

	/****
     * Run this test with Fetch=37 when UseDeclareFecth=1.
     */
	test_connect_ext("UpdatableCursors=1;Fetch=37");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmte);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle2", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "create temporary table tmptable(id int4 primary key)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect create table failed", hstmt);

	/* insert into a table */
	for (i = 0; i < count; i++)
	{
		snprintf(query, sizeof(query), "insert into tmptable values (%d)", i);
		rc = SQLExecDirect(hstmt, (SQLCHAR *) query, SQL_NTS);
		CHECK_STMT_RESULT(rc, "insert into table failed", hstmt);
	}
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Block cursor
	 */
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR, (SQLPOINTER) &rowsFetched, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROWS_FETCHED_PTR failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &id, 0, cbLen);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) rowArraySize, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROW_ARRAY_SIZE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	rc = SQLSetConnectAttr(conn, SQL_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0);
	
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "select * from tmptable", SQL_NTS);
	CHECK_STMT_RESULT(rc, "select failed", hstmt);
	rc = SQLExecDirect(hstmte, (SQLCHAR *) "savepoint yuuki", SQL_NTS);
	CHECK_STMT_RESULT(rc, "savepoint failed", hstmte);
	/*
	 * Scroll next -> EOF -> prior -> BOF -> next -> EOF ->
	 * ......
	 */
	delete_loop(hstmt);	/* the 1st loop */

	rc = SQLExecDirect(hstmte, (SQLCHAR *) "rollback to yuuki;release yuuki", SQL_NTS);
	CHECK_STMT_RESULT(rc, "rollback failed", hstmte);
	for (i = 0, j = count, i = 0; i < 2; i++)
	{
		for (k = 0; k < rowArraySize; k++)
		{
			cbLen[k] = 0;
			id[k] = j++;
		}
		/* rc = SQLBulkOperations(hstmt, SQL_ADD);
		CHECK_STMT_RESULT(rc, "SQLBulkOperations SQL_ADD failed", hstmt); */
		rc = SQLSetPos(hstmt, 0, SQL_ADD, SQL_LCK_NO_CHANGE);
		CHECK_STMT_RESULT(rc, "SQLSetPos SQL_ADD failed", hstmt);
		if (0 == i)
		{
			rc = SQLExecDirect(hstmte, (SQLCHAR *) "savepoint yuuki", SQL_NTS);
			CHECK_STMT_RESULT(rc, "savpoint failed", hstmte);
		}
	}	
	
	delete_loop(hstmt);	/* the 2nd loop */

	rc = SQLExecDirect(hstmte, (SQLCHAR *) "rollback to yuuki;release yuuki", SQL_NTS);
	CHECK_STMT_RESULT(rc, "rollback failed", hstmte);

	delete_loop(hstmt);	/* the 3rd loop */

	rc = SQLExecDirect(hstmte, (SQLCHAR *) "rollback to miho;release miho", SQL_NTS);
	CHECK_STMT_RESULT(rc, "rollback failed", hstmte);

	delete_loop(hstmt);	/* the 4th loop */

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_STMT_RESULT(rc, "SQLEndTran failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	
	/* Clean up */
	test_disconnect();

	return 0;
}
