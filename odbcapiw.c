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
#include "unicode_support.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "pgapifunc.h"
#include "connection.h"
#include "statement.h"

RETCODE  SQL_API
SQLColumnsW(HSTMT StatementHandle,
			SQLWCHAR *CatalogName, SQLSMALLINT NameLength1,
			SQLWCHAR *SchemaName, SQLSMALLINT NameLength2,
			SQLWCHAR *TableName, SQLSMALLINT NameLength3,
			SQLWCHAR *ColumnName, SQLSMALLINT NameLength4)
{
	CSTR func = "SQLColumnsW";
	RETCODE	ret;
	char	*ctName, *scName, *tbName, *clName;
	SQLLEN	nmlen1, nmlen2, nmlen3, nmlen4;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	ConnectionClass *conn;
	UWORD	flag = PODBC_SEARCH_PUBLIC_SCHEMA;
	ConnInfo	*ci;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
	ctName = ucs2_to_utf8(CatalogName, NameLength1, &nmlen1, FALSE);
	scName = ucs2_to_utf8(SchemaName, NameLength2, &nmlen2, FALSE);
	tbName = ucs2_to_utf8(TableName, NameLength3, &nmlen3, FALSE);
	clName = ucs2_to_utf8(ColumnName, NameLength4, &nmlen4, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (pg_atoi(ci->show_oid_column))
		flag |= PODBC_SHOW_OID_COLUMN;
	if (pg_atoi(ci->row_versioning))
		flag |= PODBC_ROW_VERSIONING;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) tbName, nmlen3},
			{(const SQLCHAR *) clName, nmlen4}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_Columns(StatementHandle,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				(SQLCHAR *) names[3].name, (SQLSMALLINT) names[3].len,
				flag, 0, 0);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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


RETCODE  SQL_API
SQLConnectW(HDBC ConnectionHandle,
			SQLWCHAR *ServerName, SQLSMALLINT NameLength1,
			SQLWCHAR *UserName, SQLSMALLINT NameLength2,
			SQLWCHAR *Authentication, SQLSMALLINT NameLength3)
{
	char	*svName, *usName, *auth;
	SQLLEN	nmlen1, nmlen2, nmlen3;
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;

	MYLOG(0, "Entering\n");
	CC_examine_global_transaction(conn);
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	CC_set_in_unicode_driver(conn);
	svName = ucs2_to_utf8(ServerName, NameLength1, &nmlen1, FALSE);
	usName = ucs2_to_utf8(UserName, NameLength2, &nmlen2, FALSE);
	auth = ucs2_to_utf8(Authentication, NameLength3, &nmlen3, FALSE);
	/*
	 * The converted strings are NUL-terminated and their UTF-8 length
	 * need not fit in a SQLSMALLINT, so pass SQL_NTS rather than a
	 * narrowed length.
	 */
	ret = PGAPI_Connect(ConnectionHandle,
						(SQLCHAR *) svName, SQL_NTS,
						(SQLCHAR *) usName, SQL_NTS,
						(SQLCHAR *) auth, SQL_NTS);
	LEAVE_CONN_CS(conn);
	if (svName)
		free(svName);
	if (usName)
		free(usName);
	if (auth)
		free(auth);
	return ret;
}

RETCODE SQL_API
SQLDriverConnectW(HDBC hdbc,
				  HWND hwnd,
				  SQLWCHAR *szConnStrIn,
				  SQLSMALLINT cbConnStrIn,
				  SQLWCHAR *szConnStrOut,
				  SQLSMALLINT cbConnStrOutMax,
				  SQLSMALLINT *pcbConnStrOut,
				  SQLUSMALLINT fDriverCompletion)
{
	CSTR func = "SQLDriverConnectW";
	char	*szIn, *szOut = NULL;
	SQLSMALLINT	maxlen, obuflen = 0;
	SQLLEN		inlen;
	SQLSMALLINT	olen, *pCSO;
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	MYLOG(0, "Entering\n");
	CC_examine_global_transaction(conn);
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	CC_set_in_unicode_driver(conn);
	szIn = ucs2_to_utf8(szConnStrIn, cbConnStrIn, &inlen, FALSE);
	maxlen = cbConnStrOutMax;
	pCSO = NULL;
	olen = 0;
	if (maxlen > 0)
	{
		obuflen = maxlen + 1;
		szOut = malloc(obuflen);
		if (!szOut)
		{
			CC_set_error(conn, CONN_NO_MEMORY_ERROR, "Could not allocate memory for output buffer", func);
			ret = SQL_ERROR;
			goto cleanup;
		}
		pCSO = &olen;
	}
	else if (pcbConnStrOut)
		pCSO = &olen;
	/*
	 * szIn is NUL-terminated and its UTF-8 length need not fit in a
	 * SQLSMALLINT; a narrowed length would cut the connection string
	 * and change how the rest of it is parsed, so pass SQL_NTS.
	 */
	ret = PGAPI_DriverConnect(hdbc, hwnd,
							  (SQLCHAR *) szIn, SQL_NTS,
							  (SQLCHAR *) szOut, maxlen,
							  pCSO, fDriverCompletion);
	if (ret != SQL_ERROR && NULL != pCSO)
	{
		SQLLEN outlen = olen;

		if (olen < obuflen)
			outlen = utf8_to_ucs2(szOut, olen, szConnStrOut, cbConnStrOutMax);
		else
			utf8_to_ucs2(szOut, maxlen, szConnStrOut, cbConnStrOutMax);
		if (outlen >= cbConnStrOutMax && NULL != szConnStrOut && NULL != pcbConnStrOut)
		{
MYLOG(DETAIL_LOG_LEVEL, "cbConnstrOutMax=%d pcb=%p\n", cbConnStrOutMax, pcbConnStrOut);
			if (SQL_SUCCESS == ret)
			{
				CC_set_error(conn, CONN_TRUNCATED, "the ConnStrOut is too small", func);
				ret = SQL_SUCCESS_WITH_INFO;
			}
		}
		if (pcbConnStrOut)
			*pcbConnStrOut = (SQLSMALLINT) outlen;
	}
cleanup:
	LEAVE_CONN_CS(conn);
	if (szOut)
		free(szOut);
	if (szIn)
		free(szIn);
	return ret;
}
RETCODE SQL_API
SQLBrowseConnectW(HDBC			hdbc,
				  SQLWCHAR	   *szConnStrIn,
				  SQLSMALLINT	cbConnStrIn,
				  SQLWCHAR	   *szConnStrOut,
				  SQLSMALLINT	cbConnStrOutMax,
				  SQLSMALLINT  *pcbConnStrOut)
{
	CSTR func = "SQLBrowseConnectW";
	char	*szIn, *szOut;
	SQLLEN		inlen;
	SQLUSMALLINT	obuflen;
	SQLSMALLINT	olen;
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	MYLOG(0, "Entering\n");
	CC_examine_global_transaction(conn);
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	CC_set_in_unicode_driver(conn);
	szIn = ucs2_to_utf8(szConnStrIn, cbConnStrIn, &inlen, FALSE);
	obuflen = cbConnStrOutMax + 1;
	szOut = malloc(obuflen);
	if (szOut)
		ret = PGAPI_BrowseConnect(hdbc, (SQLCHAR *) szIn, SQL_NTS,
								  (SQLCHAR *) szOut, cbConnStrOutMax, &olen);
	else
	{
		CC_set_error(conn, CONN_NO_MEMORY_ERROR, "Could not allocate memory for output buffer", func);
		ret = SQL_ERROR;
	}
	LEAVE_CONN_CS(conn);
	if (ret != SQL_ERROR)
	{
		SQLLEN	outlen = utf8_to_ucs2(szOut, olen, szConnStrOut, cbConnStrOutMax);
		if (pcbConnStrOut)
			*pcbConnStrOut = (SQLSMALLINT) outlen;
	}
	free(szOut);
	if (szIn)
		free(szIn);
	return ret;
}

RETCODE  SQL_API
SQLDataSourcesW(HENV EnvironmentHandle,
				SQLUSMALLINT Direction, SQLWCHAR *ServerName,
				SQLSMALLINT BufferLength1, SQLSMALLINT *NameLength1,
				SQLWCHAR *Description, SQLSMALLINT BufferLength2,
				SQLSMALLINT *NameLength2)
{
	MYLOG(0, "Entering\n");
	/*
	return PGAPI_DataSources(EnvironmentHandle, Direction, ServerName,
		BufferLength1, NameLength1, Description, BufferLength2,
		NameLength2);
	*/
	return SQL_ERROR;
}

RETCODE  SQL_API
SQLDescribeColW(HSTMT StatementHandle,
				SQLUSMALLINT ColumnNumber, SQLWCHAR *ColumnName,
				SQLSMALLINT BufferLength, SQLSMALLINT *NameLength,
				SQLSMALLINT *DataType, SQLULEN *ColumnSize,
				SQLSMALLINT *DecimalDigits, SQLSMALLINT *Nullable)
{
	CSTR func = "SQLDescribeColW";
	RETCODE	ret;
	StatementClass	*stmt = (StatementClass *) StatementHandle;
	SQLSMALLINT	buflen, nmlen;
	char	*clName = NULL, *clNamet = NULL;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	buflen = 0;
	if (BufferLength > 0)
		buflen = BufferLength * 3;
	else if (NameLength)
		buflen = 32;
	if (buflen > 0)
		clNamet = malloc(buflen);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	for (;; buflen = nmlen + 1, clNamet = realloc(clName, buflen))
	{
		if (!clNamet)
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Could not allocate memory for column name", func);
			ret = SQL_ERROR;
			break;
		}
		clName = clNamet;
		ret = PGAPI_DescribeCol(StatementHandle, ColumnNumber,
								(SQLCHAR *) clName, buflen,
								&nmlen, DataType, ColumnSize,
			DecimalDigits, Nullable);
		if (SQL_SUCCESS_WITH_INFO != ret || nmlen < buflen)
			break;
	}
	if (SQL_SUCCEEDED(ret))
	{
		SQLLEN	nmcount = nmlen;

		if (nmlen < buflen)
			nmcount = utf8_to_ucs2(clName, nmlen, ColumnName, BufferLength);
		/* BufferLength counts the terminator, so an exact fit is cut too */
		if (SQL_SUCCESS == ret && BufferLength > 0 && nmcount >= BufferLength)
		{
			ret = SQL_SUCCESS_WITH_INFO;
			SC_set_error(stmt, STMT_TRUNCATED, "Column name too large", func);
		}
		if (NameLength)
			*NameLength = (SQLSMALLINT) nmcount;
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	if (clName)
		free(clName);
	return ret;
}

RETCODE  SQL_API
SQLExecDirectW(HSTMT StatementHandle,
			   SQLWCHAR *StatementText, SQLINTEGER TextLength)
{
	CSTR	func = "SQLExecDirectW";
	RETCODE	ret;
	char	*stxt;
	SQLLEN	slen;
	StatementClass	*stmt = (StatementClass *) StatementHandle;
	UWORD	flag = 0;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	stxt = ucs2_to_utf8(StatementText, TextLength, &slen, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	flag |= PODBC_WITH_HOLD;
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else if (stxt && slen > INT_MAX)
	{
		/* the UTF-8 text no longer fits the SQLINTEGER length below */
		SC_set_error(stmt, STMT_INVALID_ARGUMENT_NO, "Statement text is too long", func);
		ret = SQL_ERROR;
	}
	else
		ret = PGAPI_ExecDirect(StatementHandle,
							   (SQLCHAR *) stxt, (SQLINTEGER) slen, flag);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	if (stxt)
		free(stxt);
	return ret;
}

RETCODE  SQL_API
SQLGetCursorNameW(HSTMT StatementHandle,
				  SQLWCHAR *CursorName, SQLSMALLINT BufferLength,
				  SQLSMALLINT *NameLength)
{
	CSTR func = "SQLGetCursorNameW";
	RETCODE	ret;
	StatementClass * stmt = (StatementClass *) StatementHandle;
	char	*crName = NULL, *crNamet;
	SQLSMALLINT	clen, buflen;

	MYLOG(0, "Entering\n");
	if (BufferLength > 0)
		buflen = BufferLength * 3;
	else
		buflen = 32;
	crNamet = malloc(buflen);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	for (;; buflen = clen + 1, crNamet = realloc(crName, buflen))
	{
		if (!crNamet)
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Could not allocate memory for cursor name", func);
			ret = SQL_ERROR;
			break;
		}
		crName = crNamet;
		ret = PGAPI_GetCursorName(StatementHandle, (SQLCHAR *) crName, buflen, &clen);
		if (SQL_SUCCESS_WITH_INFO != ret || clen < buflen)
			break;
	}
	if (SQL_SUCCEEDED(ret))
	{
		SQLLEN	nmcount = clen;

		if (clen < buflen)
			nmcount = utf8_to_ucs2(crName, clen, CursorName, BufferLength);
		/* BufferLength counts the terminator, so an exact fit is cut too */
		if (SQL_SUCCESS == ret && nmcount >= BufferLength)
		{
			ret = SQL_SUCCESS_WITH_INFO;
			SC_set_error(stmt, STMT_TRUNCATED, "Cursor name too large", func);
		}
		if (NameLength)
			*NameLength = (SQLSMALLINT) nmcount;
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	free(crName);
	return ret;
}

RETCODE  SQL_API
SQLGetInfoW(HDBC ConnectionHandle,
			SQLUSMALLINT InfoType, PTR InfoValue,
			SQLSMALLINT BufferLength, SQLSMALLINT *StringLength)
{
	ConnectionClass	*conn = (ConnectionClass *) ConnectionHandle;
	RETCODE	ret;

	CC_examine_global_transaction(conn);
	ENTER_CONN_CS(conn);
	CC_set_in_unicode_driver(conn);
	CC_clear_error(conn);
	MYLOG(0, "Entering\n");
	if ((ret = PGAPI_GetInfo(ConnectionHandle, InfoType, InfoValue,
							 BufferLength, StringLength)) == SQL_ERROR)
		CC_log_error("SQLGetInfoW", "", conn);
	LEAVE_CONN_CS(conn);
	return ret;
}

RETCODE  SQL_API
SQLPrepareW(HSTMT StatementHandle,
			SQLWCHAR *StatementText, SQLINTEGER TextLength)
{
	CSTR func = "SQLPrepareW";
	StatementClass *stmt = (StatementClass *) StatementHandle;
	RETCODE	ret;
	char	*stxt;
	SQLLEN	slen;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	stxt = ucs2_to_utf8(StatementText, TextLength, &slen, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else if (stxt && slen > INT_MAX)
	{
		/* the UTF-8 text no longer fits the SQLINTEGER length below */
		SC_set_error(stmt, STMT_INVALID_ARGUMENT_NO, "Statement text is too long", func);
		ret = SQL_ERROR;
	}
	else
		ret = PGAPI_Prepare(StatementHandle, (SQLCHAR *) stxt, (SQLINTEGER) slen);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	if (stxt)
		free(stxt);
	return ret;
}

RETCODE  SQL_API
SQLSetCursorNameW(HSTMT StatementHandle,
				  SQLWCHAR *CursorName, SQLSMALLINT NameLength)
{
	RETCODE	ret;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	char	*crName;
	SQLLEN	nlen;

	MYLOG(0, "Entering\n");
	crName = ucs2_to_utf8(CursorName, NameLength, &nlen, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_SetCursorName(StatementHandle, (SQLCHAR *) crName, (SQLSMALLINT) nlen);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	if (crName)
		free(crName);
	return ret;
}

RETCODE  SQL_API
SQLSpecialColumnsW(HSTMT StatementHandle,
				   SQLUSMALLINT IdentifierType, SQLWCHAR *CatalogName,
				   SQLSMALLINT NameLength1, SQLWCHAR *SchemaName,
				   SQLSMALLINT NameLength2, SQLWCHAR *TableName,
				   SQLSMALLINT NameLength3, SQLUSMALLINT Scope,
				   SQLUSMALLINT Nullable)
{
	CSTR func = "SQLSpecialColumnsW";
	RETCODE	ret;
	char	*ctName, *scName, *tbName;
	SQLLEN	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ctName = ucs2_to_utf8(CatalogName, NameLength1, &nmlen1, FALSE);
	scName = ucs2_to_utf8(SchemaName, NameLength2, &nmlen2, FALSE);
	tbName = ucs2_to_utf8(TableName, NameLength3, &nmlen3, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) tbName, nmlen3}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_SpecialColumns(StatementHandle, IdentifierType,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				Scope, Nullable);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	return ret;
}

RETCODE  SQL_API
SQLStatisticsW(HSTMT StatementHandle,
			   SQLWCHAR *CatalogName, SQLSMALLINT NameLength1,
			   SQLWCHAR *SchemaName, SQLSMALLINT NameLength2,
			   SQLWCHAR *TableName, SQLSMALLINT NameLength3,
			   SQLUSMALLINT Unique, SQLUSMALLINT Reserved)
{
	CSTR func = "SQLStatisticsW";
	RETCODE	ret;
	char	*ctName, *scName, *tbName;
	SQLLEN	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) StatementHandle;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ctName = ucs2_to_utf8(CatalogName, NameLength1, &nmlen1, FALSE);
	scName = ucs2_to_utf8(SchemaName, NameLength2, &nmlen2, FALSE);
	tbName = ucs2_to_utf8(TableName, NameLength3, &nmlen3, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) tbName, nmlen3}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_Statistics(StatementHandle,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				Unique, Reserved);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	return ret;
}

RETCODE  SQL_API
SQLTablesW(HSTMT StatementHandle,
           SQLWCHAR *CatalogName, SQLSMALLINT NameLength1,
           SQLWCHAR *SchemaName, SQLSMALLINT NameLength2,
           SQLWCHAR *TableName, SQLSMALLINT NameLength3,
           SQLWCHAR *TableType, SQLSMALLINT NameLength4)
{
	CSTR func = "SQLTablesW";
	RETCODE	ret;
	char	*ctName, *scName, *tbName, *tbType;
	SQLLEN	nmlen1, nmlen2, nmlen3, nmlen4;
	StatementClass *stmt = (StatementClass *) StatementHandle;
	UWORD	flag = 0;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ctName = ucs2_to_utf8(CatalogName, NameLength1, &nmlen1, FALSE);
	scName = ucs2_to_utf8(SchemaName, NameLength2, &nmlen2, FALSE);
	tbName = ucs2_to_utf8(TableName, NameLength3, &nmlen3, FALSE);
	tbType = ucs2_to_utf8(TableType, NameLength4, &nmlen4, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) tbName, nmlen3}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_Tables(StatementHandle,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				(SQLCHAR *) tbType, (SQLSMALLINT) nmlen4, flag);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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

RETCODE SQL_API
SQLColumnPrivilegesW(HSTMT			hstmt,
					 SQLWCHAR	   *szCatalogName,
					 SQLSMALLINT	cbCatalogName,
					 SQLWCHAR	   *szSchemaName,
					 SQLSMALLINT	cbSchemaName,
					 SQLWCHAR	   *szTableName,
					 SQLSMALLINT	cbTableName,
					 SQLWCHAR	   *szColumnName,
					 SQLSMALLINT	cbColumnName)
{
	CSTR func = "SQLColumnPrivilegesW";
	RETCODE	ret;
	char	*ctName, *scName, *tbName, *clName;
	SQLLEN	nmlen1, nmlen2, nmlen3, nmlen4;
	StatementClass *stmt = (StatementClass *) hstmt;
	UWORD	flag = 0;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, FALSE);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, FALSE);
	tbName = ucs2_to_utf8(szTableName, cbTableName, &nmlen3, FALSE);
	clName = ucs2_to_utf8(szColumnName, cbColumnName, &nmlen4, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) tbName, nmlen3},
			{(const SQLCHAR *) clName, nmlen4}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_ColumnPrivileges(hstmt,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				(SQLCHAR *) names[3].name, (SQLSMALLINT) names[3].len,
				flag);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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

RETCODE SQL_API
SQLForeignKeysW(HSTMT			hstmt,
				SQLWCHAR	   *szPkCatalogName,
				SQLSMALLINT		cbPkCatalogName,
				SQLWCHAR	   *szPkSchemaName,
				SQLSMALLINT		cbPkSchemaName,
				SQLWCHAR	   *szPkTableName,
				SQLSMALLINT		cbPkTableName,
				SQLWCHAR	   *szFkCatalogName,
				SQLSMALLINT		cbFkCatalogName,
				SQLWCHAR	   *szFkSchemaName,
				SQLSMALLINT		cbFkSchemaName,
				SQLWCHAR	   *szFkTableName,
				SQLSMALLINT		cbFkTableName)
{
	CSTR func = "SQLForeignKeysW";
	RETCODE	ret;
	char	*ctName, *scName, *tbName, *fkctName, *fkscName, *fktbName;
	SQLLEN	nmlen1, nmlen2, nmlen3, nmlen4, nmlen5, nmlen6;
	StatementClass *stmt = (StatementClass *) hstmt;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ctName = ucs2_to_utf8(szPkCatalogName, cbPkCatalogName, &nmlen1, FALSE);
	scName = ucs2_to_utf8(szPkSchemaName, cbPkSchemaName, &nmlen2, FALSE);
	tbName = ucs2_to_utf8(szPkTableName, cbPkTableName, &nmlen3, FALSE);
	fkctName = ucs2_to_utf8(szFkCatalogName, cbFkCatalogName, &nmlen4, FALSE);
	fkscName = ucs2_to_utf8(szFkSchemaName, cbFkSchemaName, &nmlen5, FALSE);
	fktbName = ucs2_to_utf8(szFkTableName, cbFkTableName, &nmlen6, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) tbName, nmlen3},
			{(const SQLCHAR *) fkctName, nmlen4, TRUE},
			{(const SQLCHAR *) fkscName, nmlen5},
			{(const SQLCHAR *) fktbName, nmlen6}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_ForeignKeys(hstmt,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				(SQLCHAR *) names[3].name, (SQLSMALLINT) names[3].len,
				(SQLCHAR *) names[4].name, (SQLSMALLINT) names[4].len,
				(SQLCHAR *) names[5].name, (SQLSMALLINT) names[5].len);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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

RETCODE SQL_API
SQLNativeSqlW(HDBC			hdbc,
			  SQLWCHAR	   *szSqlStrIn,
			  SQLINTEGER	cbSqlStrIn,
			  SQLWCHAR	   *szSqlStr,
			  SQLINTEGER	cbSqlStrMax,
			  SQLINTEGER   *pcbSqlStr)
{
	CSTR func = "SQLNativeSqlW";
	RETCODE		ret;
	char		*szIn, *szOut = NULL, *szOutt = NULL;
	SQLLEN		slen;
	SQLINTEGER	buflen, olen;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	MYLOG(0, "Entering\n");
	CC_examine_global_transaction(conn);
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	CC_set_in_unicode_driver(conn);
	szIn = ucs2_to_utf8(szSqlStrIn, cbSqlStrIn, &slen, FALSE);
	if (szIn && slen > INT_MAX)
	{
		/* the UTF-8 text no longer fits the SQLINTEGER length below */
		CC_set_error(conn, CONN_INVALID_ARGUMENT_NO, "Sql string too long", func);
		free(szIn);
		LEAVE_CONN_CS(conn);
		return SQL_ERROR;
	}
	buflen = 3 * cbSqlStrMax;
	if (buflen > 0)
		szOutt = malloc(buflen);
	for (;; buflen = olen + 1, szOutt = realloc(szOut, buflen))
	{
		if (!szOutt)
		{
			CC_set_error(conn, CONN_NO_MEMORY_ERROR, "Could not allocate memory for output buffer", func);
			ret = SQL_ERROR;
			break;
		}
		szOut = szOutt;
		ret = PGAPI_NativeSql(hdbc, (SQLCHAR *) szIn, (SQLINTEGER) slen,
							  (SQLCHAR *) szOut, buflen, &olen);
		if (SQL_SUCCESS_WITH_INFO != ret || olen < buflen)
			break;
	}
	if (szIn)
		free(szIn);
	if (SQL_SUCCEEDED(ret))
	{
		SQLLEN	szcount = olen;

		if (olen < buflen)
			szcount = utf8_to_ucs2(szOut, olen, szSqlStr, cbSqlStrMax);
		/* cbSqlStrMax counts the terminator, so an exact fit is cut too */
		if (SQL_SUCCESS == ret && szcount >= cbSqlStrMax)
		{
			ConnectionClass	*conn = (ConnectionClass *) hdbc;

			ret = SQL_SUCCESS_WITH_INFO;
			CC_set_error(conn, CONN_TRUNCATED, "Sql string too large", func);
		}
		if (pcbSqlStr)
			*pcbSqlStr = (SQLINTEGER) szcount;
	}
	LEAVE_CONN_CS(conn);
	free(szOut);
	return ret;
}

RETCODE SQL_API
SQLPrimaryKeysW(HSTMT			hstmt,
				SQLWCHAR	   *szCatalogName,
				SQLSMALLINT		cbCatalogName,
				SQLWCHAR	   *szSchemaName,
				SQLSMALLINT		cbSchemaName,
				SQLWCHAR	   *szTableName,
				SQLSMALLINT		cbTableName)
{
	CSTR func = "SQLPrimaryKeysW";
	RETCODE	ret;
	char	*ctName, *scName, *tbName;
	SQLLEN	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) hstmt;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, FALSE);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, FALSE);
	tbName = ucs2_to_utf8(szTableName, cbTableName, &nmlen3, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) tbName, nmlen3}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_PrimaryKeys(hstmt,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				0);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	return ret;
}

RETCODE SQL_API
SQLProcedureColumnsW(HSTMT			hstmt,
					 SQLWCHAR	   *szCatalogName,
					 SQLSMALLINT	cbCatalogName,
					 SQLWCHAR	   *szSchemaName,
					 SQLSMALLINT	cbSchemaName,
					 SQLWCHAR	   *szProcName,
					 SQLSMALLINT	cbProcName,
					 SQLWCHAR	   *szColumnName,
					 SQLSMALLINT	cbColumnName)
{
	CSTR func = "SQLProcedureColumnsW";
	RETCODE	ret;
	char	*ctName, *scName, *prName, *clName;
	SQLLEN	nmlen1, nmlen2, nmlen3, nmlen4;
	StatementClass *stmt = (StatementClass *) hstmt;
	UWORD	flag = 0;

	MYLOG(0, "Entering\n");
	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, FALSE);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, FALSE);
	prName = ucs2_to_utf8(szProcName, cbProcName, &nmlen3, FALSE);
	clName = ucs2_to_utf8(szColumnName, cbColumnName, &nmlen4, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) prName, nmlen3},
			{(const SQLCHAR *) clName, nmlen4}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_ProcedureColumns(hstmt,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				(SQLCHAR *) names[3].name, (SQLSMALLINT) names[3].len,
				flag);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
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

RETCODE SQL_API
SQLProceduresW(HSTMT		hstmt,
			   SQLWCHAR	   *szCatalogName,
			   SQLSMALLINT	cbCatalogName,
			   SQLWCHAR	   *szSchemaName,
			   SQLSMALLINT	cbSchemaName,
			   SQLWCHAR	   *szProcName,
			   SQLSMALLINT	cbProcName)
{
	CSTR func = "SQLProceduresW";
	RETCODE	ret;
	char	*ctName, *scName, *prName;
	SQLLEN	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) hstmt;
	UWORD	flag = 0;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, FALSE);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, FALSE);
	prName = ucs2_to_utf8(szProcName, cbProcName, &nmlen3, FALSE);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) prName, nmlen3}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_Procedures(hstmt,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				flag);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (prName)
		free(prName);
	return ret;
}

RETCODE SQL_API
SQLTablePrivilegesW(HSTMT			hstmt,
					SQLWCHAR	   *szCatalogName,
					SQLSMALLINT		cbCatalogName,
					SQLWCHAR	   *szSchemaName,
					SQLSMALLINT		cbSchemaName,
					SQLWCHAR	   *szTableName,
					SQLSMALLINT		cbTableName)
{
	CSTR func = "SQLTablePrivilegesW";
	RETCODE	ret;
	char	*ctName, *scName, *tbName;
	SQLLEN	nmlen1, nmlen2, nmlen3;
	StatementClass *stmt = (StatementClass *) hstmt;
	UWORD	flag = 0;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ctName = ucs2_to_utf8(szCatalogName, cbCatalogName, &nmlen1, FALSE);
	scName = ucs2_to_utf8(szSchemaName, cbSchemaName, &nmlen2, FALSE);
	tbName = ucs2_to_utf8(szTableName, cbTableName, &nmlen3, FALSE);
	ENTER_STMT_CS((StatementClass *) hstmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (stmt->options.metadata_id)
		flag |= PODBC_NOT_SEARCH_PATTERN;
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
	{
		CatalogNameArg names[] = {
			{(const SQLCHAR *) ctName, nmlen1, TRUE},
			{(const SQLCHAR *) scName, nmlen2},
			{(const SQLCHAR *) tbName, nmlen3}
		};
		const int	nnames = sizeof(names) / sizeof(names[0]);

		if (SC_fold_catalog_names(stmt, names, nnames))
			ret = PGAPI_TablePrivileges(hstmt,
				(SQLCHAR *) names[0].name, (SQLSMALLINT) names[0].len,
				(SQLCHAR *) names[1].name, (SQLSMALLINT) names[1].len,
				(SQLCHAR *) names[2].name, (SQLSMALLINT) names[2].len,
				flag);
		else
			ret = SQL_ERROR;
		ret = SC_finish_catalog_names(stmt, ret, names, nnames);
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS((StatementClass *) hstmt);
	if (ctName)
		free(ctName);
	if (scName)
		free(scName);
	if (tbName)
		free(tbName);
	return ret;
}

RETCODE SQL_API
SQLGetTypeInfoW(SQLHSTMT	StatementHandle,
				SQLSMALLINT	DataType)
{
	CSTR func = "SQLGetTypeInfoW";
	RETCODE	ret;
	StatementClass * stmt = (StatementClass *) StatementHandle;

	MYLOG(0, "Entering\n");
	if (SC_connection_lost_check(stmt, __FUNCTION__))
		return SQL_ERROR;

	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	if (SC_opencheck(stmt, func))
		ret = SQL_ERROR;
	else
		ret = PGAPI_GetTypeInfo(StatementHandle, DataType);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}
