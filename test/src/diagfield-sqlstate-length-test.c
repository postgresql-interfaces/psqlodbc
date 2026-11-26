/*
 * Test that SQLGetDiagField(SQL_DIAG_SQLSTATE) writes no more than
 * BufferLength bytes.
 *
 * A SQLSTATE takes 6 bytes with its terminator. A shorter buffer must get
 * the start of it, terminated, and the bytes after the buffer must stay
 * as they were.
 *
 * The length reported for a SQLSTATE that was cut depends on the driver
 * manager. The Windows driver manager passes this ANSI call to the
 * driver's SQLGetDiagFieldW and returns that function's length, which is
 * in bytes of SQLWCHARs, as it is: 10 where unixODBC reports 5.
 * diagfield-sqlstate-length_1.out is the output with 10.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
get_sqlstate(HSTMT hstmt, SQLSMALLINT buflen)
{
	SQLRETURN	rc;
	struct
	{
		char		state[8];
		char		guard[8];
	}			out;
	char	   *buffer;
	SQLSMALLINT	len = -1;
	size_t		i;
	int			touched = 0;

	/* the buffer is the last buflen bytes of state, right before guard */
	memset(&out, 'Z', sizeof(out));
	buffer = out.state + sizeof(out.state) - buflen;
	rc = SQLGetDiagField(SQL_HANDLE_STMT, hstmt, 1, SQL_DIAG_SQLSTATE,
						 buffer, buflen, &len);
	for (i = 0; i < sizeof(out.guard); i++)
		if ('Z' != out.guard[i])
			touched = 1;
	if (!SQL_SUCCEEDED(rc))
		printf("buffer of %d bytes: failed", buflen);
	else
		printf("buffer of %d bytes: \"%.*s\", whole length %d", buflen,
			   buflen > 0 ? buflen - 1 : 0, buffer, len);
	printf(", bytes after the buffer %s\n",
		   touched ? "overwritten" : "untouched");
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1/0", SQL_NTS);
	printf("SELECT 1/0 %s\n", SQL_SUCCEEDED(rc) ? "succeeded" : "failed");

	get_sqlstate(hstmt, 6);
	get_sqlstate(hstmt, 4);
	get_sqlstate(hstmt, 1);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
