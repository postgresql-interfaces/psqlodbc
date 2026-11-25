/*
 * Tests for error diagnostics through the Unicode entry points
 * (SQLGetDiagRecW, SQLGetDiagFieldW).
 *
 * The driver divides a long message text into pieces by byte count,
 * so a piece handed to the UTF-8 to UTF-16 conversion may end in the
 * middle of a multibyte character.  SQLGetDiagFieldW used to return
 * SQL_SUCCESS_WITH_INFO with StringLength -2 and an empty buffer in
 * that case, and SQLGetDiagRecW replaced the text with its sqlstate.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include <sqlucode.h>

static void
ascii_to_wchar(SQLWCHAR *dst, const char *src)
{
	while (*src)
		*dst++ = (SQLWCHAR) (unsigned char) *src++;
	*dst = 0;
}

/* Print a UTF-16 string as UTF-8 (BMP characters only) */
static void
print_wchar(const SQLWCHAR *str, int len)
{
	int		i;

	for (i = 0; i < len && str[i] != 0; i++)
	{
		unsigned int c = str[i];

		if (c < 0x80)
			putchar(c);
		else if (c < 0x800)
			printf("%c%c", 0xc0 | (c >> 6), 0x80 | (c & 0x3f));
		else
			printf("%c%c%c", 0xe0 | (c >> 12), 0x80 | ((c >> 6) & 0x3f), 0x80 | (c & 0x3f));
	}
	printf("\n");
}

static const char *
retcode_str(SQLRETURN rc)
{
	switch (rc)
	{
		case SQL_SUCCESS:
			return "SQL_SUCCESS";
		case SQL_SUCCESS_WITH_INFO:
			return "SQL_SUCCESS_WITH_INFO";
		case SQL_NO_DATA:
			return "SQL_NO_DATA";
		case SQL_ERROR:
			return "SQL_ERROR";
		default:
			return "unexpected return code";
	}
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		dsn[1024];
	SQLWCHAR	wdsn[1024];
	SQLWCHAR	wquery[500];
	SQLWCHAR   *wp;
	int			i;
	SQLWCHAR	sqlstate[6];
	SQLWCHAR	msg[2000];
	SQLSMALLINT	len;
	SQLINTEGER	nativeerror;
	char	   *envvar;

	/*
	 * Connect with SQLDriverConnectW, as a Unicode application does, so
	 * that the driver manager calls the driver's Unicode entry points.
	 */
	if ((envvar = getenv("COMMON_CONNECTION_STRING_FOR_REGRESSION_TEST")) != NULL && envvar[0] != '\0')
		snprintf(dsn, sizeof(dsn), "DSN=%s;%s", get_test_dsn(), envvar);
	else
		snprintf(dsn, sizeof(dsn), "DSN=%s;", get_test_dsn());
	ascii_to_wchar(wdsn, dsn);

	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
	SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
	SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);
	rc = SQLDriverConnectW(conn, NULL, wdsn, SQL_NTS, NULL, 0, NULL,
						   SQL_DRIVER_COMPLETE);
	if (SQL_SUCCEEDED(rc))
		printf("connected\n");
	else
	{
		print_diag("SQLDriverConnectW failed.", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * An error message quoting 400 three-byte characters (U+4E2D).  The
	 * literal comes from the client, so that a database that isn't UTF-8
	 * (SQL_ASCII) passes the bytes through as they are.
	 */
	ascii_to_wchar(wquery, "SELECT CAST('");
	wp = wquery + 13;
	for (i = 0; i < 400; i++)
		*wp++ = 0x4e2d;
	ascii_to_wchar(wp, "' AS integer)");
	rc = SQLExecDirectW(hstmt, wquery, SQL_NTS);
	printf("SQLExecDirectW: %s\n", retcode_str(rc));

	/*
	 * A 256-byte buffer gets 384 bytes of the message, which ends in the
	 * middle of a character.
	 */
	memset(msg, 0, sizeof(msg));
	rc = SQLGetDiagFieldW(SQL_HANDLE_STMT, hstmt, 1, SQL_DIAG_MESSAGE_TEXT,
						  msg, 256, &len);
	printf("SQLGetDiagFieldW(256): %s, StringLength %s\n", retcode_str(rc),
		   len < 0 ? "negative" : len == 0 ? "zero" : "positive");
	print_wchar(msg, 128);

	/* The whole message */
	memset(msg, 0, sizeof(msg));
	rc = SQLGetDiagFieldW(SQL_HANDLE_STMT, hstmt, 1, SQL_DIAG_MESSAGE_TEXT,
						  msg, sizeof(msg), &len);
	printf("SQLGetDiagFieldW(%d): %s, StringLength %d\n", (int) sizeof(msg),
		   retcode_str(rc), len);
	print_wchar(msg, sizeof(msg) / sizeof(msg[0]));

	/* SQLGetDiagRecW with a small buffer: the piece that fits, not a substitute */
	memset(msg, 0, sizeof(msg));
	rc = SQLGetDiagRecW(SQL_HANDLE_STMT, hstmt, 1, sqlstate, &nativeerror,
						msg, 128, &len);
	printf("SQLGetDiagRecW(128): %s, TextLength %d\n", retcode_str(rc), len);
	print_wchar(sqlstate, 5);
	print_wchar(msg, 128);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_STMT_RESULT(rc, "SQLEndTran failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	test_disconnect();

	return 0;
}
