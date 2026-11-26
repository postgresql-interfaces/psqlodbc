/*
 * With UseDeclareFetch=1 the driver reads rows from a server cursor in
 * blocks of Fetch rows. A failed query on another statement of the same
 * connection rolls the transaction back, which closes that cursor.
 *
 * If the forward-only cursor had already reached its end, every row left
 * to fetch is in the driver's cache, so fetching must go on to the last
 * row and end with SQL_NO_DATA. If it had not, the rows past the cache are
 * gone and the next fetch must fail with 07005.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define TOTAL_ROWS 150

static void
print_state(const char *what, SQLHANDLE hstmt)
{
	SQLCHAR		state[6] = "";
	SQLINTEGER	native;
	SQLCHAR		msg[256];
	SQLSMALLINT	len;

	SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, state, &native,
				  msg, sizeof(msg), &len);
	printf("%s: SQLSTATE=%s\n", what, (char *) state);
}

/*
 * Fetch rows_before_error rows, make a query on a second statement fail,
 * then fetch until the first statement stops returning rows.
 */
static void
run_case(int rows_before_error)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	HSTMT		hstmt2 = SQL_NULL_HSTMT;
	SQLINTEGER	value;
	SQLLEN		ind;
	int			rows = 0;
	int			out_of_order = 0;

	printf("fetch %d rows before the error\n", rows_before_error);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt2);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"SELECT g FROM generate_series(1, 150) g", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &value, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	while (rows < rows_before_error)
	{
		rc = SQLFetch(hstmt);
		CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
		rows++;
		if (value != rows)
			out_of_order = 1;
	}

	rc = SQLExecDirect(hstmt2, (SQLCHAR *) "SELECT 1/0", SQL_NTS);
	if (SQL_SUCCEEDED(rc))
	{
		printf("the failing query succeeded\n");
		exit(1);
	}
	print_state("failing query", hstmt2);

	while (SQL_SUCCEEDED(rc = SQLFetch(hstmt)))
	{
		rows++;
		if (value != rows)
			out_of_order = 1;
	}
	printf("fetched %d of %d rows%s\n", rows, TOTAL_ROWS,
		   out_of_order ? ", out of order" : "");
	if (rc == SQL_NO_DATA)
		printf("then SQL_NO_DATA\n");
	else
		print_state("then the fetch failed", hstmt);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int
main(int argc, char **argv)
{
	/*
	 * Protocol=7.4-1 makes the driver roll back the whole transaction on
	 * an error. With 7.4-2 it rolls back only to a savepoint, so the
	 * cursor stays open, and with 7.4-0 it leaves the transaction aborted.
	 */
	test_connect_ext("UseDeclareFetch=1;Fetch=100;Protocol=7.4-1");

	/* Row 101 is in the second block, which reaches the end. */
	run_case(101);
	/* Only the first block has been read. */
	run_case(5);

	test_disconnect();
	return 0;
}
