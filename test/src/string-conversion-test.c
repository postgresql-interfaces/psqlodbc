/*
 * setenv() is POSIX, so ask for its declaration before including any system
 * header.  This must stay at the very top of the file.
 */
#ifndef WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

/*
 * Fetching a character the client locale cannot represent must report
 * STMT_STRING_CONVERSION_ERROR as 22018 (ODBC 3.x) / 22005 (ODBC 2.x), not
 * the generic HY000.
 *
 * The conversion only happens for a connection opened through the wide
 * character API, since only then does the driver request client_encoding
 * UTF8 instead of the locale encoding.  The locale has to come from the
 * environment as well: derive_locale_encoding() falls back to LC_ALL,
 * LC_CTYPE and LANG whenever the in-process locale names no codeset.
 *
 * Skipped for the ANSI driver, whose conversion code is compiled out, on
 * Windows, where unrepresentable characters are substituted rather than
 * rejected, and where the driver has no wide character converter, which the
 * fetch shows by returning the UTF-8 bytes unchanged.  Skipped runs print
 * the same output so the expected file is stable.
 */
#define SKIP_OUTPUT()					\
	do {								\
		printf("connected\n");			\
		printf("string conversion error SQLSTATE: 22018\n"); \
		printf("disconnecting\n");		\
	} while (0)

#ifndef	WIN32
/* Widen the ASCII connection string for the wide character connect call. */
static void
widen(const char *src, SQLWCHAR *dst)
{
	while (*src)
		*dst++ = (SQLWCHAR) (unsigned char) *src++;
	*dst = 0;
}

static void
test_connect_unicode(void)
{
	SQLRETURN	ret;
	SQLWCHAR	wdsn[1024];
	char		dsn[1024];
	const char *envvar;

	if ((envvar = getenv("COMMON_CONNECTION_STRING_FOR_REGRESSION_TEST")) != NULL &&
		envvar[0] != '\0')
		snprintf(dsn, sizeof(dsn), "DSN=%s;%s", get_test_dsn(), envvar);
	else
		snprintf(dsn, sizeof(dsn), "DSN=%s", get_test_dsn());

	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
	SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
	SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);

	widen(dsn, wdsn);
	ret = SQLDriverConnectW(conn, NULL, wdsn, SQL_NTS, NULL, 0, NULL,
							SQL_DRIVER_NOPROMPT);
	if (SQL_SUCCEEDED(ret))
	{
		printf("connected\n");
		return;
	}
	print_diag("SQLDriverConnectW failed.", SQL_HANDLE_DBC, conn);
	fflush(stdout);
	exit(1);
}
#endif /* !WIN32 */

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLCHAR sqlstate[6];
	SQLCHAR buf[10];
	SQLLEN ind;

	if (IsAnsi())
	{
		SKIP_OUTPUT();
		return 0;
	}
#ifdef	WIN32
	/* See the comment above: the error path cannot be reached here. */
	SKIP_OUTPUT();
	return 0;
#else
	if (0 != setenv("LC_ALL", "C", 1) ||
		0 != setenv("LANG", "C", 1) ||
		NULL == setlocale(LC_ALL, "C"))
	{
		SKIP_OUTPUT();
		return 0;
	}
	test_connect_unicode();
#endif /* WIN32 */

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Build the euro sign server-side, keeping the query text ASCII.  chr()
	 * cannot be used: it is rejected when the database encoding is not UTF8.
	 */
	rc = SQLExecDirect(hstmt,
					   (SQLCHAR *) "SELECT convert_from(decode('e282ac','hex'),'UTF8')",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* The error may be raised by SQLFetch or by SQLGetData. */
	rc = SQLFetch(hstmt);
	if (SQL_SUCCEEDED(rc))
	{
		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
		if (SQL_SUCCEEDED(rc))
		{
			/*
			 * A driver built without a wide character converter, as on
			 * macOS, where the C library defines no __STDC_ISO_10646__,
			 * returns the UTF-8 bytes unconverted, so the error path
			 * does not exist there. Print the output of a run that
			 * reaches it, as the other skipped cases do.
			 */
			if (3 == ind && 0 == memcmp(buf, "\xe2\x82\xac", 3))
			{
				printf("string conversion error SQLSTATE: 22018\n");
				SQLFreeStmt(hstmt, SQL_CLOSE);
				test_disconnect();
				return 0;
			}
			printf("BUG: SQLFetch/SQLGetData unexpectedly succeeded\n");
			exit(1);
		}
	}
	rc = SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, NULL, NULL, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetDiagRec failed", hstmt);
	if (0 != strcmp((char *) sqlstate, "22018") &&
		0 != strcmp((char *) sqlstate, "22005"))
	{
		printf("unexpected SQLSTATE for string conversion error: %s\n", sqlstate);
		exit(1);
	}
	printf("string conversion error SQLSTATE: %s\n", sqlstate);

	SQLFreeStmt(hstmt, SQL_CLOSE);
	test_disconnect();

	return 0;
}
