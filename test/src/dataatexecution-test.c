#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char *param1;
	SQLLEN cbParam1, cbParam2;
	SQLLEN param1bytes;
	PTR paramid;
	SQLLEN str_ind_array[2];
	SQLUSMALLINT status_array[2];
	SQLULEN nprocessed = 0;
	int i;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/****
	 * Bind with data-at-execution params. (VARBINARY)
	 */

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT id FROM byteatab WHERE t = ? OR t = ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* prepare the parameter values */
	param1 = "bar";
	param1bytes = strlen(param1);
	cbParam1 = SQL_DATA_AT_EXEC;
	cbParam2 = SQL_DATA_AT_EXEC;

	/* bind them. */
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_BINARY,	/* value type */
						  SQL_VARBINARY, /* param type */
						  param1bytes,	/* column size */
						  0,			/* dec digits */
						  (void *) 1,	/* param value ptr. For a data-at-exec
										 * param, this is a "parameter id" */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT,
						  SQL_C_BINARY,	/* value type */
						  SQL_VARBINARY, /* param type */
						  6,	/* column size */
						  0,			/* dec digits */
						  (void *) 2,	/* param value ptr. For a data-at-exec
										 * param, this is a "parameter id" */
						  0,			/* buffer len */
						  &cbParam2		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Execute */
	rc = SQLExecute(hstmt);
	if (rc != SQL_NEED_DATA)
		CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* set parameters */
	paramid = 0;
	while ((rc = SQLParamData(hstmt, &paramid)) == SQL_NEED_DATA)
	{
	  if (paramid == (void *) 1)
	  {
		  rc = SQLPutData(hstmt, param1, param1bytes);
		  CHECK_STMT_RESULT(rc, "SQLPutData failed", hstmt);
	  }
	  else if (paramid == (void *) 2)
	  {
		  rc = SQLPutData(hstmt, "foo", 3);
		  CHECK_STMT_RESULT(rc, "SQLPutData failed", hstmt);
		  /* append more data */
		  rc = SQLPutData(hstmt, "bar", 3);
		  CHECK_STMT_RESULT(rc, "SQLPutData failed", hstmt);
	  }
	  else
	  {
		  printf("unexpected parameter id returned by SQLParamData: %p\n", paramid);
		  exit(1);
	  }
	}
	CHECK_STMT_RESULT(rc, "SQLParamData failed", hstmt);

	/* Fetch result */
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);


	/****
	 * Array binding with data-at-execution params.
	 */

	/* prepare the parameter values */
	str_ind_array[0] = SQL_DATA_AT_EXEC;
	str_ind_array[1] = SQL_DATA_AT_EXEC;

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT id FROM byteatab WHERE t = ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_STATUS_PTR, status_array, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMS_PROCESSED_PTR, &nprocessed, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) 2, 0);

	/* bind the array. */
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_BINARY,	/* value type */
						  SQL_VARBINARY, /* param type */
						  5,			/* column size */
						  0,			/* dec digits */
						  (void *) 1,	/* param value ptr. For a data-at-exec
										 * param, this is "parameter id" */
						  0,			/* buffer len */
						  str_ind_array	/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Execute */
	rc = SQLExecute(hstmt);
	if (rc != SQL_NEED_DATA)
		CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* set parameters */
	paramid = 0;
	while ((rc = SQLParamData(hstmt, &paramid)) == SQL_NEED_DATA)
	{
		if (nprocessed == 1)
			rc = SQLPutData(hstmt, "foo", strlen("foo"));
		else if (nprocessed == 2)
			rc = SQLPutData(hstmt, "barf", strlen("barf"));
		else
		{
			printf("unexpected # of rows processed after SQL_NEED_DATA: %u\n", (unsigned int) nprocessed);
			exit(1);
		}
		CHECK_STMT_RESULT(rc, "SQLPutData failed", hstmt);
	}
	CHECK_STMT_RESULT(rc, "SQLParamData failed", hstmt);

	/* Fetch results */
	printf("Parameter	Status\n");
	for (i = 0; i < nprocessed; i++)
	{
		switch (status_array[i])
		{
			case SQL_PARAM_SUCCESS:
			case SQL_PARAM_SUCCESS_WITH_INFO:
				break;

			case SQL_PARAM_ERROR:
				printf("%d\tError\n", i);
				break;

			case SQL_PARAM_UNUSED:
				printf("%d\tUnused\n", i);
				break;

			case SQL_PARAM_DIAG_UNAVAILABLE:
				printf("%d\tDiag unavailable\n", i);
				break;
		}
	}

	printf ("Fetching result sets for array bound (%u results expected)\n",
			(unsigned int) nprocessed);
	for (i = 1; rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO; i++)
	{
		printf("%d: ", i);
		print_result(hstmt);

		rc = SQLMoreResults(hstmt);
	}
	if (rc != SQL_NO_DATA)
		CHECK_STMT_RESULT(rc, "SQLMoreResults failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
