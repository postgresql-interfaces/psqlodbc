/*
 * An SQL_C_DEFAULT binding is sized for the C type the column had when it
 * was described. If another connection changes the column's type before
 * the statement runs, the fetch must fail rather than store the new C
 * type. With BufferLength 0, which ODBC allows for fixed-length C types,
 * the driver cannot use the buffer length to notice the mismatch.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;
	SQLHDBC		conn2 = SQL_NULL_HDBC;
	HSTMT		hstmt2 = SQL_NULL_HSTMT;
	SQLCHAR		dsn[1024];
	SQLSMALLINT	sqltype;
	SQLLEN		ind = 0;
	size_t		i;
	int			touched = 0;
	struct
	{
		SQLINTEGER	value;
		unsigned char guard[8];
	}			out;

	/*
	 * With server-side prepare the server refuses the changed result type
	 * at execute itself, so the driver's check is only reached without it.
	 */
	test_connect_ext("UseServerSidePrepare=0");

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn);

	/* A real table, because the second connection must see it. */
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"DROP TABLE IF EXISTS default_ctype_altered", SQL_NTS);
	CHECK_STMT_RESULT(rc, "DROP TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"CREATE TABLE default_ctype_altered (c int4)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "CREATE TABLE failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"INSERT INTO default_ctype_altered VALUES (1)", SQL_NTS);
	CHECK_STMT_RESULT(rc, "INSERT failed", hstmt);

	/* 1. Describe the column. */
	rc = SQLPrepare(hstmt, (SQLCHAR *)
		"SELECT c FROM default_ctype_altered", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);
	rc = SQLDescribeCol(hstmt, 1, NULL, 0, NULL, &sqltype, NULL, NULL,
						NULL);
	CHECK_STMT_RESULT(rc, "SQLDescribeCol failed", hstmt);
	printf("described as %s\n", datatype_str(sqltype));

	/* 2. Bind it with SQL_C_DEFAULT and BufferLength 0. */
	memset(&out, 0x5a, sizeof(out));
	rc = SQLBindCol(hstmt, 1, SQL_C_DEFAULT, &out.value, 0, &ind);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	/* 3. Change the column's type from a second connection. */
	snprintf((char *) dsn, sizeof(dsn), "DSN=%s", get_test_dsn());
	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn2);
	CHECK_CONN_RESULT(rc, "failed to allocate second connection", conn2);
	rc = SQLDriverConnect(conn2, NULL, dsn, SQL_NTS, NULL, 0, NULL,
						  SQL_DRIVER_NOPROMPT);
	CHECK_CONN_RESULT(rc, "second SQLDriverConnect failed", conn2);
	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn2, &hstmt2);
	CHECK_CONN_RESULT(rc, "failed to allocate stmt handle", conn2);
	rc = SQLExecDirect(hstmt2, (SQLCHAR *)
		"ALTER TABLE default_ctype_altered ALTER COLUMN c TYPE int8",
		SQL_NTS);
	CHECK_STMT_RESULT(rc, "ALTER TABLE failed", hstmt2);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
	SQLDisconnect(conn2);
	SQLFreeHandle(SQL_HANDLE_DBC, conn2);

	/* 4. Execute and fetch. */
	rc = SQLExecute(hstmt);
	CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);
	rc = SQLFetch(hstmt);

	for (i = 0; i < sizeof(out.guard); i++)
		if (out.guard[i] != 0x5a)
			touched = 1;
	if (SQL_SUCCEEDED(rc))
		printf("fetch succeeded");
	else
	{
		SQLCHAR		state[6] = "";
		SQLINTEGER	native;
		SQLCHAR		msg[256];
		SQLSMALLINT	len;

		SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, state, &native,
					  msg, sizeof(msg), &len);
		printf("fetch failed, SQLSTATE=%s", (char *) state);
	}
	printf(", bytes after the SQLINTEGER: %s\n",
		   touched ? "overwritten" : "untouched");

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLFreeStmt(hstmt, SQL_UNBIND);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
	rc = SQLExecDirect(hstmt, (SQLCHAR *)
		"DROP TABLE default_ctype_altered", SQL_NTS);
	CHECK_STMT_RESULT(rc, "DROP TABLE failed", hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

	test_disconnect();
	return 0;
}
