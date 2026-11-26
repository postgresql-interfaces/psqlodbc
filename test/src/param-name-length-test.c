#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/*
 * Names parameter 1 through the IPD, prints the name the driver stored, and
 * calls greet() with it.
 */
static void
call_greet(HSTMT hstmt, SQLCHAR *name, SQLINTEGER name_len,
		   SQLCHAR *value, SQLLEN value_len)
{
	SQLRETURN	rc;
	SQLHDESC	ipd;
	SQLCHAR		stored_name[256];

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_VARCHAR,	/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  value,		/* param value ptr */
						  value_len,	/* buffer len */
						  &value_len	/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, &ipd, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);

	rc = SQLSetDescField(ipd, 1, SQL_DESC_NAME, name, name_len);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLSetDescField failed", SQL_HANDLE_DESC, ipd);
		exit(1);
	}

	rc = SQLGetDescField(ipd, 1, SQL_DESC_NAME, stored_name, sizeof(stored_name), NULL);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLGetDescField failed", SQL_HANDLE_DESC, ipd);
		exit(1);
	}
	printf("SQL_DESC_NAME: %s\n", stored_name);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "{call greet(?)}", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char	   *request;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "create or replace function greet(who text) "
					   "returns text as $$ select 'hello ' || who $$ language sql", SQL_NTS);
	CHECK_STMT_RESULT(rc, "create function greet failed", hstmt);

	printf("\n-- name \"who\", SQL_NTS\n");
	call_greet(hstmt, "who", SQL_NTS, "bob", 3);

	/* exactly 7 bytes, no terminator: name and value are slices of it */
	request = malloc(7);
	memcpy(request, "who=bob", 7);
	printf("\n-- name \"who\", first 3 bytes of unterminated \"who=bob\"\n");
	call_greet(hstmt, request, 3, request + 4, 3);
	free(request);

	test_disconnect();

	return 0;
}
