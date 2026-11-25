/*
 * The driver records the C type that SQL_C_DEFAULT stands for when a
 * column is described, and refuses a fetch whose column has another
 * default C type. That record belongs to one result. After SQLMoreResults
 * the same column number is a column of the next result, which can have
 * another type, so a fetch with SQL_C_DEFAULT must not be compared with
 * what was described for the previous result.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLSMALLINT	sqltype;
	char		buf[32];
	SQLLEN		ind = 0;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT 1::int4; SELECT 'abc'::text", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLDescribeCol(hstmt, 1, NULL, 0, NULL, &sqltype, NULL, NULL,
						NULL);
	CHECK_STMT_RESULT(rc, "SQLDescribeCol failed", hstmt);
	printf("first result described as %s\n", datatype_str(sqltype));

	rc = SQLMoreResults(hstmt);
	CHECK_STMT_RESULT(rc, "SQLMoreResults failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	memset(buf, 0, sizeof(buf));
	rc = SQLGetData(hstmt, 1, SQL_C_DEFAULT, buf, sizeof(buf), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData with SQL_C_DEFAULT failed", hstmt);
	/* the Unicode driver returns SQLWCHAR, so print the length only */
	printf("second result fetched with SQL_C_DEFAULT, %s\n",
		   ind > 0 ? "with data" : "without data");

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
