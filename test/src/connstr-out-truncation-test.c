/*
 * Test that SQLDriverConnect reports a completed connection string that
 * does not fit the output buffer (issue #229): SQL_SUCCESS_WITH_INFO with
 * 01004, and the length of the whole string in StringLength2Ptr, as ODBC
 * requires. A caller that then connects again with a buffer of that length
 * plus one gets the whole string and SQL_SUCCESS.
 *
 * The length depends on the DSN, so the test prints how it compares with
 * the buffer rather than the length itself.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
build_dsn(char *dsn, size_t dsnlen)
{
	const char *envvar = getenv("COMMON_CONNECTION_STRING_FOR_REGRESSION_TEST");

	if (envvar != NULL && envvar[0] != '\0')
		snprintf(dsn, dsnlen, "DSN=%s;%s", get_test_dsn(), envvar);
	else
		snprintf(dsn, dsnlen, "DSN=%s", get_test_dsn());
}

/*
 * Connect with an output buffer of bufsize bytes, print the outcome, and
 * return the length the driver reported.
 */
static SQLSMALLINT
connect_with_buffer(const char *label, SQLSMALLINT bufsize)
{
	char		dsn[1024];
	SQLCHAR		out[4096];
	SQLSMALLINT	outlen = -1;
	SQLCHAR		sqlstate[6] = "";
	SQLRETURN	rc;

	build_dsn(dsn, sizeof(dsn));
	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
	SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
	SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);

	memset(out, 'X', sizeof(out));
	rc = SQLDriverConnect(conn, NULL, (SQLCHAR *) dsn, SQL_NTS,
						  out, bufsize, &outlen, SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLDriverConnect failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}
	if (SQL_SUCCESS_WITH_INFO == rc)
		SQLGetDiagRec(SQL_HANDLE_DBC, conn, 1, sqlstate, NULL, NULL, 0, NULL);

	printf("%s: %s%s%s", label,
		   SQL_SUCCESS == rc ? "SQL_SUCCESS" : "SQL_SUCCESS_WITH_INFO",
		   sqlstate[0] ? " " : "", (char *) sqlstate);
	if (outlen >= bufsize)
		printf(", the reported length does not fit the buffer");
	else if (outlen == (SQLSMALLINT) strlen((char *) out))
		printf(", the string fills the reported length");
	else
		printf(", the string is %d bytes but the reported length is %d",
			   (int) strlen((char *) out), (int) outlen);
	if (bufsize > 0 && out[bufsize - 1] != '\0' &&
		strlen((char *) out) >= (size_t) bufsize)
		printf(", and the buffer is not terminated");
	printf("\n");

	SQLDisconnect(conn);
	SQLFreeHandle(SQL_HANDLE_DBC, conn);
	SQLFreeHandle(SQL_HANDLE_ENV, env);
	return outlen;
}

int main(int argc, char **argv)
{
	SQLSMALLINT	needed;

	needed = connect_with_buffer("buffer of 2 bytes", 2);
	if (needed < 2 || needed >= 4095)
	{
		printf("unexpected reported length %d\n", (int) needed);
		exit(1);
	}
	connect_with_buffer("buffer of the reported length plus 1", needed + 1);

	return 0;
}
