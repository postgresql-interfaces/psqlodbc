/*
 * Test SQLPutData for a data-at-execution parameter that the application
 * described through the descriptor fields of the APD and the IPD, without
 * calling SQLBindParameter for it.
 *
 * SQLBindParameter makes room for a parameter in three arrays of the
 * driver: the APD's, the IPD's and the one that collects the data passed
 * to SQLPutData. Setting descriptor fields makes room in the descriptor
 * that is set only. SQLPutData must make sure all three hold the
 * parameter before it stores the data.
 *
 * In the first case every parameter is described that way, so the array
 * for the data does not exist yet. In the second, SQLBindParameter for
 * parameter 1 has created it with one element, and the data is for
 * parameter 2.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/*
 * Run sql, whose last parameter, number param, is described through
 * descriptor fields and gets "abc" from SQLPutData. A parameter before it
 * is bound to "x" with SQLBindParameter.
 */
static void
put_data_case(const char *sql, SQLSMALLINT param)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDESC	hApd = NULL, hIpd = NULL;
	SQLLEN		ind = SQL_DATA_AT_EXEC;
	SQLLEN		first_ind = SQL_NTS;
	char		first[] = "x";
	SQLPOINTER	token = NULL;
	char		buf[32];
	SQLLEN		buflen;

	printf("%s, data for parameter %d\n", sql, param);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLPrepare(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	if (param > 1)
	{
		rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
							  SQL_VARCHAR, 20, 0, first, 0, &first_ind);
		CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	}
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_PARAM_DESC, &hApd, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, &hIpd, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);

	rc = SQLSetDescField(hIpd, param, SQL_DESC_CONCISE_TYPE,
						 (SQLPOINTER) SQL_VARCHAR, 0);
	CHECK_STMT_RESULT(rc, "IPD SQL_DESC_CONCISE_TYPE failed", hstmt);
	rc = SQLSetDescField(hIpd, param, SQL_DESC_PARAMETER_TYPE,
						 (SQLPOINTER) SQL_PARAM_INPUT, 0);
	CHECK_STMT_RESULT(rc, "IPD SQL_DESC_PARAMETER_TYPE failed", hstmt);
	rc = SQLSetDescField(hApd, param, SQL_DESC_CONCISE_TYPE,
						 (SQLPOINTER) SQL_C_CHAR, 0);
	CHECK_STMT_RESULT(rc, "APD SQL_DESC_CONCISE_TYPE failed", hstmt);
	rc = SQLSetDescField(hApd, param, SQL_DESC_OCTET_LENGTH_PTR, &ind, 0);
	CHECK_STMT_RESULT(rc, "APD SQL_DESC_OCTET_LENGTH_PTR failed", hstmt);
	rc = SQLSetDescField(hApd, param, SQL_DESC_INDICATOR_PTR, &ind, 0);
	CHECK_STMT_RESULT(rc, "APD SQL_DESC_INDICATOR_PTR failed", hstmt);
	/* the value SQLParamData hands back for this parameter */
	rc = SQLSetDescField(hApd, param, SQL_DESC_DATA_PTR, (SQLPOINTER) 1, 0);
	CHECK_STMT_RESULT(rc, "APD SQL_DESC_DATA_PTR failed", hstmt);

	rc = SQLExecute(hstmt);
	printf("SQLExecute: %s\n", SQL_NEED_DATA == rc ? "SQL_NEED_DATA" : "other");
	rc = SQLParamData(hstmt, &token);
	printf("SQLParamData: %s\n", SQL_NEED_DATA == rc ? "SQL_NEED_DATA" : "other");
	/* a crash in SQLPutData must not lose the lines above */
	fflush(stdout);
	rc = SQLPutData(hstmt, "abc", 3);
	if (!SQL_SUCCEEDED(rc))
		print_diag("SQLPutData failed", SQL_HANDLE_STMT, hstmt);
	else
		printf("SQLPutData succeeded\n");
	rc = SQLParamData(hstmt, &token);
	if (!SQL_SUCCEEDED(rc))
		print_diag("the second SQLParamData failed", SQL_HANDLE_STMT, hstmt);
	else
	{
		rc = SQLFetch(hstmt);
		CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &buflen);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("result: %s\n", buf);
	}

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int
main(int argc, char **argv)
{
	test_connect();
	put_data_case("SELECT 'got ' || ?::text", 1);
	put_data_case("SELECT 'got ' || ?::text || ?::text", 2);
	test_disconnect();
	return 0;
}
