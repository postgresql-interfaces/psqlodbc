/*
 * Test that the Unicode functions which return a string in the
 * application's buffer terminate it there.
 *
 * BufferLength counts the terminator. A string that fills the buffer, or
 * is longer, must be cut so that the terminator fits, and the function
 * must return SQL_SUCCESS_WITH_INFO and report the whole length. The
 * functions that take the length in bytes get an odd one too, which
 * holds whole characters only.
 *
 * The calls go to the Unicode driver as they are. With the ANSI driver
 * the driver manager converts them, so the test is skipped there.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define BUFCHARS 64

static SQLWCHAR buf[BUFCHARS];

static void
to_wide(const char *s, SQLWCHAR *w)
{
	while (*s)
		*w++ = (unsigned char) *s++;
	*w = 0;
}

static void
fill(void)
{
	int			i;

	for (i = 0; i < BUFCHARS; i++)
		buf[i] = 'Z';
}

/*
 * Report on a call that wrote the string full into the first chars
 * elements of buf. reported is the length the call returned, in
 * characters.
 */
static void
check(const char *what, int chars, const char *full, SQLRETURN rc,
	  long reported)
{
	int			len = (int) strlen(full);
	int			kept = -1;
	int			i;
	int			touched = 0;
	int			prefix = 1;

	for (i = 0; i < chars; i++)
		if (0 == buf[i])
		{
			kept = i;
			break;
		}
	for (i = chars; i < BUFCHARS; i++)
		if ('Z' != buf[i])
			touched = 1;
	for (i = 0; i < kept; i++)
		if (buf[i] != (SQLWCHAR) (unsigned char) full[i])
			prefix = 0;

	printf("%s: ", what);
	if (kept < 0)
		printf("NOT TERMINATED");
	else if (!prefix)
		printf("wrong characters");
	else if (kept == len)
		printf("whole string");
	else
		printf("lost %d", len - kept);
	printf(", %s", SQL_SUCCESS == rc ? "SQL_SUCCESS" :
		   SQL_SUCCESS_WITH_INFO == rc ? "SQL_SUCCESS_WITH_INFO" : "failed");
	printf(", whole length %s", reported == len ? "reported" : "NOT reported");
	printf("%s\n", touched ? ", WROTE PAST THE BUFFER" : "");
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLWCHAR	wsql[64];
	SQLSMALLINT	slen;
	SQLINTEGER	ilen;
	char		dbname[128];
	SQLINTEGER	dblen;
	int			chars;
	int			i;
	static const int describe_sizes[] = {9, 8, 4};
	/* in bytes: room for the terminator, an exact fit, an odd count */
	static const int attr_sizes[] = {18, 16, 9};
	static const int info_sizes[] = {22, 20, 7};

	if (IsAnsi())
	{
		printf("Skipped, the calls must reach the Unicode driver\n");
		return 0;
	}
	test_connect();
	/*
	 * The database name, whatever it is, to compare with below. It is
	 * read first: after SQLGetInfoW the driver answers the ANSI
	 * SQLGetInfo of this connection in SQLWCHARs too.
	 */
	rc = SQLGetInfo(conn, SQL_DATABASE_NAME, dbname, sizeof(dbname), &slen);
	CHECK_CONN_RESULT(rc, "SQLGetInfo failed", conn);
	dblen = (SQLINTEGER) strlen(dbname);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* an 8-character cursor name, column name and statement */
	rc = SQLSetCursorName(hstmt, (SQLCHAR *) "curs_8ch", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLSetCursorName failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1 AS abcdefgh", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	for (i = 0; i < 3; i++)
	{
		char		what[80];

		chars = describe_sizes[i];
		fill();
		slen = -1;
		rc = SQLDescribeColW(hstmt, 1, buf, (SQLSMALLINT) chars, &slen,
							 NULL, NULL, NULL, NULL);
		snprintf(what, sizeof(what), "SQLDescribeColW, %d characters", chars);
		check(what, chars, "abcdefgh", rc, slen);
	}
	for (i = 0; i < 3; i++)
	{
		char		what[80];

		fill();
		slen = -1;
		rc = SQLColAttributeW(hstmt, 1, SQL_DESC_NAME, buf,
							  (SQLSMALLINT) attr_sizes[i], &slen, NULL);
		snprintf(what, sizeof(what), "SQLColAttributeW, %d bytes",
				 attr_sizes[i]);
		check(what, attr_sizes[i] / 2, "abcdefgh", rc, slen / 2);
	}
	for (chars = 9; chars >= 8; chars--)
	{
		char		what[80];

		fill();
		slen = -1;
		rc = SQLGetCursorNameW(hstmt, buf, (SQLSMALLINT) chars, &slen);
		snprintf(what, sizeof(what), "SQLGetCursorNameW, %d characters",
				 chars);
		check(what, chars, "curs_8ch", rc, slen);
	}
	to_wide("SELECT 1", wsql);
	for (chars = 9; chars >= 8; chars--)
	{
		char		what[80];

		fill();
		ilen = -1;
		rc = SQLNativeSqlW(conn, wsql, SQL_NTS, buf, chars, &ilen);
		snprintf(what, sizeof(what), "SQLNativeSqlW, %d characters", chars);
		check(what, chars, "SELECT 1", rc, ilen);
	}
	for (i = 0; i < 3; i++)
	{
		char		what[80];

		fill();
		slen = -1;
		rc = SQLGetInfoW(conn, SQL_DBMS_NAME, buf,
						 (SQLSMALLINT) info_sizes[i], &slen);
		snprintf(what, sizeof(what), "SQLGetInfoW(SQL_DBMS_NAME), %d bytes",
				 info_sizes[i]);
		check(what, info_sizes[i] / 2, "PostgreSQL", rc, slen / 2);
	}

	for (i = 0; i < 3; i++)
	{
		static const char *const sizes[] = {
			"room for the terminator", "an exact fit", "an odd byte count"
		};
		/* in bytes, for a name of dblen characters */
		int			bytes = (0 == i) ? 2 * dblen + 2 :
			(1 == i) ? 2 * dblen : 2 * dblen + 1;
		char		what[80];

		if (bytes / 2 > BUFCHARS)
			break;
		fill();
		ilen = -1;
		rc = SQLGetConnectAttrW(conn, SQL_ATTR_CURRENT_CATALOG, buf, bytes,
								&ilen);
		snprintf(what, sizeof(what),
				 "SQLGetConnectAttrW(SQL_ATTR_CURRENT_CATALOG), %s", sizes[i]);
		check(what, bytes / 2, dbname, rc, ilen / 2);
	}

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
