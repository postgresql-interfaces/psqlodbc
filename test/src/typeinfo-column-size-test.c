/*
 * Test that every SQLGetTypeInfo(SQL_ALL_TYPES) row can be fetched with
 * COLUMN_SIZE bound as SQL_C_ULONG and no indicator, the way MSDASQL binds
 * it.  bytea used to report SQL_NO_TOTAL (-4) there, which fails the 22003
 * range check for unsigned C types.  (issue #215)
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLCHAR		type_name[64];
	SQLSMALLINT	data_type;
	SQLUINTEGER	column_size;

	test_connect_ext("ByteaAsLongVarBinary=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLGetTypeInfo(hstmt, SQL_ALL_TYPES);
	CHECK_STMT_RESULT(rc, "SQLGetTypeInfo failed", hstmt);

	rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, type_name, sizeof(type_name), NULL);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_SSHORT, &data_type, sizeof(data_type), NULL);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 3, SQL_C_ULONG, &column_size, sizeof(column_size), NULL);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	while ((rc = SQLFetch(hstmt)) != SQL_NO_DATA)
	{
		if (!SQL_SUCCEEDED(rc))
		{
			print_diag("SQLFetch failed", SQL_HANDLE_STMT, hstmt);
			break;
		}
		if (data_type == SQL_LONGVARBINARY)
			printf("%s: COLUMN_SIZE=%lu\n", (char *) type_name,
				   (unsigned long) column_size);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	test_disconnect();

	return 0;
}
