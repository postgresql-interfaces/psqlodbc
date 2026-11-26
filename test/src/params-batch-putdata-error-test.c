#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define ROWS	3

static char *texts[ROWS] = {"first message", "second message", "third message"};

/* bound to the statement, so they must outlive insert_batch() */
static SQLINTEGER ids[ROWS];
static SQLLEN text_ind[ROWS];

/*
 * Insert ROWS messages with one parameter array, the text of each row sent
 * with SQLPutData. If the batch fails, note that in job_log on the same
 * statement handle.
 */
static void
insert_batch(HSTMT hstmt, int batch_no, int row_with_bad_length)
{
	SQLRETURN	rc;
	PTR			param_id;
	char		log_sql[100];
	int			row;

	printf("batch %d: bad SQLPutData length in row %d\n", batch_no, row_with_bad_length);
	for (row = 0; row < ROWS; row++)
	{
		ids[row] = batch_no * 10 + row + 1;
		text_ind[row] = SQL_DATA_AT_EXEC;
	}

	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) ROWS, 0);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
						  0, 0, ids, 0, NULL);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);
	rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARCHAR,
						  0, 0, (SQLPOINTER) 2, 0, text_ind);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "INSERT INTO messages VALUES (?, ?)", SQL_NTS);
	if (rc != SQL_NEED_DATA)
	{
		print_diag("SQLExecDirect did not ask for data", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}
	for (row = 1;; row++)
	{
		/* after the last row, this executes the batch */
		rc = SQLParamData(hstmt, &param_id);
		if (rc != SQL_NEED_DATA)
			break;
		/* -7 is none of SQL_NTS, SQL_NULL_DATA, ...: rejected with HY024 */
		rc = SQLPutData(hstmt, texts[row - 1], row == row_with_bad_length ? -7 : SQL_NTS);
		if (!SQL_SUCCEEDED(rc))
			break;
	}
	if (SQL_SUCCEEDED(rc))
	{
		printf("batch %d stored\n", batch_no);
		return;
	}
	print_diag("batch failed", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) 1, 0);

	snprintf(log_sql, sizeof(log_sql), "INSERT INTO job_log VALUES ('batch %d failed')", batch_no);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) log_sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	printf("%s: %s\n", log_sql, rc == SQL_SUCCESS ? "SQL_SUCCESS" : "SQL_SUCCESS_WITH_INFO");
}

static void
print_table(HSTMT hstmt, char *query)
{
	SQLRETURN	rc;

	printf("%s\n", query);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) query, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	HSTMT		hstmt_load = SQL_NULL_HSTMT;

	/* the rows of a batch are queued and sent together (the default) */
	test_connect_ext("BatchSize=100");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt_load);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE messages (id int4, body text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE job_log (outcome text)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	insert_batch(hstmt_load, 1, 1);
	insert_batch(hstmt_load, 2, 3);

	/* Both batches failed: messages must be empty, and job_log must note both. */
	print_table(hstmt, "SELECT id FROM messages ORDER BY id");
	print_table(hstmt, "SELECT outcome FROM job_log ORDER BY outcome");

	test_disconnect();

	return 0;
}
