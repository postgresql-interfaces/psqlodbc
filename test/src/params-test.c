#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char param1[20] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	SQLLEN cbParam1;
	long longparam;
	SQL_INTERVAL_STRUCT intervalparam;
	SQLSMALLINT colcount;
	SQLSMALLINT dataType;
	SQLULEN paramSize;
	SQLSMALLINT decDigits;
	SQLSMALLINT nullable;

	test_connect();

	rc = SQLAllocStmt(conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/**** Query with a bytea param ****/

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT id, t FROM byteatab WHERE t = ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* bind param  */
	cbParam1 = 8;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_BINARY,	/* value type */
						  SQL_BINARY,	/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Test SQLNumResultCols, called before SQLExecute() */
	rc = SQLNumResultCols(hstmt, &colcount);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);
	printf("# of result cols: %d\n", colcount);

	/* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* Fetch result */
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** Test SQLBindParameter with SQLExecDirect ***/
	printf("\nTesting SQLBindParameter with SQLExecDirect...\n");

	/* bind param  */
	strcpy(param1, "bar");
	cbParam1 = SQL_NTS;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,		/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 'foo' UNION ALL SELECT ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*** Test SQLDescribeParam ***/
	printf("\nTesting SQLDescribeParam...\n");

	rc = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT id, t FROM testtab1 WHERE id = ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	rc = SQLDescribeParam(hstmt, 1, &dataType, &paramSize, &decDigits, &nullable);
	CHECK_STMT_RESULT(rc, "SQLDescribeParams failed", hstmt);
	printf("Param 1: type %s; size %d; dec digits %d; %s\n",
		   datatype_str(dataType), paramSize, decDigits, nullable_str(nullable));
	/* bind param  */
	strcpy(param1, "3");
	cbParam1 = SQL_NTS;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_CHAR,		/* param type */
						  20,			/* column size */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Test SQLNumResultCols, called before SQLExecute() */
	rc = SQLNumResultCols(hstmt, &colcount);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);
	printf("# of result cols: %d\n", colcount);

	/* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* Fetch result */
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
