/*-------
 * Module:			pgapifunc.h
 *
 *-------
 */
#ifndef _PG_API_FUNC_H__
#define _PG_API_FUNC_H__

#include "psqlodbc.h"
#include <stdio.h>
#include <string.h>

#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */
/*	Internal flags for catalog functions */
#define	PODBC_NOT_SEARCH_PATTERN	1L
#define	PODBC_SEARCH_PUBLIC_SCHEMA	(1L << 1)
#define	PODBC_SEARCH_BY_IDS		(1L << 2)
#define	PODBC_SHOW_OID_COLUMN		(1L << 3)
#define	PODBC_ROW_VERSIONING		(1L << 4)
/*	Internal flags for PGAPI_AllocStmt functions */
#define	PODBC_EXTERNAL_STATEMENT	1L	/* visible to the driver manager */
#define	PODBC_INHERIT_CONNECT_OPTIONS	(1L << 1)
/*	Internal flags for PGAPI_Exec... functions */
#define	PODBC_WITH_HOLD			1L
#define	PODBC_RDONLY			(1L << 1)
/*	Flags for the error handling */
#define	PODBC_ALLOW_PARTIAL_EXTRACT	1L
/* #define	PODBC_ERROR_CLEAR		(1L << 1) 	no longer used */

RETCODE SQL_API PGAPI_AllocConnect(HENV EnvironmentHandle,
				   HDBC * ConnectionHandle);
RETCODE SQL_API PGAPI_AllocEnv(HENV * EnvironmentHandle);
RETCODE SQL_API PGAPI_AllocStmt(HDBC ConnectionHandle,
				HSTMT *StatementHandle, UDWORD flag);
RETCODE SQL_API PGAPI_BindCol(HSTMT StatementHandle,
			  SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType,
			  PTR TargetValue, SQLLEN BufferLength,
			  SQLLEN *StrLen_or_Ind);
RETCODE SQL_API PGAPI_Cancel(HSTMT StatementHandle);
RETCODE SQL_API PGAPI_Columns(HSTMT StatementHandle,
			  const SQLCHAR *CatalogName, SQLSMALLINT NameLength1,
			  const SQLCHAR *SchemaName, SQLSMALLINT NameLength2,
			  const SQLCHAR *TableName, SQLSMALLINT NameLength3,
			  const SQLCHAR *ColumnName, SQLSMALLINT NameLength4,
			  UWORD flag,
			  OID	reloid,
			  Int2 attnum);
RETCODE SQL_API PGAPI_Connect(HDBC ConnectionHandle,
		const SQLCHAR *ServerName, SQLSMALLINT NameLength1,
		const SQLCHAR *UserName, SQLSMALLINT NameLength2,
		const SQLCHAR *Authentication, SQLSMALLINT NameLength3);
RETCODE SQL_API PGAPI_DriverConnect(HDBC hdbc, HWND hwnd,
		const SQLCHAR * szConnStrIn, SQLSMALLINT cbConnStrIn,
		SQLCHAR * szConnStrOut, SQLSMALLINT cbConnStrOutMax,
		SQLSMALLINT * pcbConnStrOut, SQLUSMALLINT fDriverCompletion);
RETCODE SQL_API PGAPI_BrowseConnect(HDBC hdbc,
					const SQLCHAR *szConnStrIn, SQLSMALLINT cbConnStrIn,
					SQLCHAR *szConnStrOut, SQLSMALLINT cbConnStrOutMax,
					SQLSMALLINT *pcbConnStrOut);
RETCODE SQL_API PGAPI_DataSources(HENV EnvironmentHandle,
				  SQLUSMALLINT Direction, const SQLCHAR *ServerName,
				  SQLSMALLINT BufferLength1, SQLSMALLINT *NameLength1,
				  const SQLCHAR *Description, SQLSMALLINT BufferLength2,
				  SQLSMALLINT *NameLength2);
RETCODE SQL_API PGAPI_DescribeCol(HSTMT StatementHandle,
				  SQLUSMALLINT ColumnNumber, SQLCHAR *ColumnName,
				  SQLSMALLINT BufferLength, SQLSMALLINT *NameLength,
				  SQLSMALLINT *DataType, SQLULEN *ColumnSize,
				  SQLSMALLINT *DecimalDigits, SQLSMALLINT *Nullable);
RETCODE SQL_API PGAPI_Disconnect(HDBC ConnectionHandle);
RETCODE SQL_API PGAPI_Error(HENV EnvironmentHandle,
			HDBC ConnectionHandle, HSTMT StatementHandle,
			SQLCHAR *Sqlstate, SQLINTEGER *NativeError,
			SQLCHAR *MessageText, SQLSMALLINT BufferLength,
			SQLSMALLINT *TextLength);
/* Helper functions for Error handling */
RETCODE SQL_API PGAPI_EnvError(HENV EnvironmentHandle, SQLSMALLINT RecNumber,
			SQLCHAR *Sqlstate, SQLINTEGER *NativeError,
			SQLCHAR *MessageText, SQLSMALLINT BufferLength,
			SQLSMALLINT *TextLength, UWORD flag);
RETCODE SQL_API PGAPI_ConnectError(HDBC ConnectionHandle, SQLSMALLINT RecNumber,
			SQLCHAR *Sqlstate, SQLINTEGER *NativeError,
			SQLCHAR *MessageText, SQLSMALLINT BufferLength,
			SQLSMALLINT *TextLength, UWORD flag);
RETCODE SQL_API PGAPI_StmtError(HSTMT StatementHandle, SQLSMALLINT RecNumber,
			SQLCHAR *Sqlstate, SQLINTEGER *NativeError,
			SQLCHAR *MessageText, SQLSMALLINT BufferLength,
			SQLSMALLINT *TextLength, UWORD flag);

RETCODE SQL_API PGAPI_ExecDirect(HSTMT StatementHandle,
		const SQLCHAR *StatementText, SQLINTEGER TextLength, UWORD flag);
RETCODE SQL_API PGAPI_Execute(HSTMT StatementHandle, UWORD flag);
RETCODE SQL_API PGAPI_Fetch(HSTMT StatementHandle);
RETCODE SQL_API PGAPI_FreeConnect(HDBC ConnectionHandle);
RETCODE SQL_API PGAPI_FreeEnv(HENV EnvironmentHandle);
RETCODE SQL_API PGAPI_FreeStmt(HSTMT StatementHandle,
			   SQLUSMALLINT Option);
RETCODE SQL_API PGAPI_GetConnectOption(HDBC ConnectionHandle,
			SQLUSMALLINT Option, PTR Value,
			SQLINTEGER *StringLength, SQLINTEGER BufferLength);
RETCODE SQL_API PGAPI_GetCursorName(HSTMT StatementHandle,
					SQLCHAR *CursorName, SQLSMALLINT BufferLength,
					SQLSMALLINT *NameLength);
RETCODE SQL_API PGAPI_GetData(HSTMT StatementHandle,
			  SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType,
			  PTR TargetValue, SQLLEN BufferLength,
			  SQLLEN *StrLen_or_Ind);
RETCODE SQL_API PGAPI_GetFunctions(HDBC ConnectionHandle,
				   SQLUSMALLINT FunctionId, SQLUSMALLINT *Supported);
RETCODE SQL_API PGAPI_GetFunctions30(HDBC ConnectionHandle,
					 SQLUSMALLINT FunctionId, SQLUSMALLINT *Supported);
RETCODE SQL_API PGAPI_GetInfo(HDBC ConnectionHandle,
			  SQLUSMALLINT InfoType, PTR InfoValue,
			  SQLSMALLINT BufferLength, SQLSMALLINT *StringLength);
RETCODE SQL_API PGAPI_GetStmtOption(HSTMT StatementHandle,
			SQLUSMALLINT Option, PTR Value,
			SQLINTEGER *StringLength, SQLINTEGER BufferLength);
RETCODE SQL_API PGAPI_GetTypeInfo(HSTMT StatementHandle,
				  SQLSMALLINT DataType);
RETCODE SQL_API PGAPI_NumResultCols(HSTMT StatementHandle,
					SQLSMALLINT *ColumnCount);
RETCODE SQL_API PGAPI_ParamData(HSTMT StatementHandle,
				PTR *Value);
RETCODE SQL_API PGAPI_Prepare(HSTMT StatementHandle,
			  const SQLCHAR *StatementText, SQLINTEGER TextLength);
RETCODE SQL_API PGAPI_PutData(HSTMT StatementHandle,
			  PTR Data, SQLLEN StrLen_or_Ind);
RETCODE SQL_API PGAPI_RowCount(HSTMT StatementHandle,
			   SQLLEN *RowCount);
RETCODE SQL_API PGAPI_SetConnectOption(HDBC ConnectionHandle,
					   SQLUSMALLINT Option, SQLULEN Value);
RETCODE SQL_API PGAPI_SetCursorName(HSTMT StatementHandle,
					const SQLCHAR *CursorName, SQLSMALLINT NameLength);
RETCODE SQL_API PGAPI_SetParam(HSTMT StatementHandle,
			   SQLUSMALLINT ParameterNumber, SQLSMALLINT ValueType,
			   SQLSMALLINT ParameterType, SQLULEN LengthPrecision,
			   SQLSMALLINT ParameterScale, PTR ParameterValue,
			   SQLLEN *StrLen_or_Ind);
RETCODE SQL_API PGAPI_SetStmtOption(HSTMT StatementHandle,
					SQLUSMALLINT Option, SQLULEN Value);
RETCODE SQL_API PGAPI_SpecialColumns(HSTMT StatementHandle,
					 SQLUSMALLINT IdentifierType, const SQLCHAR *CatalogName,
					 SQLSMALLINT NameLength1, const SQLCHAR *SchemaName,
					 SQLSMALLINT NameLength2, const SQLCHAR *TableName,
					 SQLSMALLINT NameLength3, SQLUSMALLINT Scope,
					 SQLUSMALLINT Nullable);
RETCODE SQL_API PGAPI_Statistics(HSTMT StatementHandle,
				 const SQLCHAR *CatalogName, SQLSMALLINT NameLength1,
				 const SQLCHAR *SchemaName, SQLSMALLINT NameLength2,
				 const SQLCHAR *TableName, SQLSMALLINT NameLength3,
				 SQLUSMALLINT Unique, SQLUSMALLINT Reserved);
RETCODE SQL_API PGAPI_Tables(HSTMT StatementHandle,
			 const SQLCHAR *CatalogName, SQLSMALLINT NameLength1,
			 const SQLCHAR *SchemaName, SQLSMALLINT NameLength2,
			 const SQLCHAR *TableName, SQLSMALLINT NameLength3,
			 const SQLCHAR *TableType, SQLSMALLINT NameLength4,
			UWORD flag);
RETCODE SQL_API PGAPI_Transact(HENV EnvironmentHandle,
			   HDBC ConnectionHandle, SQLUSMALLINT CompletionType);
RETCODE SQL_API PGAPI_ColAttributes(
					HSTMT hstmt,
					SQLUSMALLINT icol,
					SQLUSMALLINT fDescType,
					PTR rgbDesc,
					SQLSMALLINT cbDescMax,
					SQLSMALLINT *pcbDesc,
					SQLLEN *pfDesc);
RETCODE SQL_API PGAPI_ColumnPrivileges(
					   HSTMT hstmt,
					   const SQLCHAR *szCatalogName,
					   SQLSMALLINT cbCatalogName,
					   const SQLCHAR *szSchemaName,
					   SQLSMALLINT cbSchemaName,
					   const SQLCHAR *szTableName,
					   SQLSMALLINT cbTableName,
					   const SQLCHAR *szColumnName,
					   SQLSMALLINT cbColumnName,
					   UWORD flag);
RETCODE SQL_API PGAPI_DescribeParam(
					HSTMT hstmt,
					SQLUSMALLINT ipar,
					SQLSMALLINT *pfSqlType,
					SQLULEN *pcbParamDef,
					SQLSMALLINT *pibScale,
					SQLSMALLINT *pfNullable);
RETCODE SQL_API PGAPI_ExtendedFetch(
					HSTMT hstmt,
					SQLUSMALLINT fFetchType,
					SQLLEN irow,
					SQLULEN *pcrow,
					SQLUSMALLINT *rgfRowStatus,
					SQLLEN FetchOffset,
					SQLLEN rowsetSize);
RETCODE SQL_API PGAPI_ForeignKeys(
				  HSTMT hstmt,
				  const SQLCHAR *szPkCatalogName,
				  SQLSMALLINT cbPkCatalogName,
				  const SQLCHAR *szPkSchemaName,
				  SQLSMALLINT cbPkSchemaName,
				  const SQLCHAR *szPkTableName,
				  SQLSMALLINT cbPkTableName,
				  const SQLCHAR *szFkCatalogName,
				  SQLSMALLINT cbFkCatalogName,
				  const SQLCHAR *szFkSchemaName,
				  SQLSMALLINT cbFkSchemaName,
				  const SQLCHAR *szFkTableName,
				  SQLSMALLINT cbFkTableName);
RETCODE SQL_API PGAPI_MoreResults(
				  HSTMT hstmt);
RETCODE SQL_API PGAPI_NativeSql(
				HDBC hdbc,
				const SQLCHAR *szSqlStrIn,
				SQLINTEGER cbSqlStrIn,
				SQLCHAR *szSqlStr,
				SQLINTEGER cbSqlStrMax,
				SQLINTEGER *pcbSqlStr);
RETCODE SQL_API PGAPI_NumParams(
				HSTMT hstmt,
				SQLSMALLINT *pcpar);
RETCODE SQL_API PGAPI_ParamOptions(
				   HSTMT hstmt,
				   SQLULEN crow,
				   SQLULEN *pirow);
RETCODE SQL_API PGAPI_PrimaryKeys(
				  HSTMT hstmt,
				  const SQLCHAR *szCatalogName,
				  SQLSMALLINT cbCatalogName,
				  const SQLCHAR *szSchemaName,
				  SQLSMALLINT cbSchemaName,
				  const SQLCHAR *szTableName,
				  SQLSMALLINT cbTableName,
				  OID	reloid);
RETCODE SQL_API PGAPI_ProcedureColumns(
					   HSTMT hstmt,
					   const SQLCHAR *szCatalogName,
					   SQLSMALLINT cbCatalogName,
					   const SQLCHAR *szSchemaName,
					   SQLSMALLINT cbSchemaName,
					   const SQLCHAR *szProcName,
					   SQLSMALLINT cbProcName,
					   const SQLCHAR *szColumnName,
					   SQLSMALLINT cbColumnName,
					   UWORD flag);
RETCODE SQL_API PGAPI_Procedures(
				 HSTMT hstmt,
				 const SQLCHAR *szCatalogName,
				 SQLSMALLINT cbCatalogName,
				 const SQLCHAR *szSchemaName,
				 SQLSMALLINT cbSchemaName,
				 const SQLCHAR *szProcName,
				 SQLSMALLINT cbProcName,
				UWORD flag);
RETCODE SQL_API PGAPI_SetPos(
			 HSTMT hstmt,
			 SQLSETPOSIROW irow,
			 SQLUSMALLINT fOption,
			 SQLUSMALLINT fLock);
RETCODE SQL_API PGAPI_TablePrivileges(
					  HSTMT hstmt,
					  const SQLCHAR *szCatalogName,
					  SQLSMALLINT cbCatalogName,
					  const SQLCHAR *szSchemaName,
					  SQLSMALLINT cbSchemaName,
					  const SQLCHAR *szTableName,
					  SQLSMALLINT cbTableName,
					  UWORD flag);
RETCODE SQL_API PGAPI_BindParameter(
					HSTMT hstmt,
					SQLUSMALLINT ipar,
					SQLSMALLINT fParamType,
					SQLSMALLINT fCType,
					SQLSMALLINT fSqlType,
					SQLULEN cbColDef,
					SQLSMALLINT ibScale,
					PTR rgbValue,
					SQLLEN cbValueMax,
					SQLLEN *pcbValue);
RETCODE SQL_API PGAPI_SetScrollOptions(
					   HSTMT hstmt,
					   SQLUSMALLINT fConcurrency,
					   SQLLEN crowKeyset,
					   SQLUSMALLINT crowRowset);

RETCODE SQL_API PGAPI_GetDiagRec(SQLSMALLINT HandleType, SQLHANDLE Handle,
		SQLSMALLINT RecNumber, SQLCHAR *Sqlstate,
		SQLINTEGER *NativeError, SQLCHAR *MessageText,
		SQLSMALLINT BufferLength, SQLSMALLINT *TextLength);
RETCODE SQL_API PGAPI_GetDiagField(SQLSMALLINT HandleType, SQLHANDLE Handle,
		SQLSMALLINT RecNumber, SQLSMALLINT DiagIdentifier,
		PTR DiagInfoPtr, SQLSMALLINT BufferLength,
		SQLSMALLINT *StringLengthPtr);
RETCODE SQL_API PGAPI_GetConnectAttr(HDBC ConnectionHandle,
			SQLINTEGER Attribute, PTR Value,
			SQLINTEGER BufferLength, SQLINTEGER *StringLength);
RETCODE SQL_API PGAPI_GetStmtAttr(HSTMT StatementHandle,
		SQLINTEGER Attribute, PTR Value,
		SQLINTEGER BufferLength, SQLINTEGER *StringLength);

/* Driver-specific connection attributes, for SQLSet/GetConnectAttr() */
enum {
	SQL_ATTR_PGOPT_DEBUG = 65536
	,SQL_ATTR_PGOPT_COMMLOG = 65537
	,SQL_ATTR_PGOPT_PARSE = 65538
	,SQL_ATTR_PGOPT_USE_DECLAREFETCH = 65539
	,SQL_ATTR_PGOPT_SERVER_SIDE_PREPARE = 65540
	,SQL_ATTR_PGOPT_FETCH = 65541
	,SQL_ATTR_PGOPT_UNKNOWNSIZES = 65542
	,SQL_ATTR_PGOPT_TEXTASLONGVARCHAR = 65543
	,SQL_ATTR_PGOPT_UNKNOWNSASLONGVARCHAR = 65544
	,SQL_ATTR_PGOPT_BOOLSASCHAR = 65545
	,SQL_ATTR_PGOPT_MAXVARCHARSIZE = 65546
	,SQL_ATTR_PGOPT_MAXLONGVARCHARSIZE = 65547
	,SQL_ATTR_PGOPT_WCSDEBUG = 65548
	,SQL_ATTR_PGOPT_MSJET = 65549
};
RETCODE SQL_API PGAPI_SetConnectAttr(HDBC ConnectionHandle,
			SQLINTEGER Attribute, PTR Value,
			SQLINTEGER StringLength);
RETCODE SQL_API PGAPI_SetStmtAttr(HSTMT StatementHandle,
		SQLINTEGER Attribute, PTR Value,
		SQLINTEGER StringLength);
RETCODE SQL_API PGAPI_BulkOperations(HSTMT StatementHandle,
			SQLSMALLINT operation);
RETCODE SQL_API PGAPI_AllocDesc(HDBC ConnectionHandle,
				SQLHDESC *DescriptorHandle);
RETCODE SQL_API PGAPI_FreeDesc(SQLHDESC DescriptorHandle);
RETCODE SQL_API PGAPI_CopyDesc(SQLHDESC SourceDescHandle,
				SQLHDESC TargetDescHandle);
RETCODE SQL_API PGAPI_SetDescField(SQLHDESC DescriptorHandle,
			SQLSMALLINT RecNumber, SQLSMALLINT FieldIdentifier,
			PTR Value, SQLINTEGER BufferLength);
RETCODE SQL_API PGAPI_GetDescField(SQLHDESC DescriptorHandle,
			SQLSMALLINT RecNumber, SQLSMALLINT FieldIdentifier,
			PTR Value, SQLINTEGER BufferLength, SQLINTEGER *StringLength);
RETCODE SQL_API PGAPI_DescError(SQLHDESC DescriptorHandle, SQLSMALLINT RecNumber,
			SQLCHAR *Sqlstate, SQLINTEGER *NativeError,
			SQLCHAR *MessageText, SQLSMALLINT BufferLength,
			SQLSMALLINT *TextLength, UWORD flag);

#ifdef	__cplusplus
}
#endif /* __cplusplus */
#endif   /* define_PG_API_FUNC_H__ */
