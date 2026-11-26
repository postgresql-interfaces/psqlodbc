/*
 * Test SQLTables with empty catalog, schema and table names and a NULL
 * table type.
 *
 * Three empty names are how an application asks for the list of table
 * types, when the table type is "%". The driver must check that the
 * table type is there before it compares it.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* a crash must not lose this line */
	printf("SQLTables with empty names and a NULL table type\n");
	fflush(stdout);
	rc = SQLTables(hstmt, (SQLCHAR *) "", 0, (SQLCHAR *) "", 0,
				   (SQLCHAR *) "", 0, NULL, 0);
	if (SQL_SUCCEEDED(rc))
		printf("succeeded\n");
	else
		print_diag("failed", SQL_HANDLE_STMT, hstmt);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
