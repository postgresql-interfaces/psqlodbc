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

#include "misc.h"
#include "pgapifunc.h"
#include "environ.h"
#include "connection.h"
#include "statement.h"
#include "qresult.h"
#include "loadlib.h"

BOOL	SC_connection_lost_check(StatementClass *stmt, const char *funcname)
{
	ConnectionClass	*conn = SC_get_conn(stmt);
	char	message[64];

	if (NULL != conn->pqconn)
		return	FALSE;
	SC_clear_error(stmt);
	SPRINTF_FIXED(message, "%s unable due to the connection lost", funcname);
	SC_set_error(stmt, STMT_COMMUNICATION_ERROR, message, funcname);
	return TRUE;
}

RETCODE		SQL_API
SQLBindCol(HSTMT StatementHandle,
		   SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType,
		   PTR TargetValue, SQLLEN BufferLength,
		   SQLLEN *StrLen_or_Ind)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLBindCol]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_BindCol(StatementHandle, ColumnNumber,
				   TargetType, TargetValue, BufferLength, StrLen_or_Ind);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLCancel(HSTMT StatementHandle)
{
	mylog("[SQLCancel]");
	/* Not that neither ENTER_STMT_CS nor StartRollbackState is called */
	/* SC_clear_error((StatementClass *) StatementHandle); maybe this neither */
	if (SC_connection_lost_check((StatementClass *) StatementHandle, __FUNCTION__))
		return SQL_ERROR;
	return PGAPI_Cancel(StatementHandle);
}

static BOOL theResultIsEmpty(const StatementClass *stmt)
{
	QResultClass	*res = SC_get_Result(stmt);
	if (NULL == res)
		return FALSE;
	return (0 == QR_get_num_total_tuples(res));
}

#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLColumns(HSTMT StatementHandle,
		   SQLCHAR *CatalogName, SQLSMALLINT NameLength1,
		   SQLCHAR *SchemaName, SQLSMALLINT NameLength2,
		   SQLCHAR *TableName, SQLSMALLINT NameLength3,
		   SQLCHAR *ColumnName, SQLSMALLINT NameLength4)
{
	CSTR func = "SQLColumns";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	SQLCHAR	*ctName = CatalogName, *scName = SchemaName,
		*tbName = TableName, *clName = ColumnName;
	ConnInfo *ci = &(SC_get_conn(stmt)->connInfo);
	UWORD	flag	= PODBC_SEARCH_PUBLIC_SCHEMA;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (atoi(ci->show_oid_column))
		flag |= PODBC_SHOW_OID_COLUMN;
	if (atoi(ci->row_versioning))
		flag |= PODBC_ROW_VERSIONING;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_Columns(StatementHandle, ctName, NameLength1,
				scName, NameLength2, tbName, NameLength3,
				clName, NameLength4, flag, 0, 0);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newCt = NULL, *newSc = NULL, *newTb = NULL, *newCl = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newCt = make_lstring_ifneeded(conn, CatalogName, NameLength1, ifallupper), NULL != newCt)
		{
			ctName = newCt;
			reexec = TRUE;
		}
		if (newSc = make_lstring_ifneeded(conn, SchemaName, NameLength2, ifallupper), NULL != newSc)
		{
			scName = newSc;
			reexec = TRUE;
		}
		if (newTb = make_lstring_ifneeded(conn, TableName, NameLength3, ifallupper), NULL != newTb)
		{
			tbName = newTb;
			reexec = TRUE;
		}
		if (newCl = make_lstring_ifneeded(conn, ColumnName, NameLength4, ifallupper), NULL != newCl)
		{
			clName = newCl;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_Columns(StatementHandle, ctName, NameLength1,
				scName, NameLength2, tbName, NameLength3,
				clName, NameLength4, flag, 0, 0);
			if (newCt)
				free(newCt);
			if (newSc)
				free(newSc);
			if (newTb)
				free(newTb);
			if (newCl)
				free(newCl);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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
	CC_examine_global_transaction(conn);
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
				 SQLCHAR * szConnStrIn,
				 SQLSMALLINT cbConnStrIn,
				 SQLCHAR * szConnStrOut,
				 SQLSMALLINT cbConnStrOutMax,
				 SQLSMALLINT * pcbConnStrOut,
				 SQLUSMALLINT fDriverCompletion)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	mylog("[SQLDriverConnect]");
	CC_examine_global_transaction(conn);
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_DriverConnect(hdbc, hwnd, szConnStrIn, cbConnStrIn,
		szConnStrOut, cbConnStrOutMax, pcbConnStrOut, fDriverCompletion);
	LEAVE_CONN_CS(conn);
	return ret;
}
RETCODE		SQL_API
SQLBrowseConnect(HDBC hdbc,
				 SQLCHAR *szConnStrIn,
				 SQLSMALLINT cbConnStrIn,
				 SQLCHAR *szConnStrOut,
				 SQLSMALLINT cbConnStrOutMax,
				 SQLSMALLINT *pcbConnStrOut)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	mylog("[SQLBrowseConnect]");
	CC_examine_global_transaction(conn);
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
			   SQLSMALLINT *DataType, SQLULEN *ColumnSize,
			   SQLSMALLINT *DecimalDigits, SQLSMALLINT *Nullable)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLDescribeCol]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_DescribeCol(StatementHandle, ColumnNumber,
							 ColumnName, BufferLength, NameLength,
						  DataType, ColumnSize, DecimalDigits, Nullable);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLDisconnect(HDBC ConnectionHandle)
{
	CSTR func = "SQLDisconnect";
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;

	mylog("[%s for %p]", func, ConnectionHandle);
#ifdef	_HANDLE_ENLIST_IN_DTC_
	if (CC_is_in_global_trans(conn))
		CALL_DtcOnDisconnect(conn);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_Disconnect(ConnectionHandle);
	LEAVE_CONN_CS(conn);
	return ret;
}

#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLExecDirect(HSTMT StatementHandle,
			  SQLCHAR *StatementText, SQLINTEGER TextLength)
{
	CSTR func = "SQLExecDirect";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	UWORD	flag = 0;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	flag |= PODBC_WITH_HOLD;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		StartRollbackState(stmt);
		ret = PGAPI_ExecDirect(StatementHandle, StatementText, TextLength, flag);
		ret = DiscardStatementSvp(stmt, ret, FALSE);
	}
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLExecute(HSTMT StatementHandle)
{
	CSTR func = "SQLExecute";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	UWORD	flag = 0;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	flag |= PODBC_WITH_HOLD;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		StartRollbackState(stmt);
		stmt->exec_current_row = -1;
		ret = PGAPI_Execute(StatementHandle, flag);
		ret = DiscardStatementSvp(stmt, ret, FALSE);
	}
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLFetch(HSTMT StatementHandle)
{
	CSTR func = "SQLFetch";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	IRDFields	*irdopts = SC_get_IRDF(stmt);
	ARDFields	*ardopts = SC_get_ARDF(stmt);
	SQLUSMALLINT *rowStatusArray = irdopts->rowStatusArray;
	SQLULEN *pcRow = irdopts->rowsFetched;

	mylog("[[%s]]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);

	ret = PGAPI_ExtendedFetch(StatementHandle, SQL_FETCH_NEXT, 0,
							   pcRow, rowStatusArray, 0, ardopts->size_of_rowset);
	stmt->transition_status = STMT_TRANSITION_FETCH_SCROLL;

	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}


RETCODE		SQL_API
SQLFreeStmt(HSTMT StatementHandle,
			SQLUSMALLINT Option)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	ConnectionClass *conn = NULL;

	mylog("[SQLFreeStmt]");

	if (stmt)
	{
		if (Option == SQL_DROP)
		{
			conn = stmt->hdbc;
			if (conn)
				ENTER_CONN_CS(conn);
		}
		else
			ENTER_STMT_CS(stmt);
	}

	ret = PGAPI_FreeStmt(StatementHandle, Option);

	if (stmt)
	{
		if (Option == SQL_DROP)
		{
			if (conn)
				LEAVE_CONN_CS(conn);
		}
		else
			LEAVE_STMT_CS(stmt);
	}

	return ret;
}


#ifndef	UNICODE_SUPPORTXX
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
	StartRollbackState(stmt);
	ret = PGAPI_GetCursorName(StatementHandle, CursorName, BufferLength,
							   NameLength);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLGetData(HSTMT StatementHandle,
		   SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType,
		   PTR TargetValue, SQLLEN BufferLength,
		   SQLLEN *StrLen_or_Ind)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLGetData]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_GetData(StatementHandle, ColumnNumber, TargetType,
						 TargetValue, BufferLength, StrLen_or_Ind);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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
	CC_examine_global_transaction(conn);
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS)
		ret = PGAPI_GetFunctions30(ConnectionHandle, FunctionId, Supported);
	else
		ret = PGAPI_GetFunctions(ConnectionHandle, FunctionId, Supported);

	LEAVE_CONN_CS(conn);
	return ret;
}

#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLGetInfo(HDBC ConnectionHandle,
		   SQLUSMALLINT InfoType, PTR InfoValue,
		   SQLSMALLINT BufferLength, SQLSMALLINT *StringLength)
{
	CSTR	func = "SQLGetInfo";
	RETCODE		ret;
	ConnectionClass	*conn = (ConnectionClass *) ConnectionHandle;

	CC_examine_global_transaction(conn);
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	mylog("[%s(30)]", func);
	if ((ret = PGAPI_GetInfo(ConnectionHandle, InfoType, InfoValue,
				BufferLength, StringLength)) == SQL_ERROR)
		CC_log_error("SQLGetInfo(30)", "", conn);
	LEAVE_CONN_CS(conn);
	return ret;
}


RETCODE		SQL_API
SQLGetTypeInfo(HSTMT StatementHandle,
			   SQLSMALLINT DataType)
{
	CSTR func = "SQLGetTypeInfo";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[%s]", func);
	if (SC_connection_lost_check((StatementClass *) StatementHandle, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		StartRollbackState(stmt);
		ret = PGAPI_GetTypeInfo(StatementHandle, DataType);
		ret = DiscardStatementSvp(stmt, ret, FALSE);
	}
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLNumResultCols(HSTMT StatementHandle,
				 SQLSMALLINT *ColumnCount)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLNumResultCols]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_NumResultCols(StatementHandle, ColumnCount);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_ParamData(StatementHandle, Value);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLPrepare(HSTMT StatementHandle,
		   SQLCHAR *StatementText, SQLINTEGER TextLength)
{
	CSTR func = "SQLPrepare";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLPrepare]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		StartRollbackState(stmt);
		ret = PGAPI_Prepare(StatementHandle, StatementText, TextLength);
		ret = DiscardStatementSvp(stmt, ret, FALSE);
	}
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLPutData(HSTMT StatementHandle,
		   PTR Data, SQLLEN StrLen_or_Ind)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLPutData]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	ret = PGAPI_PutData(StatementHandle, Data, StrLen_or_Ind);
	ret = DiscardStatementSvp(stmt, ret, TRUE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLRowCount(HSTMT StatementHandle,
			SQLLEN *RowCount)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLRowCount]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_RowCount(StatementHandle, RowCount);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}


#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLSetCursorName(HSTMT StatementHandle,
				 SQLCHAR *CursorName, SQLSMALLINT NameLength)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	mylog("[SQLSetCursorName]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_SetCursorName(StatementHandle, CursorName, NameLength);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLSetParam(HSTMT StatementHandle,
			SQLUSMALLINT ParameterNumber, SQLSMALLINT ValueType,
			SQLSMALLINT ParameterType, SQLULEN LengthPrecision,
			SQLSMALLINT ParameterScale, PTR ParameterValue,
			SQLLEN *StrLen_or_Ind)
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


#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLSpecialColumns(HSTMT StatementHandle,
				  SQLUSMALLINT IdentifierType, SQLCHAR *CatalogName,
				  SQLSMALLINT NameLength1, SQLCHAR *SchemaName,
				  SQLSMALLINT NameLength2, SQLCHAR *TableName,
				  SQLSMALLINT NameLength3, SQLUSMALLINT Scope,
				  SQLUSMALLINT Nullable)
{
	CSTR func = "SQLSpecialColumns";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	SQLCHAR *ctName = CatalogName, *scName = SchemaName, *tbName = TableName;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_SpecialColumns(StatementHandle, IdentifierType, ctName,
			NameLength1, scName, NameLength2, tbName, NameLength3,
							Scope, Nullable);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newCt =NULL, *newSc = NULL, *newTb = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newCt = make_lstring_ifneeded(conn, CatalogName, NameLength1, ifallupper), NULL != newCt)
		{
			ctName = newCt;
			reexec = TRUE;
		}
		if (newSc = make_lstring_ifneeded(conn, SchemaName, NameLength2, ifallupper), NULL != newSc)
		{
			scName = newSc;
			reexec = TRUE;
		}
		if (newTb = make_lstring_ifneeded(conn, TableName, NameLength3, ifallupper), NULL != newTb)
		{
			tbName = newTb;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_SpecialColumns(StatementHandle, IdentifierType, ctName,
			NameLength1, scName, NameLength2, tbName, NameLength3,
							Scope, Nullable);
			if (newCt)
				free(newCt);
			if (newSc)
				free(newSc);
			if (newTb)
				free(newTb);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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
	CSTR func = "SQLStatistics";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	SQLCHAR *ctName = CatalogName, *scName = SchemaName, *tbName = TableName;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_Statistics(StatementHandle, ctName, NameLength1,
				 scName, NameLength2, tbName, NameLength3,
				 Unique, Reserved);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newCt =NULL, *newSc = NULL, *newTb = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newCt = make_lstring_ifneeded(conn, CatalogName, NameLength1, ifallupper), NULL != newCt)
		{
			ctName = newCt;
			reexec = TRUE;
		}
		if (newSc = make_lstring_ifneeded(conn, SchemaName, NameLength2, ifallupper), NULL != newSc)
		{
			scName = newSc;
			reexec = TRUE;
		}
		if (newTb = make_lstring_ifneeded(conn, TableName, NameLength3, ifallupper), NULL != newTb)
		{
			tbName = newTb;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_Statistics(StatementHandle, ctName, NameLength1,
				 scName, NameLength2, tbName, NameLength3,
				 Unique, Reserved);
			if (newCt)
				free(newCt);
			if (newSc)
				free(newSc);
			if (newTb)
				free(newTb);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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
	CSTR func = "SQLTables";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	SQLCHAR *ctName = CatalogName, *scName = SchemaName, *tbName = TableName;
	UWORD	flag = 0;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_Tables(StatementHandle, ctName, NameLength1,
				scName, NameLength2, tbName, NameLength3,
					TableType, NameLength4, flag);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newCt =NULL, *newSc = NULL, *newTb = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newCt = make_lstring_ifneeded(conn, CatalogName, NameLength1, ifallupper), NULL != newCt)
		{
			ctName = newCt;
			reexec = TRUE;
		}
		if (newSc = make_lstring_ifneeded(conn, SchemaName, NameLength2, ifallupper), NULL != newSc)
		{
			scName = newSc;
			reexec = TRUE;
		}
		if (newTb = make_lstring_ifneeded(conn, TableName, NameLength3, ifallupper), NULL != newTb)
		{
			tbName = newTb;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_Tables(StatementHandle, ctName, NameLength1,
				scName, NameLength2, tbName, NameLength3,
						TableType, NameLength4, flag);
			if (newCt)
				free(newCt);
			if (newSc)
				free(newSc);
			if (newTb)
				free(newTb);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}


RETCODE		SQL_API
SQLColumnPrivileges(HSTMT hstmt,
					SQLCHAR *szCatalogName,
					SQLSMALLINT cbCatalogName,
					SQLCHAR *szSchemaName,
					SQLSMALLINT cbSchemaName,
					SQLCHAR *szTableName,
					SQLSMALLINT cbTableName,
					SQLCHAR *szColumnName,
					SQLSMALLINT cbColumnName)
{
	CSTR func = "SQLColumnPrivileges";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;
	SQLCHAR	*ctName = szCatalogName, *scName = szSchemaName,
		*tbName = szTableName, *clName = szColumnName;
	UWORD	flag = 0;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_ColumnPrivileges(hstmt, ctName, cbCatalogName,
				scName, cbSchemaName, tbName, cbTableName,
						clName, cbColumnName, flag);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newCt = NULL, *newSc = NULL, *newTb = NULL, *newCl = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newCt = make_lstring_ifneeded(conn, szCatalogName, cbCatalogName, ifallupper), NULL != newCt)
		{
			ctName = newCt;
			reexec = TRUE;
		}
		if (newSc = make_lstring_ifneeded(conn, szSchemaName, cbSchemaName, ifallupper), NULL != newSc)
		{
			scName = newSc;
			reexec = TRUE;
		}
		if (newTb = make_lstring_ifneeded(conn, szTableName, cbTableName, ifallupper), NULL != newTb)
		{
			tbName = newTb;
			reexec = TRUE;
		}
		if (newCl = make_lstring_ifneeded(conn, szColumnName, cbColumnName, ifallupper), NULL != newCl)
		{
			clName = newCl;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_ColumnPrivileges(hstmt, ctName, cbCatalogName,
				scName, cbSchemaName, tbName, cbTableName,
						clName, cbColumnName, flag);
			if (newCt)
				free(newCt);
			if (newSc)
				free(newSc);
			if (newTb)
				free(newTb);
			if (newCl)
				free(newCl);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLDescribeParam(HSTMT hstmt,
				 SQLUSMALLINT ipar,
				 SQLSMALLINT *pfSqlType,
				 SQLULEN *pcbParamDef,
				 SQLSMALLINT *pibScale,
				 SQLSMALLINT *pfNullable)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLDescribeParam]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_DescribeParam(hstmt, ipar, pfSqlType, pcbParamDef,
							   pibScale, pfNullable);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLExtendedFetch(HSTMT hstmt,
				 SQLUSMALLINT fFetchType,
				 SQLLEN irow,
#if defined(WITH_UNIXODBC) && (SIZEOF_LONG_INT != 8)
				 SQLROWSETSIZE *pcrow,
#else
				 SQLULEN *pcrow,
#endif /* WITH_UNIXODBC */
				 SQLUSMALLINT *rgfRowStatus)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLExtendedFetch]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
#ifdef WITH_UNIXODBC
	{
		SQLULEN	retrieved;

		ret = PGAPI_ExtendedFetch(hstmt, fFetchType, irow, &retrieved, rgfRowStatus, 0, SC_get_ARDF(stmt)->size_of_rowset_odbc2);
		if (pcrow)
			*pcrow = retrieved;
	}
#else
	ret = PGAPI_ExtendedFetch(hstmt, fFetchType, irow, pcrow, rgfRowStatus, 0, SC_get_ARDF(stmt)->size_of_rowset_odbc2);
#endif /* WITH_UNIXODBC */
	stmt->transition_status = STMT_TRANSITION_EXTENDED_FETCH;
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLForeignKeys(HSTMT hstmt,
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
	CSTR func = "SQLForeignKeys";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;
	SQLCHAR *pkctName = szPkCatalogName, *pkscName = szPkSchemaName,
		*pktbName = szPkTableName, *fkctName = szFkCatalogName,
		*fkscName = szFkSchemaName, *fktbName = szFkTableName;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_ForeignKeys(hstmt, pkctName, cbPkCatalogName,
			pkscName, cbPkSchemaName, pktbName, cbPkTableName,
			fkctName, cbFkCatalogName, fkscName, cbFkSchemaName,
			fktbName, cbFkTableName);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newPkct = NULL, *newPksc = NULL, *newPktb = NULL,
			*newFkct = NULL, *newFksc = NULL, *newFktb = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newPkct = make_lstring_ifneeded(conn, szPkCatalogName, cbPkCatalogName, ifallupper), NULL != newPkct)
		{
			pkctName = newPkct;
			reexec = TRUE;
		}
		if (newPksc = make_lstring_ifneeded(conn, szPkSchemaName, cbPkSchemaName, ifallupper), NULL != newPksc)
		{
			pkscName = newPksc;
			reexec = TRUE;
		}
		if (newPktb = make_lstring_ifneeded(conn, szPkTableName, cbPkTableName, ifallupper), NULL != newPktb)
		{
			pktbName = newPktb;
			reexec = TRUE;
		}
		if (newFkct = make_lstring_ifneeded(conn, szFkCatalogName, cbFkCatalogName, ifallupper), NULL != newFkct)
		{
			fkctName = newFkct;
			reexec = TRUE;
		}
		if (newFksc = make_lstring_ifneeded(conn, szFkSchemaName, cbFkSchemaName, ifallupper), NULL != newFksc)
		{
			fkscName = newFksc;
			reexec = TRUE;
		}
		if (newFktb = make_lstring_ifneeded(conn, szFkTableName, cbFkTableName, ifallupper), NULL != newFktb)
		{
			fktbName = newFktb;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_ForeignKeys(hstmt, pkctName, cbPkCatalogName,
			pkscName, cbPkSchemaName, pktbName, cbPkTableName,
			fkctName, cbFkCatalogName, fkscName, cbFkSchemaName,
			fktbName, cbFkTableName);
			if (newPkct)
				free(newPkct);
			if (newPksc)
				free(newPksc);
			if (newPktb)
				free(newPktb);
			if (newFkct)
				free(newFkct);
			if (newFksc)
				free(newFksc);
			if (newFktb)
				free(newFktb);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLMoreResults(HSTMT hstmt)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLMoreResults]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_MoreResults(hstmt);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLNativeSql(HDBC hdbc,
			 SQLCHAR *szSqlStrIn,
			 SQLINTEGER cbSqlStrIn,
			 SQLCHAR *szSqlStr,
			 SQLINTEGER cbSqlStrMax,
			 SQLINTEGER *pcbSqlStr)
{
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	mylog("[SQLNativeSql]");
	CC_examine_global_transaction(conn);
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	ret = PGAPI_NativeSql(hdbc, szSqlStrIn, cbSqlStrIn, szSqlStr,
						   cbSqlStrMax, pcbSqlStr);
	LEAVE_CONN_CS(conn);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLNumParams(HSTMT hstmt,
			 SQLSMALLINT *pcpar)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLNumParams]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_NumParams(hstmt, pcpar);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLPrimaryKeys(HSTMT hstmt,
			   SQLCHAR *szCatalogName,
			   SQLSMALLINT cbCatalogName,
			   SQLCHAR *szSchemaName,
			   SQLSMALLINT cbSchemaName,
			   SQLCHAR *szTableName,
			   SQLSMALLINT cbTableName)
{
	CSTR func = "SQLPrimaryKeys";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;
	SQLCHAR	*ctName = szCatalogName, *scName = szSchemaName,
		*tbName = szTableName;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_PrimaryKeys(hstmt, ctName, cbCatalogName,
			scName, cbSchemaName, tbName, cbTableName, 0);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newCt = NULL, *newSc = NULL, *newTb = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newCt = make_lstring_ifneeded(conn, szCatalogName, cbCatalogName, ifallupper), NULL != newCt)
		{
			ctName = newCt;
			reexec = TRUE;
		}
		if (newSc = make_lstring_ifneeded(conn, szSchemaName, cbSchemaName, ifallupper), NULL != newSc)
		{
			scName = newSc;
			reexec = TRUE;
		}
		if (newTb = make_lstring_ifneeded(conn, szTableName, cbTableName, ifallupper), NULL != newTb)
		{
			tbName = newTb;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_PrimaryKeys(hstmt, ctName, cbCatalogName,
			scName, cbSchemaName, tbName, cbTableName, 0);
			if (newCt)
				free(newCt);
			if (newSc)
				free(newSc);
			if (newTb)
				free(newTb);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLProcedureColumns(HSTMT hstmt,
					SQLCHAR *szCatalogName,
					SQLSMALLINT cbCatalogName,
					SQLCHAR *szSchemaName,
					SQLSMALLINT cbSchemaName,
					SQLCHAR *szProcName,
					SQLSMALLINT cbProcName,
					SQLCHAR *szColumnName,
					SQLSMALLINT cbColumnName)
{
	CSTR func = "SQLProcedureColumns";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;
	SQLCHAR	*ctName = szCatalogName, *scName = szSchemaName,
		*prName = szProcName, *clName = szColumnName;
	UWORD	flag = 0;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_ProcedureColumns(hstmt, ctName, cbCatalogName,
				scName, cbSchemaName, prName, cbProcName,
					clName, cbColumnName, flag);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newCt = NULL, *newSc = NULL, *newPr = NULL, *newCl = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newCt = make_lstring_ifneeded(conn, szCatalogName, cbCatalogName, ifallupper), NULL != newCt)
		{
			ctName = newCt;
			reexec = TRUE;
		}
		if (newSc = make_lstring_ifneeded(conn, szSchemaName, cbSchemaName, ifallupper), NULL != newSc)
		{
			scName = newSc;
			reexec = TRUE;
		}
		if (newPr = make_lstring_ifneeded(conn, szProcName, cbProcName, ifallupper), NULL != newPr)
		{
			prName = newPr;
			reexec = TRUE;
		}
		if (newCl = make_lstring_ifneeded(conn, szColumnName, cbColumnName, ifallupper), NULL != newCl)
		{
			clName = newCl;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_ProcedureColumns(hstmt, ctName, cbCatalogName,
				scName, cbSchemaName, prName, cbProcName,
					clName, cbColumnName, flag);
			if (newCt)
				free(newCt);
			if (newSc)
				free(newSc);
			if (newPr)
				free(newPr);
			if (newCl)
				free(newCl);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE		SQL_API
SQLProcedures(HSTMT hstmt,
			  SQLCHAR *szCatalogName,
			  SQLSMALLINT cbCatalogName,
			  SQLCHAR *szSchemaName,
			  SQLSMALLINT cbSchemaName,
			  SQLCHAR *szProcName,
			  SQLSMALLINT cbProcName)
{
	CSTR func = "SQLProcedures";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;
	SQLCHAR	*ctName = szCatalogName, *scName = szSchemaName,
		*prName = szProcName;
	UWORD	flag = 0;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_Procedures(hstmt, ctName, cbCatalogName,
					 scName, cbSchemaName, prName,
					 cbProcName, flag);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newCt = NULL, *newSc = NULL, *newPr = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newCt = make_lstring_ifneeded(conn, szCatalogName, cbCatalogName, ifallupper), NULL != newCt)
		{
			ctName = newCt;
			reexec = TRUE;
		}
		if (newSc = make_lstring_ifneeded(conn, szSchemaName, cbSchemaName, ifallupper), NULL != newSc)
		{
			scName = newSc;
			reexec = TRUE;
		}
		if (newPr = make_lstring_ifneeded(conn, szProcName, cbProcName, ifallupper), NULL != newPr)
		{
			prName = newPr;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_Procedures(hstmt, ctName, cbCatalogName,
					 scName, cbSchemaName, prName, cbProcName, flag);
			if (newCt)
				free(newCt);
			if (newSc)
				free(newSc);
			if (newPr)
				free(newPr);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLSetPos(HSTMT hstmt,
		  SQLSETPOSIROW irow,
		  SQLUSMALLINT fOption,
		  SQLUSMALLINT fLock)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLSetPos]");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_SetPos(hstmt, irow, fOption, fLock);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

#ifndef	UNICODE_SUPPORTXX
RETCODE		SQL_API
SQLTablePrivileges(HSTMT hstmt,
				   SQLCHAR *szCatalogName,
				   SQLSMALLINT cbCatalogName,
				   SQLCHAR *szSchemaName,
				   SQLSMALLINT cbSchemaName,
				   SQLCHAR *szTableName,
				   SQLSMALLINT cbTableName)
{
	CSTR func = "SQLTablePrivileges";
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;
	SQLCHAR	*ctName = szCatalogName, *scName = szSchemaName,
		*tbName = szTableName;
	UWORD	flag = 0;

	mylog("[%s]", func);
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_TablePrivileges(hstmt, ctName, cbCatalogName,
			scName, cbSchemaName, tbName, cbTableName, flag);
	if (SQL_SUCCESS == ret && theResultIsEmpty(stmt))
	{
		BOOL	ifallupper = TRUE, reexec = FALSE;
		SQLCHAR *newCt = NULL, *newSc = NULL, *newTb = NULL;
		ConnectionClass *conn = SC_get_conn(stmt);

		if (SC_is_lower_case(stmt, conn)) /* case-insensitive identifier */
			ifallupper = FALSE;
		if (newCt = make_lstring_ifneeded(conn, szCatalogName, cbCatalogName, ifallupper), NULL != newCt)
		{
			ctName = newCt;
			reexec = TRUE;
		}
		if (newSc = make_lstring_ifneeded(conn, szSchemaName, cbSchemaName, ifallupper), NULL != newSc)
		{
			scName = newSc;
			reexec = TRUE;
		}
		if (newTb = make_lstring_ifneeded(conn, szTableName, cbTableName, ifallupper), NULL != newTb)
		{
			tbName = newTb;
			reexec = TRUE;
		}
		if (reexec)
		{
			ret = PGAPI_TablePrivileges(hstmt, ctName, cbCatalogName,
			scName, cbSchemaName, tbName, cbTableName, 0);
			if (newCt)
				free(newCt);
			if (newSc)
				free(newSc);
			if (newTb)
				free(newTb);
		}
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}
#endif /* UNICODE_SUPPORTXX */

RETCODE		SQL_API
SQLBindParameter(HSTMT hstmt,
				 SQLUSMALLINT ipar,
				 SQLSMALLINT fParamType,
				 SQLSMALLINT fCType,
				 SQLSMALLINT fSqlType,
				 SQLULEN cbColDef,
				 SQLSMALLINT ibScale,
				 PTR rgbValue,
				 SQLLEN cbValueMax,
				 SQLLEN *pcbValue)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("[SQLBindParameter]");
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_BindParameter(hstmt, ipar, fParamType, fCType,
					   fSqlType, cbColDef, ibScale, rgbValue, cbValueMax,
							   pcbValue);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}
