#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char sql[100000];
	char *sqlend;
	int			i;
	/*
	 * NOTE: in the psqlodbc, we assume that SQL_C_LONG actually means a
	 * variable of type SQLINTEGER. They are not the same on platforms where
	 * "long" is a 64-bit integer. That seems a bit bogus, but it's too late
	 * to change that without breaking applications that depend on it.
	 * (on little-endian systems, you won't notice the difference if you reset
	 * the high bits to zero before calling SQLBindCol.)
	 */
	SQLINTEGER longvalue;
	SQLLEN		indLongvalue;
	char	charvalue[100];
	SQLLEN	indCharvalue;

	test_connect();

	rc = SQLAllocStmt(conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &longvalue, 0, &indLongvalue);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, &charvalue, sizeof(charvalue), &indCharvalue);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT * FROM testtab1 ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	printf("Result set:\n");
	while(1)
	{
		rc = SQLFetch(hstmt);
		if (rc == SQL_NO_DATA)
			break;
		if (rc == SQL_SUCCESS)
		{
			char buf[40];
			int i;
			SQLLEN ind;

			printf("%ld %s\n", longvalue, charvalue);
		}
		else
		{
			print_diag("SQLFetch failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();
}
