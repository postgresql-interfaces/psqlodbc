/*
 * Test ARD bookmark allocation during statement handle allocation.
 *
 * This is the normal black-box regression path for the OOM hardening in
 * ARD_AllocBookmark().  The actual allocation-failure path is validated with
 * external malloc-failure injection, because the default regression suite must
 * remain portable and deterministic.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect_ext("UseDeclareFetch=1;Fetch=1");

	printf("allocating ARD bookmark during statement allocation\n");
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("statement allocation failed", SQL_HANDLE_DBC, conn);
		test_disconnect();
		return 1;
	}
	printf("statement handle allocated\n");

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_disconnect();
	printf("ok\n");

	return 0;
}
