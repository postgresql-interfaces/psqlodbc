/*
 * Test @@identity
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static HSTMT hstmt = SQL_NULL_HSTMT;

/*
 * The driver has to parse the insert statement to extract the table name,
 * so it might or might not work depending on the table name. This function
 * tests it with a table whose name is given as argument.
 *
 * NOTE: The table name is injected directly into the SQL statement. It should
 * already contain any escaping or quoting needed!
 */
void
test_identity_col(const char *createsql, const char *insertsql, const char *verifysql)
{
	SQLRETURN rc;
	SQLLEN		rowcount;

	/* Create the test table */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) createsql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while creating temp table", hstmt);

	/* Insert some rows using the statement given by the caller */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) insertsql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLRowCount(hstmt, &rowcount);
	CHECK_STMT_RESULT(rc, "SQLRowCount failed", hstmt);
	printf("# of rows inserted: %d\n", (int) rowcount);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT @@IDENTITY AS last_insert", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Fetch result */
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Print the contents of the table after the updates to verify */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) verifysql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}


int main(int argc, char **argv)
{
	SQLRETURN	rc;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	test_identity_col(
		"CREATE TEMPORARY TABLE tmptable (id serial primary key, t text)",
		"INSERT INTO tmptable (t) VALUES ('simple')",
		"SELECT * FROM tmptable");

	test_identity_col(
		"CREATE TEMPORARY TABLE tmptable2 (id serial primary key, t text)",
		"INSERT INTO tmptable2 (t) VALUES ('value.with.dots')",
		"SELECT * FROM tmptable2");

	test_identity_col(
		"CREATE TEMPORARY TABLE \"tmp table\" (id serial primary key, t text)",
		"INSERT INTO \"tmp table\" (t) VALUES ('space in table name')",
		"SELECT * FROM \"tmp table\"");

	/* Clean up */
	test_disconnect();

	return 0;
}
