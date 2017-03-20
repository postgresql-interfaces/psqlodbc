/*
 * Initializes contrib_regression database.
 *
 * DROPs and CREATEs the database, then executes all statements passed in
 * stdin. Use "reset-db < sampletables.sql" to run.
 *
 * This uses the same psqlodbc_test_dsn datasource to connect that the
 * actual regression tests use.
 */
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#define	snprintf _snprintf
#endif

#include <sql.h>
#include <sqlext.h>

#include "src/common.h"

SQLHENV env;
SQLHDBC conn;
static HSTMT hstmt = SQL_NULL_HSTMT;

static void
connect_to_db(char *dsn)
{
	SQLRETURN	ret;
	char		errmsg[500];
	SQLSMALLINT textlen;
	char		sqlstate[20];

	SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);

	ret = SQLDriverConnect(conn, NULL, (SQLCHAR *) dsn, SQL_NTS, NULL, 0, NULL,
						   SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(ret))
	{
		printf("connection to %s failed\n", dsn);

		ret = SQLGetDiagRec(SQL_HANDLE_DBC, conn, 1, sqlstate, NULL,
							errmsg, sizeof(errmsg), &textlen);
		if (ret == SQL_INVALID_HANDLE)
			printf("Invalid handle\n");
		else if (SQL_SUCCEEDED(ret))
			printf("%s=%s\n", sqlstate, errmsg);
		exit(1);
	}

	printf("connected to %s\n", dsn);

	ret = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(ret))
	{
		printf("SQLAllocHandle failed\n");
		exit(1);
	}
}

static void
run_statement(char *statement)
{
	SQLRETURN	ret;
	char		errmsg[500];
	SQLSMALLINT textlen;
	char		sqlstate[20];

	/*
	 * Skip empty lines. The server would just ignore them too, but might as
	 * well avoid the round-trip.
	 */
	if (statement[0] == '\0' || statement[0] == '\n')
		return;

	/* Skip comment lines too. */
	if (statement[0] == '-' && statement[1] == '-')
		return;

	ret = SQLExecDirect(hstmt, (SQLCHAR *) statement, SQL_NTS);
	if (!SQL_SUCCEEDED(ret))
	{
		printf("Statement failed: %s\n", statement);
		ret = SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, NULL,
							errmsg, sizeof(errmsg), &textlen);
		if (ret == SQL_INVALID_HANDLE)
			printf("Invalid handle\n");
		else if (SQL_SUCCEEDED(ret))
			printf("%s=%s\n", sqlstate, errmsg);
		exit(1);
	}
	(void) SQLFreeStmt(hstmt, SQL_CLOSE);
}

int main(int argc, char **argv)
{
	char		line[500], dsn[100];

	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
	SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);

	snprintf(dsn, sizeof(dsn), "DSN=%s;Database=postgres", get_test_dsn());
	connect_to_db(dsn);
	printf("Dropping and creating database contrib_regression...\n");
	run_statement("DROP DATABASE IF EXISTS contrib_regression");
	run_statement("CREATE DATABASE contrib_regression");

	snprintf(dsn, sizeof(dsn), "DSN=%s;Database=contrib_regression", get_test_dsn());
	connect_to_db(dsn);

	printf("Running initialization script...\n");
	while (fgets(line, sizeof(line), stdin) != NULL)
		run_statement(line);

	printf("Done!\n");

	return 0;
}
