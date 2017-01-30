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

#define	BOOKMARK_SIZE	14

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int			i;
	SQLINTEGER	colvalue1;
	SQLINTEGER	colvalue2;
	SQLLEN		indColvalue1;
	SQLLEN		indColvalue2;
	char		bookmark[BOOKMARK_SIZE];
	SQLLEN		bookmark_ind;

	char		saved_bookmarks[3][BOOKMARK_SIZE];
	SQLLEN		saved_bookmark_inds[3];
	SQLINTEGER	colvalues1[3];
	SQLINTEGER	colvalues2[3];
	SQLLEN		indColvalues1[3];
	SQLLEN		indColvalues2[3];

	memset(bookmark, 0x7F, sizeof(bookmark));
	memset(saved_bookmarks, 0xF7, sizeof(saved_bookmarks));

	test_connect_ext("UpdatableCursors=1;Fetch=1");

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

	for (i = 1; i <= 5; i++)
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

		/* Save row # 2's bookmark for fetch test */
		if (i == 2)
		{
			memcpy(saved_bookmarks[0], bookmark, bookmark_ind);
			saved_bookmark_inds[0] = bookmark_ind;
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

	/* remember its bookmark for later fetch */
	memcpy(saved_bookmarks[1], bookmark, bookmark_ind);
	saved_bookmark_inds[1] = bookmark_ind;

	/* Perform an insertion */
	colvalue1 = 1234;
	colvalue2 = 5678;
	rc = SQLBulkOperations(hstmt, SQL_ADD);
	CHECK_STMT_RESULT(rc, "SQLBulkOperations failed", hstmt);

	/* Remember the bookmark of the inserted row */
	memcpy(saved_bookmarks[2], bookmark, bookmark_ind);
	saved_bookmark_inds[2] = bookmark_ind;

	/**** Test bulk fetch *****/
	printf("Testing bulk fetch of original, updated, and inserted rows\n");
	rc = SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, saved_bookmarks, sizeof(bookmark), saved_bookmark_inds);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, colvalues1, 0, indColvalues1);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_LONG, colvalues2, 0, indColvalues2);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) 3, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	rc = SQLBulkOperations(hstmt, SQL_FETCH_BY_BOOKMARK);
	CHECK_STMT_RESULT(rc, "SQLBulkOperations failed", hstmt);

	printf ("row no #2: %d - %d\n", colvalues1[0], colvalues2[0]);
	printf ("updated row: %d - %d\n", colvalues1[1], colvalues2[1]);
	printf ("inserted row: %d - %d\n", colvalues1[2], colvalues2[2]);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) 1, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	/**** See if the updates really took effect ****/
	printf("\nQuerying the table again\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT * FROM bulkoperations_test ORDER BY orig", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
