/*
 * Read a handle's diagnostics on one thread while another thread keeps
 * making calls on the same handle that clear and set its errors.
 *
 * SQLGetDiagRec and SQLGetDiagField used to take no lock, so they could
 * read an error object that the other thread's call had just freed.  They
 * now take the statement's or the connection's lock.  A race can't be
 * shown deterministically: this is a stress test that checks the calls
 * work and agree while they contend for the handle.  The old code passes
 * it too unless the freed memory is trapped (a sanitizer or page heap).
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

#define ITERATIONS 2000

static HSTMT hstmt = SQL_NULL_HSTMT;
static volatile int done;
static int worker_errors;
static int reader_bad;
static char first_bad[16];

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

/* fail a statement over and over: each call clears and sets its errors */
static THREAD_FUNC
stmt_worker(void *arg)
{
	int		i;

	for (i = 0; i < ITERATIONS; i++)
	{
		if (SQL_ERROR == SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1/0", SQL_NTS))
			worker_errors++;
		SQLFreeStmt(hstmt, SQL_CLOSE);
	}
	done = 1;
	THREAD_RETURN;
}

/* ask for an attribute that doesn't exist: each call sets a connection error */
static THREAD_FUNC
conn_worker(void *arg)
{
	int		i;
	SQLINTEGER	value;

	for (i = 0; i < ITERATIONS; i++)
	{
		if (SQL_ERROR == SQLGetConnectAttr(conn, 99999, &value, sizeof(value), NULL))
			worker_errors++;
	}
	done = 1;
	THREAD_RETURN;
}

/*
 * Does this SQLSTATE belong to the worker?
 *
 * The statement worker's own failure is the division error, but when the
 * driver is set not to roll back on an error (Protocol=7.4-0) and a
 * transaction is open for the cursor, the first failure leaves the
 * transaction aborted and every call after it fails with 25P02 instead.
 * Both are the worker's own SQLSTATE, and how many of each there are
 * depends on where the two threads are. A torn or stale record, which is
 * what this test looks for, matches neither.
 */
static int
worker_sqlstate(SQLSMALLINT htype, const char *sqlstate)
{
	if (SQL_HANDLE_STMT == htype)
		return 0 == strcmp(sqlstate, "22012") ||
			   0 == strcmp(sqlstate, "25P02");
	return 0 == strcmp(sqlstate, "HYC00");
}

/*
 * Read the handle's first diagnostic record until the worker is done.  It
 * may be there or not, depending on where the worker is; when it is there,
 * its SQLSTATE must be the worker's one.
 */
static THREAD_FUNC
reader(void *arg)
{
	SQLSMALLINT	htype = *(SQLSMALLINT *) arg;
	SQLHANDLE	handle = (SQL_HANDLE_STMT == htype) ? (SQLHANDLE) hstmt : (SQLHANDLE) conn;
	SQLCHAR		sqlstate[6];
	SQLCHAR		msg[256];
	SQLINTEGER	native;
	SQLSMALLINT	len;
	SQLRETURN	rc;

	while (!done)
	{
		rc = SQLGetDiagRec(htype, handle, 1, sqlstate, &native, msg, sizeof(msg), &len);
		if (SQL_SUCCEEDED(rc) && !worker_sqlstate(htype, (char *) sqlstate))
		{
			if (0 == reader_bad++)
				snprintf(first_bad, sizeof(first_bad), "rec %d %.5s", rc, (char *) sqlstate);
		}
		rc = SQLGetDiagField(htype, handle, 1, SQL_DIAG_SQLSTATE, sqlstate, sizeof(sqlstate), &len);
		if (SQL_SUCCEEDED(rc) && !worker_sqlstate(htype, (char *) sqlstate))
		{
			if (0 == reader_bad++)
				snprintf(first_bad, sizeof(first_bad), "field %d %.5s", rc, (char *) sqlstate);
		}
	}
	THREAD_RETURN;
}

static void
run(const char *label, SQLSMALLINT htype, thread_func_t worker)
{
	thread_t	tw, tr;

	done = 0;
	worker_errors = 0;
	reader_bad = 0;
	first_bad[0] = '\0';

	/*
	 * Start from the error the worker sets: the connection may still hold
	 * the statement's error (under unixODBC it does), and the reader runs
	 * before the worker's first call replaces it.
	 */
	if (SQL_HANDLE_STMT == htype)
	{
		SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1/0", SQL_NTS);
		SQLFreeStmt(hstmt, SQL_CLOSE);
	}
	else
	{
		SQLINTEGER	value;

		SQLGetConnectAttr(conn, 99999, &value, sizeof(value), NULL);
	}
	start_thread(&tr, reader, &htype);
	start_thread(&tw, worker, NULL);
	join_thread(tw);
	join_thread(tr);
	printf("%s: %d of %d calls failed as expected, %d unexpected diagnostics\n",
		   label, worker_errors, ITERATIONS, reader_bad);
	if (reader_bad)
		printf("first unexpected: %s\n", first_bad);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;

	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	run("statement", SQL_HANDLE_STMT, stmt_worker);
	run("connection", SQL_HANDLE_DBC, conn_worker);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	test_disconnect();

	return 0;
}
