/*
 * Test escaping of special characters in connection-string values.
 *
 * A connection string is processed by two independent parsers with
 * different escaping rules (see docs/config.html):
 *
 *   1. The psqlODBC parser splits on ';' and '=', and uses braces {} to
 *      protect a value that contains those characters or spaces.
 *   2. The braced pqopt value is then handed to libpq, whose conninfo
 *      parser separates parameters by spaces and uses backslash and
 *      single-quote quoting to embed a space in a value.
 *
 * Each case below sets application_name through one of these mechanisms and
 * reads it back with current_setting() to confirm the special character
 * survived intact.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
check_application_name(const char *label, const char *pqopt_value,
					   const char *expected)
{
	SQLRETURN	rc;
	SQLHENV		henv = SQL_NULL_HENV;
	SQLHDBC		hdbc = SQL_NULL_HDBC;
	SQLHSTMT	hstmt = SQL_NULL_HSTMT;
	SQLCHAR		outstr[256];
	SQLSMALLINT	outlen = 0;
	SQLCHAR		appname[256];
	SQLLEN		ind = 0;
	char		connstr[1024];

	/*
	 * The braces protect the whole libpq value at the psqlODBC layer; the
	 * caller supplies the libpq-level escaping inside the braces.
	 */
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;pqopt={application_name=%s}",
			 get_test_dsn(), pqopt_value);

	rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
	if (!SQL_SUCCEEDED(rc))
	{
		fprintf(stderr, "SQLAllocHandle(SQL_HANDLE_ENV) failed\n");
		exit(1);
	}

	rc = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION,
					   (SQLPOINTER) SQL_OV_ODBC3, 0);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLSetEnvAttr failed", SQL_HANDLE_ENV, henv);
		exit(1);
	}

	rc = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(SQL_HANDLE_DBC) failed",
				   SQL_HANDLE_ENV, henv);
		exit(1);
	}

	rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) connstr, SQL_NTS,
						  outstr, sizeof(outstr), &outlen,
						  SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLDriverConnect failed", SQL_HANDLE_DBC, hdbc);
		exit(1);
	}

	rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle(SQL_HANDLE_STMT) failed", hdbc);

	rc = SQLExecDirect(hstmt,
					   (SQLCHAR *) "SELECT current_setting('application_name')",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, appname, sizeof(appname), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);

	if (strcmp((char *) appname, expected) != 0)
	{
		printf("%s: FAILED - expected [%s], got [%s]\n",
			   label, expected, (char *) appname);
		exit(1);
	}
	printf("%s: ok\n", label);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	SQLDisconnect(hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

int
main(void)
{
	/* psqlODBC layer: braces protect the ';' attribute delimiter */
	check_application_name("braces protect semicolon", "a;b", "a;b");

	/* libpq layer: backslash escapes a space inside the value */
	check_application_name("libpq backslash escape", "my\\ app", "my app");

	/* libpq layer: single quotes quote a value containing a space */
	check_application_name("libpq single-quote quoting", "'my app'", "my app");

	return 0;
}
