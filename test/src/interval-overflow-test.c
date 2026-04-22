/*
 * Test for getPrecisionPart overflow fix (GitHub issue #173).
 *
 * Fetches intervals with fractional seconds to verify the driver
 * handles interval precision correctly and doesn't overflow the
 * internal buffer in getPrecisionPart().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQL_INTERVAL_STRUCT iv;
	SQLLEN ind;
	SQLHDESC ard;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_STMT_RESULT(rc, "SQLAllocHandle failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SET intervalstyle=postgres", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SET intervalstyle failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Test 1: default precision */
	printf("Test 1: default precision\n");
	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "SELECT '01:02:03.123456'::interval", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLBindCol(hstmt, 1, SQL_C_INTERVAL_HOUR_TO_SECOND,
					&iv, sizeof(iv), &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	printf("hour=%d min=%d sec=%d frac=%d\n",
		   (int) iv.intval.day_second.hour,
		   (int) iv.intval.day_second.minute,
		   (int) iv.intval.day_second.second,
		   (int) iv.intval.day_second.fraction);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_UNBIND);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt UNBIND failed", hstmt);

	/*
	 * Test 2: precision=20 via ARD descriptor override.
	 * Before the fix for issue #173, getPrecisionPart(20, ...)
	 * would write past the end of its 10-byte stack buffer.
	 * With the fix, precision is clamped to 9.
	 */
	printf("Test 2: precision=20 via ARD\n");
	memset(&iv, 0, sizeof(iv));

	rc = SQLExecDirect(hstmt,
		(SQLCHAR *) "SELECT '01:02:03.123456'::interval", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLBindCol(hstmt, 1, SQL_C_INTERVAL_HOUR_TO_SECOND,
					&iv, sizeof(iv), &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	/* Override precision to 20 — triggers the clamping fix */
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);

	rc = SQLSetDescField(ard, 1, SQL_DESC_PRECISION,
						 (SQLPOINTER) 20, 0);
	CHECK_STMT_RESULT(rc, "SQLSetDescField PRECISION failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	printf("hour=%d min=%d sec=%d frac=%d\n",
		   (int) iv.intval.day_second.hour,
		   (int) iv.intval.day_second.minute,
		   (int) iv.intval.day_second.second,
		   (int) iv.intval.day_second.fraction);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	printf("ok\n");

	test_disconnect();

	return 0;
}
