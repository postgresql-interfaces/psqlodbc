#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char sql[100000];
	char *sqlend;
	int i;

	test_connect();

	rc = SQLAllocStmt(conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLTables(hstmt, "", SQL_NTS,
				   "public", SQL_NTS,
				   "%", SQL_NTS,
				   "TABLE", SQL_NTS);

	CHECK_STMT_RESULT(rc, "SQLTables failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();
}
