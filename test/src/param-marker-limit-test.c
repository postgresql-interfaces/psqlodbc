/*
 * Statements with more parameter markers than the driver can number.  The
 * query builder counts markers in an Int2, so describing a statement with
 * 32769 markers must fail: the count would wrap to -32768 and index before
 * the IPD array.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/*
 * What an application does to look up a list of ids: one marker per id in
 * SELECT g FROM generate_series(1, 10) g WHERE g IN (?, ..., ?), describe
 * the result, bind the ids, execute and count the rows.
 */
static void
find_by_ids(int nids)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLSMALLINT	ncols;
	SQLINTEGER *ids;
	char	   *sql, *p;
	int			i, rows = 0;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	sql = malloc(64 + 2 * nids);
	p = sql + sprintf(sql, "SELECT g FROM generate_series(1, 10) g WHERE g IN (?");
	for (i = 1; i < nids; i++)
		p += sprintf(p, ",?");
	strcpy(p, ")");

	rc = SQLPrepare(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	printf("%d ids, SQLNumResultCols: ", nids);
	fflush(stdout);				/* keep the output if the call crashes */
	rc = SQLNumResultCols(hstmt, &ncols);
	if (!SQL_SUCCEEDED(rc))
	{
		SQLCHAR		state[6] = "";

		SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, state, NULL, NULL, 0, NULL);
		printf("error SQLSTATE=%s\n", (char *) state);
		goto done;
	}
	printf("%d column(s)\n", ncols);

	ids = malloc(nids * sizeof(SQLINTEGER));
	for (i = 0; i < nids; i++)
	{
		ids[i] = i + 1;
		rc = SQLBindParameter(hstmt, (SQLUSMALLINT) (i + 1), SQL_PARAM_INPUT,
							  SQL_C_SLONG, SQL_INTEGER, 0, 0, &ids[i], 0, NULL);
		CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	}
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);
	while (SQL_SUCCEEDED(SQLFetch(hstmt)))
		rows++;
	printf("%d ids, SQLExecute: %d row(s)\n", nids, rows);
	free(ids);

done:
	free(sql);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int
main(int argc, char **argv)
{
	test_connect();

	/*
	 * Some libpq versions refuse a ParameterDescription message longer
	 * than 30000 bytes, 4 bytes per parameter, so the statement that must
	 * work stays below 7500 parameters.
	 */
	find_by_ids(7000);
	find_by_ids(32769);

	test_disconnect();

	return 0;
}
