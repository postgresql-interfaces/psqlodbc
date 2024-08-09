#include <stdio.h>
#include <stdlib.h>

#include "common.h"

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

	/* Clean up */
	test_disconnect();

	return 0;
}
