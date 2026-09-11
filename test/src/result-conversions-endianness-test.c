/*
 * Test conversion of values received from server to SQL datatypes, for the
 * cases whose output is *not* portable across platforms or server versions.
 *
 * This file was split out of result-conversions-test.c so that the (much
 * larger) result-conversions test can have a single, architecture- and
 * server-version-independent expected-output file.  Three kinds of output
 * are inherently non-portable and live here instead:
 *
 * 1. int4 converted to SQL_C_BINARY / SQL_C_VARBOOKMARK.  The driver copies
 *    the value into the buffer with memcpy() of a host-order UInt4, so the
 *    hex dump of the buffer differs between little- and big-endian machines
 *    (e.g. 1234567 -> "87D61200" on little-endian, "0012D687" on
 *    big-endian).
 *
 * 2. An empty text value converted to SQL_C_WCHAR with a too-small buffer.
 *    The test prints the SQLWCHAR units of a 0xFF-filled buffer; the byte
 *    order within each wide character differs between little- and
 *    big-endian machines ("\FF00" vs "\  FF" for the first unit).
 *
 * 3. A float8 value (1.23456789012) converted to SQL_C_CHAR, SQL_C_WCHAR
 *    and SQL_C_NUMERIC.  PostgreSQL 12 changed the default floating-point
 *    output format (extra_float_digits), so the rendered digits differ
 *    between server versions ("1.23456789012" vs "1.2345678901199999").
 *    The whole float8 matrix row is kept here, even though only those three
 *    lines differ, so that the main test file needs no variants at all.
 *
 * Because these outputs are non-portable, this test legitimately keeps
 * multiple expected-output variants (result-conversions-endianness.out,
 * _1, _2, _3) covering big-endian and pre-PG12 combinations, just as
 * result-conversions used to before the split.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "common.h"

static HSTMT hstmt = SQL_NULL_HSTMT;

/* Same SQLWCHAR-printing logic as result-conversions-test.c. */
void
printwchar(SQLWCHAR *wstr)
{
	int			i = 0;
	/*
	 * a backstop to make sure we terminate if the string isn't null-terminated
	 * properly
	 */
	int			MAXLEN = 50;

	while (*wstr && i < MAXLEN)
	{
		if ((*wstr & 0xFFFFFF00) == 0 && isprint(*wstr))
			printf("%c", *wstr);
		else
			printf("\\%4X", *wstr);
		wstr++;
		i++;
	}
}

void
printhex(unsigned char *b, SQLLEN len)
{
	SQLLEN i;

	printf("hex: ");
	for (i = 0; i < len; i++)
		printf("%02X", b[i]);
}

static char *resultbuf = NULL;

/*
 * Run one conversion and print the result, in the same format as
 * result-conversions-test.c, so the expected-output style matches.
 */
void
test_conversion(const char *pgtype, const char *pgvalue, int sqltype, const char *sqltypestr, int buflen)
{
	char		sql[500];
	SQLRETURN	rc;
	SQLLEN		len_or_ind;

	printf("'%s' (%s) as %s: ", pgvalue, pgtype, sqltypestr);

	if (resultbuf == NULL)
		resultbuf = malloc(500);

	memset(resultbuf, 0xFF, 500);

	/*
	 * Use dollar-quotes to make the test case insensitive to
	 * standards_conforming_strings. Some of the test values we use contain
	 * backslashes.
	 */
	snprintf(sql, sizeof(sql),
			 "SELECT $$%s$$::%s AS %s_col /* convert to %s */",
			 pgvalue, pgtype, pgtype, sqltypestr);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetData(hstmt, 1, sqltype, resultbuf, buflen, &len_or_ind);
	if (SQL_SUCCEEDED(rc))
	{
		switch (sqltype)
		{
			case SQL_C_CHAR:
				printf("%s", (char *) resultbuf);
				break;
			case SQL_C_WCHAR:
				printwchar((SQLWCHAR *) resultbuf);
				break;
			case SQL_C_BINARY:
				printhex((unsigned char *) resultbuf, len_or_ind);
				break;
			case SQL_C_NUMERIC:
				{
					SQL_NUMERIC_STRUCT *ns = (SQL_NUMERIC_STRUCT *) resultbuf;
					int			i;

					/*
					 * Deliberately prints ns->scale for "sign", matching
					 * result-conversions-test.c (and its existing expected
					 * output), which has the same quirk.
					 */
					printf("precision: %u scale: %d sign: %d val: ",
						   ns->precision, ns->scale, ns->scale);
					for (i = 0; i < SQL_MAX_NUMERIC_LEN; i++)
						printf("%02x", ns->val[i]);
				}
				break;
			default:
				printf("unexpected SQL C type in this test: %u", sqltype);
				break;
		}

		if (rc == SQL_SUCCESS_WITH_INFO)
		{
			char sqlstate[10];

			rc = SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, NULL, NULL, 0, NULL);
			if (!SQL_SUCCEEDED(rc) && SQL_NO_DATA != rc)
				print_diag(" SQLGetDiagRec failed", SQL_HANDLE_STMT, hstmt);
			else
			{
				if (memcmp(sqlstate, "01004", 5) == 0)
					printf(" (truncated)");
				else if (SQL_NO_DATA == rc && IsAnsi()) /* maybe */
					printf(" (truncated)");
				else
					print_diag("SQLGetData success with info", SQL_HANDLE_STMT, hstmt);
			}
		}

		printf("\n");
	}
	else
	{
		/* some of the conversions throw an error; that's OK */
		print_diag("SQLGetData failed", SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	fflush(stdout);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Endianness-dependent: the driver memcpy()s a host-order UInt4 into the
	 * buffer for these conversions, so the byte order of the hex dump depends
	 * on the machine's endianness.
	 */
	test_conversion("int4", "1234567", SQL_C_BINARY, "SQL_C_BINARY", 4);
	test_conversion("int4", "1234567", SQL_C_VARBOOKMARK, "SQL_C_VARBOOKMARK", 4);

	/*
	 * Endianness-dependent: the SQLWCHAR units of the 0xFF-filled buffer are
	 * printed for the truncated empty string, and their byte order depends on
	 * the machine's endianness.
	 */
	test_conversion("text", "", SQL_C_WCHAR, "SQL_C_WCHAR", 1);

	/*
	 * Server-version-dependent: PG12 changed the default float output format,
	 * so the rendered digits differ between server versions.
	 */
	test_conversion("float8", "1.23456789012", SQL_C_CHAR, "SQL_C_CHAR", 100);
	test_conversion("float8", "1.23456789012", SQL_C_WCHAR, "SQL_C_WCHAR", 100);
	test_conversion("float8", "1.23456789012", SQL_C_NUMERIC, "SQL_C_NUMERIC", sizeof(SQL_NUMERIC_STRUCT));

	/* Clean up */
	test_disconnect();

	return 0;
}
