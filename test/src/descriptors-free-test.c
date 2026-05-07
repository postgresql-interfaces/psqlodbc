/*
 * Test freeing explicitly attached application descriptors.
 *
 * A statement must not keep dangling ARD/APD pointers after an application
 * descriptor is freed.  The driver should fall back to the statement's
 * implicit descriptors so later statement attributes can still be changed.
 */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static HSTMT
alloc_stmt(void)
{
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLRETURN	rc;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(SQL_HANDLE_STMT) failed",
			SQL_HANDLE_DBC, conn);
		exit(1);
	}
	return hstmt;
}

static SQLHDESC
alloc_desc(void)
{
	SQLHDESC	hdesc = SQL_NULL_HDESC;
	SQLRETURN	rc;

	rc = SQLAllocHandle(SQL_HANDLE_DESC, conn, &hdesc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(SQL_HANDLE_DESC) failed",
			SQL_HANDLE_DBC, conn);
		exit(1);
	}
	return hdesc;
}

static void
test_app_row_desc_free(void)
{
	HSTMT		hstmt = alloc_stmt();
	SQLHDESC	hdesc = alloc_desc();
	SQLHDESC	current = SQL_NULL_HDESC;
	SQLRETURN	rc;

	printf("freeing attached application row descriptor\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, hdesc,
		SQL_IS_POINTER);
	CHECK_STMT_RESULT(rc, "attach ARD failed", hstmt);

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &current, 0, NULL);
	CHECK_STMT_RESULT(rc, "get attached ARD failed", hstmt);
	if (current != hdesc)
	{
		printf("attached ARD handle mismatch\n");
		exit(1);
	}

	rc = SQLFreeHandle(SQL_HANDLE_DESC, hdesc);
	if (!SQL_SUCCEEDED(rc))
	{
		printf("SQLFreeHandle(SQL_HANDLE_DESC) for ARD failed\n");
		exit(1);
	}

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &current, 0, NULL);
	CHECK_STMT_RESULT(rc, "get ARD after free failed", hstmt);
	if (current == hdesc)
	{
		printf("freed ARD is still attached to the statement\n");
		exit(1);
	}

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
		(SQLPOINTER) 2, 0);
	CHECK_STMT_RESULT(rc, "set row array size after ARD free failed", hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "free ARD test statement failed", hstmt);
}

static void
test_app_param_desc_free(void)
{
	HSTMT		hstmt = alloc_stmt();
	SQLHDESC	hdesc = alloc_desc();
	SQLHDESC	current = SQL_NULL_HDESC;
	SQLRETURN	rc;

	printf("freeing attached application parameter descriptor\n");
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_APP_PARAM_DESC, hdesc,
		SQL_IS_POINTER);
	CHECK_STMT_RESULT(rc, "attach APD failed", hstmt);

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_PARAM_DESC, &current, 0, NULL);
	CHECK_STMT_RESULT(rc, "get attached APD failed", hstmt);
	if (current != hdesc)
	{
		printf("attached APD handle mismatch\n");
		exit(1);
	}

	rc = SQLFreeHandle(SQL_HANDLE_DESC, hdesc);
	if (!SQL_SUCCEEDED(rc))
	{
		printf("SQLFreeHandle(SQL_HANDLE_DESC) for APD failed\n");
		exit(1);
	}

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_PARAM_DESC, &current, 0, NULL);
	CHECK_STMT_RESULT(rc, "get APD after free failed", hstmt);
	if (current == hdesc)
	{
		printf("freed APD is still attached to the statement\n");
		exit(1);
	}

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE,
		(SQLPOINTER) 2, 0);
	CHECK_STMT_RESULT(rc, "set paramset size after APD free failed", hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "free APD test statement failed", hstmt);
}

int
main(int argc, char **argv)
{
	test_connect();
	test_app_row_desc_free();
	test_app_param_desc_free();
	test_disconnect();
	return 0;
}
