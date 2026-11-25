/*
 * Under statement-level rollback a failed catalog call must undo its own
 * work only.  Each case inserts rows 1 and 2, makes one catalog call, inserts
 * row 3 and commits; all three rows must be committed.
 *
 * catalog-error-rollback_1.out is the output on Windows: there the ANSI
 * entry points convert the name from the ANSI code page, where 0xe9 is a
 * valid character, so the "rejected name" calls succeed instead.  The
 * missing table case still fails there and still checks the rollback.
 */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static HSTMT hstmt = SQL_NULL_HSTMT;

/* a lone 0xe9 is not valid UTF-8, so the server rejects a query containing it */
static SQLCHAR rejected_name[] = "caf\xe9";

static void
exec_sql(const char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static void
commit(void)
{
	SQLRETURN	rc;

	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	CHECK_CONN_RESULT(rc, "SQLEndTran failed", conn);
}

static void
begin_case(void)
{
	exec_sql("INSERT INTO rollbacktab VALUES (1)");
	exec_sql("INSERT INTO rollbacktab VALUES (2)");
}

static void
end_case(const char *label, SQLRETURN catalog_rc)
{
	SQLRETURN	rc;
	SQLCHAR		sqlstate[6] = "";
	SQLCHAR		row[16];

	if (SQL_SUCCEEDED(catalog_rc))
		printf("%s: succeeded", label);
	else
	{
		SQLGetDiagField(SQL_HANDLE_STMT, hstmt, 1, SQL_DIAG_SQLSTATE,
						sqlstate, sizeof(sqlstate), NULL);
		printf("%s: SQLSTATE=%s", label, (char *) sqlstate);
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);

	exec_sql("INSERT INTO rollbacktab VALUES (3)");
	commit();

	printf(", committed rows:");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT i FROM rollbacktab ORDER BY i", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	while (SQL_SUCCEEDED(SQLFetch(hstmt)))
	{
		SQLGetData(hstmt, 1, SQL_C_CHAR, row, sizeof(row), NULL);
		printf(" %s", (char *) row);
	}
	printf("\n");
	SQLFreeStmt(hstmt, SQL_CLOSE);

	exec_sql("DELETE FROM rollbacktab");
	commit();
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;

	/* the default; installcheck-all also runs the suite with 7.4-1 and 7.4-0 */
	test_connect_ext("Protocol=7.4-2");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr failed", conn);

	exec_sql("CREATE TEMPORARY TABLE rollbacktab (i int4)");
	commit();

	/* a catalog call that succeeds */
	begin_case();
	rc = SQLSpecialColumns(hstmt, SQL_BEST_ROWID, NULL, 0, NULL, 0,
						   (SQLCHAR *) "testtab1", SQL_NTS,
						   SQL_SCOPE_SESSION, SQL_NULLABLE);
	end_case("SQLSpecialColumns existing table", rc);

	/* a catalog call that fails on the application's own statement */
	begin_case();
	rc = SQLProcedures(hstmt, NULL, 0, NULL, 0, rejected_name, SQL_NTS);
	end_case("SQLProcedures rejected name", rc);

	/* the driver reports the missing table itself; the server raises no error */
	begin_case();
	rc = SQLSpecialColumns(hstmt, SQL_BEST_ROWID, NULL, 0, NULL, 0,
						   (SQLCHAR *) "no_such_table", SQL_NTS,
						   SQL_SCOPE_SESSION, SQL_NULLABLE);
	end_case("SQLSpecialColumns missing table", rc);

	/* the server rejects the catalog query */
	begin_case();
	rc = SQLTables(hstmt, NULL, 0, NULL, 0, rejected_name, SQL_NTS, NULL, 0);
	end_case("SQLTables rejected name", rc);

	begin_case();
	rc = SQLColumns(hstmt, NULL, 0, NULL, 0, rejected_name, SQL_NTS, NULL, 0);
	end_case("SQLColumns rejected name", rc);

	begin_case();
	rc = SQLStatistics(hstmt, NULL, 0, NULL, 0, rejected_name, SQL_NTS,
					   SQL_INDEX_ALL, SQL_QUICK);
	end_case("SQLStatistics rejected name", rc);

	begin_case();
	rc = SQLPrimaryKeys(hstmt, NULL, 0, NULL, 0, rejected_name, SQL_NTS);
	end_case("SQLPrimaryKeys rejected name", rc);

	begin_case();
	rc = SQLSpecialColumns(hstmt, SQL_BEST_ROWID, NULL, 0, NULL, 0,
						   rejected_name, SQL_NTS,
						   SQL_SCOPE_SESSION, SQL_NULLABLE);
	end_case("SQLSpecialColumns rejected name", rc);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
