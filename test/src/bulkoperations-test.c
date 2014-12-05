/*
 * Test SQLBulkOperations()
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

void
printCurrentRow(HSTMT hstmt)
{
	char		buf[40];
	int			col;
	SQLLEN		ind;
	int			rc;

	for (col = 1; col <= 2; col++)
	{
		rc = SQLGetData(hstmt, col, SQL_C_CHAR, buf, sizeof(buf), &ind);
		if (!SQL_SUCCEEDED(rc))
		{
			print_diag("SQLGetData failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}
		if (ind == SQL_NULL_DATA)
			strcpy(buf, "NULL");
		printf("%s%s", (col > 1) ? "\t" : "", buf);
	}
	printf("\n");
}

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int			i;
	SQLINTEGER	colvalue1;
	SQLINTEGER	colvalue2;
	SQLLEN		indColvalue1;
	SQLLEN		indColvalue2;
	char		bookmark[100];
	SQLLEN		bookmark_ind;

	test_connect_ext("UpdatableCursors=1;UseDeclareFetch=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Initialize a table with some test data.
	 */
	printf("Creating test table bulkoperations_test\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE bulkoperations_test(i int4, orig int4)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO bulkoperations_test SELECT g, g FROM generate_series(1, 10) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	printf("Opening a cursor for update, and fetching 10 rows\n");

	rc  = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						 (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	/* Enable bookmarks */
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_USE_BOOKMARKS,
						(SQLPOINTER) SQL_UB_VARIABLE, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	rc = SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, &bookmark, sizeof(bookmark), &bookmark_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &colvalue1, 0, &indColvalue1);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_LONG, &colvalue2, 0, &indColvalue2);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT * FROM bulkoperations_test ORDER BY orig", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	for (i = 0; i < 5; i++)
	{
		rc = SQLFetch(hstmt);
		if (rc == SQL_NO_DATA)
			break;
		if (rc == SQL_SUCCESS)
			printCurrentRow(hstmt);
		else
		{
			print_diag("SQLFetch failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}
	}

	/* Do a positioned update and delete */
	printf("\nUpdating result set\n");
	colvalue1 += 100;

	rc = SQLBulkOperations(hstmt, SQL_UPDATE_BY_BOOKMARK);
	CHECK_STMT_RESULT(rc, "SQLBulkOperations failed", hstmt);

	/* Have to use an absolute position after SQLBulkOperations. */
	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, 8);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);

	rc = SQLBulkOperations(hstmt, SQL_DELETE_BY_BOOKMARK);
	CHECK_STMT_RESULT(rc, "SQLBulkOperations failed", hstmt);

	/* Have to use an absolute position after SQLBulkOperations. */
	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, 5);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);

	/* Print the updated row */
	printCurrentRow(hstmt);

	/* Perform an insertion */
	colvalue1 = 1234;
	colvalue2 = 5678;
	rc = SQLBulkOperations(hstmt, SQL_ADD);
	CHECK_STMT_RESULT(rc, "SQLBulkOperations failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/**** See if the updates really took effect ****/
	printf("\nQuerying the table again\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT * FROM bulkoperations_test ORDER BY orig", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
