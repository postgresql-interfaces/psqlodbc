/*
 * Test that the driver finds parameter markers in the same places as the
 * server's lexer when it replaces them with the parameter values itself
 * (UseServerSidePrepare=0).
 *
 * Each statement has a '?' inside a string literal or a comment, followed
 * by one real marker, which is bound to the text P. If the driver reads
 * the literal or comment differently from the server, it either puts the
 * value in place of the '?' inside it, where the value can end the
 * literal and be read as SQL, or misses the real marker. The cases:
 *
 * - an escape string written with a lower case e, with \' inside
 * - a literal right behind a keyword that ends in E: that E does not
 *   start an escape string, so the backslash is an ordinary character
 * - a comment that starts with the three characters / * /
 * - a dollar-quoted string whose tag starts with an underscore
 * - a line comment that ends at a carriage return
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
run(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc;
	char		param[] = "P";
	SQLLEN		ind = SQL_NTS;
	char		a[64], b[64];
	SQLLEN		a_ind, b_ind;
	char		shown[128];
	size_t		i;

	/* show the carriage return as ^, so the output has plain lines */
	for (i = 0; sql[i] && i < sizeof(shown) - 1; i++)
		shown[i] = ('\r' == sql[i]) ? '^' : sql[i];
	shown[i] = '\0';
	printf("%s\n", shown);

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
						  20, 0, param, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("  failed", SQL_HANDLE_STMT, hstmt);
		SQLFreeStmt(hstmt, SQL_CLOSE);
		return;
	}
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, a, sizeof(a), &a_ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	rc = SQLGetData(hstmt, 2, SQL_C_CHAR, b, sizeof(b), &b_ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("  a = [%s], b = [%s]\n", a, b);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect_ext("UseServerSidePrepare=0");
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	run(hstmt, "SELECT e'it\\'s?' AS a, ?::text AS b");
	run(hstmt, "SELECT 'x' LIKE'it\\' AS a, ?::text AS b");
	run(hstmt, "SELECT /*/ ? */ 'c' AS a, ?::text AS b");
	run(hstmt, "SELECT $_$ ? $_$ AS a, ?::text AS b");
	run(hstmt, "SELECT 'c' AS a -- ?\r , ?::text AS b");

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
