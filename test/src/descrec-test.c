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

/*
 * The length fields are SQLLEN in ODBC. Start each variable with every bit
 * set, so that a driver writing only four bytes of it shows up as a wrong
 * value here, and not just as stack corruption a build may not detect.
 */
static void
print_len_field(const char *label, SQLHDESC hDesc, SQLSMALLINT field)
{
	SQLRETURN	rc;
	SQLLEN		v = -1;

	rc = SQLGetDescField(hDesc, 1, field, &v, sizeof(v), NULL);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLGetDescField failed", SQL_HANDLE_DESC, hDesc);
		exit(1);
	}
	printf("%s: %lld\n", label, (long long) v);
}

static void
test_len_field_width(void)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDESC	hDesc;
	char		col[16], param[16] = "1";
	SQLLEN		colind = 0, paramind = SQL_NTS;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, col, sizeof(col), &colind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER,
						  0, 0, param, sizeof(param), &paramind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT ?::int4", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	printf("\n-- length fields as SQLLEN --\n");
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &hDesc, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	print_len_field("ARD SQL_DESC_OCTET_LENGTH", hDesc, SQL_DESC_OCTET_LENGTH);
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_PARAM_DESC, &hDesc, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	print_len_field("APD SQL_DESC_OCTET_LENGTH", hDesc, SQL_DESC_OCTET_LENGTH);
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, &hDesc, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	print_len_field("IPD SQL_DESC_OCTET_LENGTH", hDesc, SQL_DESC_OCTET_LENGTH);
	rc = SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_ROW_DESC, &hDesc, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLGetStmtAttr failed", hstmt);
	print_len_field("IRD SQL_DESC_OCTET_LENGTH", hDesc, SQL_DESC_OCTET_LENGTH);
	print_len_field("IRD SQL_DESC_LENGTH", hDesc, SQL_DESC_LENGTH);
	print_len_field("IRD SQL_DESC_DISPLAY_SIZE", hDesc, SQL_DESC_DISPLAY_SIZE);

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
	test_len_field_width();

	/* Clean up */
	test_disconnect();

	return 0;
}
