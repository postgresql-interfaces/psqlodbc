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
			"(inout num_cursor integer, inout ref1 refcursor default 'ref1', inout ref2 refcursor default 'ref2') as "
			"$procedure$ \n"
			"DECLARE \n"
			"BEGIN \n"
			"num_cursor := 2; \n"
			"OPEN ref1 FOR SELECT id, t FROM testtab1 ORDER BY id ASC; \n"
			"OPEN ref2 FOR SELECT t, id FROM testtab1 ORDER BY id DESC; \n"
			"END; \n"
			"$procedure$ \n"
			"LANGUAGE plpgsql\n"
			, SQL_NTS);
	CHECK_STMT_RESULT(rc, "create procedure refproc failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	test_disconnect();
}

static void refcursor_test(char* connectparams, SQLUINTEGER autocommit)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int	        num_cursor = 0;

	printf("\n-- TEST using %s and SQL_ATTR_AUTOCOMMIT=%u\n", connectparams, autocommit);

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

int main(int argc, char **argv)
{
	setup_procedure();

	refcursor_test("FetchRefcursors=0", SQL_AUTOCOMMIT_ON);
	refcursor_test("FetchRefcursors=1", SQL_AUTOCOMMIT_ON);
	refcursor_test("FetchRefcursors=1", SQL_AUTOCOMMIT_OFF);

	return 0;
}
