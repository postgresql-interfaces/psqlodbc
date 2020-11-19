#include <stdio.h>
#include <stdlib.h>

/* Must come before sql.h (declared in common.h) to suppress a warning */
#include "../../pgapifunc.h"

#include "common.h"

static void b_result(SQLRETURN rc, HSTMT stmt,  int repcnt, SQLUSMALLINT status[])
{
	int i;
	printf("insert into test_batch returns %d\n", rc);
	if (!SQL_SUCCEEDED(rc))
		print_diag("", SQL_HANDLE_STMT, stmt);
	/*
	if (SQL_SUCCESS != rc)
		disp_stmt_error(stmt);
	*/
	for (i = 0; i < repcnt; i++)
	{
		printf("row %d status=%s\n", i,
			(status[i] == SQL_PARAM_SUCCESS ? "success" :
			(status[i] == SQL_PARAM_UNUSED ? "unused" :
				(status[i] == SQL_PARAM_ERROR ? "error" :
				(status[i] == SQL_PARAM_SUCCESS_WITH_INFO ? "success_with_info" : "????")
					))));
	}
}

#define	BATCHCNT	10
static SQLRETURN	BatchExecute(HDBC conn, int batch_size)
{
	SQLRETURN	rc;
	HSTMT		hstmt;
	int vals[BATCHCNT] = { 0, 0, 1, 2, 2, 3, 4, 4, 5, 6};
	SQLCHAR strs[BATCHCNT][10] = { "0", "0-2", "1", "2", "2-2", "3", "4", "4-2", "5", "6" };
	SQLUSMALLINT	status[BATCHCNT];

	rc = SQLSetConnectAttr(conn, SQL_ATTR_PGOPT_BATCHSIZE, (SQLPOINTER)(SQLLEN)batch_size, 0);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLSetConnectAttr SQL_ATTR_PGOPT_BATCHSIZE failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);

	SQLSetStmtAttr(hstmt , SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)BATCHCNT, 0);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_STATUS_PTR, status, 0);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, vals, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter 1 failed", hstmt);
	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, sizeof(strs[0]), 0, strs, sizeof(strs[0]), NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter 2 failed", hstmt);
	rc = SQLExecDirect(hstmt, "INSERT INTO test_batch VALUES (?, ?)"
		" ON CONFLICT (id) DO UPDATE SET dt=EXCLUDED.dt"
		, SQL_NTS);
	b_result(rc, hstmt, BATCHCNT, status);
	/**
	rc = SQLExecDirect(hstmt, "SELECT * FROM test_batch where id=?"
		, SQL_NTS);
	b_result(rc, hstmt, BATCHCNT, status);
	SQLCloseCursor(hstmt);
	**/
	strncpy((SQLCHAR *) &strs[BATCHCNT - 3], "4-long", sizeof(strs[0]));
	rc = SQLExecDirect(hstmt, "INSERT INTO test_batch VALUES (?, ?)"
		" ON CONFLICT (id) DO UPDATE SET dt=EXCLUDED.dt"
		, SQL_NTS);
	b_result(rc, hstmt, BATCHCNT, status);

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	return rc;
}

int main(int argc, char **argv)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Drop the existent tables */
	rc = SQLExecDirect(hstmt, "drop table if exists test_batch", SQL_NTS);
	CHECK_STMT_RESULT(rc, "drop table failed", hstmt);
	/* Create temp table */
	rc = SQLExecDirect(hstmt, "create temporary table test_batch(id int4 primary key, dt varchar(4))", SQL_NTS);
	CHECK_STMT_RESULT(rc, "create table failed", hstmt);
	/* Create function */
	rc = SQLExecDirect(hstmt,
		"CREATE OR REPLACE FUNCTION batch_update_notice() RETURNS TRIGGER"
		" AS $$"
		" BEGIN"
		"  RAISE NOTICE 'id=% updated', NEW.id;"
		"  RETURN NULL;"
		" END;"
		" $$ LANGUAGE plpgsql"
		, SQL_NTS);
	CHECK_STMT_RESULT(rc, "create function failed", hstmt);
	/* Create trigger */
	rc = SQLExecDirect(hstmt,
		"CREATE TRIGGER batch_update_notice"
		" BEFORE update on test_batch"
		" FOR EACH ROW EXECUTE PROCEDURE batch_update_notice()"
		, SQL_NTS);
	CHECK_STMT_RESULT(rc, "create trigger failed", hstmt);

	/* 1 by 1 executiton */
	printf("one by one execution\n");
	BatchExecute(conn, 1);
	/* Truncate table */
	rc = SQLExecDirect(hstmt, "truncate table test_batch", SQL_NTS);
	CHECK_STMT_RESULT(rc, "truncate table failed", hstmt);
	/* batch executiton batch_size=2*/
	printf("batch execution\n");
	BatchExecute(conn, 2);

	/* Clean up */
	test_disconnect();

	return 0;
}
