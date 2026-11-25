/*
 * Test an output parameter bound as SQL_C_DEFAULT with a Unicode SQL type.
 *
 * The driver resolves SQL_C_DEFAULT for a parameter from its SQL type. For
 * SQL_WVARCHAR that is SQL_C_CHAR, and the value it returns in the buffer
 * must be a character string, like the input value of such a parameter.
 * Fetching it as SQL_C_WCHAR instead puts UTF-16 into the buffer, and the
 * application then reads the first character only.
 *
 * The Unicode driver takes SQL_C_WCHAR as the default C type of a Unicode
 * SQL type only for an application that connected through a W function,
 * so the test connects with SQLDriverConnectW.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include <sqlucode.h>

static void
connect_wide(void)
{
	SQLRETURN	rc;
	char		connstr[256];
	SQLWCHAR	connstrW[256];
	size_t		i;

	snprintf(connstr, sizeof(connstr), "DSN=%s", get_test_dsn());
	/* The string is plain ASCII, so widen it character by character */
	for (i = 0; i <= strlen(connstr); i++)
		connstrW[i] = (unsigned char) connstr[i];

	rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
	if (!SQL_SUCCEEDED(rc))
	{
		fprintf(stderr, "SQLAllocHandle(SQL_HANDLE_ENV) failed\n");
		exit(1);
	}
	SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(SQL_HANDLE_DBC) failed", SQL_HANDLE_ENV, env);
		exit(1);
	}
	rc = SQLDriverConnectW(conn, NULL, connstrW, SQL_NTS, NULL, 0, NULL,
						   SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLDriverConnectW failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}
	printf("connected\n");
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		result[32] = "";
	SQLLEN		result_ind = 0;
	char		input[] = "abc";
	SQLLEN		input_ind = SQL_NTS;

	connect_wide();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_OUTPUT, SQL_C_DEFAULT,
						  SQL_WVARCHAR, sizeof(result) - 1, 0,
						  result, sizeof(result), &result_ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
						  SQL_VARCHAR, sizeof(input) - 1, 0,
						  input, sizeof(input), &input_ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "{? = call upper(?)}", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	printf("output parameter = \"%s\", length %ld\n", result, (long) result_ind);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();
	return 0;
}
