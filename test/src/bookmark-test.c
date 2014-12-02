#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
printFetchResult(HSTMT hstmt, int rc)
{
	if (SQL_SUCCEEDED(rc))
	{
		char buf[40];
		SQLLEN ind;

		if (rc == SQL_SUCCESS_WITH_INFO)
			print_diag("got SUCCESS_WITH_INFO", SQL_HANDLE_STMT, hstmt);

		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("fetched: %s\n", buf);
	}
	else if (rc == SQL_NO_DATA)
		printf("Fetch: no data found\n"); /* expected */
	else
		CHECK_STMT_RESULT(rc, "Fetch failed", hstmt);
}

static void
testBookmarks(HSTMT hstmt)
{
	char		book_before[100];
	SQLLEN		book_before_ind;
	char		book_middle[100];
	SQLLEN		book_middle_ind;
	int rc;

	/* Make the cursor scrollable */
	rc = SQLSetStmtAttr(hstmt, SQL_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	/* Enable bookmarks */
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_USE_BOOKMARKS,
						(SQLPOINTER) SQL_UB_VARIABLE, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	/*
	 * Fetch a large result set.
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'foo' || g FROM generate_series(1, 3210) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Get a bookmark that points to the beginning of the result set */
	printf("Getting bookmark to beginning of result set...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0);
	printFetchResult(hstmt, rc);

	rc = SQLGetData(hstmt, 0, SQL_C_VARBOOKMARK, book_before, sizeof(book_before), &book_before_ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);

	printf("Moving +100...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, 100);
	printFetchResult(hstmt, rc);

	/* Get a bookmark that points to the middle of the result set */
	printf("Getting bookmark to middle of result set...\n");
	rc = SQLGetData(hstmt, 0, SQL_C_VARBOOKMARK, book_middle, sizeof(book_middle), &book_middle_ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);

	/*** Ok, test SQLFetchScroll with the bookmark ***/

	printf("Moving +100...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, 100);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_BOOKMARK, begin (0)...\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_FETCH_BOOKMARK_PTR,
						(SQLPOINTER) book_before, SQL_IS_POINTER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_BOOKMARK, 0);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_BOOKMARK, begin (10)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_BOOKMARK, 10);
	printFetchResult(hstmt, rc);

	/*** Test SQLFetchScroll with the middle bookmark ***/

	printf("Testing SQL_FETCH_BOOKMARK, middle (0)...\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_FETCH_BOOKMARK_PTR,
						(SQLPOINTER) book_middle, SQL_IS_POINTER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_BOOKMARK, 0);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_BOOKMARK, middle (10)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_BOOKMARK, 10);
	printFetchResult(hstmt, rc);

	printf("Testing SQL_FETCH_BOOKMARK, middle (-10)...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_BOOKMARK, -10);
	printFetchResult(hstmt, rc);

	/*** Test getting a bookmark with SQLBindCol */
	printf("Getting bookmark with SQLBindCol...\n");
	rc = SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, book_middle, sizeof(book_middle), &book_middle_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, 100);
	printFetchResult(hstmt, rc);

	printf("Unbinding boomark column...\n");
	rc = SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, NULL, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	printf("Moving +100...\n");
	rc = SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, 100);
	printFetchResult(hstmt, rc);

	printf("Goind back to bookmark acquired with SQLBindCol...\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_FETCH_BOOKMARK_PTR,
						(SQLPOINTER) book_middle, SQL_IS_POINTER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	rc = SQLFetchScroll(hstmt, SQL_FETCH_BOOKMARK, 0);
	printFetchResult(hstmt, rc);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	testBookmarks(hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
