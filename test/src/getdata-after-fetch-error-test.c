/*
 * Test SQLGetData() after a block fetch that failed on a row other than
 * the first of its rowset.
 *
 * The driver converts into the application's buffers at an offset of the
 * row being fetched. That offset must be reset when a fetch fails, so
 * that a subsequent SQLGetData() fills the buffer it was given and not
 * the memory after it. The buffers passed to SQLGetData() here are the
 * first elements of arrays, so that a stray write would show up in the
 * elements after them.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define ROWS_PER_FETCH 3
#define NOTE_SIZE 64

static void
run_test(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	id[ROWS_PER_FETCH], qty[ROWS_PER_FETCH];
	SQLLEN		id_ind[ROWS_PER_FETCH], qty_ind[ROWS_PER_FETCH];
	SQLULEN		rows_fetched, i;
	char		notes[ROWS_PER_FETCH][NOTE_SIZE] = {{0}};
	SQLLEN		note_len[ROWS_PER_FETCH] = {0};
	SQLBIGINT	qty_get[ROWS_PER_FETCH] = {0};
	SQLLEN		qty_get_len[ROWS_PER_FETCH] = {0};

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/*
	 * Six rows, three per fetch. The fifth row, the second of the second
	 * rowset, does not fit in SQL_C_SLONG, so fetching it fails.
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TEMPORARY TABLE getdata_fetch_error_tab AS "
		"SELECT g AS id, "
		"CASE g WHEN 5 THEN 99999999999 ELSE g * 10 END::bigint AS qty, "
		"'note ' || g AS note FROM generate_series(1, 6) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) ROWS_PER_FETCH, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROW_ARRAY_SIZE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR, &rows_fetched, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr ROWS_FETCHED_PTR failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, id, sizeof(id[0]), id_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol id failed", hstmt);
	rc = SQLBindCol(hstmt, 2, SQL_C_SLONG, qty, sizeof(qty[0]), qty_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol qty failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT id, qty, note FROM getdata_fetch_error_tab ORDER BY id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	while (SQL_SUCCEEDED(rc = SQLFetch(hstmt)))
	{
		for (i = 0; i < rows_fetched; i++)
			printf("id %d: qty %d\n", (int) id[i], (int) qty[i]);
	}
	if (rc == SQL_NO_DATA)
	{
		printf("fetch did not fail\n");
		exit(1);
	}
	print_diag("fetch failed", SQL_HANDLE_STMT, hstmt);

	/* Character target: the value must be stored in notes[0] only */
	rc = SQLGetData(hstmt, 3, SQL_C_CHAR, notes[0], sizeof(notes[0]), &note_len[0]);
	CHECK_STMT_RESULT(rc, "SQLGetData note failed", hstmt);
	for (i = 0; i < ROWS_PER_FETCH; i++)
		printf("notes[%d] = \"%s\", note_len[%d] = %d\n",
			   (int) i, notes[i], (int) i, (int) note_len[i]);

	/* Fixed-length target: the value must land in qty_get[0] */
	rc = SQLGetData(hstmt, 2, SQL_C_SBIGINT, &qty_get[0], 0, &qty_get_len[0]);
	CHECK_STMT_RESULT(rc, "SQLGetData qty failed", hstmt);
	for (i = 0; i < ROWS_PER_FETCH; i++)
		printf("qty_get[%d] = %lld, qty_get_len[%d] = %d\n",
			   (int) i, (long long) qty_get[i], (int) i, (int) qty_get_len[i]);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int main(int argc, char **argv)
{
	/*
	 * With UseDeclareFetch=1 and a small Fetch, the failed fetch rolls back
	 * the cursor's transaction and SQLGetData then fails with 07005.
	 */
	test_connect_ext("UseDeclareFetch=0");
	run_test();
	test_disconnect();

	return 0;
}
