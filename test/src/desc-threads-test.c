/*
 * Change a statement's implicit descriptors on one thread while another
 * thread is in calls on that statement.
 *
 * The descriptor functions used to take no lock, so SQLSetDescField on a
 * statement's ARD could free or reallocate the column bindings that a
 * SQLFetch on another thread was walking.  For an implicit descriptor
 * they now take the owning statement's lock; SQLCopyDesc between two
 * statements' descriptors takes both, in a fixed order.
 *
 * A race can't be shown deterministically: this is a stress test that
 * checks the calls work, the fetches return every row, and two SQLCopyDesc
 * calls in opposite directions don't deadlock.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#ifdef WIN32
typedef HANDLE thread_t;
#define THREAD_FUNC DWORD WINAPI
typedef DWORD (WINAPI *thread_func_t)(void *);
#define THREAD_RETURN return 0
#else
#include <pthread.h>
typedef pthread_t thread_t;
#define THREAD_FUNC void *
typedef void *(*thread_func_t)(void *);
#define THREAD_RETURN return NULL
#endif

#define QUERIES 200
#define ROWS 100
#define COPIES 5000

static HSTMT hstmt1 = SQL_NULL_HSTMT;
static HSTMT hstmt2 = SQL_NULL_HSTMT;
static HSTMT hstmt3 = SQL_NULL_HSTMT;
static SQLHDESC ard1, ard2, ard3;
static volatile int done;
static int rows_fetched;
static int binder_errors;
static int copy_errors;
static int fetch_failed;

static SQLINTEGER fetch_buf, binder_buf;
static SQLLEN fetch_ind, binder_ind;

static void
start_thread(thread_t *t, thread_func_t func, void *arg)
{
#ifdef WIN32
	*t = CreateThread(NULL, 0, func, arg, 0, NULL);
	if (NULL == *t)
#else
	if (0 != pthread_create(t, NULL, func, arg))
#endif
	{
		printf("could not start a thread\n");
		exit(1);
	}
}

static void
join_thread(thread_t t)
{
#ifdef WIN32
	WaitForSingleObject(t, INFINITE);
	CloseHandle(t);
#else
	pthread_join(t, NULL);
#endif
}

/* run a query over and over and fetch every row through the ARD */
static THREAD_FUNC
fetcher(void *arg)
{
	int		i;
	SQLRETURN	rc;

	for (i = 0; i < QUERIES; i++)
	{
		rc = SQLExecDirect(hstmt1, (SQLCHAR *) "SELECT g FROM generate_series(1, 100) g", SQL_NTS);
		if (!SQL_SUCCEEDED(rc))
		{
			print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt1);
			break;
		}
		while (SQL_SUCCEEDED(rc = SQLFetch(hstmt1)))
			rows_fetched++;
		if (SQL_NO_DATA != rc && !fetch_failed++)
			print_diag("SQLFetch failed", SQL_HANDLE_STMT, hstmt1);
		SQLFreeStmt(hstmt1, SQL_CLOSE);
	}
	done = 1;
	THREAD_RETURN;
}

/*
 * Grow the ARD's binding array and shrink it again, which reallocates it,
 * and bind its column to another buffer.  (A count of 0 would free the
 * array, and the driver then refuses to fetch.)
 */
static THREAD_FUNC
binder(void *arg)
{
	SQLRETURN	rc;
	SQLINTEGER	count;

	while (!done)
	{
		rc = SQLSetDescField(ard1, 0, SQL_DESC_COUNT, (SQLPOINTER) 3, 0);
		if (!SQL_SUCCEEDED(rc))
			binder_errors++;
		rc = SQLSetDescField(ard1, 0, SQL_DESC_COUNT, (SQLPOINTER) 1, 0);
		if (!SQL_SUCCEEDED(rc))
			binder_errors++;
		rc = SQLSetDescRec(ard1, 1, SQL_C_LONG, 0, sizeof(binder_buf), 0, 0,
						   &binder_buf, &binder_ind, &binder_ind);
		if (!SQL_SUCCEEDED(rc))
			binder_errors++;
		rc = SQLGetDescField(ard1, 0, SQL_DESC_COUNT, &count, sizeof(count), NULL);
		if (!SQL_SUCCEEDED(rc))
			binder_errors++;
	}
	THREAD_RETURN;
}

/* copy one statement's ARD to the other's, COPIES times */
static THREAD_FUNC
copier(void *arg)
{
	int		forward = *(int *) arg;
	int		i;

	for (i = 0; i < COPIES; i++)
	{
		if (!SQL_SUCCEEDED(forward ? SQLCopyDesc(ard2, ard3) : SQLCopyDesc(ard3, ard2)))
			copy_errors++;
	}
	THREAD_RETURN;
}

static SQLHDESC
get_ard(HSTMT hstmt)
{
	SQLHDESC	ard;
	SQLRETURN	rc;

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	return ard;
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	thread_t	t1, t2;
	int			forward = 1, backward = 0;

	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt1);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt2);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt3);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	ard1 = get_ard(hstmt1);
	ard2 = get_ard(hstmt2);
	ard3 = get_ard(hstmt3);

	/* fetch on one thread, rebind the same statement's ARD on another */
	rc = SQLBindCol(hstmt1, 1, SQL_C_LONG, &fetch_buf, sizeof(fetch_buf), &fetch_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt1);
	start_thread(&t2, binder, NULL);
	start_thread(&t1, fetcher, NULL);
	join_thread(t1);
	join_thread(t2);
	printf("fetch while rebinding: %d of %d rows, %d descriptor errors\n",
		   rows_fetched, QUERIES * ROWS, binder_errors);

	/* copy between two statements' ARDs in both directions at once */
	rc = SQLBindCol(hstmt2, 1, SQL_C_LONG, &fetch_buf, sizeof(fetch_buf), &fetch_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt2);
	rc = SQLBindCol(hstmt3, 1, SQL_C_LONG, &binder_buf, sizeof(binder_buf), &binder_ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt3);
	start_thread(&t1, copier, &forward);
	start_thread(&t2, copier, &backward);
	join_thread(t1);
	join_thread(t2);
	printf("copies in both directions: %d errors\n", copy_errors);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt3);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt1);
	test_disconnect();

	return 0;
}
