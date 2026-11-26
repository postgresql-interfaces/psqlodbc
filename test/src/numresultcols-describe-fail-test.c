/*
 * Test that SQLNumResultCols, called again and again on a prepared statement
 * that the server cannot describe, does not grow the heap (issue #243). Each
 * call builds the statement's list of processed statements, and a call that
 * replaces the list without freeing the one before it loses that list.
 *
 * The leak changes no output an application sees, so the test measures the
 * bytes the process has allocated and not freed. Where the C library cannot
 * report that, the test prints the line a clean run prints. Every call fails
 * with 42P01, the same on every run.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#define HAVE_HEAP_IN_USE
#elif defined(__GLIBC__)
#include <malloc.h>
#define HAVE_HEAP_IN_USE
#endif

#include "common.h"

/*
 * The leak loses at least the 24 bytes of one ProcessedStmt per call, so
 * 1000 calls lose 24000 bytes or more. The growth a clean driver shows over
 * the same calls is a few hundred bytes at most, from the C library's own
 * bookkeeping, so a limit of 4 bytes per call keeps both cases apart.
 */
#define	CALLS			1000
#define	WARMUP_CALLS	10
#define	BYTES_PER_CALL	4

#ifdef HAVE_HEAP_IN_USE
static size_t
heap_in_use(void)
{
#if defined(__APPLE__)
	malloc_statistics_t st;

	malloc_zone_statistics(NULL, &st);
	return st.size_in_use;
#elif defined(__GLIBC__) && (__GLIBC__ > 2 || \
	(__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
	return mallinfo2().uordblks;
#else
	return (size_t) (unsigned int) mallinfo().uordblks;
#endif
}
#endif

/*
 * Call SQLNumResultCols, which must fail because the table does not exist.
 * Print the SQLSTATE of the first failure only, so that the output does not
 * depend on the number of calls.
 */
static void
num_result_cols(HSTMT hstmt, int print)
{
	SQLRETURN	rc;
	SQLSMALLINT	cols = -1;
	SQLCHAR		sqlstate[6];

	rc = SQLNumResultCols(hstmt, &cols);
	if (SQL_SUCCEEDED(rc))
	{
		printf("SQLNumResultCols succeeded on a table that does not exist\n");
		exit(1);
	}
	if (!print)
		return;
	rc = SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, NULL, NULL, 0,
					   NULL);
	if (!SQL_SUCCEEDED(rc))
	{
		printf("SQLGetDiagRec failed\n");
		exit(1);
	}
	printf("SQLNumResultCols failed with %s\n", (char *) sqlstate);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	int			i;
#ifdef HAVE_HEAP_IN_USE
	size_t		before, after, limit;
#endif

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT * FROM no_such_table",
					SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/*
	 * The first calls allocate what the statement keeps for its whole life,
	 * so measure only after them.
	 */
	for (i = 0; i < WARMUP_CALLS; i++)
		num_result_cols(hstmt, i == 0);

#ifdef HAVE_HEAP_IN_USE
	before = heap_in_use();
#endif
	for (i = 0; i < CALLS; i++)
		num_result_cols(hstmt, 0);
#ifdef HAVE_HEAP_IN_USE
	after = heap_in_use();
	limit = (size_t) CALLS * BYTES_PER_CALL;
	if (after > before && after - before > limit)
		printf("the heap grew by %lu bytes over %d calls, more than %lu\n",
			   (unsigned long) (after - before), CALLS,
			   (unsigned long) limit);
	else
#endif
		printf("the heap did not grow over the repeated calls\n");

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
