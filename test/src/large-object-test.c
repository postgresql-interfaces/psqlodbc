#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
printhex(unsigned char *b, int len)
{
	int i;

	printf("hex: ");
	for (i = 0; i < len; i++)
		printf("%02X", b[i]);
}

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char param1[20] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	SQLLEN cbParam1;
	char buf[100];
	SQLLEN ind;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/**** Insert a Large Object */
	printf("inserting large object...\n");
	rc = SQLPrepare(hstmt, (SQLCHAR *) "INSERT INTO lo_test_tab VALUES (1, ?)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* bind LO param  */
	cbParam1 = 8;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_BINARY,	/* value type */
						  SQL_LONGVARBINARY,	/* param type */
						  200,			/* column size */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/**** Read it back ****/
	printf("reading it back...\n");
	SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id, large_data FROM lo_test_tab WHERE id = 1", SQL_NTS);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetData(hstmt, 2, SQL_C_BINARY, buf, sizeof(buf), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);

	printhex(buf, ind);
	printf("\n");
	
	/* Clean up */
	test_disconnect();

	return 0;
}
