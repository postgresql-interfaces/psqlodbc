/*
 * Test SQLForeignKeys on a connection whose Database setting is empty.
 *
 * libpq then picks the database from PGDATABASE (or the user name), and
 * the driver's catalog name is the empty string. SQLForeignKeys must still
 * run its catalog query; an empty catalog name is not an error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		dbname[100];
	SQLSMALLINT	len;

	/* the database to use when the connection string leaves it empty */
#ifdef WIN32
	_putenv_s("PGDATABASE", "contrib_regression");
#else
	setenv("PGDATABASE", "contrib_regression", 1);
#endif
	test_connect_ext("Database=");

	rc = SQLGetInfo(conn, SQL_DATABASE_NAME, dbname, sizeof(dbname), &len);
	CHECK_CONN_RESULT(rc, "SQLGetInfo failed", conn);
	printf("catalog name: \"%s\"\n", dbname);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* testtab1 and testtab_fk are created by sampletables.sql */
	rc = SQLForeignKeys(hstmt, NULL, 0,
						(SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) "testtab1", SQL_NTS,
						NULL, 0,
						(SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) "testtab_fk", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
	{
		printf("SQLForeignKeys succeeded\n");
		print_result(hstmt);
	}
	else
	{
		printf("SQLForeignKeys failed\n");
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
