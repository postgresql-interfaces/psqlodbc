/*
 * Test SQLSetPos(SQL_UPDATE / SQL_ADD) on an updatable cursor whose result
 * has more columns than the driver's own parser found in the statement.
 *
 * For "SELECT (t).*, now() FROM t" the parser counts two columns while the
 * server returns three. SQLDescribeCol() then fills in the table's columns
 * from the result, but the trailing expression column is left without any
 * FIELD_INFO. Binding that column and updating the row must not
 * dereference the missing entry. The same happens for
 * "SELECT *, now() FROM t" once another session has added a column to t.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define NCOLS	3

static void
describe_and_bind(HSTMT hstmt, char (*buf)[64], SQLLEN *ind)
{
	SQLRETURN	rc;
	SQLSMALLINT	i, ncols, namelen, type, nullable;
	SQLCHAR		name[64];

	rc = SQLNumResultCols(hstmt, &ncols);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);
	printf("%d columns\n", ncols);
	if (ncols != NCOLS)
	{
		printf("unexpected number of columns\n");
		exit(1);
	}
	for (i = 1; i <= ncols; i++)
	{
		rc = SQLDescribeCol(hstmt, i, name, sizeof(name), &namelen,
							&type, NULL, NULL, &nullable);
		CHECK_STMT_RESULT(rc, "SQLDescribeCol failed", hstmt);
		printf("column %d: %s\n", i, name);
		rc = SQLBindCol(hstmt, i, SQL_C_CHAR, buf[i - 1], sizeof(buf[0]), &ind[i - 1]);
		CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	}
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		buf[NCOLS][64];
	SQLLEN		ind[NCOLS];

	/*
	 * With Parse=1 the driver's parser does not expand (t).*, and the
	 * statement below would have the wrong column count.
	 */
	test_connect_ext("Parse=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE setpos_extra_col(id int4 primary key, entry text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO setpos_extra_col VALUES (1, 'one')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);

	printf("Updating through a cursor with an unparsed expression column\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT (setpos_extra_col).*, now() FROM setpos_extra_col", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	describe_and_bind(hstmt, buf, ind);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	printf("fetched: %s\t%s\n", buf[0], buf[1]);

	/* Update 'entry'; the expression column stays bound with a length */
	ind[0] = SQL_IGNORE;
	strcpy(buf[1], "two");
	ind[1] = SQL_NTS;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos UPDATE failed", hstmt);
	printf("SQLSetPos UPDATE succeeded\n");

	/* Add a row the same way */
	strcpy(buf[0], "2");
	ind[0] = SQL_NTS;
	strcpy(buf[1], "three");
	ind[1] = SQL_NTS;
	rc = SQLSetPos(hstmt, 1, SQL_ADD, SQL_LOCK_NO_CHANGE);
	CHECK_STMT_RESULT(rc, "SQLSetPos ADD failed", hstmt);
	printf("SQLSetPos ADD succeeded\n");

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_UNBIND);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	printf("Querying the table again\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT id, entry FROM setpos_extra_col ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
