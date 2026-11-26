#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/*
 * Tests SQLSetPos(SQL_UPDATE) with a data-at-execution column whose value
 * is sent with SQLPutData in pieces, including a final piece of length 0,
 * and that the statement stays usable after a failed SQLPutData.
 *
 * The application streams a long value in pieces of PIECE bytes, the way it
 * would copy it from a file or a socket: a short piece, possibly empty, ends
 * the value.
 */
#define PIECE 8

static HSTMT hstmt = SQL_NULL_HSTMT;
static char note[64];
static SQLLEN note_ind;

/*
 * Replace the note of the current row with SQLSetPos(SQL_UPDATE), sending it
 * at execution time.  A note that cannot be saved is reported, and the
 * application goes on with the next one.
 */
static void
update_note(const char *text)
{
	SQLRETURN	rc;
	PTR			paramid;
	const char *rest = text;
	size_t		piece;

	note_ind = SQL_DATA_AT_EXEC;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	if (rc == SQL_NEED_DATA)
		rc = SQLParamData(hstmt, &paramid);
	if (rc == SQL_NEED_DATA)
	{
		do
		{
			piece = strlen(rest) < PIECE ? strlen(rest) : PIECE;
			rc = SQLPutData(hstmt, (PTR) rest, piece);
			rest += piece;
		} while (SQL_SUCCEEDED(rc) && piece == PIECE);
	}
	if (SQL_SUCCEEDED(rc))
		rc = SQLParamData(hstmt, &paramid);

	if (SQL_SUCCEEDED(rc))
		printf("saved \"%s\"\n", text);
	else
	{
		printf("could not save \"%s\"\n", text);
		print_diag(NULL, SQL_HANDLE_STMT, hstmt);
	}
}

/*
 * Start replacing the note like update_note(), but abandon it with SQLCancel
 * after the first piece, as an upload that breaks off would.  The statement
 * must be usable for the next update.  The Microsoft driver manager no
 * longer considers the cursor positioned after the cancel, so the row is
 * fetched again before the next SQLSetPos.
 */
static void
abandon_note(const char *text)
{
	SQLRETURN	rc;
	PTR			paramid;

	note_ind = SQL_DATA_AT_EXEC;
	rc = SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE);
	if (rc == SQL_NEED_DATA)
		rc = SQLParamData(hstmt, &paramid);
	if (rc == SQL_NEED_DATA)
		rc = SQLPutData(hstmt, (PTR) text, PIECE);
	if (SQL_SUCCEEDED(rc))
		rc = SQLCancel(hstmt);
	if (SQL_SUCCEEDED(rc))
		rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);

	if (SQL_SUCCEEDED(rc))
		printf("abandoned \"%s\"\n", text);
	else
	{
		printf("could not abandon \"%s\"\n", text);
		print_diag(NULL, SQL_HANDLE_STMT, hstmt);
	}
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;

	/*
	 * Keep what was printed before a crash.  Unbuffered rather than line
	 * buffered: the MSVC runtime has no line buffering and rejects _IOLBF
	 * with a size of 0 as an invalid parameter.
	 */
	setvbuf(stdout, NULL, _IONBF, 0);

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE notes(id int4 primary key, note text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO notes VALUES (1, 'first note'), (2, 'second note')", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT note FROM notes ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, note, sizeof(note), &note_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLFetchScroll(hstmt, SQL_FETCH_FIRST, 0);
	CHECK_STMT_RESULT(rc, "SQLFetchScroll failed", hstmt);
	printf("editing row 1: %s\n", note);

	/* 8 + 7 bytes */
	update_note("fifteen letters");
	update_note("short");
	/* 8 + 8 + 0 bytes */
	update_note("sixteen letters!");
	update_note("short");
	/* SQLCancel after the first 8 bytes */
	abandon_note("interrupted note");
	update_note("after cancel");

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT id, note FROM notes ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	test_disconnect();

	return 0;
}
