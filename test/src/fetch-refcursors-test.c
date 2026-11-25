/*
 * Test FetchRefcursors setting
 */

#include "common.h"


static void print_all_results(HSTMT hstmt)
{
	int i;
	int rc = SQL_SUCCESS;
	for (i = 1; rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO; i++)
	{
		printf("--%d ", i);
		print_result(hstmt);

		rc = SQLMoreResults(hstmt);
	}
	if (rc != SQL_NO_DATA)
		CHECK_STMT_RESULT(rc, "SQLMoreResults failed", hstmt);
}

static void setup_procedure()
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	printf("Creating procedure 'refproc'\n");

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, "create or replace procedure refproc"
			"(inout num_cursor integer, inout ref1 refcursor default null, inout ref2 refcursor default null) as "
			"$procedure$ \n"
			"DECLARE \n"
			"BEGIN \n"
			"IF num_cursor > 0 THEN \n"
			"    OPEN ref1 FOR SELECT id, t FROM testtab1 ORDER BY id ASC; \n"
			"END IF; \n"
			"IF num_cursor > 1 THEN \n"
			"    OPEN ref2 FOR SELECT t, id FROM testtab1 ORDER BY id DESC; \n"
			"END IF; \n"
			"IF num_cursor > 2 THEN \n"
			"    num_cursor := 2; \n"
			"END IF; \n"
			"END; \n"
			"$procedure$ \n"
			"LANGUAGE plpgsql\n"
			, SQL_NTS);
	CHECK_STMT_RESULT(rc, "create procedure refproc failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	test_disconnect();
}

static void refcursor_test(char* connectparams, SQLUINTEGER autocommit, int numresults)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int	        num_cursor = numresults;

	printf("\n-- TEST using %s, autocommit=%u, numresults=%d\n", connectparams, autocommit, numresults);

	test_connect_ext(connectparams);

	/* Conditionally start a transaction */
	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) (SQLULEN) autocommit, SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetConnectAttr failed", hstmt);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLPrepare(hstmt, "CALL refproc(?)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT_OUTPUT,
						  SQL_C_LONG,	/* value type */
						  SQL_INTEGER,	/* param type */
						  0,			/* column size */
						  0,			/* dec digits */
						  &num_cursor,	/* param value ptr */
						  0,	        /* buffer len */
						  NULL		    /* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecute(hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLExecute failed", SQL_HANDLE_STMT, hstmt);
		return;
	}

	printf("Output param num_cursor is %d\n", num_cursor);
	print_all_results(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	test_disconnect();
}

/*
 * Leave a statement with several refcursor results open across a COMMIT,
 * then close it in the next transaction. The cursors were closed by the
 * COMMIT, so the driver must not try to close them again and break the
 * next transaction.
 */
static void refcursor_close_after_commit_test(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	HSTMT		hstmt2 = SQL_NULL_HSTMT;
	int	        num_cursor = 2;
	SQLINTEGER	count;

	printf("\n-- TEST closing a statement with refcursor results after a commit\n");

	test_connect_ext("Fetch=1;FetchRefcursors=1");

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt2);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt2, "CREATE TEMPORARY TABLE refcursor_commit_tab (id int4)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt2);
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	rc = SQLPrepare(hstmt, "CALL refproc(?)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT_OUTPUT,
						  SQL_C_LONG, SQL_INTEGER, 0, 0,
						  &num_cursor, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* Read only the first result set, leaving the second one untouched */
	printf("--1 ");
	print_result(hstmt);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	/* Next transaction: do some work, then close the old statement */
	rc = SQLExecDirect(hstmt2, "INSERT INTO refcursor_commit_tab VALUES (1)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt2);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	/* The INSERT must have been committed */
	rc = SQLExecDirect(hstmt2, "SELECT count(*) FROM refcursor_commit_tab", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt2);
	rc = SQLFetch(hstmt2);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt2);
	rc = SQLGetData(hstmt2, 1, SQL_C_LONG, &count, sizeof(count), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt2);
	printf("rows committed: %d\n", (int) count);

	rc = SQLFreeStmt(hstmt2, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt2);
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	test_disconnect();
}

int main(int argc, char **argv)
{
	setup_procedure();

	/* Using a fetch cache size of 1 to test multiple fetches per cursor */
	refcursor_test("Fetch=1;FetchRefcursors=0", SQL_AUTOCOMMIT_ON, 2);
	refcursor_test("Fetch=1;FetchRefcursors=1", SQL_AUTOCOMMIT_ON, 2);
	refcursor_test("Fetch=1;FetchRefcursors=1", SQL_AUTOCOMMIT_OFF, 0);
	refcursor_test("Fetch=1;FetchRefcursors=1", SQL_AUTOCOMMIT_OFF, 1);
	refcursor_test("Fetch=1;FetchRefcursors=1", SQL_AUTOCOMMIT_OFF, 2);
	refcursor_test("Fetch=1;FetchRefcursors=1", SQL_AUTOCOMMIT_OFF, 3);

	refcursor_close_after_commit_test();

	return 0;
}
