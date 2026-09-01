/*
 * Test that reading an integer value that does not fit the requested C type
 * fails with SQLSTATE 22003 (numeric value out of range) instead of silently
 * returning the wrapped low bits.  (issue #207)
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
read_as(const char *sql, SQLSMALLINT ctype, const char *ctype_name)
{
	SQLRETURN	rc;
	SQLHSTMT	hstmt = SQL_NULL_HSTMT;
	SQLBIGINT	big = 0;
	SQLINTEGER	i32 = 0;
	SQLSMALLINT	i16 = 0;
	SQLLEN		ind = 0;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	if (ctype == SQL_C_SBIGINT)
		rc = SQLGetData(hstmt, 1, ctype, &big, sizeof(big), &ind);
	else if (ctype == SQL_C_SLONG)
		rc = SQLGetData(hstmt, 1, ctype, &i32, sizeof(i32), &ind);
	else
		rc = SQLGetData(hstmt, 1, ctype, &i16, sizeof(i16), &ind);

	printf("%-34s as %-13s: ", sql, ctype_name);
	if (SQL_SUCCEEDED(rc))
	{
		long long	v = (ctype == SQL_C_SBIGINT) ? (long long) big :
					(ctype == SQL_C_SLONG) ? (long long) i32 : (long long) i16;

		printf("ok value=%lld\n", v);
	}
	else
	{
		SQLCHAR		state[6] = "";
		SQLINTEGER	native;
		SQLCHAR		msg[256];
		SQLSMALLINT	len;

		SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, state, &native,
					  msg, sizeof(msg), &len);
		printf("error SQLSTATE=%s\n", (char *) state);
	}
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int
main(int argc, char **argv)
{
	test_connect();

	/* out of range -> 22003 */
	read_as("SELECT 9223372036854775807::bigint", SQL_C_SLONG, "SQL_C_SLONG");
	read_as("SELECT 9223372036854775807::bigint", SQL_C_SSHORT, "SQL_C_SSHORT");
	read_as("SELECT 4294967297::bigint", SQL_C_SLONG, "SQL_C_SLONG");
	read_as("SELECT 70000::int4", SQL_C_SSHORT, "SQL_C_SSHORT");
	/* in range -> ok */
	read_as("SELECT 9223372036854775807::bigint", SQL_C_SBIGINT, "SQL_C_SBIGINT");
	read_as("SELECT 2147483647::bigint", SQL_C_SLONG, "SQL_C_SLONG");
	read_as("SELECT (-32768)::int4", SQL_C_SSHORT, "SQL_C_SSHORT");

	test_disconnect();

	return 0;
}
