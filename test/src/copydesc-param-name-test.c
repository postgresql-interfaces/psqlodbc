/*
 * Test SQLCopyDesc from an IPD that has a parameter name.
 *
 * The name is an allocated string. Each descriptor frees its own, so the
 * copy must get a string of its own: it must still have the name after
 * the source's statement is freed, and freeing both statements must not
 * free the same string twice.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		source = SQL_NULL_HSTMT, target = SQL_NULL_HSTMT;
	SQLHDESC	source_ipd = NULL, target_ipd = NULL;
	char		name[32] = "";
	SQLINTEGER	len = 0;

	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &source);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &target);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLGetStmtAttr(source, SQL_ATTR_IMP_PARAM_DESC, &source_ipd, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", source);
	rc = SQLGetStmtAttr(target, SQL_ATTR_IMP_PARAM_DESC, &target_ipd, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", target);

	rc = SQLSetDescField(source_ipd, 1, SQL_DESC_NAME,
						 (SQLPOINTER) "param_one", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLSetDescField failed", source);
	rc = SQLCopyDesc(source_ipd, target_ipd);
	CHECK_STMT_RESULT(rc, "SQLCopyDesc failed", target);
	printf("copied the IPD\n");

	rc = SQLFreeHandle(SQL_HANDLE_STMT, source);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", source);
	rc = SQLGetDescField(target_ipd, 1, SQL_DESC_NAME, name, sizeof(name),
						 &len);
	CHECK_STMT_RESULT(rc, "SQLGetDescField failed", target);
	printf("the copy's parameter name after the source is freed: %s\n", name);

	/* a second free of the same string must not lose this line */
	fflush(stdout);
	rc = SQLFreeHandle(SQL_HANDLE_STMT, target);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", target);
	printf("freed both statements\n");

	test_disconnect();
	return 0;
}
