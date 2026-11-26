/*
 * Test output parameters of a procedure call that the application does not
 * want: bound with a NULL ParameterValuePtr, with or without a
 * StrLen_or_IndPtr. ODBC lets an application discard an output value that
 * way. The driver must not convert such a value into a dummy buffer of
 * its own: a 2-byte one overflows for the fixed-length C types.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static void
call_proc(const char *label, int ref_arg, SQLGUID *ref, SQLLEN *ref_ind)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLINTEGER	total = 0;
	SQLLEN		total_ind = SQL_DATA_AT_EXEC;
	TIMESTAMP_STRUCT stamp;
	SQLLEN		stamp_ind = SQL_DATA_AT_EXEC;

	/*
	 * The call sets the indicator of an output parameter, so the driver
	 * must ignore its value before the call. SQL_DATA_AT_EXEC checks that the
	 * driver does not take it for a data-at-execution request.
	 */
	if (ref_ind)
		*ref_ind = SQL_DATA_AT_EXEC;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
						  0, 0, &ref_arg, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_OUTPUT, SQL_C_GUID, SQL_GUID,
						  0, 0, ref, 0, ref_ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLBindParameter(hstmt, 3, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER,
						  0, 0, &total, 0, &total_ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLBindParameter(hstmt, 4, SQL_PARAM_OUTPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP,
						  0, 0, &stamp, 0, &stamp_ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "{call discard_out_proc(?, ?, ?, ?)}", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	printf("%s:", label);
	if (ref)
		printf(" ref %08lx-%04x-%04x", (unsigned long) ref->Data1, ref->Data2, ref->Data3);
	if (ref_ind)
		printf(" ref_ind %d", (int) *ref_ind);
	printf(" total %d (ind %d) stamp %04d-%02d-%02d (ind %d)\n",
		   (int) total, (int) total_ind,
		   stamp.year, stamp.month, stamp.day, (int) stamp_ind);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLGUID		ref;
	SQLLEN		ref_ind;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	/* ref is NULL when asked for 0 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
					   "CREATE OR REPLACE FUNCTION discard_out_proc(IN n int, OUT ref uuid, OUT total int, OUT stamp timestamp) AS $$ "
					   "SELECT CASE WHEN $1 = 0 THEN NULL ELSE '41414141-4242-4343-4444-454546464747'::uuid END, "
					   "$1, '2024-01-15 10:00:00'::timestamp $$ LANGUAGE SQL",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	/* every output parameter has a buffer */
	call_proc("bound", 42, &ref, &ref_ind);
	/* ref is discarded: neither a buffer nor an indicator */
	call_proc("discarded", 42, NULL, NULL);
	/* ref has an indicator but no buffer */
	call_proc("indicator only", 42, NULL, &ref_ind);
	/* the discarded value is NULL */
	call_proc("discarded NULL", 0, NULL, NULL);
	call_proc("indicator only NULL", 0, NULL, &ref_ind);

	test_disconnect();

	return 0;
}
