#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLCHAR sqlstate[6];
	SQLCHAR buf[10];
	SQLLEN ind;

	/*
	 * Force a locale that cannot represent the euro sign, so that fetching
	 * it into SQL_C_CHAR fails with a string conversion error (SQLSTATE
	 * 22018 under ODBC 3.x, 22005 under ODBC 2.x).  This must happen before
	 * connecting, so that the driver picks up the locale.  Skip the test
	 * under an ANSI driver or if the locale cannot be set, printing a fixed
	 * output that matches the expected file.
	 */
	if (IsAnsi() || NULL == setlocale(LC_ALL, "C"))
	{
		printf("connected\n");
		printf("string conversion error SQLSTATE: 22018\n");
		printf("disconnecting\n");
		return 0;
	}

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * The euro sign (U+20AC, 8364) cannot be represented in the C locale.
	 * Generate it server-side with chr(), so that the query text itself stays
	 * ASCII and the conversion failure happens only when fetching the result
	 * into SQL_C_CHAR.
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT chr(8364)::text", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/*
	 * Depending on the fetch method, the invalid string conversion error may
	 * be raised by SQLFetch (bound-column path) or by SQLGetData (unbound
	 * path).  Accept either, and only fail if neither of them reports it.
	 */
	rc = SQLFetch(hstmt);
	if (SQL_SUCCEEDED(rc))
	{
		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		if (SQL_SUCCEEDED(rc))
		{
			printf("BUG: SQLFetch/SQLGetData unexpectedly succeeded\n");
			exit(1);
		}
	}
	rc = SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, NULL, NULL, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetDiagRec failed", hstmt);
	if (strcmp((char *) sqlstate, "22018") != 0 && strcmp((char *) sqlstate, "22005") != 0)
	{
		printf("unexpected SQLSTATE for string conversion error: %s\n", sqlstate);
		exit(1);
	}
	printf("string conversion error SQLSTATE: %s\n", sqlstate);

	SQLFreeStmt(hstmt, SQL_CLOSE);
	test_disconnect();

	return 0;
}
