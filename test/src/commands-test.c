#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Test VACUUM */
	printf("Testing VACUUM with SQLExecDirect...\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "vacuum (analyze) testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Same with SQLPrepare/SQLExecute */
	printf("Testing VACUUM with SQLPrepare/SQLExecute...\n");
	rc = SQLPrepare(hstmt, (SQLCHAR *) "VACUUM ANALYZE testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Now the same with autocommit disabled. The driver should recognize
	 * that the commands are VACUUMs, which cannot be run in a transaction
	 * block, and not issue a BEGIN even it normally would in autocommit
	 * mode. In other words, these commands should behave the same with or
	 * without autocommit. But if you issued a normal query, like a SELECT,
	 * first in the same transaction, and then tried to run a VACUUM, it
	 * would fail with "VACUUM cannot run inside a transaction block" error.
	 */
	printf("Disabling autocommit...\n");

	rc = SQLSetConnectAttr(conn,
						   SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER)SQL_AUTOCOMMIT_OFF,
						   SQL_IS_UINTEGER);
	CHECK_STMT_RESULT(rc, "SQLSetConnectAttr failed", hstmt);

	/* Test VACUUM */
	printf("Testing VACUUM with SQLExecDirect...\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "vacuum analyze testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Same with SQLPrepare/SQLExecute */
	printf("Testing VACUUM with SQLPrepare/SQLExecute...\n");
	rc = SQLPrepare(hstmt, (SQLCHAR *) "VACUUM (ANALYZE) testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * COPY ... FROM STDIN / TO STDOUT hand the connection over to the COPY
	 * streaming sub-protocol, which we don't implement. Left alone, the
	 * server waits forever for CopyData we never send while the driver
	 * spins on the same COPY result. The driver should instead unwind the
	 * copy, report it as unimplemented, and leave the connection usable --
	 * on both the SQLExecDirect and SQLPrepare/SQLExecute paths.
	 */
	printf("Testing COPY FROM STDIN with SQLExecDirect...\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "COPY testtab1 FROM STDIN", SQL_NTS);
	/* Print error, it is expected */
	if (!SQL_SUCCEEDED(rc))
		print_diag("", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	printf("Testing COPY TO STDOUT with SQLExecDirect...\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "COPY testtab1 TO STDOUT", SQL_NTS);
	/* Print error, it is expected */
	if (!SQL_SUCCEEDED(rc))
		print_diag("", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Same with SQLPrepare/SQLExecute. The statement itself parses fine, so
	 * the rejection only happens once the server reports copy-in at execute
	 * time.
	 */
	printf("Testing COPY FROM STDIN with SQLPrepare/SQLExecute...\n");
	rc = SQLPrepare(hstmt, (SQLCHAR *) "COPY testtab1 FROM STDIN", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	rc = SQLExecute(hstmt);
	/* Print error, it is expected */
	if (!SQL_SUCCEEDED(rc))
		print_diag("", SQL_HANDLE_STMT, hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * A COPY naming a column called "stdin" is a perfectly ordinary
	 * file-based COPY and must not be caught by the check above.
	 */
	printf("Testing that a column named stdin is not mistaken for COPY FROM STDIN...\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMPORARY TABLE copy_stdin_col (stdin text, id int)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "COPY copy_stdin_col (stdin, id) TO '/dev/null'", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* The connection must still be perfectly usable afterwards */
	printf("Testing that the connection still works after a rejected COPY...\n");
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
