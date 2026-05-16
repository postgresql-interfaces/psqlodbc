#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
test_setdescrec_ard_binding(void)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLHDESC hDesc = SQL_NULL_HDESC;
	char value[16] = "";
	SQLLEN ind = 0;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &hDesc, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);

	rc = SQLSetDescRec(hDesc, 1, SQL_C_CHAR, 0, sizeof(value), 0, 0,
					   value, &ind, &ind);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLSetDescRec failed", SQL_HANDLE_DESC, hDesc);
		exit(1);
	}

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 42", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	printf("\n-- SQLSetDescRec ARD binding --\n");
	printf("VALUE: %s\n", value);
	printf("INDICATOR: %ld\n", (long) ind);

	if (strcmp(value, "42") != 0 || ind != 2)
	{
		printf("SQLSetDescRec ARD binding mismatch\n");
		exit(1);
	}

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
    SQLSMALLINT colcount;
    SQLHDESC hDesc;
    SQLCHAR name[64];
    SQLSMALLINT nameLen = 64;
    SQLSMALLINT type, subType;
    SQLSMALLINT precision, scale, nullable;
    SQLLEN length;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Prepare a test table */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE desctable (col1 int4 not null, col2 numeric(4,2), col3 varchar(10) not null, col4 bigint not null)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed while creating temp table", hstmt);

	/* Prepare a statement */
	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT * FROM desctable", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

    /* Execute */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	rc = SQLNumResultCols(hstmt, &colcount);
	CHECK_STMT_RESULT(rc, "SQLNumResultCols failed", hstmt);

    /* Get the descriptor handle */
    rc = SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_ROW_DESC, &hDesc, 0, NULL);
    CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);

    for (int i = 1; i <= colcount; i++)
    {
        rc = SQLGetDescRec(hDesc, i, name, nameLen, NULL, &type, &subType, &length, &precision, &scale, &nullable);
        if (!SQL_SUCCEEDED(rc))
        {
            print_diag("SQLGetDescRec failed", SQL_HANDLE_DESC, hDesc);
            exit(1);
        }

        printf("\n-- Column %d --\n", i);
        printf("SQL_DESC_NAME: %s\n", name);
        printf("SQL_DESC_TYPE: %d\n", type);
        printf("SQL_DESC_OCTET_LENGTH: %d\n", (int) length);
        printf("SQL_DESC_PRECISION: %d\n", precision);
        printf("SQL_DESC_SCALE: %d\n", scale);
        printf("SQL_DESC_NULLABLE: %d\n", nullable);
    }

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	CHECK_STMT_RESULT(rc, "SQLFreeHandle failed", hstmt);

	test_setdescrec_ard_binding();

	/* Clean up */
	test_disconnect();

	return 0;
}
