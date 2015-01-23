#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char *param1;
	SQLLEN cbParam1;

	test_connect_ext("AB=0x08;UseServerSidePrepare=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/**** A simple query with one text param ****/

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE nulldate (d date)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/*
	 * Bind empty string to the date param. In cvt_null_date mode, the driver
	 * maps it to NULL. (In normal mode, the driver fills in the current date)
	 */
	param1 = "";
	cbParam1 = SQL_NTS;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,		/* param type */
						  5,			/* column size */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Execute */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO nulldate VALUES (?)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Check the resulting table */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT d IS NULL FROM nulldate", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check how the NULL comes out when we read it back */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT d FROM nulldate", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
