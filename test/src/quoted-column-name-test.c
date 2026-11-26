/*
 * Test names containing a single quote where the driver writes them into
 * its own SQL as bare quoted identifiers: the SET list of SQLSetPos
 * (SQL_UPDATE), the column list of SQLBulkOperations(SQL_ADD), and a named
 * procedure parameter.
 *
 * Inside a quoted identifier only " is doubled. A ' must stay single;
 * doubling it names a different column or parameter. A " in a column or
 * parameter name must be doubled, or the rest of the name is read as SQL.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
setpos_and_bulk_add(const char *query)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	id;
	char		val[40];
	SQLLEN		ind_id, ind_val;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
						(SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CONCURRENCY failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
						(SQLPOINTER) SQL_CURSOR_KEYSET_DRIVEN, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr CURSOR_TYPE failed", hstmt);

	rc = SQLBindCol(hstmt, 1, SQL_C_LONG, &id, 0, &ind_id);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, val, sizeof(val), &ind_val);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) query, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	printf("%s\n", query);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	printf("fetched %d %s\n", (int) id, val);

	/* only the quoted column is written */
	ind_id = SQL_IGNORE;
	strcpy(val, "updated");
	ind_val = SQL_NTS;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	if (SQL_SUCCEEDED(rc))
		printf("SQLSetPos(SQL_UPDATE) succeeded\n");
	else
	{
		printf("SQLSetPos(SQL_UPDATE) failed\n");
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}

	strcpy(val, "added");
	ind_val = SQL_NTS;
	rc = SQLBulkOperations(hstmt, SQL_ADD);
	if (SQL_SUCCEEDED(rc))
		printf("SQLBulkOperations(SQL_ADD) succeeded\n");
	else
	{
		printf("SQLBulkOperations(SQL_ADD) failed\n");
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) query, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

/* Call the function fn with its one parameter passed by the name name */
static void
named_param(const char *fn, const char *name)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDESC	hIpd = NULL;
	char		param[] = "hello";
	SQLLEN		ind = SQL_NTS;
	char		sql[128];

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	snprintf(sql, sizeof(sql), "{call pg_temp.%s(?)}", fn);
	rc = SQLPrepare(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
						  20, 0, param, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, &hIpd, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	rc = SQLSetDescField(hIpd, 1, SQL_DESC_NAME, (SQLPOINTER) name, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLSetDescField failed", hstmt);

	rc = SQLExecute(hstmt);
	if (SQL_SUCCEEDED(rc))
	{
		printf("named parameter %s succeeded\n", name);
		print_result(hstmt);
	}
	else
	{
		printf("named parameter %s failed\n", name);
		print_diag("", SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect_ext("UseDeclareFetch=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE quoted_col (id serial, \"o'brien\" text)",
		SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO quoted_col (\"o'brien\") VALUES ('one')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE dquoted_col (id serial, \"we\"\"ird\" text)",
		SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO dquoted_col (\"we\"\"ird\") VALUES ('one')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE OR REPLACE FUNCTION pg_temp.quoted_param_fn(\"p'x\" text)"
		" RETURNS text AS $$ SELECT 'got ' || $1 $$ LANGUAGE sql", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE FUNCTION failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE OR REPLACE FUNCTION pg_temp.dquote_param_fn(\"p\"\"x\" text)"
		" RETURNS text AS $$ SELECT 'got ' || $1 $$ LANGUAGE sql", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE FUNCTION failed", hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	setpos_and_bulk_add("SELECT id, \"o'brien\" FROM quoted_col ORDER BY id");
	setpos_and_bulk_add("SELECT id, \"we\"\"ird\" FROM dquoted_col ORDER BY id");
	named_param("quoted_param_fn", "p'x");
	named_param("dquote_param_fn", "p\"x");

	test_disconnect();

	return 0;
}
