#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char *sql;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	sql =
		"CREATE OR REPLACE FUNCTION raisenotice(s text) RETURNS void AS $$"
		"begin\n"
		"  raise notice 'test notice: %',s;\n"
		"end;\n"
		"$$ LANGUAGE plpgsql";

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeStmt failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	/* Call the function that gives a NOTICE */
	sql = "SELECT raisenotice('foo')";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	if (rc == SQL_SUCCESS_WITH_INFO)
		print_diag("got SUCCESS_WITH_INFO", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeStmt failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	/*
	 * The same, with a really long notice.
	 */
	sql = "SELECT raisenotice(repeat('foo', 100))";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	if (rc == SQL_SUCCESS_WITH_INFO)
		print_diag("got SUCCESS_WITH_INFO", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeStmt failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	/* Clean up */
	test_disconnect();

	return 0;
}
