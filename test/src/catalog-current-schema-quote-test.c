/*
 * Test the catalog functions when they are called with a table or
 * procedure name and without a schema, and the current schema's name
 * contains a single quote.
 *
 * Without a schema argument the driver searches the current schema, and
 * puts the name the server reports for it into its catalog query as a
 * string literal. A quote in that name must be escaped like one in a name
 * the application passes. Otherwise the literal ends at the quote, and
 * the rest of the schema name, which whoever created the schema chose, is
 * read as SQL in the session of the user who calls the catalog function.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
exec(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);

	CHECK_STMT_RESULT(rc, (char *) sql, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

/*
 * Print the distinct values of two columns of the result of the catalog
 * function whose return code is rc, in the order of the rows.
 */
static void
show(HSTMT hstmt, const char *what, SQLRETURN rc, SQLUSMALLINT col1,
	 SQLUSMALLINT col2)
{
	char		seen[16][160];
	char		v1[64], v2[64], both[160];
	SQLLEN		ind1, ind2;
	int			nseen = 0;
	int			i;

	printf("%s: ", what);
	if (!SQL_SUCCEEDED(rc))
	{
		printf("failed\n");
		print_diag("", SQL_HANDLE_STMT, hstmt);
		SQLFreeStmt(hstmt, SQL_CLOSE);
		return;
	}
	while (SQL_SUCCEEDED(SQLFetch(hstmt)))
	{
		v1[0] = v2[0] = '\0';
		rc = SQLGetData(hstmt, col1, SQL_C_CHAR, v1, sizeof(v1), &ind1);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		rc = SQLGetData(hstmt, col2, SQL_C_CHAR, v2, sizeof(v2), &ind2);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		snprintf(both, sizeof(both), "%s / %s",
				 SQL_NULL_DATA == ind1 ? "NULL" : v1,
				 SQL_NULL_DATA == ind2 ? "NULL" : v2);
		for (i = 0; i < nseen; i++)
			if (strcmp(seen[i], both) == 0)
				break;
		if (i == nseen && nseen < 16)
			strcpy(seen[nseen++], both);
	}
	for (i = 0; i < nseen; i++)
		printf("%s%s", i ? ", " : "", seen[i]);
	printf(nseen ? "\n" : "no rows\n");
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLCHAR    *tab = (SQLCHAR *) "quote_tab";

	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	exec(hstmt, "SET client_min_messages = warning");
	exec(hstmt, "DROP SCHEMA IF EXISTS \"curr'schema\" CASCADE");
	exec(hstmt, "CREATE SCHEMA \"curr'schema\"");
	exec(hstmt, "CREATE TABLE \"curr'schema\".quote_tab "
		 "(id int4 PRIMARY KEY, t text)");
	exec(hstmt, "CREATE FUNCTION \"curr'schema\".quote_fn(a int4) "
		 "RETURNS int4 LANGUAGE sql AS 'SELECT 1'");
	exec(hstmt, "SET search_path TO \"curr'schema\"");

	/* schema and table of each row, or schema and procedure */
	rc = SQLTables(hstmt, NULL, 0, NULL, 0, tab, SQL_NTS, NULL, 0);
	show(hstmt, "SQLTables", rc, 2, 3);
	/* table and column */
	rc = SQLColumns(hstmt, NULL, 0, NULL, 0, tab, SQL_NTS, NULL, 0);
	show(hstmt, "SQLColumns", rc, 3, 4);
	rc = SQLPrimaryKeys(hstmt, NULL, 0, NULL, 0, tab, SQL_NTS);
	show(hstmt, "SQLPrimaryKeys", rc, 3, 4);
	/* column and type name */
	rc = SQLSpecialColumns(hstmt, SQL_BEST_ROWID, NULL, 0, NULL, 0,
						   tab, SQL_NTS, SQL_SCOPE_SESSION, SQL_NO_NULLS);
	show(hstmt, "SQLSpecialColumns", rc, 2, 4);
	/* table and column */
	rc = SQLStatistics(hstmt, NULL, 0, NULL, 0, tab, SQL_NTS,
					   SQL_INDEX_ALL, SQL_QUICK);
	show(hstmt, "SQLStatistics", rc, 3, 9);
	rc = SQLTablePrivileges(hstmt, NULL, 0, (SQLCHAR *) "", 0, tab, SQL_NTS);
	show(hstmt, "SQLTablePrivileges", rc, 2, 3);
	rc = SQLProcedures(hstmt, NULL, 0, NULL, 0,
					   (SQLCHAR *) "quote_fn", SQL_NTS);
	show(hstmt, "SQLProcedures", rc, 2, 3);

	exec(hstmt, "RESET search_path");
	exec(hstmt, "DROP SCHEMA \"curr'schema\" CASCADE");
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return 0;
}
