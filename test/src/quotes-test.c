/* Test parameter quoting, with standard_conforming_strings on/off */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
execWithParam(HSTMT hstmt, char *sql, char *param)
{
	SQLLEN		cbParam1;
	int			rc;

	printf("Executing: %s with param: %s\n", sql, param);

	/* bind param  */
	cbParam1 = SQL_NTS;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,		/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  param,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

void
runtest(HSTMT hstmt, int scs)
{
	/* Turn standard_conforming_strings on or off, as requested by caller */
	char		sql[50];
	int			rc;

	snprintf(sql, sizeof(sql), "SET standard_conforming_strings=%s",
			 scs ? "on" : "off");
	printf("\n%s\n", sql);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Check that the driver escapes quotes correctly when sending
	 * parameters to the server. (This is mostly of concern with
	 * UseServerSidePrepare=0, but it's worth checking with
	 * UseServerSidePrepare=1 too, to make sure that the driver doesn't
	 * incorrectly quote values sent as out-of-band parameters when it
	 * shouldn't do so.
	 */

	execWithParam(hstmt, "SELECT 'foo', ?::text", "param'quote");
	execWithParam(hstmt, "SELECT 'foo', ?::text", "param\\backslash");
	execWithParam(hstmt, "SELECT 'foo', ?::text", "ends with backslash\\");

	/*
	 * Check that the driver's built-in parser interprets quotes
	 * correctly. It needs to know about quoting so that it can
	 * distinguish between ? parameter markers and ? question marks
	 * within string literals.
	 */
	execWithParam(hstmt, "SELECT 'doubled '' quotes', ?::text", "param");
	execWithParam(hstmt, "SELECT E'escaped quote\\' here', ?::text", "param");
	execWithParam(hstmt, "SELECT $$dollar quoted string$$, ?::text", "param");
	execWithParam(hstmt, "SELECT $xx$complex $dollar quotes$xx$, ?::text", "param");
	execWithParam(hstmt, "SELECT $dollar$morecomplex $dollar quotes$dollar$, ?::text", "param");
	/*
	 * With standards_conforming_strings off, also test backslash escaping
	 * without the E'' syntax.
	 */
	if (!scs)
		execWithParam(hstmt, "SELECT 'escaped quote\\' here', ?::text", "param");
}

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	runtest(hstmt, 1);
	runtest(hstmt, 0);

	/* Clean up */
	test_disconnect();

	return 0;
}
