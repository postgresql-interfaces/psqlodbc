/*
 * Test connection strings whose UTF-8 form does not fit in a SQLSMALLINT.
 *
 * SQLDriverConnectW converts the connection string to UTF-8, and the
 * converted byte length must not pass through a SQLSMALLINT. There a string
 * of 32768 to 65535 bytes becomes a negative length (NULL dereference), and
 * a longer one is cut to its length modulo 65536. The last case below is
 * built so that the cut would fall inside a braced value, right behind an
 * escaped "}}", where it makes the parser read the rest of the value as
 * further keywords.
 *
 * Each case connects with a braced Description of the given size and reads
 * application_name back: it must be empty, as no keyword in the value may
 * take effect. Description has no effect on the connection, so the DSN's
 * own password still logs in on a server that checks it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include <sqlucode.h>

static void
check_long_value(const char *label, const char *value_start,
				 size_t padding)
{
	SQLRETURN	rc;
	SQLHENV		henv = SQL_NULL_HENV;
	SQLHDBC		hdbc = SQL_NULL_HDBC;
	SQLHSTMT	hstmt = SQL_NULL_HSTMT;
	SQLCHAR		appname[256];
	SQLLEN		ind = 0;
	char	   *connstr;
	SQLWCHAR   *connstrW;
	size_t		len, i;

	/*
	 * The braces protect the value at the psqlODBC layer; the caller
	 * supplies the doubled closing braces inside it.
	 */
	connstr = malloc(strlen(value_start) + padding + 256);
	if (connstr == NULL)
	{
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	len = sprintf(connstr, "DSN=%s;Description={%s", get_test_dsn(), value_start);
	memset(connstr + len, 'p', padding);
	strcpy(connstr + len + padding, "}");

	/* The string is plain ASCII, so widen it character by character */
	len = strlen(connstr);
	connstrW = malloc((len + 1) * sizeof(SQLWCHAR));
	if (connstrW == NULL)
	{
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	for (i = 0; i <= len; i++)
		connstrW[i] = (unsigned char) connstr[i];

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

	printf("%s: ", label);
	fflush(stdout);

	/*
	 * The Windows driver manager refuses a string longer than 32767
	 * characters with HY090 itself, so the driver never sees it there.
	 */
	rc = SQLDriverConnectW(hdbc, NULL, connstrW, SQL_NTS,
						   NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLDriverConnectW failed", SQL_HANDLE_DBC, hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
		free(connstrW);
		free(connstr);
		return;
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

	printf("connected, application_name=[%s]\n", (char *) appname);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle(SQL_HANDLE_STMT) failed", hstmt);

	rc = SQLDisconnect(hdbc);
	CHECK_CONN_RESULT(rc, "SQLDisconnect failed", hdbc);

	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
	free(connstrW);
	free(connstr);
}

int
main(int argc, char **argv)
{
	check_long_value("short", "x", 1000);
	check_long_value("more than 32767 bytes", "x", 33000);

	/*
	 * 65536 bytes follow the escaped "}}": 65535 of padding and the closing
	 * brace. A length narrowed to a SQLSMALLINT would cut the string there.
	 */
	check_long_value("more than 65535 bytes",
					 "x;pqopt=application_name=injected;y=}}", 65535);

	return 0;
}
