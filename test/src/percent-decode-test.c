/*
 * Test percent decoding in connection-string values.
 *
 * Malformed percent escapes should not be decoded.  Before the fix,
 * pqopt=application_name=% reached conv_from_hex("%") and triggered
 * sanitizer-detected undefined behavior while SQLDriverConnect() parsed the
 * connection string.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
check_pqopt_value(const char *label, const char *application_name)
{
	SQLRETURN	rc;
	SQLHENV		henv = SQL_NULL_HENV;
	SQLHDBC		hdbc = SQL_NULL_HDBC;
	SQLCHAR		outstr[256];
	SQLSMALLINT	outlen = 0;
	char		connstr[1024];

	snprintf(connstr, sizeof(connstr), "DSN=%s;pqopt=application_name=%s",
			 get_test_dsn(), application_name);

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
	SQLDisconnect(hdbc);

	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);

	printf("%s: ok\n", label);
}

int
main(void)
{
	check_pqopt_value("truncated percent", "%");
	check_pqopt_value("one hex digit", "%A");
	check_pqopt_value("non-hex escape", "%G1");
	check_pqopt_value("valid percent escape", "%20");

	return 0;
}
