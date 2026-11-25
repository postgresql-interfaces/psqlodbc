/*
 * Test the name arguments of the ten catalog functions when they are
 * identifiers, which is the case with SQL_ATTR_METADATA_ID on and with
 * the LowerCaseIdentifier setting on.
 *
 * An unquoted name must be folded to lower case before the search, as the
 * server folds an unquoted identifier, so Items finds the table items and
 * not the table "Items". With SQL_ATTR_METADATA_ID on, a name in double
 * quotes must be searched for as written between the quotes, so "Items"
 * finds the table "Items". The functions are called through their ANSI
 * and their Unicode entry points, and one column of each result is
 * printed that tells the two tables, or the two functions, apart.
 *
 * With SQL_ATTR_METADATA_ID on, ODBC does not allow a NULL identifier
 * argument, and the Windows driver manager refuses a NULL schema, table
 * or column name with HY009. So those calls pass all of them, also the
 * foreign key table of SQLForeignKeys, which exists in both spellings
 * like the tables. The catalog stays NULL, which that driver manager
 * accepts. SQLProcedureColumns can't be called that way: the driver
 * returns no rows when it is given a column name. It is called only with
 * LowerCaseIdentifier on, where a NULL column name is allowed. All ten
 * functions are called in that mode with unquoted names; quotes are not
 * read as written there.
 *
 * In a database with a single-byte encoding the server folds accented
 * letters too, and the driver can't. An unquoted État must then find no
 * rows, although the tables état and "État" both exist, and a quoted
 * "État" must still find "État". That part runs in a LATIN1 database of
 * its own (test_create_latin1_db()) and is skipped where the server can't
 * create one.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define LATIN1_DB "psqlodbc_ident_args"

enum
{
	F_TABLES,
	F_COLUMNS,
	F_PRIMARYKEYS,
	F_SPECIALCOLUMNS,
	F_STATISTICS,
	F_FOREIGNKEYS,
	F_TABLEPRIVILEGES,
	F_COLUMNPRIVILEGES,
	F_PROCEDURES,
	F_PROCEDURECOLUMNS,
	F_COUNT
};

/* The catalog functions and the result column printed for each */
static const struct
{
	const char *name;
	SQLUSMALLINT column;
}			functions[F_COUNT] = {
	{"SQLTables", 3},			/* TABLE_NAME */
	{"SQLColumns", 4},			/* COLUMN_NAME */
	{"SQLPrimaryKeys", 4},		/* COLUMN_NAME */
	{"SQLSpecialColumns", 2},	/* COLUMN_NAME */
	{"SQLStatistics", 9},		/* COLUMN_NAME */
	{"SQLForeignKeys", 8},		/* FKCOLUMN_NAME */
	{"SQLTablePrivileges", 3},	/* TABLE_NAME */
	{"SQLColumnPrivileges", 4},	/* COLUMN_NAME */
	{"SQLProcedures", 3},		/* PROCEDURE_NAME */
	{"SQLProcedureColumns", 4}	/* COLUMN_NAME */
};

static void
exec(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);

	CHECK_STMT_RESULT(rc, (char *) sql, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

/*
 * Convert the UTF-8 string s, of characters below U+0800, to SQLWCHAR.
 * Return NULL for NULL.
 */
static SQLWCHAR *
to_wide(const char *s, SQLWCHAR *w)
{
	const unsigned char *p = (const unsigned char *) s;
	SQLWCHAR   *start = w;

	if (!s)
		return NULL;
	while (*p)
	{
		if (*p < 0x80)
			*w++ = *p++;
		else
		{
			*w++ = (SQLWCHAR) (((p[0] & 0x1f) << 6) | (p[1] & 0x3f));
			p += 2;
		}
	}
	*w = 0;
	return start;
}

/*
 * Call one catalog function, through its Unicode entry point if wide is
 * set, and print the distinct values of its result column, in the order
 * of the rows. name is the table for the table functions and the
 * procedure for the procedure functions. column is passed to the
 * functions that take a column name, and fktable to SQLForeignKeys;
 * NULL for the other functions.
 */
static void
call(HSTMT hstmt, int fn, BOOL wide, const char *schema, const char *name,
	 const char *column, const char *fktable)
{
	SQLRETURN	rc;
	SQLWCHAR	wschema[64], wname[64], wcolumn[64], wfktable[64];
	SQLWCHAR   *ws = to_wide(schema, wschema);
	SQLWCHAR   *wn = to_wide(name, wname);
	SQLWCHAR   *wc = to_wide(column, wcolumn);
	SQLWCHAR   *wf = to_wide(fktable, wfktable);
	SQLCHAR    *s = (SQLCHAR *) schema;
	SQLCHAR    *n = (SQLCHAR *) name;
	SQLCHAR    *c = (SQLCHAR *) column;
	SQLCHAR    *f = (SQLCHAR *) fktable;
	SQLSMALLINT clen = column ? SQL_NTS : 0;
	char		values[16][64];
	char		value[64];
	SQLLEN		ind;
	int			nvalues = 0;
	int			i;

	switch (fn)
	{
		case F_TABLES:
			rc = wide ?
				SQLTablesW(hstmt, NULL, 0, ws, SQL_NTS, wn, SQL_NTS, NULL, 0) :
				SQLTables(hstmt, NULL, 0, s, SQL_NTS, n, SQL_NTS, NULL, 0);
			break;
		case F_COLUMNS:
			rc = wide ?
				SQLColumnsW(hstmt, NULL, 0, ws, SQL_NTS, wn, SQL_NTS, wc, clen) :
				SQLColumns(hstmt, NULL, 0, s, SQL_NTS, n, SQL_NTS, c, clen);
			break;
		case F_PRIMARYKEYS:
			rc = wide ?
				SQLPrimaryKeysW(hstmt, NULL, 0, ws, SQL_NTS, wn, SQL_NTS) :
				SQLPrimaryKeys(hstmt, NULL, 0, s, SQL_NTS, n, SQL_NTS);
			break;
		case F_SPECIALCOLUMNS:
			rc = wide ?
				SQLSpecialColumnsW(hstmt, SQL_BEST_ROWID, NULL, 0, ws, SQL_NTS,
								   wn, SQL_NTS, SQL_SCOPE_SESSION,
								   SQL_NO_NULLS) :
				SQLSpecialColumns(hstmt, SQL_BEST_ROWID, NULL, 0, s, SQL_NTS,
								  n, SQL_NTS, SQL_SCOPE_SESSION, SQL_NO_NULLS);
			break;
		case F_STATISTICS:
			rc = wide ?
				SQLStatisticsW(hstmt, NULL, 0, ws, SQL_NTS, wn, SQL_NTS,
							   SQL_INDEX_ALL, SQL_QUICK) :
				SQLStatistics(hstmt, NULL, 0, s, SQL_NTS, n, SQL_NTS,
							  SQL_INDEX_ALL, SQL_QUICK);
			break;
		case F_FOREIGNKEYS:
			/* name is the table with the primary key */
			rc = wide ?
				SQLForeignKeysW(hstmt, NULL, 0, ws, SQL_NTS, wn, SQL_NTS,
								NULL, 0, ws, SQL_NTS, wf, SQL_NTS) :
				SQLForeignKeys(hstmt, NULL, 0, s, SQL_NTS, n, SQL_NTS,
							   NULL, 0, s, SQL_NTS, f, SQL_NTS);
			break;
		case F_TABLEPRIVILEGES:
			rc = wide ?
				SQLTablePrivilegesW(hstmt, NULL, 0, ws, SQL_NTS, wn, SQL_NTS) :
				SQLTablePrivileges(hstmt, NULL, 0, s, SQL_NTS, n, SQL_NTS);
			break;
		case F_COLUMNPRIVILEGES:
			rc = wide ?
				SQLColumnPrivilegesW(hstmt, NULL, 0, ws, SQL_NTS, wn, SQL_NTS,
									 wc, clen) :
				SQLColumnPrivileges(hstmt, NULL, 0, s, SQL_NTS, n, SQL_NTS,
									c, clen);
			break;
		case F_PROCEDURES:
			rc = wide ?
				SQLProceduresW(hstmt, NULL, 0, ws, SQL_NTS, wn, SQL_NTS) :
				SQLProcedures(hstmt, NULL, 0, s, SQL_NTS, n, SQL_NTS);
			break;
		default:
			rc = wide ?
				SQLProcedureColumnsW(hstmt, NULL, 0, ws, SQL_NTS, wn, SQL_NTS,
									 wc, clen) :
				SQLProcedureColumns(hstmt, NULL, 0, s, SQL_NTS, n, SQL_NTS,
									c, clen);
			break;
	}
	printf("%s%s(%s): ", functions[fn].name, wide ? "W" : "", name);
	if (!SQL_SUCCEEDED(rc))
	{
		printf("failed\n");
		print_diag("", SQL_HANDLE_STMT, hstmt);
		SQLFreeStmt(hstmt, SQL_CLOSE);
		return;
	}
	while (SQL_SUCCEEDED(SQLFetch(hstmt)))
	{
		rc = SQLGetData(hstmt, functions[fn].column, SQL_C_CHAR, value,
						sizeof(value), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		/* the return value of a function has an empty column name */
		if (SQL_NULL_DATA == ind || '\0' == value[0])
			continue;
		for (i = 0; i < nvalues; i++)
			if (strcmp(values[i], value) == 0)
				break;
		if (i == nvalues && nvalues < 16)
			strcpy(values[nvalues++], value);
	}
	for (i = 0; i < nvalues; i++)
		printf("%s%s", i ? ", " : "", values[i]);
	printf(nvalues ? "\n" : "no rows\n");
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

/*
 * Create, in the schema ident_args, the table and the function that the
 * server makes of the unquoted names, and the ones with exactly these
 * names. The table orders references the first table and "Orders" the
 * second. The tables' columns and the functions' parameters have
 * different names.
 */
static void
create_objects(HSTMT hstmt, const char *table, const char *proc)
{
	char		sql[512];

	exec(hstmt, "SET client_min_messages = warning");
	exec(hstmt, "DROP SCHEMA IF EXISTS ident_args CASCADE");
	exec(hstmt, "CREATE SCHEMA ident_args");
	snprintf(sql, sizeof(sql),
			 "CREATE TABLE ident_args.%s "
			 "(item_id int4 PRIMARY KEY, lower_only text)", table);
	exec(hstmt, sql);
	snprintf(sql, sizeof(sql),
			 "CREATE TABLE ident_args.\"%s\" "
			 "(\"Item_Id\" int4 PRIMARY KEY, mixed_only text)", table);
	exec(hstmt, sql);
	snprintf(sql, sizeof(sql),
			 "CREATE TABLE ident_args.Orders "
			 "(lower_ref int4 REFERENCES ident_args.%s)", table);
	exec(hstmt, sql);
	snprintf(sql, sizeof(sql),
			 "CREATE TABLE ident_args.\"Orders\" "
			 "(mixed_ref int4 REFERENCES ident_args.\"%s\")", table);
	exec(hstmt, sql);
	snprintf(sql, sizeof(sql),
			 "CREATE FUNCTION ident_args.%s(lower_arg int4) RETURNS int4 "
			 "LANGUAGE sql AS 'SELECT 1'", proc);
	exec(hstmt, sql);
	snprintf(sql, sizeof(sql),
			 "CREATE FUNCTION ident_args.\"%s\"(mixed_arg int4) RETURNS int4 "
			 "LANGUAGE sql AS 'SELECT 2'", proc);
	exec(hstmt, sql);
}

/*
 * Call the functions with the unquoted table or procedure name, and the
 * other names unquoted too. The schema is passed in upper case, so it has
 * to be folded as well. With metadata_id, which says that
 * SQL_ATTR_METADATA_ID is on, also call them with every name quoted, and
 * leave out SQLProcedureColumns.
 */
static void
call_all(HSTMT hstmt, const char *table, const char *proc, BOOL metadata_id)
{
	char		quoted_table[64], quoted_proc[64];
	int			wide, fn;

	snprintf(quoted_table, sizeof(quoted_table), "\"%s\"", table);
	snprintf(quoted_proc, sizeof(quoted_proc), "\"%s\"", proc);
	for (wide = 0; wide < 2; wide++)
	{
		for (fn = 0; fn < F_COUNT; fn++)
		{
			if (fn >= F_PROCEDURES)
			{
				if (metadata_id && F_PROCEDURECOLUMNS == fn)
					continue;
				call(hstmt, fn, wide, "IDENT_ARGS", proc, NULL, NULL);
				if (metadata_id)
					call(hstmt, fn, wide, "IDENT_ARGS", quoted_proc, NULL,
						 NULL);
			}
			else
			{
				call(hstmt, fn, wide, "IDENT_ARGS", table, "Item_Id",
					 "Orders");
				if (metadata_id)
					call(hstmt, fn, wide, "IDENT_ARGS", quoted_table,
						 "\"Item_Id\"", "\"Orders\"");
			}
		}
	}
}

/*
 * Run the calls with SQL_ATTR_METADATA_ID on, and then on a connection
 * with LowerCaseIdentifier on. The objects must exist in the database
 * that connectparams names, or in the test database if it is NULL.
 */
static void
run_both_modes(char *connectparams, const char *table, const char *proc,
			   BOOL quoting_cases)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		params[256];

	test_connect_ext(connectparams);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER) SQL_TRUE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	printf("With SQL_ATTR_METADATA_ID on\n");
	call_all(hstmt, table, proc, TRUE);
	if (quoting_cases)
	{
		/* blanks around the quotes are not part of the name */
		call(hstmt, F_TABLES, FALSE, "IDENT_ARGS", "  \"Items\" ", NULL, NULL);
		/* a doubled quote between the quotes stands for one quote */
		call(hstmt, F_TABLES, FALSE, "IDENT_ARGS", "\"Wei\"\"rd\"", NULL,
			 NULL);
	}
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	snprintf(params, sizeof(params), "%s%sLowerCaseIdentifier=1",
			 connectparams ? connectparams : "", connectparams ? ";" : "");
	test_connect_ext(params);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	printf("With LowerCaseIdentifier=1\n");
	call_all(hstmt, table, proc, FALSE);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
}

/*
 * The catalog functions do not search for a catalog name, so a catalog
 * name with an accented letter must not take away the rows found for the
 * other names. "Orders" unquoted folds to the table orders.
 */
static void
call_with_catalog(char *connectparams)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		value[64];
	SQLLEN		ind;
	int			nrows = 0;

	test_connect_ext(connectparams);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER) SQL_TRUE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLTables(hstmt, (SQLCHAR *) "Café", SQL_NTS,
				   (SQLCHAR *) "IDENT_ARGS", SQL_NTS,
				   (SQLCHAR *) "Orders", SQL_NTS, NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLTables failed", hstmt);
	printf("SQLTables(catalog Café, Orders): ");
	while (SQL_SUCCEEDED(SQLFetch(hstmt)))
	{
		rc = SQLGetData(hstmt, 3, SQL_C_CHAR, value, sizeof(value), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("%s%s", nrows++ ? ", " : "", value);
	}
	printf(nrows ? "\n" : "no rows\n");
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
}

/* Create the objects in the database that connectparams names */
static void
setup(char *connectparams, const char *table, const char *proc,
	  const char *extra_sql)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect_ext(connectparams);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	create_objects(hstmt, table, proc);
	if (extra_sql)
		exec(hstmt, extra_sql);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	setup(NULL, "Items", "Calc",
		  "CREATE TABLE ident_args.\"Wei\"\"rd\" (id int4)");
	run_both_modes(NULL, "Items", "Calc", TRUE);
	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	exec(hstmt, "DROP SCHEMA ident_args CASCADE");
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	printf("\nIn a LATIN1 database\n");
	if (!test_create_latin1_db(LATIN1_DB))
		return 0;
	setup("Database=" LATIN1_DB, "État", "Écart", NULL);
	run_both_modes("Database=" LATIN1_DB, "État", "Écart", FALSE);
	call_with_catalog("Database=" LATIN1_DB);
	test_drop_db(LATIN1_DB);
	return 0;
}
