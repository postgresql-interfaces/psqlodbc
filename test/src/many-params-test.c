/*
 * A statement with 1000 parameters, all bound as SQL_TYPE_TIMESTAMP.
 *
 * With server-side prepare, the driver writes each placeholder with a
 * type cast, e.g. "$1000::timestamp". The placeholder must reach the
 * server whole, whatever the parameter number.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define NPARAMS 1000

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	char		sql[16 + NPARAMS * 3];
	char	   *p;
	int			i;
	SQL_TIMESTAMP_STRUCT ts;
	SQLLEN		cbTs = sizeof(ts);
	char		buf[64];
	SQLLEN		ind;

	test_connect_ext("UseServerSidePrepare=1");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	/* SELECT ?, ?, ... with NPARAMS placeholders */
	p = sql + sprintf(sql, "SELECT ?");
	for (i = 1; i < NPARAMS; i++)
		p += sprintf(p, ", ?");

	memset(&ts, 0, sizeof(ts));
	ts.year = 2026;
	ts.month = 10;
	ts.day = 8;
	ts.hour = 12;
	ts.minute = 34;
	ts.second = 56;

	for (i = 1; i <= NPARAMS; i++)
	{
		rc = SQLBindParameter(hstmt, i, SQL_PARAM_INPUT,
							  SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP,
							  19, 0, &ts, sizeof(ts), &cbTs);
		CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	}

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetData(hstmt, NPARAMS, SQL_C_CHAR, buf, sizeof(buf), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("column %d: %s\n", NPARAMS, buf);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();

	return 0;
}
