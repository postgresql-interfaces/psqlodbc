/*
 * Test the SSLmode keyword of a connection string.
 *
 * The driver maps the value to one of the libpq sslmode names before
 * connecting. A request for TLS must survive the spellings that the
 * connection-string syntax allows (braces, a different case), and a
 * value the driver does not understand must make the connection fail
 * rather than silently turn into "disable".
 *
 * Whether TLS can be negotiated depends on the test server: over a Unix
 * socket libpq never uses it, a server without SSL refuses a mode that
 * requires it, and a server with SSL refuses verify-ca and verify-full
 * without a root certificate. So a connection that succeeds is checked
 * by the mode in the completed connection string, and one that fails
 * must fail in libpq's TLS setup. Both print the same line, so the
 * output does not depend on the server.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
check_sslmode(const char *value, const char *expected)
{
	SQLRETURN	rc;
	SQLHENV		henv = SQL_NULL_HENV;
	SQLHDBC		hdbc = SQL_NULL_HDBC;
	SQLCHAR		outstr[2048];
	SQLSMALLINT	outlen = 0;
	char		connstr[1024];
	char		want[64];
	const char *p;

	snprintf(connstr, sizeof(connstr), "DSN=%s;SSLmode=%s",
			 get_test_dsn(), value);

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
	if (expected == NULL)
	{
		/* the value must be rejected, not mapped to "disable" */
		if (SQL_SUCCEEDED(rc))
		{
			printf("SSLmode=%s: FAILED - connected with [%s]\n",
				   value, (char *) outstr);
			exit(1);
		}
		printf("SSLmode=%s: refused, ok\n", value);
		SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
		return;
	}
	if (!SQL_SUCCEEDED(rc))
	{
		SQLCHAR		sqlstate[6], msg[1024];
		SQLINTEGER	nativeerror;
		SQLSMALLINT	msglen;

		/*
		 * A failure in libpq's TLS setup shows that the value did not
		 * become "disable": "SSL was required" from a server without
		 * SSL, or a missing root certificate for verify-ca and
		 * verify-full.
		 */
		if (strcmp(expected, "disable") != 0 &&
			SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_DBC, hdbc, 1, sqlstate,
										&nativeerror, msg, sizeof(msg), &msglen)) &&
			(strstr((char *) msg, "SSL") != NULL ||
			 strstr((char *) msg, "certificate") != NULL))
		{
			printf("SSLmode=%s: %s, ok\n", value, expected);
			SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
			SQLFreeHandle(SQL_HANDLE_ENV, henv);
			return;
		}
		print_diag("SQLDriverConnect failed", SQL_HANDLE_DBC, hdbc);
		exit(1);
	}

	snprintf(want, sizeof(want), ";SSLmode=%s;", expected);
	p = strstr((char *) outstr, ";SSLmode=");
	if (p == NULL || strncmp(p, want, strlen(want)) != 0)
	{
		printf("SSLmode=%s: FAILED - expected [%s], got [%s]\n",
			   value, want, p ? p : "(none)");
		exit(1);
	}
	printf("SSLmode=%s: %s, ok\n", value, expected);

	SQLDisconnect(hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

int
main(void)
{
	/* the plain names and the driver's own abbreviations */
	check_sslmode("require", "require");
	check_sslmode("verify-ca", "verify-ca");
	check_sslmode("verify-full", "verify-full");
	check_sslmode("vf", "verify-full");
	check_sslmode("disable", "disable");

	/* braces may enclose any connection-string value */
	check_sslmode("{require}", "require");
	check_sslmode("{verify-full}", "verify-full");

	/* the keyword is case-insensitive, so is the value */
	check_sslmode("Require", "require");
	check_sslmode("VERIFY-CA", "verify-ca");
	check_sslmode("Verify-Full", "verify-full");

	/* an unknown value is left to libpq, which rejects it */
	check_sslmode("bogus", NULL);
	check_sslmode("yes", NULL);

	return 0;
}
