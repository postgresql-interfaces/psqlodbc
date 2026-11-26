/*
 * A document id that the end user supplies as bytes with a length, bound as
 * SQL_C_CHAR -> SQL_INTEGER. A NUL byte inside it must not change which rows
 * "... WHERE id = ? AND owner = ?" reads or deletes.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
create_docs(HSTMT hstmt)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE docs (id int4, owner text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while creating temp table", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO docs VALUES (1, 'alice'), (1, 'bob')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while inserting", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

static void
exec_as_owner(HSTMT hstmt, char *sql, char *id, SQLLEN id_len, char *owner)
{
	SQLRETURN	rc;
	SQLLEN		owner_len = SQL_NTS;

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_INTEGER,	/* param type */
						  0,			/* column size */
						  0,			/* dec digits */
						  id,			/* param value ptr */
						  0,			/* buffer len */
						  &id_len		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_VARCHAR,	/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  owner,		/* param value ptr */
						  0,			/* buffer len */
						  &owner_len	/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	/* default settings: for a static cursor the driver inlines the values */
	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	create_docs(hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);

	printf("\nalice selects document \"1\"\n");
	exec_as_owner(hstmt, "SELECT id, owner FROM docs WHERE id = ? AND owner = ?",
				  "1", 1, "alice");
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	printf("\nalice selects document \"1\\0\"\n");
	exec_as_owner(hstmt, "SELECT id, owner FROM docs WHERE id = ? AND owner = ?",
				  "1\0", 2, "alice");
	print_result(hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
	test_disconnect();

	/* without server-side prepare every statement gets its values inlined */
	test_connect_ext("UseServerSidePrepare=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	create_docs(hstmt);

	printf("\nalice deletes document \"1\\0\"\n");
	exec_as_owner(hstmt, "DELETE FROM docs WHERE id = ? AND owner = ?",
				  "1\0", 2, "alice");
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	printf("\ndocuments left\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id, owner FROM docs ORDER BY owner", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
	test_disconnect();

	return 0;
}
