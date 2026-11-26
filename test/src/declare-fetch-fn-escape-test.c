/*
 * Test {fn ...} escapes on a connection with UseDeclareFetch=1 whose
 * arguments contain FOR UPDATE, ';' or SELECT ... INTO ... FROM. Each of
 * these makes the driver give up its "declare ... cursor for" prefix while
 * it is converting the arguments.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
run_query(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc;

	printf("\nQuery: %s\n", sql);
	fflush(stdout);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		print_result(hstmt);
	else
		print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect_ext("UseDeclareFetch=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* Valid: the subquery that locks its row is the argument of ucase() */
	run_query(hstmt, "SELECT {fn ucase((SELECT t FROM testtab1 WHERE id = 1 FOR UPDATE))}");
	/* The same, with more argument text in front of FOR than the prefix is long */
	run_query(hstmt, "SELECT {fn ucase((SELECT t FROM testtab1 WHERE id = 1 AND t IS NOT NULL AND id IS NOT NULL FOR UPDATE))}");

	/* Not valid SQL: these must end in an error */
	run_query(hstmt, "SELECT {fn concat('a'; 'b')}");
	run_query(hstmt, "SELECT {fn concat(into t from x)}");

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	return 0;
}
