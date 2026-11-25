/*
 * Test {fn convert(?, type)} whose parameter marker is bound as an output
 * parameter, while the driver substitutes parameters itself
 * (UseServerSidePrepare=0) and so leaves output parameters out of the
 * statement text.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
exec_with_output_param(const char *sql)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	result = 0;
	SQLLEN		result_ind = 0;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_OUTPUT, SQL_C_LONG, SQL_INTEGER,
						  0, 0, &result, sizeof(result), &result_ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	printf("%s\n", sql);
	fflush(stdout);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("output parameter = %d\n", (int) result);
	else
		print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);
	fflush(stdout);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int main(int argc, char **argv)
{
	test_connect_ext("UseServerSidePrepare=0");

	exec_with_output_param("{? = call char_length('four')}");
	exec_with_output_param("SELECT {fn convert(?, SQL_INTEGER)}");

	test_disconnect();
	return 0;
}
