/*
 * Test describing a prepared statement before its parameters are filled in.
 *
 * The buffers given to SQLBindParameter() are deferred: the application only
 * has to fill them before SQLExecute(). Describing the statement with
 * SQLNumResultCols() or SQLDescribeCol() before that must not read them.
 * With UseServerSidePrepare=0, the driver used to send the current content
 * of the buffers to the server as literals to describe the statement.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
run_test(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		param1[20];
	SQLSMALLINT	colcount;
	SQLCHAR		colname[50];
	SQLSMALLINT	colnamelen;
	SQLSMALLINT	datatype;
	SQLULEN		colsize;
	SQLSMALLINT	decdigits;
	SQLSMALLINT	nullable;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT id, t FROM testtab1 WHERE id = ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/*
	 * Bind a character buffer as an integer parameter. Whatever the buffer
	 * holds now is not a valid integer; it gets its value only after the
	 * statement has been described.
	 */
	strcpy(param1, "not yet filled in");
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_INTEGER,	/* param type */
						  0,			/* column size */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  sizeof(param1),	/* buffer len */
						  NULL			/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Describe the statement before SQLExecute() */
	rc = SQLNumResultCols(hstmt, &colcount);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);
	printf("# of result cols: %d\n", colcount);

	rc = SQLDescribeCol(hstmt, 1, colname, sizeof(colname), &colnamelen,
						&datatype, &colsize, &decdigits, &nullable);
	CHECK_STMT_RESULT(rc, "SQLDescribeCol failed", hstmt);
	printf("column 1: %s %s\n", colname, datatype_str(datatype));

	/* Now fill in the parameter and execute */
	strcpy(param1, "2");
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int main(int argc, char **argv)
{
	printf("Testing with UseServerSidePrepare=1\n");
	test_connect_ext("UseServerSidePrepare=1");
	run_test();
	test_disconnect();

	printf("Testing with UseServerSidePrepare=0\n");
	test_connect_ext("UseServerSidePrepare=0");
	run_test();
	test_disconnect();

	return 0;
}
