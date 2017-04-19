/*
 * This test is used to check the output and processing of the catalog
 * functions listed as below:
 * - SQLGetTypeInfo
 * - SQLTables
 * - SQLColumns
 * - SQLSpecialColumns
 * - SQLStatistics
 * - SQLPrimaryKeys
 * - SQLForeignKeys
 * - SQLProcedureColumns
 * - SQLTablePrivileges
 * - SQLColumnPrivileges
 * - SQLProcedures
 * - SQLGetInfo
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(int argc, char **argv)
{
	int			rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	/* Cases where output is limited to relevant information only */
	SQLSMALLINT sql_tab_privileges_ids[6] = {1, 2, 3, 4, 6, 7};
	SQLSMALLINT sql_column_ids[6] = {1, 2, 3, 4, 5, 6};
	SQLSMALLINT sql_pro_column_ids[] = {1, 2, 3, 4, 5, 6, 7};
	char		buf[1000];
	char		username[100];
	SQLSMALLINT	len;
	int			i;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Check for SQLGetTypeInfo */
	printf("Check for SQLTypeInfo\n");
	rc = SQLGetTypeInfo(hstmt, SQL_VARCHAR);
	CHECK_STMT_RESULT(rc, "SQLGetTypeInfo failed", hstmt);
	print_result_meta(hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check for SQLTables */
	printf("Check for SQLTables\n");
	rc = SQLTables(hstmt, NULL, 0,
				   (SQLCHAR *) "public", SQL_NTS,
				   (SQLCHAR *) "%", SQL_NTS,
				   (SQLCHAR *) "TABLE", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLTables failed", hstmt);
	print_result_meta(hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check for SQLColumns */
	printf("Check for SQLColumns\n");
	rc = SQLColumns(hstmt,
					NULL, 0,
					(SQLCHAR *) "public", SQL_NTS,
					(SQLCHAR *) "%", SQL_NTS,
					NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLColumns failed", hstmt);
	print_result_meta(hstmt);
	/*
	 * Print only the 6 first columns, we do not want for example
	 * to get the OID in output, and this information looks to be
	 * enough.
	 */
	print_result_series(hstmt, sql_column_ids, 6, -1);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check for SQLColumnPrivileges */
	//printf("Check for SQLColumnPrivileges\n");
	//rc = SQLColumnPrivileges(hstmt,
	//						 NULL, 0,
	//						 (SQLCHAR *) "public", SQL_NTS,
	//						 (SQLCHAR *) "testtab1", SQL_NTS,
	//						 (SQLCHAR *) "id", SQL_NTS);
	//CHECK_STMT_RESULT(rc, "SQLColumnPrivileges failed", hstmt);
	//print_result_meta(hstmt);
	//print_result(hstmt);
	//rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	//CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check for SQLSpecialColumns */
	printf("Check for SQLSpecialColumns\n");
	rc = SQLSpecialColumns(hstmt, SQL_ROWVER,
						   NULL, 0,
						   (SQLCHAR *) "public", SQL_NTS,
						   (SQLCHAR *) "testtab1", SQL_NTS,
						   SQL_SCOPE_SESSION,
						   SQL_NO_NULLS);
	CHECK_STMT_RESULT(rc, "SQLSpecialColumns failed", hstmt);
	print_result_meta(hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Check for SQLStatistics. It is important to note that this function
	 * returns statistics like the number of pages used and the number of
	 * index scans.
	 */
	printf("Check for SQLStatistics\n");
	rc = SQLStatistics(hstmt,
					   NULL, 0,
					   (SQLCHAR *) "public", SQL_NTS,
					   (SQLCHAR *) "testtab1", SQL_NTS,
					   0, 0);
	CHECK_STMT_RESULT(rc, "SQLStatistics failed", hstmt);
	print_result_meta(hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check for SQLPrimaryKeys */
	printf("Check for SQLPrimaryKeys\n");
	rc = SQLPrimaryKeys(hstmt,
						NULL, 0,
						(SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) "testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrimaryKeys failed", hstmt);
	print_result_meta(hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check for SQLForeignKeys */
	printf("Check for SQLForeignKeys\n");
	rc = SQLForeignKeys(hstmt,
						NULL, 0,
						(SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) "testtab1", SQL_NTS,
						NULL, 0,
						(SQLCHAR *) "public", SQL_NTS,
						(SQLCHAR *) "testtab_fk", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLForeignKeys failed", hstmt);
	print_result_meta(hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check for SQLProcedures */
	printf("Check for SQLProcedures\n");
	rc = SQLProcedures(hstmt,
					   NULL, 0,
					   (SQLCHAR *) "public", SQL_NTS,
					   (SQLCHAR *) "simple_add", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLProcedures failed", hstmt);
	print_result_meta(hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check for SQLProcedureColumns */
	printf("Check for SQLProcedureColumns\n");
	rc = SQLProcedureColumns(hstmt,
							 NULL, 0,
							 (SQLCHAR *) "public", SQL_NTS,
							 (SQLCHAR *) "simple_add", SQL_NTS,
							 NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLProcedureColumns failed", hstmt);
	print_result_meta(hstmt);
	print_result(hstmt);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	/*
	 * Check for SQLProcedureColumns for a function
	 * 	returning normal type
	 */
	rc = SQLProcedureColumns(hstmt, NULL, 0,
				 NULL, 0,
				 (SQLCHAR *) "set_byte", SQL_NTS,
				 NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLProcedureColumns failed", hstmt);
	print_result_series(hstmt, sql_pro_column_ids, sizeof(sql_pro_column_ids) / sizeof(sql_pro_column_ids[0]), -1);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	/*
	 * Check for SQLProcedureColumns for a function
	 * 	returning composite type
	 */
	rc = SQLProcedureColumns(hstmt, NULL, 0,
				 (SQLCHAR *) "public", SQL_NTS,
				 (SQLCHAR *) "getfoo", SQL_NTS,
				 NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLProcedureColumns failed", hstmt);
	print_result_series(hstmt, sql_pro_column_ids, sizeof(sql_pro_column_ids) / sizeof(sql_pro_column_ids[0]), -1);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	/*
	 * Check for SQLProcedureColumns for a function
	 * 	returning setof composite type
	 */
	rc = SQLProcedureColumns(hstmt, NULL, 0,
				 (SQLCHAR *) "public", SQL_NTS,
				 (SQLCHAR *) "getboo", SQL_NTS,
				 NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLProcedureColumns failed", hstmt);
	print_result_series(hstmt, sql_pro_column_ids, sizeof(sql_pro_column_ids) / sizeof(sql_pro_column_ids[0]), -1);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	/*
	 * Check for SQLProcedureColumns for a function
	 * 	returning table
	 */
	rc = SQLProcedureColumns(hstmt, NULL, 0,
				 (SQLCHAR *) "public", SQL_NTS,
				 (SQLCHAR *) "tbl_arg", SQL_NTS,
				 NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLProcedureColumns failed", hstmt);
	print_result_series(hstmt, sql_pro_column_ids, sizeof(sql_pro_column_ids) / sizeof(sql_pro_column_ids[0]), -1);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	/*
	 * Check for SQLProcedureColumns for a function with OUT
	 * 	parameters returning setof
	 *	This is equivalent to tbl_arg.
	 */
	rc = SQLProcedureColumns(hstmt, NULL, 0,
				 (SQLCHAR *) "public", SQL_NTS,
				 (SQLCHAR *) "set_of", SQL_NTS,
				 NULL, 0);
	CHECK_STMT_RESULT(rc, "SQLProcedureColumns failed", hstmt);
	print_result_series(hstmt, sql_pro_column_ids, sizeof(sql_pro_column_ids) / sizeof(sql_pro_column_ids[0]), -1);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Check for SQLTablePrivileges */
	printf("Check for SQLTablePrivileges\n");
	rc = SQLTablePrivileges(hstmt,
							NULL, 0,
							(SQLCHAR *) "public", 0,
							(SQLCHAR *) "testtab1", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLTablePrivileges failed", hstmt);
	print_result_meta(hstmt);
	print_result_series(hstmt, sql_tab_privileges_ids, 6, 5);
	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Test SQLGetInfo */
	printf("Check for SQLGetInfo\n");
	rc = SQLGetInfo(conn, SQL_TABLE_TERM, buf, sizeof(buf), &len);
	CHECK_STMT_RESULT(rc, "SQLGetInfo failed", hstmt);
	printf("Term for \"table\": %s\n", buf);

	/****
	 * Misc extra tests.
	 */

	/*
	 * Older versions of the driver had a bug in handling table-types lists
	 * longer than 32 entries. Check for that.
	 */
	rc = SQLTables(hstmt, "", SQL_NTS,
				   "public", SQL_NTS,
				   "testtab%", SQL_NTS,
				   "1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5, TABLES", SQL_NTS);

	CHECK_STMT_RESULT(rc, "SQLTables failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Older versions of the driver had a buffer overflow bug in handling table
	 * patterns with lots of escapes, with standard_conforming_strings=off.
	 * Check for that.
	 */
	for (i = 0; i < sizeof(buf) - 1; i++)
		buf[i] = '\\';
	buf[i] = '\0';
	rc = SQLTables(hstmt, "", SQL_NTS,
				   "public", SQL_NTS,
				   buf, SQL_NTS,
				   "TABLES", SQL_NTS);

	CHECK_STMT_RESULT(rc, "SQLTables failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/*
	 * Check that things work with an invalid search_path. current_schema()
	 * returns NULL in that case, which used to cause a segfault.
	 *
	 * Perform these tests in a fresh connection, so that we don't use a
	 * cached current_schema value.
	 */
	test_disconnect();
	test_connect();
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) "set search_path='bogus_schema'", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLGetInfo(conn, SQL_USER_NAME, username, sizeof(username), &len);
	CHECK_STMT_RESULT(rc, "SQLGetInfo failed", hstmt);

	rc = SQLTables(hstmt, "", SQL_NTS,
				   username, SQL_NTS,
				   "testtab%", SQL_NTS,
				   "", SQL_NTS);

	CHECK_STMT_RESULT(rc, "SQLTables failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Clean up */
	test_disconnect();

	return 0;
}
