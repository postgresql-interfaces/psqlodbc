#include <stdio.h>
#include <stdlib.h>

#include "common.h"

/*
 * This test case tests that things still work when you have a lot of
 * statements open simultaneously.
 *
 * As of writing this, there's an internal limit of just below 2^15 stmt
 * handles. Also, unixodbc's method of checking if a handle is valid, by
 * scanning a linked list of statements, grinds to a halt as you have
 * a lot of statements. If you want to test those limits, increase
 * NUM_STMT_HANDLES value.
 */
#define NUM_STMT_HANDLES 100

#define MAX_QUERY_SIZE 100

int main(int argc, char **argv)
{
	HSTMT hstmt[NUM_STMT_HANDLES];
	SQLRETURN rc;
	int i, nhandles;
	char *sqlbufs[NUM_STMT_HANDLES];

	test_connect();

	/* Allocate a lot of stmt handles */
	for (i = 0; i < NUM_STMT_HANDLES; i++)
	{
		rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt[i]);
		if (!SQL_SUCCEEDED(rc))
		{
			printf("failed to allocate stmt handle %d\n", i + 1);
			print_diag(NULL, SQL_HANDLE_DBC, conn);
			break;
		}
		if ((i + 1) % (NUM_STMT_HANDLES / 10) == 0)
		{
			printf("%d statements allocated...\n", i + 1);
			fflush(stdout);
		}
		/* allocate some space to hold the query */
		sqlbufs[i] = malloc(MAX_QUERY_SIZE);
	}
	nhandles = i;

	/* Execute a query using each of them to verify they all work */
	for (i = 0; i < nhandles; i++)
	{
		sprintf(sqlbufs[i], "SELECT 'stmt no %d'", i + 1);
		rc = SQLExecDirect(hstmt[i], (SQLCHAR *) sqlbufs[i], SQL_NTS);
		if (!SQL_SUCCEEDED(rc))
		{
			print_diag("SQLExecDirect failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}
		if ((i + 1) % (NUM_STMT_HANDLES / 10) == 0)
		{
			printf("%d statements executed...\n", i + 1);
			fflush(stdout);
		}
	}

	for (i = 0; i < nhandles; i += (NUM_STMT_HANDLES / 10))
	{
		print_result(hstmt[i]);
		fflush(stdout);
	}

	for (i = 0; i < nhandles; i++)
	{
		rc = SQLFreeStmt(hstmt[i], SQL_CLOSE);
		CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt[i]);
	}

	/* Try SQLPrepare/SQLExecute on multiple statements. */
	printf("\nTesting interleaving operations on multiple prepared statements\n");
	for (i = 0; i < 5; i++)
	{
		SQLSMALLINT colcount;
		int j;
		char *sqlbuf = sqlbufs[i];

		sprintf(sqlbuf, "SELECT 'stmt no %d'", i);
		for (j = 0; j < i; j++)
		{
			sprintf(&sqlbuf[strlen(sqlbuf)], ", 'col %d'", j);
		}
		strcat(sqlbuf, " FROM generate_series(1, 3)");
		rc = SQLPrepare(hstmt[i], (SQLCHAR *) sqlbuf, SQL_NTS);
		CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt[i]);

		/* Test SQLNumResultCols, called before SQLExecute() */
		rc = SQLNumResultCols(hstmt[i], &colcount);
		CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);
		printf("# of result cols: %d\n", colcount);
	}

	for (i = 0; i < 5; i++)
	{
		rc = SQLExecute(hstmt[i]);
		CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt[i]);
		print_result(hstmt[i]);
	}

	test_disconnect();

	/* free the buffers, to keep Valgrind from complaining about the leaks */
	for (i = 0; i < NUM_STMT_HANDLES; i++)
		free(sqlbufs[i]);

	return 0;
}
