/*
 * Test SQLForeignKeys on a connection whose Database setting is longer
 * than the database name the server keeps.
 *
 * SQLForeignKeys puts the Database setting into its catalog query as a
 * string literal, escaped, so a quote in the setting takes two bytes. The
 * setting holds up to 255 bytes, but the server uses only the first 63
 * bytes of a database name, so a longer setting still connects. The
 * literal must hold the whole escaped setting with its closing quote, or
 * the rest of the setting is read as SQL.
 *
 * The test creates a database with a 63-byte name and connects with
 * settings that start with that name and end in single quotes. It is
 * skipped where the server refuses to create the database.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* 63 bytes, the longest database name the server keeps */
#define LONG_DB "psqlodbc_long_catalog_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

static void
exec(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);

	CHECK_STMT_RESULT(rc, (char *) sql, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

/* Run sql on a connection to the default test database */
static BOOL
exec_on_default_db(const char *sql)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	return SQL_SUCCEEDED(rc);
}

/* Create the tables that SQLForeignKeys reports on */
static void
create_tables(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect_ext("Database=" LONG_DB);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	exec(hstmt, "CREATE TABLE fk_parent (id int4 PRIMARY KEY)");
	exec(hstmt, "CREATE TABLE fk_child (parent_id int4 REFERENCES fk_parent)");
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
}

/*
 * Connect with a Database setting of LONG_DB followed by nquotes single
 * quotes, and call SQLForeignKeys.
 */
static void
check_foreign_keys(int nquotes)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		setting[256];
	char		connstr[300];
	char		cat[300], pktable[64], fktable[64], fkcolumn[64];
	SQLLEN		ind;
	size_t		len = strlen(LONG_DB);
	int			rows = 0;

	memcpy(setting, LONG_DB, len);
	memset(setting + len, '\'', nquotes);
	setting[len + nquotes] = '\0';
	printf("Database setting of %d bytes, %d escaped\n",
		   (int) strlen(setting), (int) (len + 2 * nquotes));

	snprintf(connstr, sizeof(connstr), "Database=%s", setting);
	test_connect_ext(connstr);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLForeignKeys(hstmt, NULL, 0,
						(SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) "fk_parent", SQL_NTS,
						NULL, 0,
						(SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) "fk_child", SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLForeignKeys failed", SQL_HANDLE_STMT, hstmt);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		test_disconnect();
		return;
	}
	while (SQL_SUCCEEDED(rc = SQLFetch(hstmt)))
	{
		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, cat, sizeof(cat), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		rc = SQLGetData(hstmt, 3, SQL_C_CHAR, pktable, sizeof(pktable), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		rc = SQLGetData(hstmt, 7, SQL_C_CHAR, fktable, sizeof(fktable), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		rc = SQLGetData(hstmt, 8, SQL_C_CHAR, fkcolumn, sizeof(fkcolumn), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		/*
		 * The literal is cast to name, which keeps its first 63 bytes, so
		 * the whole literal reached the server if this is LONG_DB.
		 */
		printf("%s.%s references %s, PKTABLE_CAT is %s\n",
			   fktable, fkcolumn, pktable,
			   strcmp(cat, LONG_DB) == 0 ? "the database name" :
			   "something else");
		rows++;
	}
	printf("%d foreign key rows\n", rows);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
}

int
main(int argc, char **argv)
{
	exec_on_default_db("DROP DATABASE IF EXISTS " LONG_DB);
	if (!exec_on_default_db("CREATE DATABASE " LONG_DB))
	{
		printf("Skipped, the server can't create the test database\n");
		return 0;
	}
	create_tables();

	/* Over 128 escaped bytes, where a 128-byte buffer loses the closing quote */
	check_foreign_keys(40);
	/* The longest setting the Database field holds */
	check_foreign_keys(255 - (int) strlen(LONG_DB));

	exec_on_default_db("DROP DATABASE " LONG_DB);
	return 0;
}
