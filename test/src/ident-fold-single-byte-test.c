/*
 * The server folds an unquoted name to lower case before it looks it up.
 * It folds A-Z, and when the database encoding is single-byte it also
 * folds accented letters, using the server's locale. The driver folds only
 * A-Z. In a LATIN1 database, INSERT INTO État goes to the table état, but
 * the driver derives the name État, and a table "État" can exist that
 * belongs to someone else.
 *
 * The driver can't fold the way the server's locale does, so it must not
 * derive a name for an unquoted identifier with a non-ASCII letter in a
 * single-byte database. Three features that use the derived name are
 * checked here, each against the decoy table "État":
 *
 *   - SELECT @@IDENTITY after INSERT INTO État must not read the decoy's
 *     sequence. With the name left unresolved it returns NULL.
 *   - With Parse=1, SELECT note FROM État must not describe the decoy's
 *     note column, which is a uuid. The server's description is text.
 *   - SQLColumns and SQLColumnsW must not return the decoy's note column.
 *     With SQL_ATTR_METADATA_ID on, the name État is an identifier, and
 *     with it off, the driver retries an all upper case name such as ÉTAT
 *     in lower case when it finds no rows. Either way the driver would
 *     derive État, so the call returns no rows.
 *
 * Ville and "Ville" are the same pair in ASCII. Those names are still
 * folded by the driver, so all three features must keep working for
 * them. With SQL_ATTR_METADATA_ID on, SQLColumns must find ville, not
 * "Ville", because an unquoted identifier is folded before the search.
 *
 * The test creates its own LATIN1 database (test_create_latin1_db()) and
 * is skipped where the server can't create one. If the database's locale
 * doesn't fold É, the count of tables named état or État is 1 rather than
 * 2, and the test fails.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define FOLD_DB "psqlodbc_ident_fold"

static void
exec(HSTMT hstmt, const char *sql)
{
	SQLRETURN	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);

	CHECK_STMT_RESULT(rc, (char *) sql, hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

/* Create name, which the server folds, and its quoted decoy */
static void
create_pair(HSTMT hstmt, const char *name)
{
	char		sql[256];

	snprintf(sql, sizeof(sql),
			 "CREATE TABLE %s (id serial, note text)", name);
	exec(hstmt, sql);
	/* SQLColumns reports id as auto-increment only if it is NOT NULL */
	snprintf(sql, sizeof(sql),
			 "CREATE TABLE \"%s\" (id int4 NOT NULL "
			 "DEFAULT nextval('decoy_seq'), note uuid)", name);
	exec(hstmt, sql);
}

static void
check_identity(HSTMT hstmt, const char *name)
{
	SQLRETURN	rc;
	char		sql[256];
	char		buf[40];
	SQLLEN		ind;

	snprintf(sql, sizeof(sql),
			 "INSERT INTO %s (note) VALUES ('row')", name);
	exec(hstmt, sql);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT @@IDENTITY", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SELECT @@IDENTITY failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("%s: @@IDENTITY = %s\n", sql,
		   ind == SQL_NULL_DATA ? "NULL" : buf);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

static void
check_describe(HSTMT hstmt, const char *name)
{
	SQLRETURN	rc;
	char		sql[256];

	snprintf(sql, sizeof(sql), "SELECT note FROM %s", name);
	printf("%s\n", sql);
	rc = SQLPrepare(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	print_result_meta(hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
}

/* Convert the UTF-8 string s, of characters below U+0800, to SQLWCHAR */
static void
to_wide(const char *s, SQLWCHAR *w)
{
	const unsigned char *p = (const unsigned char *) s;

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
}

/*
 * Print the type of each note column that SQLColumns, or SQLColumnsW when
 * wide is set, finds in the table name of schema public.
 */
static void
check_columns(HSTMT hstmt, BOOL wide, BOOL metadata_id, const char *name)
{
	SQLRETURN	rc;
	SQLWCHAR	wschema[16], wname[64], wcolumn[16];
	char		type[64];
	SQLLEN		ind;
	int			rows = 0;

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_METADATA_ID,
						(SQLPOINTER) (SQLULEN) (metadata_id ? SQL_TRUE : SQL_FALSE), 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	if (wide)
	{
		to_wide("public", wschema);
		to_wide(name, wname);
		to_wide("note", wcolumn);
		rc = SQLColumnsW(hstmt, NULL, 0, wschema, SQL_NTS, wname, SQL_NTS,
						 wcolumn, SQL_NTS);
	}
	else
		rc = SQLColumns(hstmt, NULL, 0, (SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) name, SQL_NTS, (SQLCHAR *) "note", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLColumns failed", hstmt);
	printf("%s(%s), METADATA_ID %s: ", wide ? "SQLColumnsW" : "SQLColumns",
		   name, metadata_id ? "on" : "off");
	while (SQL_SUCCEEDED(rc = SQLFetch(hstmt)))
	{
		rc = SQLGetData(hstmt, 6, SQL_C_CHAR, type, sizeof(type), &ind);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("%snote is %s", rows++ ? ", " : "", type);
	}
	printf(rows ? "\n" : "no rows\n");
	SQLFreeStmt(hstmt, SQL_CLOSE);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_METADATA_ID,
						(SQLPOINTER) (SQLULEN) SQL_FALSE, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLLEN		ind;
	SQLINTEGER	tables = 0;

	if (!test_create_latin1_db(FOLD_DB))
		return 0;

	test_connect_ext("Database=" FOLD_DB ";Parse=1");
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* currval() of the decoy's sequence is 1000 from here on */
	exec(hstmt, "CREATE SEQUENCE decoy_seq START 1000");
	exec(hstmt, "SELECT nextval('decoy_seq')");
	create_pair(hstmt, "État");
	create_pair(hstmt, "Ville");

	/* The premise: the server folded État, so there are two tables */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT count(*) FROM pg_catalog.pg_class "
		"WHERE relname IN ('état', 'État')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SELECT count failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_SLONG, &tables, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);
	printf("tables named état or État: %d\n", (int) tables);

	check_identity(hstmt, "État");
	check_identity(hstmt, "Ville");
	check_describe(hstmt, "État");
	check_describe(hstmt, "Ville");
	check_columns(hstmt, FALSE, TRUE, "État");
	check_columns(hstmt, FALSE, FALSE, "ÉTAT");
	check_columns(hstmt, TRUE, TRUE, "État");
	check_columns(hstmt, FALSE, TRUE, "Ville");
	check_columns(hstmt, FALSE, FALSE, "VILLE");
	check_columns(hstmt, TRUE, TRUE, "Ville");

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();
	test_drop_db(FOLD_DB);
	return 0;
}
