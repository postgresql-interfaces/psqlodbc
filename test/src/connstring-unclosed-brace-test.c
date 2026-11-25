/*
 * Test that a braced connection-string value which is never closed is
 * rejected.
 *
 * A braced value may contain ';' and a literal '}' is written as "}}"
 * (see docs/config.html). When the string ends inside such a value, the
 * parser must fail instead of handing the tokenizer a position inside the
 * value: the text after the first ';' would otherwise be parsed a second
 * time as attribute=value pairs, so a value such as a password could set
 * pqopt, ConnSettings, Servername or any other attribute.
 *
 * Each case below connects with a Description value that carries a pqopt
 * attribute and reads application_name back to see whether the attribute
 * took effect. Description has no effect on the connection, so the DSN's
 * own password still logs in on a server that checks it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
try_connect(const char *label, const char *attributes)
{
	SQLRETURN	rc;
	SQLHENV		henv = SQL_NULL_HENV;
	SQLHDBC		hdbc = SQL_NULL_HDBC;
	SQLHSTMT	hstmt = SQL_NULL_HSTMT;
	SQLCHAR		appname[256];
	SQLLEN		ind = 0;
	char		connstr[1024];

	snprintf(connstr, sizeof(connstr), "DSN=%s;%s", get_test_dsn(), attributes);

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
	rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) connstr, SQL_NTS,
						  NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("rejected", SQL_HANDLE_DBC, hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
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

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	SQLDisconnect(hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

int
main(void)
{
	/* A complete braced value: the text inside is only a description */
	try_connect("closed",
				"Description={x;pqopt=application_name=injected;y=}}}");

	/* The string ends inside the value */
	try_connect("unclosed",
				"Description={x;pqopt=application_name=injected;y=");

	/* The string ends inside the value, right after an escaped brace */
	try_connect("unclosed after escaped brace",
				"Description={x;pqopt=application_name=injected;y=}}");

	return 0;
}
