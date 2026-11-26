/*
 * Test a backslash escape followed by a multibyte character in an E''
 * literal.
 *
 * In E'\é' the backslash escapes the multibyte character, and the next
 * quote ends the literal. The driver's scanners must agree with the
 * server, or they count parameter markers inside or outside the literal
 * wrongly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* E'\é' in UTF-8; é is 0xc3 0xa9 */
#define QUERY	"SELECT E'\\\xc3\xa9' AS a, ? AS b"

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLSMALLINT	nparams;
	char		param[] = "param";
	SQLLEN		ind = SQL_NTS;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLPrepare(hstmt, (SQLCHAR *) QUERY, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	rc = SQLNumParams(hstmt, &nparams);
	CHECK_STMT_RESULT(rc, "SQLNumParams failed", hstmt);
	printf("SQLNumParams: %d\n", (int) nparams);

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
						  20, 0, param, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLExecute(hstmt);
	if (SQL_SUCCEEDED(rc))
	{
		/* print only column b: column a depends on the client encoding */
		print_result_series(hstmt, (SQLSMALLINT []) {2}, 1, -1, FALSE);
	}
	else
		print_diag("SQLExecute failed", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
