#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define ARRAY_SIZE 10000
#define ARRAY_SIZE_SMALL 5

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char *sql;

	int i;

	SQLUINTEGER int_array[ARRAY_SIZE];
	SQLCHAR str_array[ARRAY_SIZE][30];
	SQLCHAR str_array2[ARRAY_SIZE][6];
	SQLLEN int_ind_array[ARRAY_SIZE];
	SQLLEN str_ind_array[ARRAY_SIZE];
	SQLUSMALLINT status_array[ARRAY_SIZE];
	SQLULEN nprocessed;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	sql = "CREATE TEMPORARY TABLE tmptable (i int4, t text)";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while creating temp table", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/****
	 * 1. Test column-wise binding
	 */
	for (i = 0; i < ARRAY_SIZE; i++)
	{
		int_array[i] = i;
		int_ind_array[i] = 0;
		sprintf(str_array[i], "columnwise %d", i);
		str_ind_array[i] = SQL_NTS;
	}

	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_STATUS_PTR, status_array, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMS_PROCESSED_PTR, &nprocessed, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) ARRAY_SIZE, 0);

	/* Bind the parameter arrays. */
	SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 5, 0,
					 int_array, 0, int_ind_array);
	SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 29, 0,
					 str_array, 30, str_ind_array);

	/* Execute */
	sql = "INSERT INTO tmptable VALUES (?, ?)";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

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

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Free and allocate a new handle for the next SELECT statement, as we don't
	 * want to array bind that one. The parameters set with SQLSetStmtAttr
	 * survive SQLFreeStmt.
	 */
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Check that all the rows were inserted */
	sql = "SELECT COUNT(*) FROM tmptable";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check the contents of a few rows */
	sql = "SELECT * FROM tmptable WHERE i IN (0, 1, 100, 9999, 10000) ORDER BY i";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/****
	 * 2. Test column-wise binding. With a column_size=5 VARCHAR param - that
	 * causes the driver to do a server-side prepare, assuming BoolsAsChar=1.
	 */

	/* a small array will do for this test */
	for (i = 0; i < ARRAY_SIZE_SMALL; i++)
	{
		sprintf(str_array2[i], "%d", 100+i);
		str_ind_array[i] = SQL_NTS;
	}

	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_STATUS_PTR, status_array, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMS_PROCESSED_PTR, &nprocessed, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) ARRAY_SIZE_SMALL, 0);

	/* Bind the parameter array. */
	SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 5, 0,
					 str_array2, 6, str_ind_array);

	/* Execute */
	sql = "DELETE FROM tmptable WHERE i = ? RETURNING (t)";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

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

	/*
	 * Free and allocate a new handle for the next SELECT statement, as we don't
	 * want to array bind that one. The parameters set with SQLSetStmtAttr
	 * survive SQLFreeStmt.
	 */
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Check that all the rows were inserted */
	printf("Number of rows in table:\n");
	sql = "SELECT COUNT(*) FROM tmptable";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check the contents of a few rows */
	sql = "SELECT * FROM tmptable WHERE i IN (0, 1, 100, 9999, 10000) ORDER BY i";
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
