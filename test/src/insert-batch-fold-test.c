/*
 * Test the batch execution of an array of parameters for an INSERT.
 *
 * The rows of an INSERT that ends with a single VALUES tuple fold into one
 * multi-row INSERT, so that the server parses and plans the statement once
 * for the batch instead of once per row. A statement of any other shape is
 * sent as one statement per row.
 *
 * A statement level trigger counts the INSERT commands the server runs,
 * which is how each case below shows whether its rows folded. Either way
 * every row has to reach the table.
 */
#include <stdio.h>
#include <stdlib.h>

/* Must come before sql.h (declared in common.h) to suppress a warning */
#include "../../pgapifunc.h"

#include "common.h"

#define ROWCNT		6
#define BATCHSIZE	3

static const char *
status_str(SQLUSMALLINT status)
{
	switch (status)
	{
		case SQL_PARAM_SUCCESS:
			return "success";
		case SQL_PARAM_SUCCESS_WITH_INFO:
			return "success_with_info";
		case SQL_PARAM_ERROR:
			return "error";
		case SQL_PARAM_UNUSED:
			return "unused";
	}
	return "????";
}

/*
 * Insert ROWCNT rows with one array execution of the given statement. Where
 * null_row is not negative, the second parameter of that row is NULL, which
 * is a row whose text differs from the rest beyond the parameter values.
 */
static void
array_insert(const char *sql, int null_row)
{
	SQLRETURN	rc;
	HSTMT		hstmt;
	SQLINTEGER	ids[ROWCNT] = { 1, 2, 3, 4, 5, 6 };
	SQLCHAR		dts[ROWCNT][8] = { "one", "two", "three",
					   "four", "five", "six" };
	SQLLEN		dtlen[ROWCNT];
	SQLUSMALLINT	status[ROWCNT];
	SQLULEN		processed = 0;
	SQLLEN		rowcount = -1;
	int		i;

	printf("\nexecuting \"%s\" for %d rows", sql, ROWCNT);
	if (null_row >= 0)
		printf(", row %d null", null_row);
	printf("\n");

	for (i = 0; i < ROWCNT; i++)
		dtlen[i] = (i == null_row) ? SQL_NULL_DATA : SQL_NTS;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle failed", conn);

	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE,
			    (SQLPOINTER) ROWCNT, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr PARAMSET_SIZE failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_STATUS_PTR, status, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr PARAM_STATUS_PTR failed", hstmt);
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMS_PROCESSED_PTR,
			    &processed, 0);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr PARAMS_PROCESSED_PTR failed",
			  hstmt);

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG,
			      SQL_INTEGER, 0, 0, ids, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter 1 failed", hstmt);
	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
			      sizeof(dts[0]), 0, dts, sizeof(dts[0]), dtlen);
	CHECK_STMT_RESULT(rc, "SQLBindParameter 2 failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	/*
	 * SQLRowCount reports the rows of the last batch the driver sent, so
	 * a folded batch reports its whole tuple list where one statement per
	 * row reports the single row of the last statement.
	 */
	rc = SQLRowCount(hstmt, &rowcount);
	CHECK_STMT_RESULT(rc, "SQLRowCount failed", hstmt);
	printf("rows processed=%d, SQLRowCount=%d\n",
	       (int) processed, (int) rowcount);
	for (i = 0; i < ROWCNT; i++)
		printf("row %d status=%s\n", i, status_str(status[i]));

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

/*
 * Report the rows of the table and the number of INSERT commands that put
 * them there, then empty the table for the next case.
 */
static void
report_and_reset(HSTMT hstmt)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt,
			   "SELECT commands FROM test_fold_commands",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "select of the command count failed", hstmt);
	print_result(hstmt);
	rc = SQLCloseCursor(hstmt);
	CHECK_STMT_RESULT(rc, "SQLCloseCursor failed", hstmt);

	rc = SQLExecDirect(hstmt,
			   "SELECT id, dt FROM test_fold ORDER BY id",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "select of the rows failed", hstmt);
	print_result(hstmt);
	rc = SQLCloseCursor(hstmt);
	CHECK_STMT_RESULT(rc, "SQLCloseCursor failed", hstmt);

	rc = SQLExecDirect(hstmt, "DELETE FROM test_fold", SQL_NTS);
	CHECK_STMT_RESULT(rc, "delete failed", hstmt);
	rc = SQLExecDirect(hstmt,
			   "UPDATE test_fold_commands SET commands = 0",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "reset of the command count failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/*
	 * Batch BATCHSIZE rows at a time, so that ROWCNT rows take more than
	 * one batch and the count of a batch cannot be mistaken for the count
	 * of the whole array.
	 */
	rc = SQLSetConnectAttr(conn, SQL_ATTR_PGOPT_BATCHSIZE,
			       (SQLPOINTER)(SQLLEN) BATCHSIZE, 0);
	CHECK_CONN_RESULT(rc, "SQLSetConnectAttr BATCHSIZE failed", conn);

	rc = SQLExecDirect(hstmt,
			   "CREATE TEMPORARY TABLE test_fold"
			   "(id int4 PRIMARY KEY, dt varchar(8))",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "create table failed", hstmt);
	rc = SQLExecDirect(hstmt,
			   "CREATE TEMPORARY TABLE test_fold_commands"
			   "(commands int4)",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "create count table failed", hstmt);
	rc = SQLExecDirect(hstmt,
			   "INSERT INTO test_fold_commands VALUES (0)",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "initialize count failed", hstmt);
	rc = SQLExecDirect(hstmt,
			   "CREATE OR REPLACE FUNCTION test_fold_count()"
			   " RETURNS TRIGGER AS $$"
			   " BEGIN"
			   "  UPDATE test_fold_commands"
			   "   SET commands = commands + 1;"
			   "  RETURN NULL;"
			   " END;"
			   " $$ LANGUAGE plpgsql",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "create function failed", hstmt);
	rc = SQLExecDirect(hstmt,
			   "CREATE TRIGGER test_fold_count"
			   " AFTER INSERT ON test_fold"
			   " FOR EACH STATEMENT"
			   " EXECUTE PROCEDURE test_fold_count()",
			   SQL_NTS);
	CHECK_STMT_RESULT(rc, "create trigger failed", hstmt);

	/* The tuple ends the statement, so the rows fold */
	array_insert("INSERT INTO test_fold VALUES (?, ?)", -1);
	report_and_reset(hstmt);

	/* A ';' that ends the statement is dropped, so the rows still fold */
	array_insert("INSERT INTO test_fold (id, dt) VALUES (?, ?) ;", -1);
	report_and_reset(hstmt);

	/* Parentheses of their own inside the tuple keep it foldable */
	array_insert("INSERT INTO test_fold VALUES (?, upper(?))", -1);
	report_and_reset(hstmt);

	/* A null parameter is one tuple of the fold like any other */
	array_insert("INSERT INTO test_fold VALUES (?, ?)", 3);
	report_and_reset(hstmt);

	/*
	 * Text behind the tuple refuses the fold, because a command whose
	 * VALUES names a key twice is one the ON CONFLICT clause rejects.
	 */
	array_insert("INSERT INTO test_fold VALUES (?, ?)"
		     " ON CONFLICT (id) DO NOTHING", -1);
	report_and_reset(hstmt);

	/* A statement without a VALUES tuple refuses the fold as well */
	array_insert("INSERT INTO test_fold SELECT ?, ?", -1);
	report_and_reset(hstmt);

	/* The trigger holds the function, so the table goes first */
	rc = SQLExecDirect(hstmt, "DROP TABLE test_fold", SQL_NTS);
	CHECK_STMT_RESULT(rc, "drop table failed", hstmt);
	rc = SQLExecDirect(hstmt, "DROP FUNCTION test_fold_count()", SQL_NTS);
	CHECK_STMT_RESULT(rc, "drop function failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	test_disconnect();

	return 0;
}
