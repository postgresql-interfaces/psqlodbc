/*-------
 * Module:			odbcapiw.c
 *
 * Description:		This module contains UNICODE routines
 *
 * Classes:			n/a
 *
 * API functions:	SQLColumnPrivilegesW, SQLColumnsW,
 			SQLConnectW, SQLDataSourcesW, SQLDescribeColW,
			SQLDriverConnectW, SQLExecDirectW,
			SQLForeignKeysW,
			SQLGetCursorNameW, SQLGetInfoW, SQLNativeSqlW,
			SQLPrepareW, SQLPrimaryKeysW, SQLProcedureColumnsW,
			SQLProceduresW, SQLSetCursorNameW,
			SQLSpecialColumnsW, SQLStatisticsW, SQLTablesW,
			SQLTablePrivilegesW, SQLGetTypeInfoW
 *-------
 */

#include "psqlodbc.h"
#include <stdio.h>
#include <string.h>

#include "pgapifunc.h"
#include "connection.h"
#include "statement.h"

RETCODE  SQL_API SQLColumnsW(HSTMT StatementHandle,
           SQLWCHAR *CatalogName, SQLSMALLINT NameLength1,
           SQLWCHAR *SchemaName, SQLSMALLINT NameLength2,
           SQLWCHAR *TableName, SQLSMALLINT NameLength3,
           SQLWCHAR *ColumnName, SQLSMALLINT NameLength4)
{
	RETCODE	ret;
	char	*ctName, *scName, *tbName, *clName;
	UInt4	nmlen1, nmlen2, nmlen3, nmlen4;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	ConnectionClass *conn;
	BOOL	lower_id;

	mylog("[SQLColumnsW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(CatalogName, NameLength1, &nmlen1, lower_id);
	scName = ucs2_to_utf8(SchemaName, NameLength2, &nmlen2, lower_id);
	tbName = ucs2_to_utf8(TableName, NameLength3, &nmlen3, lower_id);
	clName = ucs2_to_utf8(ColumnName, NameLength4, &nmlen4, lower_id);
	ENTER_STMT_CS(stmt);
	ret = PGAPI_Columns(StatementHandle, ctName, (SWORD) nmlen1,
           	scName, (SWORD) nmlen2, tbName, (SWORD) nmlen3,
           	clName, (SWORD) nmlen4, PODBC_SEARCH_PUBLIC_SCHEMA);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	if (clName)
		free(clName);
	return ret;
}


RETCODE  SQL_API SQLConnectW(HDBC ConnectionHandle,
           SQLWCHAR *ServerName, SQLSMALLINT NameLength1,
           SQLWCHAR *UserName, SQLSMALLINT NameLength2,
           SQLWCHAR *Authentication, SQLSMALLINT NameLength3)
{
	char	*svName, *usName, *auth;
	UInt4	nmlen1, nmlen2, nmlen3;
	RETCODE	ret;

	mylog("[SQLConnectW]");
	ENTER_CONN_CS((ConnectionClass *) ConnectionHandle);
	((ConnectionClass *) ConnectionHandle)->unicode = 1;
	svName = ucs2_to_utf8(ServerName, NameLength1, &nmlen1, FALSE);
	usName = ucs2_to_utf8(UserName, NameLength2, &nmlen2, FALSE);
	auth = ucs2_to_utf8(Authentication, NameLength3, &nmlen3, FALSE);
	ret = PGAPI_Connect(ConnectionHandle, svName, (SWORD) nmlen1,
           	usName, (SWORD) nmlen2, auth, (SWORD) nmlen3);
	LEAVE_CONN_CS((ConnectionClass *) ConnectionHandle);
	if (svName)
		free(svName);
	if (usName)
		free(usName);
	if (auth)
		free(auth);
	return ret;
}

RETCODE SQL_API SQLDriverConnectW(HDBC hdbc,
                                 HWND hwnd,
                                 SQLWCHAR *szConnStrIn,
                                 SWORD cbConnStrIn,
                                 SQLWCHAR *szConnStrOut,
                                 SWORD cbConnStrOutMax,
                                 SWORD FAR *pcbConnStrOut,
                                 UWORD fDriverCompletion)
{
	char	*szIn, *szOut;
	UInt4	inlen, obuflen;
	SWORD	olen;
	RETCODE	ret;

	mylog("[SQLDriverConnectW]");
	ENTER_CONN_CS((ConnectionClass *) hdbc);
	((ConnectionClass *) hdbc)->unicode = 1;
	szIn = ucs2_to_utf8(szConnStrIn, cbConnStrIn, &inlen, FALSE);
	obuflen = cbConnStrOutMax + 1;
	szOut = malloc(obuflen);
	ret = PGAPI_DriverConnect(hdbc, hwnd, szIn, (SWORD) inlen,
		szOut, cbConnStrOutMax, &olen, fDriverCompletion);
	LEAVE_CONN_CS((ConnectionClass *) hdbc);
	if (ret != SQL_ERROR)
	{
		UInt4 outlen = utf8_to_ucs2(szOut, olen, szConnStrOut, cbConnStrOutMax);
		if (pcbConnStrOut)
			*pcbConnStrOut = outlen;
	}
	free(szOut);
	if (szIn)
		free(szIn);
	return ret;
}
RETCODE SQL_API SQLBrowseConnectW(
    HDBC            hdbc,
    SQLWCHAR 		  *szConnStrIn,
    SQLSMALLINT        cbConnStrIn,
    SQLWCHAR 		  *szConnStrOut,
    SQLSMALLINT        cbConnStrOutMax,
    SQLSMALLINT       *pcbConnStrOut)
{
	char	*szIn, *szOut;
	UInt4	inlen, obuflen;
	SWORD	olen;
	RETCODE	ret;

	mylog("[SQLBrowseConnectW]");
	ENTER_CONN_CS((ConnectionClass *) hdbc);
	((ConnectionClass *) hdbc)->unicode = 1;
	szIn = ucs2_to_utf8(szConnStrIn, cbConnStrIn, &inlen, FALSE);
	obuflen = cbConnStrOutMax + 1;
	szOut = malloc(obuflen);
	ret = PGAPI_BrowseConnect(hdbc, szIn, (SWORD) inlen,
		szOut, cbConnStrOutMax, &olen);
	LEAVE_CONN_CS((ConnectionClass *) hdbc);
	if (ret != SQL_ERROR)
	{
		UInt4	outlen = utf8_to_ucs2(szOut, olen, szConnStrOut, cbConnStrOutMax);
		if (pcbConnStrOut)
			*pcbConnStrOut = outlen;
	}
	free(szOut);
	if (szIn)
		free(szIn);
	return ret;
}

RETCODE  SQL_API SQLDataSourcesW(HENV EnvironmentHandle,
           SQLUSMALLINT Direction, SQLWCHAR *ServerName,
           SQLSMALLINT BufferLength1, SQLSMALLINT *NameLength1,
           SQLWCHAR *Description, SQLSMALLINT BufferLength2,
           SQLSMALLINT *NameLength2)
{
	mylog("[SQLDataSourcesW]");
	/*
	return PGAPI_DataSources(EnvironmentHandle, Direction, ServerName,
		 BufferLength1, NameLength1, Description, BufferLength2,
           	NameLength2);
	*/
	return SQL_ERROR;
}

RETCODE  SQL_API SQLDescribeColW(HSTMT StatementHandle,
           SQLUSMALLINT ColumnNumber, SQLWCHAR *ColumnName,
           SQLSMALLINT BufferLength, SQLSMALLINT *NameLength,
           SQLSMALLINT *DataType, SQLUINTEGER *ColumnSize,
           SQLSMALLINT *DecimalDigits, SQLSMALLINT *Nullable)
{
	RETCODE	ret;
	SWORD	buflen, nmlen;
	char	*clName;

	mylog("[SQLDescribeColW]");
	buflen = BufferLength * 3 + 1;
	clName = malloc(buflen);
	ENTER_STMT_CS((StatementClass *) StatementHandle);
	ret = PGAPI_DescribeCol(StatementHandle, ColumnNumber,
		clName, buflen, &nmlen, DataType, ColumnSize,
		DecimalDigits, Nullable);
	if (ret == SQL_SUCCESS)
	{
		UInt4	nmcount = utf8_to_ucs2(clName, nmlen, ColumnName, BufferLength);
		if (nmcount > (UInt4) BufferLength)
		{
			StatementClass	*stmt = (StatementClass *) StatementHandle;
			ret = SQL_SUCCESS_WITH_INFO;
			SC_set_error(stmt, STMT_TRUNCATED, "Column name too large");
		}
		if (NameLength)
			*NameLength = nmcount;
	}
	LEAVE_STMT_CS((StatementClass *) StatementHandle);
	free(clName);
	return ret;
}

RETCODE  SQL_API SQLExecDirectW(HSTMT StatementHandle,
           SQLWCHAR *StatementText, SQLINTEGER TextLength)
{
	RETCODE	ret;
	char	*stxt;
	UInt4	slen;

	mylog("[SQLExecDirectW]");
	stxt = ucs2_to_utf8(StatementText, TextLength, &slen, FALSE);
	ENTER_STMT_CS((StatementClass *) StatementHandle);
	ret = PGAPI_ExecDirect(StatementHandle, stxt, slen, 0);
	LEAVE_STMT_CS((StatementClass *) StatementHandle);
	if (stxt)
		free(stxt);
	return ret;
}

RETCODE  SQL_API SQLGetCursorNameW(HSTMT StatementHandle,
           SQLWCHAR *CursorName, SQLSMALLINT BufferLength,
           SQLSMALLINT *NameLength)
{
	RETCODE	ret;
	char	*crName;
	SWORD	clen, buflen;

	mylog("[SQLGetCursorNameW]");
	buflen = BufferLength * 3 + 1;
	crName = malloc(buflen);
	ENTER_STMT_CS((StatementClass *) StatementHandle);
	ret = PGAPI_GetCursorName(StatementHandle, crName, buflen, &clen);
	if (ret == SQL_SUCCESS)
	{
		UInt4	nmcount = utf8_to_ucs2(crName, (Int4) clen, CursorName, BufferLength);
		if (nmcount > (UInt4) BufferLength)
		{
			StatementClass *stmt = (StatementClass *) StatementHandle;
			ret = SQL_SUCCESS_WITH_INFO;
			SC_set_error(stmt, STMT_TRUNCATED, "Cursor name too large");
		}
		if (NameLength)
			*NameLength = utf8_to_ucs2(crName, (Int4) clen, CursorName, BufferLength);
	}
	LEAVE_STMT_CS((StatementClass *) StatementHandle);
	free(crName);
	return ret;
}

RETCODE  SQL_API SQLGetInfoW(HDBC ConnectionHandle,
           SQLUSMALLINT InfoType, PTR InfoValue,
           SQLSMALLINT BufferLength, SQLSMALLINT *StringLength)
{
	ConnectionClass	*conn = (ConnectionClass *) ConnectionHandle;
	RETCODE	ret;

	ENTER_CONN_CS((ConnectionClass *) ConnectionHandle);
	conn->unicode = 1;
	CC_clear_error(conn);

	mylog("[SQLGetInfoW(30)]");
	if ((ret = PGAPI_GetInfo(ConnectionHandle, InfoType, InfoValue,
           	BufferLength, StringLength)) == SQL_ERROR)
	{
		if (conn->driver_version >= 0x0300)
		{
			CC_clear_error(conn);
			ret = PGAPI_GetInfo30(ConnectionHandle, InfoType, InfoValue,
           			BufferLength, StringLength);
		}
	}
	if (SQL_ERROR == ret)
		CC_log_error("SQLGetInfoW(30)", "", conn);

	LEAVE_CONN_CS((ConnectionClass *) ConnectionHandle);
	return ret;
}

RETCODE  SQL_API SQLPrepareW(HSTMT StatementHandle,
           SQLWCHAR *StatementText, SQLINTEGER TextLength)
{
	RETCODE	ret;
	char	*stxt;
	UInt4	slen;

	mylog("[SQLPrepareW]");
	stxt = ucs2_to_utf8(StatementText, TextLength, &slen, FALSE);
	ENTER_STMT_CS((StatementClass *) StatementHandle);
	ret = PGAPI_Prepare(StatementHandle, stxt, slen);
	LEAVE_STMT_CS((StatementClass *) StatementHandle);
	if (stxt)
		free(stxt);
	return ret;
}

RETCODE  SQL_API SQLSetCursorNameW(HSTMT StatementHandle,
           SQLWCHAR *CursorName, SQLSMALLINT NameLength)
{
	RETCODE	ret;
	char	*crName;
	UInt4	nlen;

	mylog("[SQLSetCursorNameW]");
	crName = ucs2_to_utf8(CursorName, NameLength, &nlen, FALSE);
	ENTER_STMT_CS((StatementClass *) StatementHandle);
	ret = PGAPI_SetCursorName(StatementHandle, crName, (SWORD) nlen);
	LEAVE_STMT_CS((StatementClass *) StatementHandle);
	if (crName)
		free(crName);
	return ret;
}

RETCODE  SQL_API SQLSpecialColumnsW(HSTMT StatementHandle,
           SQLUSMALLINT IdentifierType, SQLWCHAR *CatalogName,
           SQLSMALLINT NameLength1, SQLWCHAR *SchemaName,
           SQLSMALLINT NameLength2, SQLWCHAR *TableName,
           SQLSMALLINT NameLength3, SQLUSMALLINT Scope,
           SQLUSMALLINT Nullable)
{
	RETCODE	ret;
	char	*ctName, *scName, *tbName;
	UInt4	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	ConnectionClass *conn;
	BOOL lower_id;

	mylog("[SQLSpecialColumnsW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(CatalogName, NameLength1, &nmlen1, lower_id);
	scName = ucs2_to_utf8(SchemaName, NameLength2, &nmlen2, lower_id);
	tbName = ucs2_to_utf8(TableName, NameLength3, &nmlen3, lower_id);
	ENTER_STMT_CS(stmt);
	ret = PGAPI_SpecialColumns(StatementHandle, IdentifierType, ctName,
           (SWORD) nmlen1, scName, (SWORD) nmlen2, tbName, (SWORD) nmlen3,
		Scope, Nullable);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	return ret;
}

RETCODE  SQL_API SQLStatisticsW(HSTMT StatementHandle,
           SQLWCHAR *CatalogName, SQLSMALLINT NameLength1,
           SQLWCHAR *SchemaName, SQLSMALLINT NameLength2,
           SQLWCHAR *TableName, SQLSMALLINT NameLength3,
           SQLUSMALLINT Unique, SQLUSMALLINT Reserved)
{
	RETCODE	ret;
	char	*ctName, *scName, *tbName;
	UInt4	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	ConnectionClass *conn;
	BOOL lower_id;

	mylog("[SQLStatisticsW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(CatalogName, NameLength1, &nmlen1, lower_id);
	scName = ucs2_to_utf8(SchemaName, NameLength2, &nmlen2, lower_id);
	tbName = ucs2_to_utf8(TableName, NameLength3, &nmlen3, lower_id);
	ENTER_STMT_CS(stmt);
	ret = PGAPI_Statistics(StatementHandle, ctName, (SWORD) nmlen1,
           scName, (SWORD) nmlen2, tbName, (SWORD) nmlen3, Unique,
		Reserved);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	return ret;
}

RETCODE  SQL_API SQLTablesW(HSTMT StatementHandle,
           SQLWCHAR *CatalogName, SQLSMALLINT NameLength1,
           SQLWCHAR *SchemaName, SQLSMALLINT NameLength2,
           SQLWCHAR *TableName, SQLSMALLINT NameLength3,
           SQLWCHAR *TableType, SQLSMALLINT NameLength4)
{
	RETCODE	ret;
	char	*ctName, *scName, *tbName, *tbType;
	UInt4	nmlen1, nmlen2, nmlen3, nmlen4;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	ConnectionClass *conn;
	BOOL lower_id;

	mylog("[SQLTablesW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(CatalogName, NameLength1, &nmlen1, lower_id);
	scName = ucs2_to_utf8(SchemaName, NameLength2, &nmlen2, lower_id);
	tbName = ucs2_to_utf8(TableName, NameLength3, &nmlen3, lower_id);
	tbType = ucs2_to_utf8(TableType, NameLength4, &nmlen4, FALSE);
	ENTER_STMT_CS(stmt);
	ret = PGAPI_Tables(StatementHandle, ctName, (SWORD) nmlen1,
           scName, (SWORD) nmlen2, tbName, (SWORD) nmlen3,
           tbType, (SWORD) nmlen4);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	if (tbType)
		free(tbType);
	return ret;
}

RETCODE SQL_API SQLColumnPrivilegesW(
    HSTMT           hstmt,
    SQLWCHAR 		  *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLWCHAR 		  *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLWCHAR 		  *szTableName,
    SQLSMALLINT        cbTableName,
    SQLWCHAR 		  *szColumnName,
    SQLSMALLINT        cbColumnName)
{
	RETCODE	ret;
	char	*ctName, *scName, *tbName, *clName;
	UInt4	nmlen1, nmlen2, nmlen3, nmlen4;
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	BOOL	lower_id;

	mylog("[SQLColumnPrivilegesW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, lower_id);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, lower_id);
	tbName = ucs2_to_utf8(szTableName, cbTableName, &nmlen3, lower_id);
	clName = ucs2_to_utf8(szColumnName, cbColumnName, &nmlen4, lower_id);
	ENTER_STMT_CS(stmt);
	ret = PGAPI_ColumnPrivileges(hstmt, ctName, (SWORD) nmlen1,
		scName, (SWORD) nmlen2, tbName, (SWORD) nmlen3,
		clName, (SWORD) nmlen4);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	if (clName)
		free(clName);
	return ret;
}

RETCODE SQL_API SQLForeignKeysW(
    HSTMT           hstmt,
    SQLWCHAR 		  *szPkCatalogName,
    SQLSMALLINT        cbPkCatalogName,
    SQLWCHAR 		  *szPkSchemaName,
    SQLSMALLINT        cbPkSchemaName,
    SQLWCHAR 		  *szPkTableName,
    SQLSMALLINT        cbPkTableName,
    SQLWCHAR 		  *szFkCatalogName,
    SQLSMALLINT        cbFkCatalogName,
    SQLWCHAR 		  *szFkSchemaName,
    SQLSMALLINT        cbFkSchemaName,
    SQLWCHAR 		  *szFkTableName,
    SQLSMALLINT        cbFkTableName)
{
	RETCODE	ret;
	char	*ctName, *scName, *tbName, *fkctName, *fkscName, *fktbName;
	UInt4	nmlen1, nmlen2, nmlen3, nmlen4, nmlen5, nmlen6;
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	BOOL	lower_id;

	mylog("[SQLForeignKeysW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(szPkCatalogName, cbPkCatalogName, &nmlen1, lower_id);
	scName = ucs2_to_utf8(szPkSchemaName, cbPkSchemaName, &nmlen2, lower_id);
	tbName = ucs2_to_utf8(szPkTableName, cbPkTableName, &nmlen3, lower_id);
	fkctName = ucs2_to_utf8(szFkCatalogName, cbFkCatalogName, &nmlen4, lower_id);
	fkscName = ucs2_to_utf8(szFkSchemaName, cbFkSchemaName, &nmlen5, lower_id);
	fktbName = ucs2_to_utf8(szFkTableName, cbFkTableName, &nmlen6, lower_id);
	ENTER_STMT_CS(stmt);
	ret = PGAPI_ForeignKeys(hstmt, ctName, (SWORD) nmlen1,
		scName, (SWORD) nmlen2, tbName, (SWORD) nmlen3,
		fkctName, (SWORD) nmlen4, fkscName, (SWORD) nmlen5,
		fktbName, (SWORD) nmlen6);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	if (fkctName)
		free(fkctName);
	if (fkscName)
		free(fkscName);
	if (fktbName)
		free(fktbName);
	return ret;
}

RETCODE SQL_API SQLNativeSqlW(
    HDBC            hdbc,
    SQLWCHAR 		  *szSqlStrIn,
    SQLINTEGER         cbSqlStrIn,
    SQLWCHAR 		  *szSqlStr,
    SQLINTEGER         cbSqlStrMax,
    SQLINTEGER 		  *pcbSqlStr)
{
	RETCODE		ret;
	char		*szIn, *szOut;
	UInt4		slen;
	SQLINTEGER	buflen, olen;

	mylog("[SQLNativeSqlW]");
	ENTER_CONN_CS((ConnectionClass *) hdbc);
	((ConnectionClass *) hdbc)->unicode = 1;
	szIn = ucs2_to_utf8(szSqlStrIn, cbSqlStrIn, &slen, FALSE);
	buflen = 3 * cbSqlStrMax + 1;
	szOut = malloc(buflen);
	ret = PGAPI_NativeSql(hdbc, szIn, (SQLINTEGER) slen,
		szOut, buflen, &olen);
	if (szIn)
		free(szIn);
	if (ret == SQL_SUCCESS)
	{
		UInt4	szcount = utf8_to_ucs2(szOut, olen, szSqlStr, cbSqlStrMax);
		if (szcount > (UInt4) cbSqlStrMax)
		{
			ConnectionClass	*conn = (ConnectionClass *) hdbc;

			ret = SQL_SUCCESS_WITH_INFO;
			CC_set_error(conn, CONN_TRUNCATED, "Sql string too large");
		}
		if (pcbSqlStr)
			*pcbSqlStr = szcount;
	}
	LEAVE_CONN_CS((ConnectionClass *) hdbc);
	free(szOut);
	return ret;
}

RETCODE SQL_API SQLPrimaryKeysW(
    HSTMT           hstmt,
    SQLWCHAR 		  *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLWCHAR 		  *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLWCHAR 		  *szTableName,
    SQLSMALLINT        cbTableName)
{
	RETCODE	ret;
	char	*ctName, *scName, *tbName;
	UInt4	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	BOOL	lower_id;

	mylog("[SQLPrimaryKeysW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, lower_id);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, lower_id);
	tbName = ucs2_to_utf8(szTableName, cbTableName, &nmlen3, lower_id);
	ENTER_STMT_CS(stmt);
	ret = PGAPI_PrimaryKeys(hstmt, ctName, (SWORD) nmlen1,
		scName, (SWORD) nmlen2, tbName, (SWORD) nmlen3);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	return ret;
}

RETCODE SQL_API SQLProcedureColumnsW(
    HSTMT           hstmt,
    SQLWCHAR 		  *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLWCHAR 		  *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLWCHAR 		  *szProcName,
    SQLSMALLINT        cbProcName,
    SQLWCHAR 		  *szColumnName,
    SQLSMALLINT        cbColumnName)
{
	RETCODE	ret;
	char	*ctName, *scName, *prName, *clName;
	UInt4	nmlen1, nmlen2, nmlen3, nmlen4;
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	BOOL	lower_id;

	mylog("[SQLProcedureColumnsW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, lower_id);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, lower_id);
	prName = ucs2_to_utf8(szProcName, cbProcName, &nmlen3, lower_id);
	clName = ucs2_to_utf8(szColumnName, cbColumnName, &nmlen4, lower_id);
	ENTER_STMT_CS(stmt);
	ret = PGAPI_ProcedureColumns(hstmt, ctName, (SWORD) nmlen1,
		scName, (SWORD) nmlen2, prName, (SWORD) nmlen3,
		clName, (SWORD) nmlen4);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (prName)
		free(prName);
	if (clName)
		free(clName);
	return ret;
}

RETCODE SQL_API SQLProceduresW(
    HSTMT           hstmt,
    SQLWCHAR 		  *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLWCHAR 		  *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLWCHAR 		  *szProcName,
    SQLSMALLINT        cbProcName)
{
	RETCODE	ret;
	char	*ctName, *scName, *prName;
	UInt4	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	BOOL	lower_id;

	mylog("[SQLProceduresW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, lower_id);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, lower_id);
	prName = ucs2_to_utf8(szProcName, cbProcName, &nmlen3, lower_id);
	ENTER_STMT_CS(stmt);
	ret = PGAPI_Procedures(hstmt, ctName, (SWORD) nmlen1,
		scName, (SWORD) nmlen2, prName, (SWORD) nmlen3);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (prName)
		free(prName);
	return ret;
}

RETCODE SQL_API SQLTablePrivilegesW(
    HSTMT           hstmt,
    SQLWCHAR 		  *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLWCHAR 		  *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLWCHAR 		  *szTableName,
    SQLSMALLINT        cbTableName)
{
	RETCODE	ret;
	char	*ctName, *scName, *tbName;
	UInt4	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	BOOL	lower_id;

	mylog("[SQLTablePrivilegesW]");
	conn = SC_get_conn(stmt);
	lower_id = SC_is_lower_case(stmt, conn);
	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, lower_id);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, lower_id);
	tbName = ucs2_to_utf8(szTableName, cbTableName, &nmlen3, lower_id);
	ENTER_STMT_CS((StatementClass *) hstmt);
	ret = PGAPI_TablePrivileges(hstmt, ctName, (SWORD) nmlen1,
		scName, (SWORD) nmlen2, tbName, (SWORD) nmlen3, 0);
	LEAVE_STMT_CS((StatementClass *) hstmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	return ret;
}

RETCODE SQL_API	SQLGetTypeInfoW(
		SQLHSTMT	StatementHandle,
		SQLSMALLINT	DataType)
{
	RETCODE	ret;

	mylog("[SQLGetInfoW]");
	ENTER_STMT_CS((StatementClass *) StatementHandle);
	ret = PGAPI_GetTypeInfo(StatementHandle, DataType);
	LEAVE_STMT_CS((StatementClass *) StatementHandle);
	return ret;
}
