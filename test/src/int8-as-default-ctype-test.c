/*
 * With BI=4 ("Int8 As: int4") an int8 column is reported as SQL_INTEGER, so
 * SQL_C_DEFAULT must store a 4-byte SQLINTEGER, not an 8-byte SQLBIGINT,
 * and the reported buffer length must match.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
fetch_default(const char *sql)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLSMALLINT	sqltype;
	SQLLEN		buffer_length = 0;
	SQLLEN		ind = 0;
	size_t		i;
	int			touched = 0;
	struct
	{
		SQLINTEGER	value;
		unsigned char guard[8];
	}			out;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLDescribeCol(hstmt, 1, NULL, 0, NULL, &sqltype, NULL, NULL, NULL);
	CHECK_STMT_RESULT(rc, "SQLDescribeCol failed", hstmt);
	rc = SQLColAttribute(hstmt, 1, SQL_COLUMN_LENGTH, NULL, 0, NULL, &buffer_length);
	CHECK_STMT_RESULT(rc, "SQLColAttribute failed", hstmt);
	printf("%s: reported as %s, buffer length %ld\n",
		   sql, datatype_str(sqltype), (long) buffer_length);

	memset(&out, 0x5a, sizeof(out));
	rc = SQLBindCol(hstmt, 1, SQL_C_DEFAULT, &out.value, sizeof(out.value), &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLFetch(hstmt);

	for (i = 0; i < sizeof(out.guard); i++)
		if (out.guard[i] != 0x5a)
			touched = 1;
	if (SQL_SUCCEEDED(rc))
		printf("ok value %d, length %ld", (int) out.value, (long) ind);
	else
	{
		SQLCHAR		state[6] = "";
		SQLINTEGER	native;
		SQLCHAR		msg[256];
		SQLSMALLINT	len;

		SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, state, &native,
					  msg, sizeof(msg), &len);
		printf("error SQLSTATE=%s", (char *) state);
	}
	printf(", bytes behind the SQLINTEGER: %s\n",
		   touched ? "overwritten" : "untouched");

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int
main(int argc, char **argv)
{
	test_connect_ext("BI=4");

	fetch_default("SELECT 42::int8");
	fetch_default("SELECT 4294967297::int8");

	test_disconnect();
	return 0;
}
