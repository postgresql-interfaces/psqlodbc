#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char *param1;
	SQLLEN cbParam1;
	SQLINTEGER longparam;
	SQL_INTERVAL_STRUCT intervalparam;
	SQLSMALLINT colcount;
	char		byteaParam[5000];
	int			i;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/**** A simple query with one text param ****/

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SET intervalstyle=postgres_verbose", SQL_NTS);
	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT id, t::varchar(25) FROM testtab1 WHERE t = ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* bind param  */
	param1 = "bar";
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

	/**** A query with an integer param ****/

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT id, t FROM testtab1 WHERE id = ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* bind param  */
	longparam = 3;
	cbParam1 = sizeof(longparam);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_SLONG,	/* value type */
						  SQL_INTEGER,	/* param type */
						  0,			/* column size (ignored for SQL_INTEGER) */
						  0,			/* dec digits */
						  &longparam,	/* param value ptr */
						  sizeof(longparam), /* buffer len (ignored for SQL_INTEGER) */
						  &cbParam1		/* StrLen_or_IndPtr (ignored for SQL_INTEGER) */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* Fetch result */
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/**** Test a query with a bytea param of various sizes.  ****/

	/*
	 * The driver has some special handling for byteas, as it sends them in
	 * binary mode. This particular test case exercises an old bug where
	 * the bind packet size was calculated incorrectly, and there was an
	 * out-of-bounds write of two bytes when the total packet size was exactly
	 * 4097 bytes. So, exercise packet sizes near that boundary.
	 */

	rc = SQLExecDirect(hstmt,
					   (SQLCHAR *) "CREATE TEMPORARY TABLE btest (len int4, b bytea)",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "INSERT INTO btest VALUES(?, ?)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* fill in test data */
	for (i = 0; i < sizeof(byteaParam); i++)
		byteaParam[i] = (char) i;

	printf ("inserting bytea values...");
	for (i = 4000; i < 4100; i++)
	{
		printf(" %d", i); fflush(stdout);
		/* bind int param  */
		longparam = i;
		cbParam1 = sizeof(longparam);
		rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
							  SQL_C_SLONG,	/* value type */
							  SQL_INTEGER,	/* param type */
							  0,			/* column size (ignored for SQL_INTEGER) */
							  0,			/* dec digits */
							  &longparam,	/* param value ptr */
							  sizeof(longparam), /* buffer len (ignored for SQL_INTEGER) */
							  &cbParam1		/* StrLen_or_IndPtr (ignored for SQL_INTEGER) */);
		CHECK_STMT_RESULT(rc, "\nSQLBindParameter failed", hstmt);

		cbParam1 = i;
		rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT,
							  SQL_C_BINARY,	/* value type */
							  SQL_VARBINARY,	/* param type */
							  sizeof(byteaParam), /* column size */
							  0,			/* dec digits */
							  byteaParam,	/* param value ptr */
							  sizeof(byteaParam), /* buffer len */
							  &cbParam1		/* StrLen_or_IndPtr */);
		CHECK_STMT_RESULT(rc, "\nSQLBindParameter failed", hstmt);

		/* Execute */
		rc = SQLExecute(hstmt);
		CHECK_STMT_RESULT(rc, "\nSQLExecute failed", hstmt);
	}
	printf(" done!\n");
	printf("Now reading them back...\n");

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check the inserted data */
	rc = SQLExecDirect(hstmt,
					   (SQLCHAR *) "SELECT len, length(b) FROM btest",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/**** A query with an interval param (SQL_C_INTERVAL_SECOND) ****/

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT id, iv, d FROM intervaltable WHERE iv < ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* bind param  */
	intervalparam.interval_type = SQL_IS_SECOND;
	intervalparam.interval_sign = 0;
	intervalparam.intval.day_second.day = 1;
	intervalparam.intval.day_second.hour = 2;
	intervalparam.intval.day_second.minute = 3;
	intervalparam.intval.day_second.second = 4;
	intervalparam.intval.day_second.fraction = 5;

	cbParam1 = sizeof(intervalparam);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_INTERVAL_SECOND,	/* value type */
						  SQL_INTERVAL_SECOND,	/* param type */
						  0,			/* column size (ignored for SQL_INTERVAL_SECOND) */
						  0,			/* dec digits */
						  &intervalparam, /* param value ptr */
						  sizeof(intervalparam), /* buffer len (ignored for SQL_C_INTERVAL_SECOND) */
						  &cbParam1 /* StrLen_or_IndPtr (ignored for SQL_C_INTERVAL_SECOND) */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* Fetch result */
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);


	/****
	 * With BoolsAsChar=1, a varchar param with column_size=5 forces a
	 * server-side Prepare. So test that.
	 */

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT id, t::varchar(25) FROM testtab1 WHERE id = ?", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* bind param  */
	param1 = "2";
	cbParam1 = SQL_NTS;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_CHAR,	/* value type */
						  SQL_VARCHAR,	/* param type */
						  5,			/* column size. 5 Triggers special
										 * behavior with BoolsAsChar=1 */
						  0,			/* dec digits */
						  param1,		/* param value ptr */
						  0,			/* buffer len */
						  &cbParam1		/* StrLen_or_IndPtr */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Test SQLNumResultCols/SQLDescribeCol, called before SQLExecute() */
	rc = SQLNumResultCols(hstmt, &colcount);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);
	printf("# of result cols: %d\n", colcount);
	for (i = 1; i <= colcount; i++)
	{
		SQLCHAR		colname[64];
		SQLSMALLINT	collen, type, scale;
		SQLULEN		len;

		rc = SQLDescribeCol(hstmt, i, colname, sizeof(colname), &collen, &type, &len, &scale, NULL);
		CHECK_STMT_RESULT(rc, "SQLDescribeCol failed", hstmt);
		printf("col:%d name=%s type=%d len=%d\n", i, colname, type, (int) len);
	}

	/* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* Fetch result */
	print_result(hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeStmt failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}

	/* Clean up */
	test_disconnect();

	return 0;
}
