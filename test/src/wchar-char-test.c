#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "common.h"

// #define UNICODE
#ifndef	_T
#ifdef	UNICODE
#define _T(t)	L##t
#else
#define _T(t)	t
#endif /* UNICODE */
#endif /* _T */

#ifdef	WIN32
#define	stricmp	_stricmp
#else
#define stricmp strcasecmp
#endif

/*
 * Print the code points of a UTF-16 string.  Read the SQLWCHAR values
 * directly: looking at their bytes printed the values correctly only on
 * little-endian machines.
 */
static void
print_utf16(const SQLWCHAR *wdt)
{
	int	i;

	for (i = 0; wdt[i]; i++)
		printf("U+%04X", (unsigned int) wdt[i]);
	printf("\n");
	fflush(stdout);
}

/*
 * The test parameters hold UTF-16 code units.  Filling a byte buffer with
 * them by hand, least significant byte first, gave the wrong characters on
 * big-endian machines.  Copy the code units in the byte order of the host.
 */
static const SQLWCHAR lovedt_units[] = {0x4E95, 0x4E0A, 0x535A, 0x53F2};
static const SQLWCHAR lovedt2_units[] = {0x53F2, 0x535A, 0x4E0A, 0x4E95};

static void
init_lovedt(unsigned char *lovedt, unsigned char *lovedt2, size_t buflen)
{
	memset(lovedt, 0, buflen);
	memset(lovedt2, 0, buflen);
	memcpy(lovedt, lovedt_units, sizeof(lovedt_units));
	memcpy(lovedt2, lovedt2_units, sizeof(lovedt2_units));
}

#include	"wchar-char-test-sjis.c"
#include	"wchar-char-test-utf8.c"
#include	"wchar-char-test-eucjp.c"

enum {
	SJIS_TEST
	,UTF8_TEST
	,EUCJP_TEST
};

int main(int argc, char **argv)
{
	int rc, testsw = -1;
	HSTMT hstmt = SQL_NULL_HSTMT;
	const char	*loc, *ptr;

	struct {
		const char *name;
		int id;
	} loctbl[] = {
		{ "sjis", SJIS_TEST }
		,{ "shiftjis", SJIS_TEST }
		,{ "932", SJIS_TEST }
		,{ "utf-8", UTF8_TEST }
		,{ "utf8", UTF8_TEST }
		,{ "eucjp", EUCJP_TEST }
	};

	loc = setlocale(LC_ALL, "");
	if (NULL != loc &&
	    NULL != (ptr = strchr(loc, '.')))
	{
		int i;

		ptr++;
		for (i = 0; i < sizeof(loctbl) / sizeof(loctbl[0]); i++)
		{
			if (stricmp(ptr, loctbl[i].name) == 0)
			{
				testsw = loctbl[i].id;
				break;
			}
		}
	}
	if (testsw < 0)
	{
		printf("Unfortunately this program can't handle this locale\n");
		printf("Or you are testing an ansi driver\n");
		printf("Anyway bypass this program\n");
		printf("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n");
		printf("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n");
		printf("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n");
		exit(0);
	}
	test_connect_ext("");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	switch (testsw)
	{
		case SJIS_TEST:
			printf("SJIS test\n");
			fflush(stdout);
			rc = sjis_test(hstmt);
			break;
		case UTF8_TEST:
			printf("UTF8 test\n");
			fflush(stdout);
			rc = utf8_test(hstmt);
			break;
		case EUCJP_TEST:
			printf("EUCJP test\n");
			fflush(stdout);
			rc = eucjp_test(hstmt);
			break;
	}

	/* Clean up */
	test_disconnect();

	return 0;
}

