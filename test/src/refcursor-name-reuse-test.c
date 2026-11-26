/*
 * Test that a refcursor name refused by FetchRefcursors is not used later.
 *
 * With FetchRefcursors=1 the driver takes the cursor name from the server
 * and writes it between double quotes, so it refuses a name containing ".
 * The refused name must not stay on the statement. With UseDeclareFetch=1
 * the next query on the same handle declares its cursor under the
 * statement's cursor name, so a stale name puts server-chosen text into
 * SQL the application did not write, and that query fails.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect_ext("UseDeclareFetch=1;FetchRefcursors=1");

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE OR REPLACE FUNCTION pg_temp.quoted_refcursor()"
		" RETURNS refcursor AS $$"
		" DECLARE x refcursor := 'x';"
		" c refcursor := 'x\"; create temp table refcur_inj (); --';"
		" BEGIN OPEN x FOR SELECT 1; OPEN c FOR SELECT 1; RETURN c; END"
		" $$ LANGUAGE plpgsql", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE FUNCTION failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"{call pg_temp.quoted_refcursor()}", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		printf("refcursor with a quoted name was fetched\n");
	else
	{
		printf("refcursor with a quoted name was refused\n");
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);

	/* the same handle, a plain query */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'second query'", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		print_result(hstmt);
	else
		print_diag("second query failed", SQL_HANDLE_STMT, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT count(*) FROM pg_class"
		" WHERE relname = 'refcur_inj'"
		" AND relpersistence = 't'", SQL_NTS);
	printf("injected tables found:\n");
	if (SQL_SUCCEEDED(rc))
		print_result(hstmt);
	else
		print_diag("count query failed", SQL_HANDLE_STMT, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_ROLLBACK);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
