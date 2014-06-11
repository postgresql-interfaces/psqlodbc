#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#if 0
#include <iconv.h>
#endif

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	SQLLEN		ccharlen;
	SQLLEN		wcharlen;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char	   *sql;
	int			i;
	char		buf[1000];
	SQLWCHAR	wbuf[1000];

	/* Enable LF -> CR+LF conversion */
	test_connect_ext("CX=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Return several columns that all contain the same string, with newlines.
	 * We want to try getting the column contents with several different
	 * options, and the driver doesn't let you fetch the same column more than
	 * once.
	 */
	sql = "SELECT E'string\nwith\nnewlines', E'string\nwith\nnewlines', "
		"E'string\nwith\nnewlines', E'string\nwith\nnewlines', "
		"E'string\nwith\nnewlines'";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	printf("reading to char buffer...\n");
	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ccharlen);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("strlen %d, SQLGetData claims %d\n\n", (int) strlen(buf), (int) ccharlen);

	printf("reading to char buffer, with truncation...\n");
	rc = SQLGetData(hstmt, 2, SQL_C_CHAR, buf, 10, &ccharlen);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("strlen %d, SQLGetData claims %d\n\n", (int) strlen(buf), (int) ccharlen);

	printf("reading to SQLWCHAR buffer...\n");
	rc = SQLGetData(hstmt, 3, SQL_C_WCHAR, wbuf, sizeof(wbuf), &wcharlen);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);

	/* On some platforms, SQLWCHAR != wchar_t, so we cannot use wcslen here */
	for (i = 0; i < sizeof(wbuf) && wbuf[i] != 0; i++);
	printf("len %d chars, SQLGetData claims %d bytes\n\n", i, (int) wcharlen);

	printf("reading to SQLWCHAR buffer, with truncation...\n");
	rc = SQLGetData(hstmt, 4, SQL_C_WCHAR, wbuf, 10, &wcharlen);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);

	for (i = 0; i < sizeof(wbuf) && wbuf[i] != 0; i++);
	printf("len %d chars, SQLGetData claims %d bytes\n\n", i, (int) wcharlen);

	/*
	 * Read into a buffer that's slightly too small, so that it would fit
	 * if it wasn't for the LF->CR+LF conversion.
	 */
	printf("reading to SQLWCHAR buffer, with LF->CR+LF conversion causing truncation...\n");
	rc = SQLGetData(hstmt, 5, SQL_C_WCHAR, wbuf, 42, &wcharlen);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);

	for (i = 0; i < sizeof(wbuf) && wbuf[i] != 0; i++);
	printf("len %d chars, SQLGetData claims %d bytes\n\n", i, (int) wcharlen);

	/*
	 * Print out the string, but on Unix we have to convert it to UTF-8 first.
	 * On Windows we could just use wprintf.
	 */
#if 0
	{
		iconv_t cd = iconv_open("UTF-8", "UCS-2");
		char utf8buf[1000];
		size_t l;
		size_t inbytes = wcharlen;
		size_t outbytes = sizeof(utf8buf);
		char *obuf = utf8buf;
		char *ibuf = (char *) wbuf;

		l = iconv(cd, &ibuf, &inbytes, &obuf, &outbytes);
		*obuf = 0;
		printf("inremains %d outremains %d l: %d s: %s\n", inbytes, outbytes, l, utf8buf);
	}
#endif

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
