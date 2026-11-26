/*
 * Test that SQLGetData(SQL_C_BINARY) on a uuid or int4 column reports
 * SQL_NO_DATA once the value has been returned, like it does for bytea,
 * so that an application reading a binary column in pieces until
 * SQL_NO_DATA does not loop forever.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
read_until_no_data(const char *sql, SQLLEN buflen)
{
	SQLRETURN	rc;
	SQLHSTMT	hstmt = SQL_NULL_HSTMT;
	char		buf[64];
	SQLLEN		ind;
	int			i;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	printf("%s, buflen %d:", sql, (int) buflen);
	/* a few calls are enough: a correct column ends in SQL_NO_DATA */
	for (i = 0; i < 4; i++)
	{
		ind = -99;
		rc = SQLGetData(hstmt, 1, SQL_C_BINARY, buf, buflen, &ind);
		if (rc == SQL_NO_DATA)
		{
			printf(" SQL_NO_DATA\n");
			break;
		}
		if (rc == SQL_SUCCESS)
			printf(" SQL_SUCCESS(%ld)", (long) ind);
		else if (rc == SQL_SUCCESS_WITH_INFO)
			printf(" SQL_SUCCESS_WITH_INFO(%ld)", (long) ind);
		else
		{
			printf("\n");
			print_diag("SQLGetData failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}
	}
	if (rc != SQL_NO_DATA)
		printf(" ... no SQL_NO_DATA after %d calls\n", i);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int
main(int argc, char **argv)
{
	test_connect();

	/* buffer large enough for the value */
	read_until_no_data("SELECT '\\x00112233'::bytea", 64);
	read_until_no_data("SELECT 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid", 64);
	read_until_no_data("SELECT 123456::int4", 64);
	/* buffer too small for the value */
	read_until_no_data("SELECT '\\x00112233'::bytea", 3);
	read_until_no_data("SELECT 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid", 3);
	read_until_no_data("SELECT 123456::int4", 3);

	test_disconnect();

	return 0;
}
