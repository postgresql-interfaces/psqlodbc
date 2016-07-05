/*
 * Test data-at-execution, for a large object.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
printhex(unsigned char *b, int len)
{
	int i;

	printf("hex: ");
	for (i = 0; i < len; i++)
		printf("%02X", b[i]);
}

/*
 * Insert a large object to table, using a data-at-exec param. Then read it
 * back and print.
 */
static void
do_test(HSTMT hstmt, int testno, int lobByteSize, char *lobData)
{
	char		sql[200];
	int			rc;
	SQLLEN		cbParam1;
	char		*buf;
	SQLLEN		ind;

	/**** Insert a Large Object */
	printf("inserting large object with len %d...\n", lobByteSize);
	snprintf(sql, sizeof(sql),
			 "INSERT INTO lo_test_tab VALUES (%d, ?)", testno);
	rc = SQLPrepare(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* bind LO param as a data at execution parameter */
	cbParam1 = SQL_DATA_AT_EXEC;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_BINARY,	/* value type */
						  SQL_LONGVARBINARY,	/* param type */
						  0,			/* column size */
						  0,			/* dec digits */
						  lobData,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Execute */
	rc = SQLExecute(hstmt);

	/* Check for data-at-execute parameters */
	if (SQL_NEED_DATA == rc)
	{
		SQLLEN lobChunkSize = 4096;
		SQLPOINTER pParamId = 0;
		const char *what = 0;
		int error;

		/* Get the parameter that needs data */
		what = "SQLParamData failed (first call)";
		rc = SQLParamData(hstmt, &pParamId);
		error = SQL_SUCCESS != rc && SQL_SUCCESS_WITH_INFO != rc && SQL_NEED_DATA != rc;
		if (SQL_NEED_DATA == rc)
		{
			/* Send parameter data in chunks */
			what = "SQLPutData failed (next chunk)";
			while (!error && lobByteSize > lobChunkSize)
			{
				rc = SQLPutData(hstmt, (SQLPOINTER)pParamId, lobChunkSize);
				lobByteSize -= lobChunkSize;
				error = SQL_SUCCESS != rc && SQL_SUCCESS_WITH_INFO != rc && SQL_NEED_DATA != rc;
			}

			/* Send final chunk */
			if (!error)
			{
				what = "SQLPutData failed (final chunk)";
				rc = SQLPutData(hstmt, (SQLPOINTER)pParamId, lobByteSize);
				error = SQL_SUCCESS != rc && SQL_SUCCESS_WITH_INFO != rc && SQL_NEED_DATA != rc;
			}

			/* Make final call */
			if (!error)
			{
				what = "SQLParamData failed (final call)";
				rc = SQLParamData(hstmt, &pParamId);
				error = SQL_SUCCESS != rc && SQL_SUCCESS_WITH_INFO != rc;
			}
		}
		CHECK_STMT_RESULT(rc, (char*)what, hstmt);
	}
	else
	{
		print_diag("SQLExecute didn't return SQL_NEED_DATA as expected",
				   SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/**** Read it back ****/
	printf("reading it back...\n");

	snprintf(sql, sizeof(sql),
			 "SELECT id, large_data FROM lo_test_tab WHERE id = %d", testno);
	SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	buf = malloc(lobByteSize * 2);
	rc = SQLGetData(hstmt, 2, SQL_C_BINARY, buf, lobByteSize * 2, &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);

	printhex(buf, ind);
	printf("\n");

	free(buf);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int
main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		param1[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	char		param2[100];
	int			i;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	do_test(hstmt, 101, sizeof(param1), param1);

	for (i = 0; i < sizeof(param2); i++)
	{
		param2[i] = i % 200;
	}
	do_test(hstmt, 102, sizeof(param2), param2);

	/* Clean up */
	test_disconnect();

	return 0;
}
