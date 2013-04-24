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

int main(int argc, char **argv)
{
	HSTMT hstmt[NUM_STMT_HANDLES];
	SQLRETURN rc;
	int i, nhandles;

	test_connect();

	/* Allocate a lot of stmt handles */
	for (i = 0; i < NUM_STMT_HANDLES; i++)
	{
		rc = SQLAllocStmt(conn, &hstmt[i]);
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
	}
	nhandles = i;

	/* Execute a query using each of them to verify they all work */
	for (i = 0; i < nhandles; i++)
	{
		char sqlbuf[100];
		snprintf(sqlbuf, sizeof(sqlbuf), "SELECT 'stmt no %d'", i + 1);
		rc = SQLExecDirect(hstmt[i], (SQLCHAR *) sqlbuf, SQL_NTS);
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

	test_disconnect();
}
