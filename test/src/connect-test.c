#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
test_SQLConnect()
{
	SQLRETURN ret;
	SQLCHAR *dsn = (SQLCHAR *) "psqlodbc_test_dsn";

	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);

	SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);

	SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);

	printf("Connecting with SQLConnect...");

	ret = SQLConnect(conn, dsn, SQL_NTS, NULL, 0, NULL, 0);
	if (SQL_SUCCEEDED(ret)) {
		printf("connected\n");
	} else {
		print_diag("SQLDriverConnect failed.", SQL_HANDLE_DBC, conn);
		exit(1);
	}
}

int main(int argc, char **argv)
{
	/* the common test_connect() function uses SQLDriverConnect */
	test_connect();
	test_disconnect();

	test_SQLConnect();
	test_disconnect();

	return 0;
}
