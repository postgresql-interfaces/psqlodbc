/*
 * Test the limits on parameter numbers and descriptor counts.  The driver
 * keeps parameter binding counts in SQLSMALLINT, so SQLBindParameter with a
 * parameter number above 32767, or SQL_DESC_COUNT set negative, used to
 * wrap the count negative and make the next binding write zeros before the
 * start of the bindings array.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define MAX_PARAMS	32767

static void
print_state(const char *what, SQLRETURN rc, SQLSMALLINT htype, SQLHANDLE h)
{
	SQLCHAR		state[6] = "";
	SQLINTEGER	native;
	SQLCHAR		msg[256];
	SQLSMALLINT	len;

	printf("%s: ", what);
	if (SQL_SUCCEEDED(rc))
	{
		printf("ok\n");
		return;
	}
	SQLGetDiagRec(htype, h, 1, state, &native, msg, sizeof(msg), &len);
	printf("error SQLSTATE=%s\n", (char *) state);
}

/* SELECT 1 WHERE 1 IN (?, ?, ..., ?) with nparams markers, all bound to 1 */
static void
run_with_params(int nparams)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	one = 1;
	SQLINTEGER	result = 0;
	char	   *sql, *p;
	char		what[64];
	int			i;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	sql = malloc(32 + 2 * nparams);
	p = sql + sprintf(sql, "SELECT 1 WHERE 1 IN (");
	for (i = 0; i < nparams; i++)
	{
		if (i > 0)
			*p++ = ',';
		*p++ = '?';
	}
	strcpy(p, ")");

	for (i = 1; i <= nparams; i++)
	{
		rc = SQLBindParameter(hstmt, (SQLUSMALLINT) i, SQL_PARAM_INPUT,
							  SQL_C_SLONG, SQL_INTEGER, 0, 0, &one, 0, NULL);
		if (!SQL_SUCCEEDED(rc))
			break;
	}
	snprintf(what, sizeof(what), "%d params, bind %d", nparams, i <= nparams ? i : nparams);
	print_state(what, rc, SQL_HANDLE_STMT, hstmt);

	if (SQL_SUCCEEDED(rc))
	{
		rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
		snprintf(what, sizeof(what), "%d params, execute", nparams);
		print_state(what, rc, SQL_HANDLE_STMT, hstmt);
	}
	if (SQL_SUCCEEDED(rc))
	{
		rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &result, 0, NULL);
		CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
		rc = SQLFetch(hstmt);
		CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
		printf("%d params, result %d\n", nparams, (int) result);
	}

	free(sql);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

/* Set SQL_DESC_COUNT of the statement's APD or IPD to -1, then bind */
static void
negative_desc_count(SQLINTEGER desc_attr, const char *desc_name)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDESC	hdesc;
	SQLINTEGER	one = 1;
	char		what[64];

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLGetStmtAttr(hstmt, desc_attr, &hdesc, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);

	rc = SQLSetDescField(hdesc, 0, SQL_DESC_COUNT, (SQLPOINTER) (SQLLEN) -1, 0);
	snprintf(what, sizeof(what), "%s SQL_DESC_COUNT=-1", desc_name);
	print_state(what, rc, SQL_HANDLE_DESC, hdesc);

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG,
						  SQL_INTEGER, 0, 0, &one, 0, NULL);
	snprintf(what, sizeof(what), "%s bind 1 after SQL_DESC_COUNT=-1", desc_name);
	print_state(what, rc, SQL_HANDLE_STMT, hstmt);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	one = 1;

	test_connect();

	negative_desc_count(SQL_ATTR_APP_PARAM_DESC, "APD");
	negative_desc_count(SQL_ATTR_IMP_PARAM_DESC, "IPD");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);
	rc = SQLBindParameter(hstmt, 0, SQL_PARAM_INPUT, SQL_C_SLONG,
						  SQL_INTEGER, 0, 0, &one, 0, NULL);
	print_state("bind 0", rc, SQL_HANDLE_STMT, hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

	run_with_params(MAX_PARAMS);
	run_with_params(MAX_PARAMS + 1);
	run_with_params(MAX_PARAMS + 2);

	test_disconnect();

	return 0;
}
