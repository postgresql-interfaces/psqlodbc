/*-------
 * Module:			odbcapi.c
 *
 * Description:		This module contains routines related to
 *					preparing and executing an SQL statement.
 *
 * Classes:			n/a
 *
 * API functions:	SQLAllocConnect, SQLAllocEnv, SQLAllocStmt,
			SQLBindCol, SQLCancel, SQLColumns, SQLConnect,
			SQLDataSources, SQLDescribeCol, SQLDisconnect,
			SQLError, SQLExecDirect, SQLExecute, SQLFetch,
			SQLFreeConnect, SQLFreeEnv, SQLFreeStmt,
			SQLGetConnectOption, SQLGetCursorName, SQLGetData,
			SQLGetFunctions, SQLGetInfo, SQLGetStmtOption,
			SQLGetTypeInfo, SQLNumResultCols, SQLParamData,
			SQLPrepare, SQLPutData, SQLRowCount,
			SQLSetConnectOption, SQLSetCursorName, SQLSetParam,
			SQLSetStmtOption, SQLSpecialColumns, SQLStatistics,
			SQLTables, SQLTransact, SQLColAttributes,
			SQLColumnPrivileges, SQLDescribeParam, SQLExtendedFetch,
			SQLForeignKeys, SQLMoreResults, SQLNativeSql,
			SQLNumParams, SQLParamOptions, SQLPrimaryKeys,
			SQLProcedureColumns, SQLProcedures, SQLSetPos,
			SQLTablePrivileges, SQLBindParameter
 *-------
 */

#include "psqlodbc.h"
#include <stdio.h>
#include <string.h>

#include "pgapifunc.h"
#include "environ.h"
#include "connection.h"
#include "statement.h"

#if (ODBCVER < 0x0300)
RETCODE		SQL_API
SQLAllocConnect(HENV EnvironmentHandle,
				HDBC FAR * ConnectionHandle)
{
	RETCODE	ret;
	EnvironmentClass *env = (EnvironmentClass *) EnvironmentHandle;

	mylog("[SQLAllocConnect]");
	ENTER_ENV_CS(env);
	ret = PGAPI_AllocConnect(EnvironmentHandle, ConnectionHandle);
	LEAVE_ENV_CS(env);
	return ret;
}

RETCODE		SQL_API
SQLAllocEnv(HENV FAR * EnvironmentHandle)
{
	RETCODE	ret;

	mylog("[SQLAllocEnv]");
	ret = PGAPI_AllocEnv(EnvironmentHandle);
	return ret;
}

RETCODE		SQL_API
SQLAllocStmt(HDBC ConnectionHandle,
			 HSTMT *StatementHandle)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;

	mylog("[SQLAllocStmt]");
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_AllocStmt(ConnectionHandle, StatementHandle);
	LEAVE_CONN_CS(conn);
	return ret;
}
#endif /* ODBCVER */

RETCODE		SQL_API
SQLBindCol(HSTMT StatementHandle,
		   SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType,
		   PTR TargetValue, SQLINTEGER BufferLength,
		   SQLINTEGER *StrLen_or_Ind)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLBindCol]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_BindCol(StatementHandle, ColumnNumber,
				   TargetType, TargetValue, BufferLength, StrLen_or_Ind);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLCancel(HSTMT StatementHandle)
{
	mylog("[SQLCancel]");
	SC_clear_error((StatementClass *) StatementHandle);
	return PGAPI_Cancel(StatementHandle);
}

RETCODE		SQL_API
SQLColumns(HSTMT StatementHandle,
		   SQLCHAR *CatalogName, SQLSMALLINT NameLength1,
		   SQLCHAR *SchemaName, SQLSMALLINT NameLength2,
		   SQLCHAR *TableName, SQLSMALLINT NameLength3,
		   SQLCHAR *ColumnName, SQLSMALLINT NameLength4)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLColumns]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_Columns(StatementHandle, CatalogName, NameLength1,
					SchemaName, NameLength2, TableName, NameLength3,
					ColumnName, NameLength4, 0);
	LEAVE_STMT_CS(stmt);
	return ret;
}


RETCODE		SQL_API
SQLConnect(HDBC ConnectionHandle,
		   SQLCHAR *ServerName, SQLSMALLINT NameLength1,
		   SQLCHAR *UserName, SQLSMALLINT NameLength2,
		   SQLCHAR *Authentication, SQLSMALLINT NameLength3)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;

	mylog("[SQLConnect]");
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_Connect(ConnectionHandle, ServerName, NameLength1,
					 UserName, NameLength2, Authentication, NameLength3);
	LEAVE_CONN_CS(conn);
	return ret;
}

RETCODE		SQL_API
SQLDriverConnect(HDBC hdbc,
				 HWND hwnd,
				 UCHAR FAR * szConnStrIn,
				 SWORD cbConnStrIn,
				 UCHAR FAR * szConnStrOut,
				 SWORD cbConnStrOutMax,
				 SWORD FAR * pcbConnStrOut,
				 UWORD fDriverCompletion)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	mylog("[SQLDriverConnect]");
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_DriverConnect(hdbc, hwnd, szConnStrIn, cbConnStrIn,
		szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
	LEAVE_CONN_CS(conn);
	return ret;
}
RETCODE		SQL_API
SQLBrowseConnect(
				 HDBC hdbc,
				 SQLCHAR *szConnStrIn,
				 SQLSMALLINT cbConnStrIn,
				 SQLCHAR *szConnStrOut,
				 SQLSMALLINT cbConnStrOutMax,
				 SQLSMALLINT *pcbConnStrOut)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	mylog("[SQLBrowseConnect]");
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_BrowseConnect(hdbc, szConnStrIn, cbConnStrIn,
						   szConnStrOut, cbConnStrOutMax, pcbConnStrOut);
	LEAVE_CONN_CS(conn);
	return ret;
}

RETCODE		SQL_API
SQLDataSources(HENV EnvironmentHandle,
			   SQLUSMALLINT Direction, SQLCHAR *ServerName,
			   SQLSMALLINT BufferLength1, SQLSMALLINT *NameLength1,
			   SQLCHAR *Description, SQLSMALLINT BufferLength2,
			   SQLSMALLINT *NameLength2)
{
	mylog("[SQLDataSources]");

	/*
	 * return PGAPI_DataSources(EnvironmentHandle, Direction, ServerName,
	 * BufferLength1, NameLength1, Description, BufferLength2,
	 * NameLength2);
	 */
	return SQL_ERROR;
}

RETCODE		SQL_API
SQLDescribeCol(HSTMT StatementHandle,
			   SQLUSMALLINT ColumnNumber, SQLCHAR *ColumnName,
			   SQLSMALLINT BufferLength, SQLSMALLINT *NameLength,
			   SQLSMALLINT *DataType, SQLUINTEGER *ColumnSize,
			   SQLSMALLINT *DecimalDigits, SQLSMALLINT *Nullable)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLDescribeCol]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_DescribeCol(StatementHandle, ColumnNumber,
							 ColumnName, BufferLength, NameLength,
						  DataType, ColumnSize, DecimalDigits, Nullable);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLDisconnect(HDBC ConnectionHandle)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;

	mylog("[SQLDisconnect]");
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_Disconnect(ConnectionHandle);
	LEAVE_CONN_CS(conn);
	return ret;
}

#if (ODBCVER < 0x0300)
RETCODE		SQL_API
SQLError(HENV EnvironmentHandle,
		 HDBC ConnectionHandle, HSTMT StatementHandle,
		 SQLCHAR *Sqlstate, SQLINTEGER *NativeError,
		 SQLCHAR *MessageText, SQLSMALLINT BufferLength,
		 SQLSMALLINT *TextLength)
{
	RETCODE	ret;

	mylog("[SQLError]");
	if (NULL != EnvironmentHandle)
		ENTER_ENV_CS((EnvironmentClass *) EnvironmentHandle);
	ret = PGAPI_Error(EnvironmentHandle, ConnectionHandle, StatementHandle,
		   Sqlstate, NativeError, MessageText, BufferLength,
		TextLength);
	if (NULL != EnvironmentHandle)
		LEAVE_ENV_CS((EnvironmentClass *) EnvironmentHandle);
	return ret;
}
#endif /* ODBCVER */

RETCODE		SQL_API
SQLExecDirect(HSTMT StatementHandle,
			  SQLCHAR *StatementText, SQLINTEGER TextLength)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLExecDirect]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_ExecDirect(StatementHandle, StatementText, TextLength);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLExecute(HSTMT StatementHandle)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLExecute]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_Execute(StatementHandle);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLFetch(HSTMT StatementHandle)
{
	static char *func = "SQLFetch";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
#if (ODBCVER >= 0x0300)
	if (SC_get_conn(stmt)->driver_version >= 0x0300)
	{
		IRDFields	*irdopts = SC_get_IRD(stmt);
		ARDFields	*ardopts = SC_get_ARD(stmt);
		SQLUSMALLINT *rowStatusArray = irdopts->rowStatusArray;
		SQLINTEGER *pcRow = irdopts->rowsFetched;

		mylog("[[%s]]", func);
		ret = PGAPI_ExtendedFetch(StatementHandle, SQL_FETCH_NEXT, 0,
								   pcRow, rowStatusArray, 0, ardopts->size_of_rowset);
		stmt->transition_status = 6;
	}
	else
#endif
	{
		mylog("[%s]", func);
		ret = PGAPI_Fetch(StatementHandle);
	}
	LEAVE_STMT_CS(stmt);
	return ret;
}

#if (ODBCVER < 0x0300) 
RETCODE		SQL_API
SQLFreeConnect(HDBC ConnectionHandle)
{
	RETCODE	ret;

	mylog("[SQLFreeConnect]");
	ret = PGAPI_FreeConnect(ConnectionHandle);
	return ret;
}

RETCODE		SQL_API
SQLFreeEnv(HENV EnvironmentHandle)
{
	RETCODE	ret;

	mylog("[SQLFreeEnv]");
	ret = PGAPI_FreeEnv(EnvironmentHandle);
	return ret;
}
#endif /* ODBCVER */ 

RETCODE		SQL_API
SQLFreeStmt(HSTMT StatementHandle,
			SQLUSMALLINT Option)
{
	RETCODE	ret;

	mylog("[SQLFreeStmt]");
	ret = PGAPI_FreeStmt(StatementHandle, Option);
	return ret;
}

#if (ODBCVER < 0x0300)
RETCODE		SQL_API
SQLGetConnectOption(HDBC ConnectionHandle,
					SQLUSMALLINT Option, PTR Value)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;

	mylog("[SQLGetConnectOption]");
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_GetConnectOption(ConnectionHandle, Option, Value);
	LEAVE_CONN_CS(conn);
	return ret;
}
#endif /* ODBCVER */

RETCODE		SQL_API
SQLGetCursorName(HSTMT StatementHandle,
				 SQLCHAR *CursorName, SQLSMALLINT BufferLength,
				 SQLSMALLINT *NameLength)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLGetCursorName]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_GetCursorName(StatementHandle, CursorName, BufferLength,
							   NameLength);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLGetData(HSTMT StatementHandle,
		   SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType,
		   PTR TargetValue, SQLINTEGER BufferLength,
		   SQLINTEGER *StrLen_or_Ind)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLGetData]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_GetData(StatementHandle, ColumnNumber, TargetType,
						 TargetValue, BufferLength, StrLen_or_Ind);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLGetFunctions(HDBC ConnectionHandle,
				SQLUSMALLINT FunctionId, SQLUSMALLINT *Supported)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;

	mylog("[SQLGetFunctions]");
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
#if (ODBCVER >= 0x0300)
	if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS)
		ret = PGAPI_GetFunctions30(ConnectionHandle, FunctionId, Supported);
	else
#endif
	{
		ret = PGAPI_GetFunctions(ConnectionHandle, FunctionId, Supported);
	}
	LEAVE_CONN_CS(conn);
	return ret;
}
RETCODE		SQL_API
SQLGetInfo(HDBC ConnectionHandle,
		   SQLUSMALLINT InfoType, PTR InfoValue,
		   SQLSMALLINT BufferLength, SQLSMALLINT *StringLength)
{
	RETCODE		ret;
	ConnectionClass	*conn = (ConnectionClass *) ConnectionHandle;

	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
#if (ODBCVER >= 0x0300)
	mylog("[SQLGetInfo(30)]");
	if ((ret = PGAPI_GetInfo(ConnectionHandle, InfoType, InfoValue,
							 BufferLength, StringLength)) == SQL_ERROR)
	{
		if (((ConnectionClass *) ConnectionHandle)->driver_version >= 0x0300)
		{
			CC_clear_error(conn);
			ret = PGAPI_GetInfo30(ConnectionHandle, InfoType, InfoValue,
								   BufferLength, StringLength);
		}
	}
	if (SQL_ERROR == ret)
		CC_log_error("SQLGetInfo30", "", conn);
#else
	mylog("[SQLGetInfo]");
	if (ret = PGAPI_GetInfo(ConnectionHandle, InfoType, InfoValue,
			BufferLength, StringLength), SQL_ERROR == ret)
		CC_log_error("PGAPI_GetInfo", "", conn);
#endif
	LEAVE_CONN_CS(conn);
	return ret;
}

#if (ODBCVER < 0x0300)
RETCODE		SQL_API
SQLGetStmtOption(HSTMT StatementHandle,
				 SQLUSMALLINT Option, PTR Value)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLGetStmtOption]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_GetStmtOption(StatementHandle, Option, Value);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* ODBCVER */

RETCODE		SQL_API
SQLGetTypeInfo(HSTMT StatementHandle,
			   SQLSMALLINT DataType)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLGetTypeInfo]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_GetTypeInfo(StatementHandle, DataType);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLNumResultCols(HSTMT StatementHandle,
				 SQLSMALLINT *ColumnCount)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLNumResultCols]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_NumResultCols(StatementHandle, ColumnCount);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLParamData(HSTMT StatementHandle,
			 PTR *Value)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLParamData]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_ParamData(StatementHandle, Value);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLPrepare(HSTMT StatementHandle,
		   SQLCHAR *StatementText, SQLINTEGER TextLength)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLPrepare]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_Prepare(StatementHandle, StatementText, TextLength);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLPutData(HSTMT StatementHandle,
		   PTR Data, SQLINTEGER StrLen_or_Ind)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLPutData]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_PutData(StatementHandle, Data, StrLen_or_Ind);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLRowCount(HSTMT StatementHandle,
			SQLINTEGER *RowCount)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLRowCount]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_RowCount(StatementHandle, RowCount);
	LEAVE_STMT_CS(stmt);
	return ret;
}

#if (ODBCVER < 0x0300)
RETCODE		SQL_API
SQLSetConnectOption(HDBC ConnectionHandle,
					SQLUSMALLINT Option, SQLUINTEGER Value)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;

	mylog("[SQLSetConnectionOption]");
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_SetConnectOption(ConnectionHandle, Option, Value);
	LEAVE_CONN_CS(conn);
	return ret;
}
#endif /* ODBCVER */

RETCODE		SQL_API
SQLSetCursorName(HSTMT StatementHandle,
				 SQLCHAR *CursorName, SQLSMALLINT NameLength)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLSetCursorName]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_SetCursorName(StatementHandle, CursorName, NameLength);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLSetParam(HSTMT StatementHandle,
			SQLUSMALLINT ParameterNumber, SQLSMALLINT ValueType,
			SQLSMALLINT ParameterType, SQLUINTEGER LengthPrecision,
			SQLSMALLINT ParameterScale, PTR ParameterValue,
			SQLINTEGER *StrLen_or_Ind)
{
	mylog("[SQLSetParam]");
	SC_clear_error((StatementClass *) StatementHandle);

	/*
	 * return PGAPI_SetParam(StatementHandle, ParameterNumber, ValueType,
	 * ParameterType, LengthPrecision, ParameterScale, ParameterValue,
	 * StrLen_or_Ind);
	 */
	return SQL_ERROR;
}

#if (ODBCVER < 0x0300)
RETCODE		SQL_API
SQLSetStmtOption(HSTMT StatementHandle,
				 SQLUSMALLINT Option, SQLUINTEGER Value)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLSetStmtOption]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_SetStmtOption(StatementHandle, Option, Value);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* ODBCVER */

RETCODE		SQL_API
SQLSpecialColumns(HSTMT StatementHandle,
				  SQLUSMALLINT IdentifierType, SQLCHAR *CatalogName,
				  SQLSMALLINT NameLength1, SQLCHAR *SchemaName,
				  SQLSMALLINT NameLength2, SQLCHAR *TableName,
				  SQLSMALLINT NameLength3, SQLUSMALLINT Scope,
				  SQLUSMALLINT Nullable)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLSpecialColumns]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_SpecialColumns(StatementHandle, IdentifierType, CatalogName,
			NameLength1, SchemaName, NameLength2, TableName, NameLength3,
								Scope, Nullable);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLStatistics(HSTMT StatementHandle,
			  SQLCHAR *CatalogName, SQLSMALLINT NameLength1,
			  SQLCHAR *SchemaName, SQLSMALLINT NameLength2,
			  SQLCHAR *TableName, SQLSMALLINT NameLength3,
			  SQLUSMALLINT Unique, SQLUSMALLINT Reserved)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLStatistics]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_Statistics(StatementHandle, CatalogName, NameLength1,
				 SchemaName, NameLength2, TableName, NameLength3, Unique,
							Reserved);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLTables(HSTMT StatementHandle,
		  SQLCHAR *CatalogName, SQLSMALLINT NameLength1,
		  SQLCHAR *SchemaName, SQLSMALLINT NameLength2,
		  SQLCHAR *TableName, SQLSMALLINT NameLength3,
		  SQLCHAR *TableType, SQLSMALLINT NameLength4)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLTables]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_Tables(StatementHandle, CatalogName, NameLength1,
						SchemaName, NameLength2, TableName, NameLength3,
						TableType, NameLength4);
	LEAVE_STMT_CS(stmt);
	return ret;
}

#if (ODBCVER < 0x0300)
RETCODE		SQL_API
SQLTransact(HENV EnvironmentHandle,
			HDBC ConnectionHandle, SQLUSMALLINT CompletionType)
{
	RETCODE	ret;

	mylog("[SQLTransact]");
	if (NULL != EnvironmentHandle)
		ENTER_ENV_CS((EnvironmentClass *) EnvironmentHandle);
	else
		ENTER_CONN_CS((ConnectionClass *) ConnectionHandle);
	ret = PGAPI_Transact(EnvironmentHandle, ConnectionHandle, CompletionType);
	if (NULL != EnvironmentHandle)
		LEAVE_ENV_CS((EnvironmentClass *) EnvironmentHandle);
	else
		LEAVE_CONN_CS((ConnectionClass *) ConnectionHandle);
	return ret;
}

RETCODE		SQL_API
SQLColAttributes(
				 HSTMT hstmt,
				 SQLUSMALLINT icol,
				 SQLUSMALLINT fDescType,
				 PTR rgbDesc,
				 SQLSMALLINT cbDescMax,
				 SQLSMALLINT *pcbDesc,
				 SQLINTEGER *pfDesc)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLColAttributes]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_ColAttributes(hstmt, icol, fDescType, rgbDesc,
							   cbDescMax, pcbDesc, pfDesc);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* ODBCVER */

RETCODE		SQL_API
SQLColumnPrivileges(
					HSTMT hstmt,
					SQLCHAR *szCatalogName,
					SQLSMALLINT cbCatalogName,
					SQLCHAR *szSchemaName,
					SQLSMALLINT cbSchemaName,
					SQLCHAR *szTableName,
					SQLSMALLINT cbTableName,
					SQLCHAR *szColumnName,
					SQLSMALLINT cbColumnName)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLColumnPrivileges]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_ColumnPrivileges(hstmt, szCatalogName, cbCatalogName,
					szSchemaName, cbSchemaName, szTableName, cbTableName,
								  szColumnName, cbColumnName);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLDescribeParam(
				 HSTMT hstmt,
				 SQLUSMALLINT ipar,
				 SQLSMALLINT *pfSqlType,
				 SQLUINTEGER *pcbParamDef,
				 SQLSMALLINT *pibScale,
				 SQLSMALLINT *pfNullable)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLDescribeParam]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_DescribeParam(hstmt, ipar, pfSqlType, pcbParamDef,
							   pibScale, pfNullable);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLExtendedFetch(
				 HSTMT hstmt,
				 SQLUSMALLINT fFetchType,
				 SQLINTEGER irow,
				 SQLUINTEGER *pcrow,
				 SQLUSMALLINT *rgfRowStatus)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLExtendedFetch]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_ExtendedFetch(hstmt, fFetchType, irow, pcrow, rgfRowStatus, 0, SC_get_ARD(stmt)->size_of_rowset_odbc2);
	stmt->transition_status = 7;
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLForeignKeys(
			   HSTMT hstmt,
			   SQLCHAR *szPkCatalogName,
			   SQLSMALLINT cbPkCatalogName,
			   SQLCHAR *szPkSchemaName,
			   SQLSMALLINT cbPkSchemaName,
			   SQLCHAR *szPkTableName,
			   SQLSMALLINT cbPkTableName,
			   SQLCHAR *szFkCatalogName,
			   SQLSMALLINT cbFkCatalogName,
			   SQLCHAR *szFkSchemaName,
			   SQLSMALLINT cbFkSchemaName,
			   SQLCHAR *szFkTableName,
			   SQLSMALLINT cbFkTableName)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLForeignKeys]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_ForeignKeys(hstmt, szPkCatalogName, cbPkCatalogName,
						   szPkSchemaName, cbPkSchemaName, szPkTableName,
						 cbPkTableName, szFkCatalogName, cbFkCatalogName,
		   szFkSchemaName, cbFkSchemaName, szFkTableName, cbFkTableName);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLMoreResults(HSTMT hstmt)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLMoreResults]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_MoreResults(hstmt);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLNativeSql(
			 HDBC hdbc,
			 SQLCHAR *szSqlStrIn,
			 SQLINTEGER cbSqlStrIn,
			 SQLCHAR *szSqlStr,
			 SQLINTEGER cbSqlStrMax,
			 SQLINTEGER *pcbSqlStr)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	mylog("[SQLNativeSql]");
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_NativeSql(hdbc, szSqlStrIn, cbSqlStrIn, szSqlStr,
						   cbSqlStrMax, pcbSqlStr);
	LEAVE_CONN_CS(conn);
	return ret;
}

RETCODE		SQL_API
SQLNumParams(
			 HSTMT hstmt,
			 SQLSMALLINT *pcpar)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLNumParams]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_NumParams(hstmt, pcpar);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLParamOptions(
				HSTMT hstmt,
				SQLUINTEGER crow,
				SQLUINTEGER *pirow)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLParamOptions]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_ParamOptions(hstmt, crow, pirow);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLPrimaryKeys(
			   HSTMT hstmt,
			   SQLCHAR *szCatalogName,
			   SQLSMALLINT cbCatalogName,
			   SQLCHAR *szSchemaName,
			   SQLSMALLINT cbSchemaName,
			   SQLCHAR *szTableName,
			   SQLSMALLINT cbTableName)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLPrimaryKeys]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_PrimaryKeys(hstmt, szCatalogName, cbCatalogName,
				   szSchemaName, cbSchemaName, szTableName, cbTableName);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLProcedureColumns(
					HSTMT hstmt,
					SQLCHAR *szCatalogName,
					SQLSMALLINT cbCatalogName,
					SQLCHAR *szSchemaName,
					SQLSMALLINT cbSchemaName,
					SQLCHAR *szProcName,
					SQLSMALLINT cbProcName,
					SQLCHAR *szColumnName,
					SQLSMALLINT cbColumnName)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLProcedureColumns]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_ProcedureColumns(hstmt, szCatalogName, cbCatalogName,
					  szSchemaName, cbSchemaName, szProcName, cbProcName,
								  szColumnName, cbColumnName);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLProcedures(
			  HSTMT hstmt,
			  SQLCHAR *szCatalogName,
			  SQLSMALLINT cbCatalogName,
			  SQLCHAR *szSchemaName,
			  SQLSMALLINT cbSchemaName,
			  SQLCHAR *szProcName,
			  SQLSMALLINT cbProcName)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLProcedures]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_Procedures(hstmt, szCatalogName, cbCatalogName,
					 szSchemaName, cbSchemaName, szProcName, cbProcName);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLSetPos(
		  HSTMT hstmt,
		  SQLUSMALLINT irow,
		  SQLUSMALLINT fOption,
		  SQLUSMALLINT fLock)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLSetPos]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_SetPos(hstmt, irow, fOption, fLock);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLTablePrivileges(
				   HSTMT hstmt,
				   SQLCHAR *szCatalogName,
				   SQLSMALLINT cbCatalogName,
				   SQLCHAR *szSchemaName,
				   SQLSMALLINT cbSchemaName,
				   SQLCHAR *szTableName,
				   SQLSMALLINT cbTableName)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLTablePrivileges]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_TablePrivileges(hstmt, szCatalogName, cbCatalogName,
				   szSchemaName, cbSchemaName, szTableName, cbTableName, 0);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLBindParameter(
				 HSTMT hstmt,
				 SQLUSMALLINT ipar,
				 SQLSMALLINT fParamType,
				 SQLSMALLINT fCType,
				 SQLSMALLINT fSqlType,
				 SQLUINTEGER cbColDef,
				 SQLSMALLINT ibScale,
				 PTR rgbValue,
				 SQLINTEGER cbValueMax,
				 SQLINTEGER *pcbValue)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLBindParameter]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_BindParameter(hstmt, ipar, fParamType, fCType,
					   fSqlType, cbColDef, ibScale, rgbValue, cbValueMax,
							   pcbValue);
	LEAVE_STMT_CS(stmt);
	return ret;
}
