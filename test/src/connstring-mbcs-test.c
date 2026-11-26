/*
 * Test braced connection-string values containing multibyte characters.
 *
 * In SJIS, BIG5, GBK and GB18030 the byte 0x7d ('}') is a legal trailing
 * byte of a two-byte character. An application that escapes a value by
 * character leaves that byte undoubled; one that escapes by byte doubles
 * it. Read either way, the other kind of application can end the value
 * early and have the rest parsed as further keywords, so the driver must
 * refuse a braced value in which a multibyte lead byte precedes 0x7d.
 *
 * The ANSI driver takes the connection string in the encoding of the
 * locale, which PGCLIENTENCODING overrides, so the test can run with a
 * GBK connection string regardless of the locale. The Unicode driver
 * receives the string as UTF-8, which has no such trailing bytes, so the
 * test connects through the ANSI DSN, and is skipped where there is none.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* U+5009 in GBK: the trailing byte is 0x7d */
#define GBK_CHAR_WITH_7D	"\x82\x7d"
/* U+4E2D in GBK: neither byte is 0x7d */
#define GBK_CHAR_WITHOUT_7D	"\xd6\xd0"

static char ansi_dsn[256];

/*
 * Connect with connparams and check that setting has the value expected,
 * or, when expected is NULL, that the connection string is refused.
 * Return FALSE if the DSN does not exist.
 */
static BOOL
check_setting(const char *label, const char *connparams,
			  const char *setting, const char *expected)
{
	SQLRETURN	rc;
	SQLHENV		henv = SQL_NULL_HENV;
	SQLHDBC		hdbc = SQL_NULL_HDBC;
	SQLHSTMT	hstmt = SQL_NULL_HSTMT;
	SQLCHAR		outstr[256];
	SQLSMALLINT	outlen = 0;
	SQLCHAR		value[256];
	SQLLEN		ind = 0;
	char		connstr[1024];
	char		query[256];

	snprintf(connstr, sizeof(connstr), "DSN=%s;%s", ansi_dsn, connparams);

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
		SQLCHAR		sqlstate[6] = "";
		SQLINTEGER	nativeerror;
		SQLCHAR		msg[512];
		SQLSMALLINT	msglen;

		SQLGetDiagRec(SQL_HANDLE_DBC, hdbc, 1, sqlstate, &nativeerror,
					  msg, sizeof(msg), &msglen);
		if (strcmp((char *) sqlstate, "IM002") == 0)
			return FALSE;
		/* only the parser's refusal counts, not any failure to connect */
		if (expected == NULL &&
			strstr((char *) msg, "Connection string parse error") != NULL)
		{
			printf("%s: refused, %s\n", label, (char *) msg);
			SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
			SQLFreeHandle(SQL_HANDLE_ENV, henv);
			return TRUE;
		}
		printf("%s: FAILED - %s=%s\n", label, (char *) sqlstate, (char *) msg);
		exit(1);
	}
	if (expected == NULL)
	{
		printf("%s: FAILED - the connection string was accepted\n", label);
		exit(1);
	}

	rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle(SQL_HANDLE_STMT) failed", hdbc);

	snprintf(query, sizeof(query), "SELECT current_setting('%s')", setting);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) query, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, value, sizeof(value), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);

	if (strcmp((char *) value, expected) != 0)
	{
		printf("%s: FAILED - expected [%s], got [%s]\n",
			   label, expected, (char *) value);
		exit(1);
	}
	printf("%s: ok\n", label);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	SQLDisconnect(hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
	return TRUE;
}

/* TRUE if the test database's encoding is SQL_ASCII */
static BOOL
database_is_sql_ascii(void)
{
	SQLRETURN	rc;
	SQLHENV		henv = SQL_NULL_HENV;
	SQLHDBC		hdbc = SQL_NULL_HDBC;
	SQLHSTMT	hstmt = SQL_NULL_HSTMT;
	SQLCHAR		value[64] = "";
	SQLLEN		ind = 0;
	char		connstr[300];

	snprintf(connstr, sizeof(connstr), "DSN=%s", ansi_dsn);
	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
	SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
	SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
	rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) connstr, SQL_NTS,
						  NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLDriverConnect failed", SQL_HANDLE_DBC, hdbc);
		exit(1);
	}
	rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle(SQL_HANDLE_STMT) failed", hdbc);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT current_setting('server_encoding')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, value, sizeof(value), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	SQLDisconnect(hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
	return strcmp((char *) value, "SQL_ASCII") == 0;
}

int
main(void)
{
	/* the ANSI DSN of the test setup has the Unicode DSN's name plus _ansi */
	if (IsAnsi())
		snprintf(ansi_dsn, sizeof(ansi_dsn), "%s", get_test_dsn());
	else
		snprintf(ansi_dsn, sizeof(ansi_dsn), "%s_ansi", get_test_dsn());

#ifdef WIN32
	_putenv_s("PGCLIENTENCODING", "GBK");
#else
	setenv("PGCLIENTENCODING", "GBK", 1);
#endif

	/*
	 * A value escaped by character: the 0x7d of the character is not
	 * doubled, and the keywords after it are meant to be part of the value.
	 */
	if (!check_setting("character-escaped lead byte before 0x7d",
					   "ConnSettings=set application_name to 'expected';"
					   "Description={" GBK_CHAR_WITH_7D ";"
					   "ConnSettings=set application_name to 'injected';x=}",
					   "application_name", NULL))
	{
		printf("Skipped, there is no ANSI DSN\n");
		return 0;
	}

	/*
	 * A value escaped by byte: the 0x7d is doubled, so the next '}' closes
	 * the value if the pair is read as one character.
	 */
	check_setting("byte-escaped lead byte before 0x7d",
				  "ConnSettings=set application_name to 'expected';"
				  "Description={" GBK_CHAR_WITH_7D "};"
				  "ConnSettings=set application_name to 'injected';x=}",
				  "application_name", NULL);

	/*
	 * The server refuses non-ASCII bytes from a GBK client when the
	 * database encoding is SQL_ASCII, so the values can't be read back.
	 */
	if (database_is_sql_ascii())
	{
		printf("Skipped the values read back, the database encoding is SQL_ASCII\n");
		return 0;
	}

	/* A multibyte character without 0x7d survives in a braced value */
	check_setting("multibyte character in a braced value",
				  "ConnSettings={set psqlodbc_test.mbcs to '" GBK_CHAR_WITHOUT_7D "'}",
				  "psqlodbc_test.mbcs", GBK_CHAR_WITHOUT_7D);

	/* Without braces the value ends at ';', so 0x7d is plain data */
	check_setting("lead byte before 0x7d in a value without braces",
				  "ConnSettings=set psqlodbc_test.mbcs to '" GBK_CHAR_WITH_7D "'",
				  "psqlodbc_test.mbcs", GBK_CHAR_WITH_7D);

	return 0;
}
