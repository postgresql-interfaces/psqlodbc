#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(int argc, char **argv)
{
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLRETURN	rc;
	SQLSMALLINT	nparams = -1;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle(SQL_HANDLE_STMT) failed", conn);

	rc = SQLPrepare(hstmt, (SQLCHAR *) "'abc'", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	rc = SQLNumParams(hstmt, &nparams);
	CHECK_STMT_RESULT(rc, "SQLNumParams failed", hstmt);
	printf("nparams=%d\n", (int) nparams);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	return 0;
}
