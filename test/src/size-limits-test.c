/*
 * Test sizes and counts that the driver must range-check:
 *
 * - a row array size below 1 is raised to 1 (SQLSTATE 01S02), through
 *   SQL_ATTR_ROW_ARRAY_SIZE and through the ARD's SQL_DESC_ARRAY_SIZE
 * - an APD SQL_DESC_COUNT above 32767 is ignored instead of becoming
 *   negative
 * - an SQL_C_NUMERIC parameter with a negative scale is sent with trailing
 *   zeros
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
show_row_array_size(HSTMT hstmt, const char *label, SQLRETURN rc)
{
	SQLULEN		size = 0;

	SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, 0, NULL);
	printf("%s: rc=%s, row array size %lu\n", label,
		   SQL_SUCCESS == rc ? "SQL_SUCCESS" :
		   SQL_SUCCESS_WITH_INFO == rc ? "SQL_SUCCESS_WITH_INFO" : "error",
		   (unsigned long) size);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDESC	ard, apd;
	SQL_NUMERIC_STRUCT num;
	SQLLEN		numInd = sizeof(num);
	SQLINTEGER	one = 1;
	SQLLEN		oneInd = 0;
	char		buf[64];

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* row array size */
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) 0, 0);
	show_row_array_size(hstmt, "SQL_ATTR_ROW_ARRAY_SIZE 0", rc);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) (SQLLEN) -1, 0);
	show_row_array_size(hstmt, "SQL_ATTR_ROW_ARRAY_SIZE -1", rc);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) 2, 0);
	show_row_array_size(hstmt, "SQL_ATTR_ROW_ARRAY_SIZE 2", rc);

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC) failed", hstmt);
	rc = SQLSetDescField(ard, 0, SQL_DESC_ARRAY_SIZE, (SQLPOINTER) 0, 0);
	show_row_array_size(hstmt, "ARD SQL_DESC_ARRAY_SIZE 0", rc);
	SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) 1, 0);

	/* APD count above 32767, then bind a parameter and use it */
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_PARAM_DESC, &apd, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC) failed", hstmt);
	SQLSetDescField(apd, 0, SQL_DESC_COUNT, (SQLPOINTER) 40000, 0);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
						  0, 0, &one, 0, &oneInd);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT ?::int + 1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SELECT with a parameter failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), NULL);
	printf("APD SQL_DESC_COUNT 40000, then a parameter: %s\n", buf);
	SQLFreeStmt(hstmt, SQL_CLOSE);
	SQLFreeStmt(hstmt, SQL_RESET_PARAMS);

	/* 123 with scale -2 is 12300 */
	memset(&num, 0, sizeof(num));
	num.precision = 5;
	num.scale = -2;
	num.sign = 1;
	num.val[0] = 123;
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_NUMERIC, SQL_NUMERIC,
						  5, -2, &num, 0, &numInd);
	CHECK_STMT_RESULT(rc, "SQLBindParameter(SQL_C_NUMERIC) failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT ?::numeric", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SELECT with a numeric parameter failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), NULL);
	printf("SQL_C_NUMERIC 123 with scale -2: %s\n", buf);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
