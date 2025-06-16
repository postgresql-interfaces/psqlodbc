/*-------
 * Module:			pgapi30.c
 *
 * Description:		This module contains routines related to ODBC 3.0
 *			most of their implementations are temporary
 *			and must be rewritten properly.
 *			2001/07/23	inoue
 *
 * Classes:			n/a
 *
 * API functions:	PGAPI_ColAttribute, PGAPI_GetDiagRec,
			PGAPI_GetConnectAttr, PGAPI_GetStmtAttr,
			PGAPI_SetConnectAttr, PGAPI_SetStmtAttr
 *-------
 */

#include "psqlodbc.h"
#include "misc.h"

#include <stdio.h>
#include <string.h>


#include "environ.h"
#include "connection.h"
#include "statement.h"
#include "descriptor.h"
#include "qresult.h"
#include "pgapifunc.h"
#include "loadlib.h"
#include "dlg_specific.h"


/*	SQLError -> SQLDiagRec */
RETCODE		SQL_API
PGAPI_GetDiagRec(SQLSMALLINT HandleType, SQLHANDLE Handle,
				 SQLSMALLINT RecNumber, SQLCHAR *Sqlstate,
				 SQLINTEGER *NativeError, SQLCHAR *MessageText,
				 SQLSMALLINT BufferLength, SQLSMALLINT *TextLength)
{
	RETCODE		ret;

	MYLOG(0, "entering type=%d rec=%d buffer=%d\n", HandleType, RecNumber, BufferLength);
	switch (HandleType)
	{
		case SQL_HANDLE_ENV:
			ret = PGAPI_EnvError(Handle, RecNumber, Sqlstate,
					NativeError, MessageText,
					BufferLength, TextLength, 0);
			break;
		case SQL_HANDLE_DBC:
			ret = PGAPI_ConnectError(Handle, RecNumber, Sqlstate,
					NativeError, MessageText, BufferLength,
					TextLength, 0);
			break;
		case SQL_HANDLE_STMT:
			ret = PGAPI_StmtError(Handle, RecNumber, Sqlstate,
					NativeError, MessageText, BufferLength,
					TextLength, 0);
			break;
		case SQL_HANDLE_DESC:
			ret = PGAPI_DescError(Handle, RecNumber, Sqlstate,
					NativeError,
					MessageText, BufferLength,
					TextLength, 0);
			break;
		default:
			ret = SQL_ERROR;
	}
	MYLOG(0, "leaving %d\n", ret);
	return ret;
}

/*
 *	Minimal implementation.
 *
 */
RETCODE		SQL_API
PGAPI_GetDiagField(SQLSMALLINT HandleType, SQLHANDLE Handle,
				   SQLSMALLINT RecNumber, SQLSMALLINT DiagIdentifier,
				   PTR DiagInfoPtr, SQLSMALLINT BufferLength,
				   SQLSMALLINT *StringLengthPtr)
{
	RETCODE		ret = SQL_ERROR, rtn;
	ConnectionClass	*conn;
	StatementClass	*stmt;
	SQLLEN		rc;
	SQLSMALLINT	pcbErrm;
	ssize_t		rtnlen = -1;
	int		rtnctype = SQL_C_CHAR;

	MYLOG(0, "entering rec=%d\n", RecNumber);
	switch (HandleType)
	{
		case SQL_HANDLE_ENV:
			switch (DiagIdentifier)
			{
				case SQL_DIAG_CLASS_ORIGIN:
				case SQL_DIAG_SUBCLASS_ORIGIN:
				case SQL_DIAG_CONNECTION_NAME:
				case SQL_DIAG_SERVER_NAME:
					rtnlen = 0;
					if (DiagInfoPtr && BufferLength > rtnlen)
					{
						ret = SQL_SUCCESS;
						*((char *) DiagInfoPtr) = '\0';
					}
					else
						ret = SQL_SUCCESS_WITH_INFO;
					break;
				case SQL_DIAG_MESSAGE_TEXT:
					ret = PGAPI_EnvError(Handle, RecNumber,
										 NULL, NULL, DiagInfoPtr,
										 BufferLength, StringLengthPtr, 0);
					break;
				case SQL_DIAG_NATIVE:
					rtnctype = SQL_C_LONG;
					ret = PGAPI_EnvError(Handle, RecNumber,
										 NULL, (SQLINTEGER *) DiagInfoPtr, NULL,
										 0, NULL, 0);
					break;
				case SQL_DIAG_NUMBER:
					rtnctype = SQL_C_LONG;
					ret = PGAPI_EnvError(Handle, RecNumber,
										 NULL, NULL, NULL,
										 0, NULL, 0);
					if (SQL_SUCCEEDED(ret))
					{
						*((SQLINTEGER *) DiagInfoPtr) = 1;
					}
					break;
				case SQL_DIAG_SQLSTATE:
					rtnlen = 5;
					ret = PGAPI_EnvError(Handle, RecNumber,
										 DiagInfoPtr, NULL, NULL,
										 0, NULL, 0);
					if (SQL_SUCCESS_WITH_INFO == ret)
						ret = SQL_SUCCESS;
					break;
				case SQL_DIAG_RETURNCODE: /* driver manager returns */
					break;
				case SQL_DIAG_CURSOR_ROW_COUNT:
				case SQL_DIAG_ROW_COUNT:
				case SQL_DIAG_DYNAMIC_FUNCTION:
				case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
					/* options for statement type only */
					break;
			}
			break;
		case SQL_HANDLE_DBC:
			conn = (ConnectionClass *) Handle;
			switch (DiagIdentifier)
			{
				case SQL_DIAG_CLASS_ORIGIN:
				case SQL_DIAG_SUBCLASS_ORIGIN:
				case SQL_DIAG_CONNECTION_NAME:
					rtnlen = 0;
					if (DiagInfoPtr && BufferLength > rtnlen)
					{
						ret = SQL_SUCCESS;
						*((char *) DiagInfoPtr) = '\0';
					}
					else
						ret = SQL_SUCCESS_WITH_INFO;
					break;
				case SQL_DIAG_SERVER_NAME:
					rtnlen = strlen(CC_get_DSN(conn));
					if (DiagInfoPtr)
					{
						strncpy_null(DiagInfoPtr, CC_get_DSN(conn), BufferLength);
						ret = (BufferLength > rtnlen ? SQL_SUCCESS : SQL_SUCCESS_WITH_INFO);
					}
					else
						ret = SQL_SUCCESS_WITH_INFO;
					break;
				case SQL_DIAG_MESSAGE_TEXT:
					ret = PGAPI_ConnectError(Handle, RecNumber,
											 NULL, NULL, DiagInfoPtr,
											 BufferLength, StringLengthPtr, 0);
					break;
				case SQL_DIAG_NATIVE:
					rtnctype = SQL_C_LONG;
					ret = PGAPI_ConnectError(Handle, RecNumber,
											 NULL, (SQLINTEGER *) DiagInfoPtr, NULL,
											 0, NULL, 0);
					break;
				case SQL_DIAG_NUMBER:
					ret = SQL_NO_DATA_FOUND;
					*((SQLINTEGER *) DiagInfoPtr) = 0;
					rtnctype = SQL_C_LONG;
					{
						SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH + 1];
						ret = PGAPI_ConnectError(Handle, 1,
											 NULL, NULL, msg,
											 sizeof(msg), &pcbErrm, 0);
						MYLOG(0, "pcbErrm=%d\n", pcbErrm);
					}
					if (SQL_SUCCEEDED(ret))
					{
						*((SQLINTEGER *) DiagInfoPtr) = (pcbErrm - 1) / SQL_MAX_MESSAGE_LENGTH + 1;
						ret = SQL_SUCCESS;
					}
					break;
				case SQL_DIAG_SQLSTATE:
					rtnlen = 5;
					ret = PGAPI_ConnectError(Handle, RecNumber,
											 DiagInfoPtr, NULL, NULL,
											 0, NULL, 0);
					if (SQL_SUCCESS_WITH_INFO == ret)
						ret = SQL_SUCCESS;
					break;
				case SQL_DIAG_RETURNCODE: /* driver manager returns */
					break;
				case SQL_DIAG_CURSOR_ROW_COUNT:
				case SQL_DIAG_ROW_COUNT:
				case SQL_DIAG_DYNAMIC_FUNCTION:
				case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
					/* options for statement type only */
					break;
			}
			break;
		case SQL_HANDLE_STMT:
			conn = (ConnectionClass *) SC_get_conn(((StatementClass *) Handle));
			switch (DiagIdentifier)
			{
				case SQL_DIAG_CLASS_ORIGIN:
				case SQL_DIAG_SUBCLASS_ORIGIN:
				case SQL_DIAG_CONNECTION_NAME:
					rtnlen = 0;
					if (DiagInfoPtr && BufferLength > rtnlen)
					{
						ret = SQL_SUCCESS;
						*((char *) DiagInfoPtr) = '\0';
					}
					else
						ret = SQL_SUCCESS_WITH_INFO;
					break;
				case SQL_DIAG_SERVER_NAME:
					rtnlen = strlen(CC_get_DSN(conn));
					if (DiagInfoPtr)
					{
						strncpy_null(DiagInfoPtr, CC_get_DSN(conn), BufferLength);
						ret = (BufferLength > rtnlen ? SQL_SUCCESS : SQL_SUCCESS_WITH_INFO);
					}
					else
						ret = SQL_SUCCESS_WITH_INFO;
					break;
				case SQL_DIAG_MESSAGE_TEXT:
					ret = PGAPI_StmtError(Handle, RecNumber,
										  NULL, NULL, DiagInfoPtr,
										  BufferLength, StringLengthPtr, 0);
					break;
				case SQL_DIAG_NATIVE:
					rtnctype = SQL_C_LONG;
					ret = PGAPI_StmtError(Handle, RecNumber,
										  NULL, (SQLINTEGER *) DiagInfoPtr, NULL,
										  0, NULL, 0);
					break;
				case SQL_DIAG_NUMBER:
					rtnctype = SQL_C_LONG;
					*((SQLINTEGER *) DiagInfoPtr) = 0;
					ret = SQL_NO_DATA_FOUND;
					stmt = (StatementClass *) Handle;
					rtn = PGAPI_StmtError(Handle, -1, NULL,
						 NULL, NULL, 0, &pcbErrm, 0);
					switch (rtn)
					{
						case SQL_SUCCESS:
						case SQL_SUCCESS_WITH_INFO:
							ret = SQL_SUCCESS;
							if (pcbErrm > 0 && stmt->pgerror)

								*((SQLINTEGER *) DiagInfoPtr) = (pcbErrm  - 1)/ stmt->pgerror->recsize + 1;
							break;
						default:
							break;
					}
					break;
				case SQL_DIAG_SQLSTATE:
					rtnlen = 5;
					ret = PGAPI_StmtError(Handle, RecNumber,
										  DiagInfoPtr, NULL, NULL,
										  0, NULL, 0);
					if (SQL_SUCCESS_WITH_INFO == ret)
						ret = SQL_SUCCESS;
					break;
				case SQL_DIAG_CURSOR_ROW_COUNT:
					rtnctype = SQL_C_LONG;
					stmt = (StatementClass *) Handle;
					rc = -1;
					if (stmt->status == STMT_FINISHED)
					{
						QResultClass *res = SC_get_Curres(stmt);

						/*if (!res)
							return SQL_ERROR;*/
						if (stmt->proc_return > 0)
							rc = 0;
						else if (res && QR_NumResultCols(res) > 0 && !SC_is_fetchcursor(stmt))
							rc = QR_get_num_total_tuples(res) - res->dl_count;
					}
					*((SQLLEN *) DiagInfoPtr) = rc;
MYLOG(DETAIL_LOG_LEVEL, "rc=" FORMAT_LEN "\n", rc);
					ret = SQL_SUCCESS;
					break;
				case SQL_DIAG_ROW_COUNT:
					rtnctype = SQL_C_LONG;
					stmt = (StatementClass *) Handle;
					*((SQLLEN *) DiagInfoPtr) = stmt->diag_row_count;
					ret = SQL_SUCCESS;
					break;
                                case SQL_DIAG_ROW_NUMBER:
					rtnctype = SQL_C_LONG;
					*((SQLLEN *) DiagInfoPtr) = SQL_ROW_NUMBER_UNKNOWN;
					ret = SQL_SUCCESS;
					break;
                                case SQL_DIAG_COLUMN_NUMBER:
					rtnctype = SQL_C_LONG;
					*((SQLINTEGER *) DiagInfoPtr) = SQL_COLUMN_NUMBER_UNKNOWN;
					ret = SQL_SUCCESS;
					break;
				case SQL_DIAG_RETURNCODE: /* driver manager returns */
					break;
			}
			break;
		case SQL_HANDLE_DESC:
			conn = DC_get_conn(((DescriptorClass *) Handle));
			switch (DiagIdentifier)
			{
				case SQL_DIAG_CLASS_ORIGIN:
				case SQL_DIAG_SUBCLASS_ORIGIN:
				case SQL_DIAG_CONNECTION_NAME:
					rtnlen = 0;
					if (DiagInfoPtr && BufferLength > rtnlen)
					{
						ret = SQL_SUCCESS;
						*((char *) DiagInfoPtr) = '\0';
					}
					else
						ret = SQL_SUCCESS_WITH_INFO;
					break;
				case SQL_DIAG_SERVER_NAME:
					rtnlen = strlen(CC_get_DSN(conn));
					if (DiagInfoPtr)
					{
						strncpy_null(DiagInfoPtr, CC_get_DSN(conn), BufferLength);
						ret = (BufferLength > rtnlen ? SQL_SUCCESS : SQL_SUCCESS_WITH_INFO);
					}
					else
						ret = SQL_SUCCESS_WITH_INFO;
					break;
				case SQL_DIAG_MESSAGE_TEXT:
				case SQL_DIAG_NATIVE:
				case SQL_DIAG_NUMBER:
					break;
				case SQL_DIAG_SQLSTATE:
					rtnlen = 5;
					ret = PGAPI_DescError(Handle, RecNumber,
										  DiagInfoPtr, NULL, NULL,
										  0, NULL, 0);
					if (SQL_SUCCESS_WITH_INFO == ret)
						ret = SQL_SUCCESS;
					break;
				case SQL_DIAG_RETURNCODE: /* driver manager returns */
					break;
				case SQL_DIAG_CURSOR_ROW_COUNT:
				case SQL_DIAG_ROW_COUNT:
				case SQL_DIAG_DYNAMIC_FUNCTION:
				case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
					rtnctype = SQL_C_LONG;
					/* options for statement type only */
					break;
			}
			break;
		default:
			ret = SQL_ERROR;
	}
	if (SQL_C_LONG == rtnctype)
	{
		if (SQL_SUCCESS_WITH_INFO == ret)
			ret = SQL_SUCCESS;
		if (StringLengthPtr)
			*StringLengthPtr = sizeof(SQLINTEGER);
	}
	else if (rtnlen >= 0)
	{
		if (rtnlen >= BufferLength)
		{
			if (SQL_SUCCESS == ret)
				ret = SQL_SUCCESS_WITH_INFO;
			if (BufferLength > 0)
				((char *) DiagInfoPtr) [BufferLength - 1] = '\0';
		}
		if (StringLengthPtr)
			*StringLengthPtr = (SQLSMALLINT) rtnlen;
	}
	MYLOG(0, "leaving %d\n", ret);
	return ret;
}

/*	SQLGetConnectOption -> SQLGetconnectAttr */
RETCODE		SQL_API
PGAPI_GetConnectAttr(HDBC ConnectionHandle,
					 SQLINTEGER Attribute, PTR Value,
					 SQLINTEGER BufferLength, SQLINTEGER *StringLength)
{
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;
	RETCODE	ret = SQL_SUCCESS;
	SQLINTEGER	len = 4;

	MYLOG(0, "entering " FORMAT_INTEGER "\n", Attribute);
	switch (Attribute)
	{
		case SQL_ATTR_ASYNC_ENABLE:
			*((SQLINTEGER *) Value) = SQL_ASYNC_ENABLE_OFF;
			break;
		case SQL_ATTR_AUTO_IPD:
			*((SQLINTEGER *) Value) = SQL_FALSE;
			break;
		case SQL_ATTR_CONNECTION_DEAD:
			*((SQLUINTEGER *) Value) = CC_not_connected(conn);
			break;
		case SQL_ATTR_CONNECTION_TIMEOUT:
			*((SQLUINTEGER *) Value) = 0;
			break;
		case SQL_ATTR_METADATA_ID:
			*((SQLUINTEGER *) Value) = conn->stmtOptions.metadata_id;
			break;
		case SQL_ATTR_PGOPT_DEBUG:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.debug;
			break;
		case SQL_ATTR_PGOPT_COMMLOG:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.commlog;
			break;
		case SQL_ATTR_PGOPT_PARSE:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.parse;
			break;
		case SQL_ATTR_PGOPT_USE_DECLAREFETCH:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.use_declarefetch;
			break;
		case SQL_ATTR_PGOPT_SERVER_SIDE_PREPARE:
			*((SQLINTEGER *) Value) = conn->connInfo.use_server_side_prepare;
			break;
		case SQL_ATTR_PGOPT_FETCH:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.fetch_max;
			break;
		case SQL_ATTR_PGOPT_UNKNOWNSIZES:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.unknown_sizes;
			break;
		case SQL_ATTR_PGOPT_TEXTASLONGVARCHAR:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.text_as_longvarchar;
			break;
		case SQL_ATTR_PGOPT_UNKNOWNSASLONGVARCHAR:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.unknowns_as_longvarchar;
			break;
		case SQL_ATTR_PGOPT_BOOLSASCHAR:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.bools_as_char;
			break;
		case SQL_ATTR_PGOPT_MAXVARCHARSIZE:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.max_varchar_size;
			break;
		case SQL_ATTR_PGOPT_MAXLONGVARCHARSIZE:
			*((SQLINTEGER *) Value) = conn->connInfo.drivers.max_longvarchar_size;
			break;
		case SQL_ATTR_PGOPT_MSJET:
			*((SQLINTEGER *) Value) = conn->ms_jet;
			break;
		case SQL_ATTR_PGOPT_BATCHSIZE:
			*((SQLINTEGER *) Value) = conn->connInfo.batch_size;
			break;
		case SQL_ATTR_PGOPT_IGNORETIMEOUT:
			*((SQLINTEGER *) Value) = conn->connInfo.ignore_timeout;
			break;
		default:
			ret = PGAPI_GetConnectOption(ConnectionHandle, (UWORD) Attribute, Value, &len, BufferLength);
	}
	if (StringLength)
		*StringLength = len;
	return ret;
}

static SQLHDESC
descHandleFromStatementHandle(HSTMT StatementHandle, SQLINTEGER descType)
{
	StatementClass	*stmt = (StatementClass *) StatementHandle;

	switch (descType)
	{
		case SQL_ATTR_APP_ROW_DESC:		/* 10010 */
			return (HSTMT) stmt->ard;
		case SQL_ATTR_APP_PARAM_DESC:		/* 10011 */
			return (HSTMT) stmt->apd;
		case SQL_ATTR_IMP_ROW_DESC:		/* 10012 */
			return (HSTMT) stmt->ird;
		case SQL_ATTR_IMP_PARAM_DESC:		/* 10013 */
			return (HSTMT) stmt->ipd;
	}
	return (HSTMT) 0;
}

static  void column_bindings_set(ARDFields *opts, int cols, BOOL maxset)
{
	int	i;

	if (cols == opts->allocated)
		return;
	if (cols > opts->allocated)
	{
		extend_column_bindings(opts, cols);
		return;
	}
	if (maxset)	return;

	for (i = opts->allocated; i > cols; i--)
		reset_a_column_binding(opts, i);
	opts->allocated = cols;
	if (0 == cols)
	{
		free(opts->bindings);
		opts->bindings = NULL;
	}
}

static RETCODE SQL_API
ARDSetField(DescriptorClass *desc, SQLSMALLINT RecNumber,
			SQLSMALLINT FieldIdentifier, PTR Value, SQLINTEGER BufferLength)
{
	RETCODE		ret = SQL_SUCCESS;
	ARDFields	*opts = &(desc->ardf);
	SQLSMALLINT	row_idx;
	BOOL		unbind = TRUE;

	switch (FieldIdentifier)
	{
		case SQL_DESC_ARRAY_SIZE:
			opts->size_of_rowset = CAST_UPTR(SQLULEN, Value);
			return ret;
		case SQL_DESC_ARRAY_STATUS_PTR:
			opts->row_operation_ptr = Value;
			return ret;
		case SQL_DESC_BIND_OFFSET_PTR:
			opts->row_offset_ptr = Value;
			return ret;
		case SQL_DESC_BIND_TYPE:
			opts->bind_size = CAST_UPTR(SQLUINTEGER, Value);
			return ret;
		case SQL_DESC_COUNT:
			column_bindings_set(opts, CAST_PTR(SQLSMALLINT, Value), FALSE);
			return ret;

		case SQL_DESC_TYPE:
		case SQL_DESC_DATETIME_INTERVAL_CODE:
		case SQL_DESC_CONCISE_TYPE:
			column_bindings_set(opts, RecNumber, TRUE);
			break;
	}
	if (RecNumber < 0 || RecNumber > opts->allocated)
	{
		DC_set_error(desc, DESC_INVALID_COLUMN_NUMBER_ERROR, "invalid column number");
		return SQL_ERROR;
	}
	if (0 == RecNumber) /* bookmark column */
	{
		BindInfoClass	*bookmark = ARD_AllocBookmark(opts);

		switch (FieldIdentifier)
		{
			case SQL_DESC_TYPE:
				bookmark->returntype = CAST_PTR(SQLSMALLINT, Value);
				break;
			case SQL_DESC_DATETIME_INTERVAL_CODE:
				switch (bookmark->returntype)
				{
					case SQL_DATETIME:
					case SQL_C_TYPE_DATE:
					case SQL_C_TYPE_TIME:
					case SQL_C_TYPE_TIMESTAMP:
					switch ((LONG_PTR) Value)
					{
						case SQL_CODE_DATE:
							bookmark->returntype = SQL_C_TYPE_DATE;
							break;
						case SQL_CODE_TIME:
							bookmark->returntype = SQL_C_TYPE_TIME;
							break;
						case SQL_CODE_TIMESTAMP:
							bookmark->returntype = SQL_C_TYPE_TIMESTAMP;
							break;
					}
					break;
				}
				break;
			case SQL_DESC_DATA_PTR:
				bookmark->buffer = Value;
				break;
			case SQL_DESC_INDICATOR_PTR:
				bookmark->indicator = Value;
				break;
			case SQL_DESC_OCTET_LENGTH_PTR:
				bookmark->used = Value;
				break;
			case SQL_DESC_OCTET_LENGTH:
				bookmark->buflen = CAST_PTR(SQLLEN, Value);
				break;
			case SQL_DESC_PRECISION:
				bookmark->precision = CAST_PTR(SQLSMALLINT, Value);
				break;
			case SQL_DESC_SCALE:
				bookmark->scale = CAST_PTR(SQLSMALLINT, Value);
				break;
			default:
				DC_set_error(desc, DESC_INVALID_COLUMN_NUMBER_ERROR, "invalid column number");
				ret = SQL_ERROR;
		}
		return ret;
	}
	row_idx = RecNumber - 1;
	switch (FieldIdentifier)
	{
		case SQL_DESC_TYPE:
			opts->bindings[row_idx].returntype = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			switch (opts->bindings[row_idx].returntype)
			{
				case SQL_DATETIME:
				case SQL_C_TYPE_DATE:
				case SQL_C_TYPE_TIME:
				case SQL_C_TYPE_TIMESTAMP:
				switch ((LONG_PTR) Value)
				{
					case SQL_CODE_DATE:
						opts->bindings[row_idx].returntype = SQL_C_TYPE_DATE;
						break;
					case SQL_CODE_TIME:
						opts->bindings[row_idx].returntype = SQL_C_TYPE_TIME;
						break;
					case SQL_CODE_TIMESTAMP:
						opts->bindings[row_idx].returntype = SQL_C_TYPE_TIMESTAMP;
						break;
				}
				break;
			}
			break;
		case SQL_DESC_CONCISE_TYPE:
			opts->bindings[row_idx].returntype = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_DATA_PTR:
			unbind = FALSE;
			opts->bindings[row_idx].buffer = Value;
			break;
		case SQL_DESC_INDICATOR_PTR:
			unbind = FALSE;
			opts->bindings[row_idx].indicator = Value;
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			unbind = FALSE;
			opts->bindings[row_idx].used = Value;
			break;
		case SQL_DESC_OCTET_LENGTH:
			opts->bindings[row_idx].buflen = CAST_PTR(SQLLEN, Value);
			break;
		case SQL_DESC_PRECISION:
			opts->bindings[row_idx].precision = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_SCALE:
			opts->bindings[row_idx].scale = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_ALLOC_TYPE: /* read-only */
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
		case SQL_DESC_LENGTH:
		case SQL_DESC_NUM_PREC_RADIX:
		default:ret = SQL_ERROR;
			DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
				"invalid descriptor identifier");
	}
	if (unbind)
		opts->bindings[row_idx].buffer = NULL;
	return ret;
}

static  void parameter_bindings_set(APDFields *opts, int params, BOOL maxset)
{
	int	i;

	if (params == opts->allocated)
		return;
	if (params > opts->allocated)
	{
		extend_parameter_bindings(opts, params);
		return;
	}
	if (maxset)	return;

	for (i = opts->allocated; i > params; i--)
		reset_a_parameter_binding(opts, i);
	opts->allocated = params;
	if (0 == params)
	{
		free(opts->parameters);
		opts->parameters = NULL;
	}
}

static  void parameter_ibindings_set(IPDFields *opts, int params, BOOL maxset)
{
	int	i;

	if (params == opts->allocated)
		return;
	if (params > opts->allocated)
	{
		extend_iparameter_bindings(opts, params);
		return;
	}
	if (maxset)	return;

	for (i = opts->allocated; i > params; i--)
		reset_a_iparameter_binding(opts, i);
	opts->allocated = params;
	if (0 == params)
	{
		free(opts->parameters);
		opts->parameters = NULL;
	}
}

static RETCODE SQL_API
APDSetField(DescriptorClass *desc, SQLSMALLINT RecNumber,
			SQLSMALLINT FieldIdentifier, PTR Value, SQLINTEGER BufferLength)
{
	RETCODE		ret = SQL_SUCCESS;
	APDFields	*opts = &(desc->apdf);
	SQLSMALLINT	para_idx;
	BOOL		unbind = TRUE;

	switch (FieldIdentifier)
	{
		case SQL_DESC_ARRAY_SIZE:
			opts->paramset_size = CAST_UPTR(SQLUINTEGER, Value);
			return ret;
		case SQL_DESC_ARRAY_STATUS_PTR:
			opts->param_operation_ptr = Value;
			return ret;
		case SQL_DESC_BIND_OFFSET_PTR:
			opts->param_offset_ptr = Value;
			return ret;
		case SQL_DESC_BIND_TYPE:
			opts->param_bind_type = CAST_UPTR(SQLUINTEGER, Value);
			return ret;
		case SQL_DESC_COUNT:
			parameter_bindings_set(opts, CAST_PTR(SQLSMALLINT, Value), FALSE);
			return ret;

		case SQL_DESC_TYPE:
		case SQL_DESC_DATETIME_INTERVAL_CODE:
		case SQL_DESC_CONCISE_TYPE:
			parameter_bindings_set(opts, RecNumber, TRUE);
			break;
	}
	if (RecNumber <=0)
	{
MYLOG(DETAIL_LOG_LEVEL, "RecN=%d allocated=%d\n", RecNumber, opts->allocated);
		DC_set_error(desc, DESC_BAD_PARAMETER_NUMBER_ERROR,
				"bad parameter number");
		return SQL_ERROR;
	}
	if (RecNumber > opts->allocated)
	{
MYLOG(DETAIL_LOG_LEVEL, "RecN=%d allocated=%d\n", RecNumber, opts->allocated);
		parameter_bindings_set(opts, RecNumber, TRUE);
		/* DC_set_error(desc, DESC_BAD_PARAMETER_NUMBER_ERROR,
				"bad parameter number");
		return SQL_ERROR;*/
	}
	para_idx = RecNumber - 1;
	switch (FieldIdentifier)
	{
		case SQL_DESC_TYPE:
			opts->parameters[para_idx].CType = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			switch (opts->parameters[para_idx].CType)
			{
				case SQL_DATETIME:
				case SQL_C_TYPE_DATE:
				case SQL_C_TYPE_TIME:
				case SQL_C_TYPE_TIMESTAMP:
				switch ((LONG_PTR) Value)
				{
					case SQL_CODE_DATE:
						opts->parameters[para_idx].CType = SQL_C_TYPE_DATE;
						break;
					case SQL_CODE_TIME:
						opts->parameters[para_idx].CType = SQL_C_TYPE_TIME;
						break;
					case SQL_CODE_TIMESTAMP:
						opts->parameters[para_idx].CType = SQL_C_TYPE_TIMESTAMP;
						break;
				}
				break;
			}
			break;
		case SQL_DESC_CONCISE_TYPE:
			opts->parameters[para_idx].CType = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_DATA_PTR:
			unbind = FALSE;
			opts->parameters[para_idx].buffer = Value;
			break;
		case SQL_DESC_INDICATOR_PTR:
			unbind = FALSE;
			opts->parameters[para_idx].indicator = Value;
			break;
		case SQL_DESC_OCTET_LENGTH:
			opts->parameters[para_idx].buflen = CAST_PTR(Int4, Value);
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			unbind = FALSE;
			opts->parameters[para_idx].used = Value;
			break;
		case SQL_DESC_PRECISION:
			opts->parameters[para_idx].precision = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_SCALE:
			opts->parameters[para_idx].scale = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_ALLOC_TYPE: /* read-only */
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
		case SQL_DESC_LENGTH:
		case SQL_DESC_NUM_PREC_RADIX:
		default:ret = SQL_ERROR;
			DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
				"invalid descriptor identifier");
	}
	if (unbind)
		opts->parameters[para_idx].buffer = NULL;

	return ret;
}

static RETCODE SQL_API
IRDSetField(DescriptorClass *desc, SQLSMALLINT RecNumber,
			SQLSMALLINT FieldIdentifier, PTR Value, SQLINTEGER BufferLength)
{
	RETCODE		ret = SQL_SUCCESS;
	IRDFields	*opts = &(desc->irdf);

	switch (FieldIdentifier)
	{
		case SQL_DESC_ARRAY_STATUS_PTR:
			opts->rowStatusArray = (SQLUSMALLINT *) Value;
			break;
		case SQL_DESC_ROWS_PROCESSED_PTR:
			opts->rowsFetched = (SQLULEN *) Value;
			break;
		case SQL_DESC_ALLOC_TYPE: /* read-only */
		case SQL_DESC_COUNT: /* read-only */
		case SQL_DESC_AUTO_UNIQUE_VALUE: /* read-only */
		case SQL_DESC_BASE_COLUMN_NAME: /* read-only */
		case SQL_DESC_BASE_TABLE_NAME: /* read-only */
		case SQL_DESC_CASE_SENSITIVE: /* read-only */
		case SQL_DESC_CATALOG_NAME: /* read-only */
		case SQL_DESC_CONCISE_TYPE: /* read-only */
		case SQL_DESC_DATETIME_INTERVAL_CODE: /* read-only */
		case SQL_DESC_DATETIME_INTERVAL_PRECISION: /* read-only */
		case SQL_DESC_DISPLAY_SIZE: /* read-only */
		case SQL_DESC_FIXED_PREC_SCALE: /* read-only */
		case SQL_DESC_LABEL: /* read-only */
		case SQL_DESC_LENGTH: /* read-only */
		case SQL_DESC_LITERAL_PREFIX: /* read-only */
		case SQL_DESC_LITERAL_SUFFIX: /* read-only */
		case SQL_DESC_LOCAL_TYPE_NAME: /* read-only */
		case SQL_DESC_NAME: /* read-only */
		case SQL_DESC_NULLABLE: /* read-only */
		case SQL_DESC_NUM_PREC_RADIX: /* read-only */
		case SQL_DESC_OCTET_LENGTH: /* read-only */
		case SQL_DESC_PRECISION: /* read-only */
		case SQL_DESC_ROWVER: /* read-only */
		case SQL_DESC_SCALE: /* read-only */
		case SQL_DESC_SCHEMA_NAME: /* read-only */
		case SQL_DESC_SEARCHABLE: /* read-only */
		case SQL_DESC_TABLE_NAME: /* read-only */
		case SQL_DESC_TYPE: /* read-only */
		case SQL_DESC_TYPE_NAME: /* read-only */
		case SQL_DESC_UNNAMED: /* read-only */
		case SQL_DESC_UNSIGNED: /* read-only */
		case SQL_DESC_UPDATABLE: /* read-only */
		default:ret = SQL_ERROR;
			DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
				"invalid descriptor identifier");
	}
	return ret;
}

static RETCODE SQL_API
IPDSetField(DescriptorClass *desc, SQLSMALLINT RecNumber,
			SQLSMALLINT FieldIdentifier, PTR Value, SQLINTEGER BufferLength)
{
	RETCODE		ret = SQL_SUCCESS;
	IPDFields	*ipdopts = &(desc->ipdf);
	SQLSMALLINT	para_idx;

	switch (FieldIdentifier)
	{
		case SQL_DESC_ARRAY_STATUS_PTR:
			ipdopts->param_status_ptr = (SQLUSMALLINT *) Value;
			return ret;
		case SQL_DESC_ROWS_PROCESSED_PTR:
			ipdopts->param_processed_ptr = (SQLULEN *) Value;
			return ret;
		case SQL_DESC_COUNT:
			parameter_ibindings_set(ipdopts, CAST_PTR(SQLSMALLINT, Value), FALSE);
			return ret;
		case SQL_DESC_UNNAMED: /* only SQL_UNNAMED is allowed */
			if (SQL_UNNAMED !=  CAST_PTR(SQLSMALLINT, Value))
			{
				ret = SQL_ERROR;
				DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
					"invalid descriptor identifier");
				return ret;
			}
		case SQL_DESC_NAME:
		case SQL_DESC_TYPE:
		case SQL_DESC_DATETIME_INTERVAL_CODE:
		case SQL_DESC_CONCISE_TYPE:
			parameter_ibindings_set(ipdopts, RecNumber, TRUE);
			break;
	}
	if (RecNumber <= 0 || RecNumber > ipdopts->allocated)
	{
MYLOG(DETAIL_LOG_LEVEL, "RecN=%d allocated=%d\n", RecNumber, ipdopts->allocated);
		DC_set_error(desc, DESC_BAD_PARAMETER_NUMBER_ERROR,
				"bad parameter number");
		return SQL_ERROR;
	}
	para_idx = RecNumber - 1;
	switch (FieldIdentifier)
	{
		case SQL_DESC_TYPE:
			if (ipdopts->parameters[para_idx].SQLType != CAST_PTR(SQLSMALLINT, Value))
			{
				reset_a_iparameter_binding(ipdopts, RecNumber);
				ipdopts->parameters[para_idx].SQLType = CAST_PTR(SQLSMALLINT, Value);
			}
			break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			switch (ipdopts->parameters[para_idx].SQLType)
			{
				case SQL_DATETIME:
				case SQL_TYPE_DATE:
				case SQL_TYPE_TIME:
				case SQL_TYPE_TIMESTAMP:
				switch ((LONG_PTR) Value)
				{
					case SQL_CODE_DATE:
						ipdopts->parameters[para_idx].SQLType = SQL_TYPE_DATE;
						break;
					case SQL_CODE_TIME:
						ipdopts->parameters[para_idx].SQLType = SQL_TYPE_TIME;
						break;
					case SQL_CODE_TIMESTAMP:
						ipdopts->parameters[para_idx].SQLType = SQL_TYPE_TIMESTAMP;
						break;
				}
				break;
			}
			break;
		case SQL_DESC_CONCISE_TYPE:
			ipdopts->parameters[para_idx].SQLType = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_NAME:
			if (Value)
				STR_TO_NAME(ipdopts->parameters[para_idx].paramName, Value);
			else
				NULL_THE_NAME(ipdopts->parameters[para_idx].paramName);
			break;
		case SQL_DESC_PARAMETER_TYPE:
			ipdopts->parameters[para_idx].paramType = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_PRECISION:
			switch (ipdopts->parameters[para_idx].SQLType)
			{
				case SQL_TYPE_DATE:
				case SQL_TYPE_TIME:
				case SQL_TYPE_TIMESTAMP:
				case SQL_DATETIME:
					ipdopts->parameters[para_idx].decimal_digits = CAST_PTR(SQLSMALLINT, Value);
					break;
				case SQL_NUMERIC:
					ipdopts->parameters[para_idx].precision = CAST_PTR(SQLINTEGER, Value);
					break;
			}
			break;
		case SQL_DESC_SCALE:
			ipdopts->parameters[para_idx].decimal_digits = CAST_PTR(SQLSMALLINT, Value);
			break;
		case SQL_DESC_UNNAMED: /* only SQL_UNNAMED is allowed */
			if (SQL_UNNAMED !=  CAST_PTR(SQLSMALLINT, Value))
			{
				ret = SQL_ERROR;
				DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
					"invalid descriptor identifier");
			}
			else
				NULL_THE_NAME(ipdopts->parameters[para_idx].paramName);
			break;
		case SQL_DESC_OCTET_LENGTH:
			break;
		case SQL_DESC_ALLOC_TYPE: /* read-only */
		case SQL_DESC_CASE_SENSITIVE: /* read-only */
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
		case SQL_DESC_FIXED_PREC_SCALE: /* read-only */
		case SQL_DESC_LENGTH:
		case SQL_DESC_LOCAL_TYPE_NAME: /* read-only */
		case SQL_DESC_NULLABLE: /* read-only */
		case SQL_DESC_NUM_PREC_RADIX:
		case SQL_DESC_ROWVER: /* read-only */
		case SQL_DESC_TYPE_NAME: /* read-only */
		case SQL_DESC_UNSIGNED: /* read-only */
		default:ret = SQL_ERROR;
			DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
				"invalid descriptor identifier");
	}
	return ret;
}


static RETCODE SQL_API
ARDGetField(DescriptorClass *desc, SQLSMALLINT RecNumber,
			SQLSMALLINT FieldIdentifier, PTR Value, SQLINTEGER BufferLength,
			SQLINTEGER *StringLength)
{
	RETCODE		ret = SQL_SUCCESS;
	SQLLEN		ival = 0;
	SQLINTEGER	len, rettype = 0;
	PTR		ptr = NULL;
	const ARDFields	*opts = &(desc->ardf);
	SQLSMALLINT	row_idx;

	len = sizeof(SQLINTEGER);
	if (0 == RecNumber) /* bookmark */
	{
		BindInfoClass	*bookmark = opts->bookmark;
		switch (FieldIdentifier)
		{
			case SQL_DESC_DATA_PTR:
				rettype = SQL_IS_POINTER;
				ptr = bookmark ? bookmark->buffer : NULL;
				break;
			case SQL_DESC_INDICATOR_PTR:
				rettype = SQL_IS_POINTER;
				ptr = bookmark ? bookmark->indicator : NULL;
				break;
			case SQL_DESC_OCTET_LENGTH_PTR:
				rettype = SQL_IS_POINTER;
				ptr = bookmark ? bookmark->used : NULL;
				break;
		}
		if (ptr)
		{
			*((void **) Value) = ptr;
			if (StringLength)
				*StringLength = len;
			return ret;
		}
	}
	switch (FieldIdentifier)
	{
		case SQL_DESC_ALLOC_TYPE:
		case SQL_DESC_ARRAY_SIZE:
		case SQL_DESC_ARRAY_STATUS_PTR:
		case SQL_DESC_BIND_OFFSET_PTR:
		case SQL_DESC_BIND_TYPE:
		case SQL_DESC_COUNT:
			break;
		default:
			if (RecNumber <= 0)
			{
				DC_set_error(desc, DESC_INVALID_COLUMN_NUMBER_ERROR,
					"invalid column number");
				return SQL_ERROR;
			}
			/* RecNumber should be less than or equal to the number of descriptor records */
			else if (RecNumber > opts->allocated)
				return SQL_NO_DATA;
	}
	row_idx = RecNumber - 1;
	switch (FieldIdentifier)
	{
		case SQL_DESC_ARRAY_SIZE:
			ival = opts->size_of_rowset;
			break;
		case SQL_DESC_ARRAY_STATUS_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->row_operation_ptr;
			break;
		case SQL_DESC_BIND_OFFSET_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->row_offset_ptr;
			break;
		case SQL_DESC_BIND_TYPE:
			ival = opts->bind_size;
			break;
		case SQL_DESC_TYPE:
			rettype = SQL_IS_SMALLINT;
			switch (opts->bindings[row_idx].returntype)
			{
				case SQL_C_TYPE_DATE:
				case SQL_C_TYPE_TIME:
				case SQL_C_TYPE_TIMESTAMP:
					ival = SQL_DATETIME;
					break;
				default:
					ival = opts->bindings[row_idx].returntype;
			}
			break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			rettype = SQL_IS_SMALLINT;
			switch (opts->bindings[row_idx].returntype)
			{
				case SQL_C_TYPE_DATE:
					ival = SQL_CODE_DATE;
					break;
				case SQL_C_TYPE_TIME:
					ival = SQL_CODE_TIME;
					break;
				case SQL_C_TYPE_TIMESTAMP:
					ival = SQL_CODE_TIMESTAMP;
					break;
				default:
					ival = 0;
					break;
			}
			break;
		case SQL_DESC_CONCISE_TYPE:
			rettype = SQL_IS_SMALLINT;
			ival = opts->bindings[row_idx].returntype;
			break;
		case SQL_DESC_DATA_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->bindings[row_idx].buffer;
			break;
		case SQL_DESC_INDICATOR_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->bindings[row_idx].indicator;
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->bindings[row_idx].used;
			break;
		case SQL_DESC_COUNT:
			rettype = SQL_IS_SMALLINT;
			ival = opts->allocated;
			break;
		case SQL_DESC_OCTET_LENGTH:
			ival = opts->bindings[row_idx].buflen;
			break;
		case SQL_DESC_ALLOC_TYPE: /* read-only */
			rettype = SQL_IS_SMALLINT;
			if (DC_get_embedded(desc))
				ival = SQL_DESC_ALLOC_AUTO;
			else
				ival = SQL_DESC_ALLOC_USER;
			break;
		case SQL_DESC_PRECISION:
			rettype = SQL_IS_SMALLINT;
			ival = opts->bindings[row_idx].precision;
			break;
		case SQL_DESC_SCALE:
			rettype = SQL_IS_SMALLINT;
			ival = opts->bindings[row_idx].scale;
			break;
		case SQL_DESC_NUM_PREC_RADIX:
			ival = 10;
			break;
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
		case SQL_DESC_LENGTH:
		default:
			ret = SQL_ERROR;
			DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
				"invalid descriptor identifier");
	}
	switch (rettype)
	{
		case 0:
		case SQL_IS_INTEGER:
			len = sizeof(SQLINTEGER);
			*((SQLINTEGER *) Value) = (SQLINTEGER) ival;
			break;
		case SQL_IS_SMALLINT:
			len = sizeof(SQLSMALLINT);
			*((SQLSMALLINT *) Value) = (SQLSMALLINT) ival;
			break;
		case SQL_IS_POINTER:
			len = sizeof(SQLPOINTER);
			*((void **) Value) = ptr;
			break;
	}

	if (StringLength)
		*StringLength = len;
	return ret;
}

static RETCODE SQL_API
APDGetField(DescriptorClass *desc, SQLSMALLINT RecNumber,
			SQLSMALLINT FieldIdentifier, PTR Value, SQLINTEGER BufferLength,
			SQLINTEGER *StringLength)
{
	RETCODE		ret = SQL_SUCCESS;
	SQLLEN		ival = 0;
	SQLINTEGER	len, rettype = 0;
	PTR		ptr = NULL;
	const APDFields	*opts = (const APDFields *) &(desc->apdf);
	SQLSMALLINT	para_idx;

	len = sizeof(SQLINTEGER);
	switch (FieldIdentifier)
	{
		case SQL_DESC_ALLOC_TYPE:
		case SQL_DESC_ARRAY_SIZE:
		case SQL_DESC_ARRAY_STATUS_PTR:
		case SQL_DESC_BIND_OFFSET_PTR:
		case SQL_DESC_BIND_TYPE:
		case SQL_DESC_COUNT:
			break;
		default:
			if (RecNumber <= 0)
			{
				MYLOG(DETAIL_LOG_LEVEL, "RecN=%d allocated=%d\n", RecNumber, opts->allocated);
				DC_set_error(desc, DESC_BAD_PARAMETER_NUMBER_ERROR,
					"bad parameter number");
				return SQL_ERROR;
			}
			/* RecNumber should be less than or equal to the number of descriptor records */
			else if (RecNumber > opts->allocated)
				return SQL_NO_DATA;
	}
	para_idx = RecNumber - 1;
	switch (FieldIdentifier)
	{
		case SQL_DESC_ARRAY_SIZE:
			rettype = SQL_IS_LEN;
			ival = opts->paramset_size;
			break;
		case SQL_DESC_ARRAY_STATUS_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->param_operation_ptr;
			break;
		case SQL_DESC_BIND_OFFSET_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->param_offset_ptr;
			break;
		case SQL_DESC_BIND_TYPE:
			ival = opts->param_bind_type;
			break;

		case SQL_DESC_TYPE:
			rettype = SQL_IS_SMALLINT;
			switch (opts->parameters[para_idx].CType)
			{
				case SQL_C_TYPE_DATE:
				case SQL_C_TYPE_TIME:
				case SQL_C_TYPE_TIMESTAMP:
					ival = SQL_DATETIME;
					break;
				default:
					ival = opts->parameters[para_idx].CType;
			}
			break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			rettype = SQL_IS_SMALLINT;
			switch (opts->parameters[para_idx].CType)
			{
				case SQL_C_TYPE_DATE:
					ival = SQL_CODE_DATE;
					break;
				case SQL_C_TYPE_TIME:
					ival = SQL_CODE_TIME;
					break;
				case SQL_C_TYPE_TIMESTAMP:
					ival = SQL_CODE_TIMESTAMP;
					break;
				default:
					ival = 0;
					break;
			}
			break;
		case SQL_DESC_CONCISE_TYPE:
			rettype = SQL_IS_SMALLINT;
			ival = opts->parameters[para_idx].CType;
			break;
		case SQL_DESC_DATA_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->parameters[para_idx].buffer;
			break;
		case SQL_DESC_INDICATOR_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->parameters[para_idx].indicator;
			break;
		case SQL_DESC_OCTET_LENGTH:
			ival = opts->parameters[para_idx].buflen;
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->parameters[para_idx].used;
			break;
		case SQL_DESC_COUNT:
			rettype = SQL_IS_SMALLINT;
			ival = opts->allocated;
			break;
		case SQL_DESC_ALLOC_TYPE: /* read-only */
			rettype = SQL_IS_SMALLINT;
			if (DC_get_embedded(desc))
				ival = SQL_DESC_ALLOC_AUTO;
			else
				ival = SQL_DESC_ALLOC_USER;
			break;
		case SQL_DESC_NUM_PREC_RADIX:
			ival = 10;
			break;
		case SQL_DESC_PRECISION:
			rettype = SQL_IS_SMALLINT;
			ival = opts->parameters[para_idx].precision;
			break;
		case SQL_DESC_SCALE:
			rettype = SQL_IS_SMALLINT;
			ival = opts->parameters[para_idx].scale;
			break;
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
		case SQL_DESC_LENGTH:
		default:ret = SQL_ERROR;
			DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
					"invalid descriptor identifier");
	}
	switch (rettype)
	{
		case SQL_IS_LEN:
			len = sizeof(SQLLEN);
			*((SQLLEN *) Value) = ival;
			break;
		case 0:
		case SQL_IS_INTEGER:
			len = sizeof(SQLINTEGER);
			*((SQLINTEGER *) Value) = (SQLINTEGER) ival;
			break;
		case SQL_IS_SMALLINT:
			len = sizeof(SQLSMALLINT);
			*((SQLSMALLINT *) Value) = (SQLSMALLINT) ival;
			break;
		case SQL_IS_POINTER:
			len = sizeof(SQLPOINTER);
			*((void **) Value) = ptr;
			break;
	}

	if (StringLength)
		*StringLength = len;
	return ret;
}

static RETCODE SQL_API
IRDGetField(DescriptorClass *desc, SQLSMALLINT RecNumber,
			SQLSMALLINT FieldIdentifier, PTR Value, SQLINTEGER BufferLength,
			SQLINTEGER *StringLength)
{
	RETCODE		ret = SQL_SUCCESS;
	SQLLEN		ival = 0;
	SQLINTEGER	len = 0, rettype = 0;
	PTR		ptr = NULL;
	BOOL		bCallColAtt = FALSE;
	const IRDFields	*opts = &(desc->irdf);

	switch (FieldIdentifier)
	{
		case SQL_DESC_ARRAY_STATUS_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->rowStatusArray;
			break;
		case SQL_DESC_ROWS_PROCESSED_PTR:
			rettype = SQL_IS_POINTER;
			ptr = opts->rowsFetched;
			break;
		case SQL_DESC_ALLOC_TYPE: /* read-only */
			rettype = SQL_IS_SMALLINT;
			ival = SQL_DESC_ALLOC_AUTO;
			break;
		case SQL_DESC_COUNT: /* read-only */
		case SQL_DESC_AUTO_UNIQUE_VALUE: /* read-only */
		case SQL_DESC_CASE_SENSITIVE: /* read-only */
		case SQL_DESC_CONCISE_TYPE: /* read-only */
		case SQL_DESC_DATETIME_INTERVAL_CODE: /* read-only */
		case SQL_DESC_DATETIME_INTERVAL_PRECISION: /* read-only */
		case SQL_DESC_DISPLAY_SIZE: /* read-only */
		case SQL_DESC_FIXED_PREC_SCALE: /* read-only */
		case SQL_DESC_LENGTH: /* read-only */
		case SQL_DESC_NULLABLE: /* read-only */
		case SQL_DESC_NUM_PREC_RADIX: /* read-only */
		case SQL_DESC_OCTET_LENGTH: /* read-only */
		case SQL_DESC_PRECISION: /* read-only */
		case SQL_DESC_ROWVER: /* read-only */
		case SQL_DESC_SCALE: /* read-only */
		case SQL_DESC_SEARCHABLE: /* read-only */
		case SQL_DESC_TYPE: /* read-only */
		case SQL_DESC_UNNAMED: /* read-only */
		case SQL_DESC_UNSIGNED: /* read-only */
		case SQL_DESC_UPDATABLE: /* read-only */
			bCallColAtt = TRUE;
			break;
		case SQL_DESC_BASE_COLUMN_NAME: /* read-only */
		case SQL_DESC_BASE_TABLE_NAME: /* read-only */
		case SQL_DESC_CATALOG_NAME: /* read-only */
		case SQL_DESC_LABEL: /* read-only */
		case SQL_DESC_LITERAL_PREFIX: /* read-only */
		case SQL_DESC_LITERAL_SUFFIX: /* read-only */
		case SQL_DESC_LOCAL_TYPE_NAME: /* read-only */
		case SQL_DESC_NAME: /* read-only */
		case SQL_DESC_SCHEMA_NAME: /* read-only */
		case SQL_DESC_TABLE_NAME: /* read-only */
		case SQL_DESC_TYPE_NAME: /* read-only */
			rettype = SQL_NTS;
			bCallColAtt = TRUE;
			break;
		default:
			ret = SQL_ERROR;
			DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
				"invalid descriptor identifier");
	}
	if (bCallColAtt)
	{
		SQLSMALLINT	pcbL;
		StatementClass	*stmt;

		stmt = opts->stmt;
		/* if statement is in the prepared or executed state but there is no open cursor, return SQL_NO_DATA */
		if ((stmt->prepared >= PREPARED_PERMANENTLY || stmt->status == STMT_FINISHED) && NAME_IS_NULL(stmt->cursor_name))
			return SQL_NO_DATA;
		/* HY007: statement handle had not been prepared or executed */
		if (SC_get_Curres(stmt) == NULL && stmt->prepared == NOT_YET_PREPARED)
		{
			DC_set_error(desc, DESC_STATEMENT_NOT_PREPARED,
				"associated statement is not prepared");
			return SQL_ERROR;
		}
		ret = PGAPI_ColAttributes(stmt, RecNumber,
			FieldIdentifier, Value, (SQLSMALLINT) BufferLength,
				&pcbL, &ival);
		len = pcbL;
	}
	switch (rettype)
	{
		case 0:
		case SQL_IS_INTEGER:
			len = sizeof(SQLINTEGER);
			*((SQLINTEGER *) Value) = (SQLINTEGER) ival;
			break;
		case SQL_IS_UINTEGER:
			len = sizeof(SQLUINTEGER);
			*((SQLUINTEGER *) Value) = (SQLUINTEGER) ival;
			break;
		case SQL_IS_SMALLINT:
			len = sizeof(SQLSMALLINT);
			*((SQLSMALLINT *) Value) = (SQLSMALLINT) ival;
			break;
		case SQL_IS_POINTER:
			len = sizeof(SQLPOINTER);
			*((void **) Value) = ptr;
			break;
		case SQL_NTS:
			break;
	}

	if (StringLength)
		*StringLength = len;
	/* 01004: String data, right-truncated */
	if (ret == SQL_SUCCESS_WITH_INFO)
		DC_set_error(desc, DESC_STRING_DATA_TRUNCATED, "string data truncated");
	return ret;
}

static RETCODE SQL_API
IPDGetField(DescriptorClass *desc, SQLSMALLINT RecNumber,
			SQLSMALLINT FieldIdentifier, PTR Value, SQLINTEGER BufferLength,
			SQLINTEGER *StringLength)
{
	RETCODE		ret = SQL_SUCCESS;
	SQLINTEGER	ival = 0, len = 0, rettype = 0;
	PTR		ptr = NULL;
	const IPDFields	*ipdopts = (const IPDFields *) &(desc->ipdf);
	SQLSMALLINT	para_idx;

	switch (FieldIdentifier)
	{
		case SQL_DESC_ALLOC_TYPE:
		case SQL_DESC_ARRAY_STATUS_PTR:
		case SQL_DESC_ROWS_PROCESSED_PTR:
		case SQL_DESC_COUNT:
			break;
		default:
			if (RecNumber <= 0)
			{
				MYLOG(DETAIL_LOG_LEVEL, "RecN=%d allocated=%d\n", RecNumber, ipdopts->allocated);
				DC_set_error(desc, DESC_BAD_PARAMETER_NUMBER_ERROR,
					"bad parameter number");
				return SQL_ERROR;
			}
			/* RecNumber should be less than or equal to the number of descriptor records */
			else if (RecNumber > ipdopts->allocated)
				return SQL_NO_DATA;
	}
	para_idx = RecNumber - 1;
	switch (FieldIdentifier)
	{
		case SQL_DESC_ARRAY_STATUS_PTR:
			rettype = SQL_IS_POINTER;
			ptr = ipdopts->param_status_ptr;
			break;
		case SQL_DESC_ROWS_PROCESSED_PTR:
			rettype = SQL_IS_POINTER;
			ptr = ipdopts->param_processed_ptr;
			break;
		case SQL_DESC_UNNAMED:
			rettype = SQL_IS_SMALLINT;
			ival = NAME_IS_NULL(ipdopts->parameters[para_idx].paramName) ? SQL_UNNAMED : SQL_NAMED;
			break;
		case SQL_DESC_TYPE:
			rettype = SQL_IS_SMALLINT;
			switch (ipdopts->parameters[para_idx].SQLType)
			{
				case SQL_TYPE_DATE:
				case SQL_TYPE_TIME:
				case SQL_TYPE_TIMESTAMP:
					ival = SQL_DATETIME;
					break;
				default:
					ival = ipdopts->parameters[para_idx].SQLType;
			}
			break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			rettype = SQL_IS_SMALLINT;
			switch (ipdopts->parameters[para_idx].SQLType)
			{
				case SQL_TYPE_DATE:
					ival = SQL_CODE_DATE;
					break;
				case SQL_TYPE_TIME:
					ival = SQL_CODE_TIME;
					break;
				case SQL_TYPE_TIMESTAMP:
					ival = SQL_CODE_TIMESTAMP;
					break;
				default:
					ival = 0;
			}
			break;
		case SQL_DESC_CONCISE_TYPE:
			rettype = SQL_IS_SMALLINT;
			ival = ipdopts->parameters[para_idx].SQLType;
			break;
		case SQL_DESC_COUNT:
			rettype = SQL_IS_SMALLINT;
			ival = ipdopts->allocated;
			break;
		case SQL_DESC_PARAMETER_TYPE:
			rettype = SQL_IS_SMALLINT;
			ival = ipdopts->parameters[para_idx].paramType;
			break;
		case SQL_DESC_PRECISION:
			rettype = SQL_IS_SMALLINT;
			switch (ipdopts->parameters[para_idx].SQLType)
			{
				case SQL_TYPE_DATE:
				case SQL_TYPE_TIME:
				case SQL_TYPE_TIMESTAMP:
				case SQL_DATETIME:
					ival = ipdopts->parameters[para_idx].decimal_digits;
					break;
				case SQL_NUMERIC:
					ival = ipdopts->parameters[para_idx].precision;
					break;
			}
			break;
		case SQL_DESC_SCALE:
			rettype = SQL_IS_SMALLINT;
			switch (ipdopts->parameters[para_idx].SQLType)
			{
				case SQL_NUMERIC:
					ival = ipdopts->parameters[para_idx].decimal_digits;
					break;
			}
			break;
		case SQL_DESC_OCTET_LENGTH:
			break;
		case SQL_DESC_ALLOC_TYPE: /* read-only */
			rettype = SQL_IS_SMALLINT;
			ival = SQL_DESC_ALLOC_AUTO;
			break;
		case SQL_DESC_NAME:
			rettype = SQL_NTS;
			if (NAME_IS_NULL(ipdopts->parameters[para_idx].paramName))
				ptr = (char *) SAFE_NAME(ipdopts->parameters[para_idx].paramName);
			else
				ptr = GET_NAME(ipdopts->parameters[para_idx].paramName);
			if (ptr)
			{
				len = (SQLINTEGER) strlen(ptr);
				if (Value)
				{
					strncpy_null((char *) Value, ptr, BufferLength);
					if (len >= BufferLength)
						ret = SQL_SUCCESS_WITH_INFO;
				}
			}
			break;
		case SQL_DESC_NULLABLE: /* read-only */
			rettype = SQL_IS_SMALLINT;
			ival = SQL_NULLABLE;
			break;
		case SQL_DESC_CASE_SENSITIVE: /* read-only */
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
		case SQL_DESC_FIXED_PREC_SCALE: /* read-only */
		case SQL_DESC_LENGTH:
		case SQL_DESC_LOCAL_TYPE_NAME: /* read-only */
		case SQL_DESC_NUM_PREC_RADIX:
		case SQL_DESC_ROWVER: /* read-only */
		case SQL_DESC_TYPE_NAME: /* read-only */
		case SQL_DESC_UNSIGNED: /* read-only */
		default:ret = SQL_ERROR;
			DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER,
				"invalid descriptor identifier");
	}
	switch (rettype)
	{
		case 0:
		case SQL_IS_INTEGER:
			len = sizeof(SQLINTEGER);
			*((SQLINTEGER *) Value) = ival;
			break;
		case SQL_IS_SMALLINT:
			len = sizeof(SQLSMALLINT);
			*((SQLSMALLINT *) Value) = (SQLSMALLINT) ival;
			break;
		case SQL_IS_POINTER:
			len = sizeof(SQLPOINTER);
			*((void **)Value) = ptr;
			break;
		case SQL_NTS:
			break;
	}

	if (StringLength)
		*StringLength = len;
	/* 01004: String data, right-truncated */
	if (ret == SQL_SUCCESS_WITH_INFO)
		DC_set_error(desc, DESC_STRING_DATA_TRUNCATED, "string data truncated");
	return ret;
}

/*
 * PGAPI_GetStmtAttr
 *
 * Description:
 *      Core implementation function that retrieves the current setting of a statement attribute.
 *      This function is called by both SQLGetStmtAttr (ANSI) and SQLGetStmtAttrW (Unicode) functions.
 *
 * Parameters:
 *      StatementHandle - Handle to the statement
 *      Attribute - The attribute to retrieve (SQL_ATTR_* constant)
 *      Value - Buffer to store the attribute value
 *      BufferLength - Length of the Value buffer in bytes
 *      StringLength - Pointer to store the actual length of string data
 *
 * Returns:
 *      SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE
 *
 * Implementation Strategy:
 *      This function uses a switch statement to handle different attribute types.
 *      Most attributes are stored directly in the statement structure or its descriptors.
 *      For unsupported attributes, it returns an error.
 *      For backward compatibility with ODBC 2.0, it calls PGAPI_GetStmtOption for some attributes.
 */
RETCODE		SQL_API
PGAPI_GetStmtAttr(HSTMT StatementHandle,
				  SQLINTEGER Attribute, PTR Value,
				  SQLINTEGER BufferLength, SQLINTEGER *StringLength)
{
	CSTR func = "PGAPI_GetStmtAttr";
	StatementClass *stmt = (StatementClass *) StatementHandle;
	RETCODE		ret = SQL_SUCCESS;
	SQLINTEGER	len = 0;

	MYLOG(0, "entering Handle=%p " FORMAT_INTEGER "\n", StatementHandle, Attribute);
	switch (Attribute)
	{
		case SQL_ATTR_FETCH_BOOKMARK_PTR:	/* 16 */
			/* Pointer to bookmark value */
			*((void **) Value) = stmt->options.bookmark_ptr;
			len = sizeof(SQLPOINTER);
			break;
		case SQL_ATTR_PARAM_BIND_OFFSET_PTR:	/* 17 */
			/* Pointer to parameter binding offset */
			*((SQLULEN **) Value) = SC_get_APDF(stmt)->param_offset_ptr;
			len = sizeof(SQLPOINTER);
			break;
		case SQL_ATTR_PARAM_BIND_TYPE:	/* 18 */
			/* Parameter binding type */
			*((SQLUINTEGER *) Value) = SC_get_APDF(stmt)->param_bind_type;
			len = sizeof(SQLUINTEGER);
			break;
		case SQL_ATTR_PARAM_OPERATION_PTR:		/* 19 */
			/* Pointer to parameter operation array */
			*((SQLUSMALLINT **) Value) = SC_get_APDF(stmt)->param_operation_ptr;
			len = sizeof(SQLPOINTER);
			break;
		case SQL_ATTR_PARAM_STATUS_PTR: /* 20 */
			/* Pointer to parameter status array */
			*((SQLUSMALLINT **) Value) = SC_get_IPDF(stmt)->param_status_ptr;
			len = sizeof(SQLPOINTER);
			break;
		case SQL_ATTR_PARAMS_PROCESSED_PTR:		/* 21 */
			/* Pointer to count of processed parameters */
			*((SQLULEN **) Value) = SC_get_IPDF(stmt)->param_processed_ptr;
			len = sizeof(SQLPOINTER);
			break;
		case SQL_ATTR_PARAMSET_SIZE:	/* 22 */
			/* Number of sets of parameters */
			*((SQLULEN *) Value) = SC_get_APDF(stmt)->paramset_size;
			len = sizeof(SQLUINTEGER);
			break;
		case SQL_ATTR_ROW_BIND_OFFSET_PTR:		/* 23 */
			/* Pointer to row binding offset */
			*((SQLULEN **) Value) = SC_get_ARDF(stmt)->row_offset_ptr;
			len = 4;
			break;
		case SQL_ATTR_ROW_OPERATION_PTR:		/* 24 */
			/* Pointer to row operation array */
			*((SQLUSMALLINT **) Value) = SC_get_ARDF(stmt)->row_operation_ptr;
			len = 4;
			break;
		case SQL_ATTR_ROW_STATUS_PTR:	/* 25 */
			/* Pointer to row status array */
			*((SQLUSMALLINT **) Value) = SC_get_IRDF(stmt)->rowStatusArray;
			len = 4;
			break;
		case SQL_ATTR_ROWS_FETCHED_PTR: /* 26 */
			/* Pointer to count of fetched rows */
			*((SQLULEN **) Value) = SC_get_IRDF(stmt)->rowsFetched;
			len = 4;
			break;
		case SQL_ATTR_ROW_ARRAY_SIZE:	/* 27 */
			/* Number of rows to fetch */
			*((SQLULEN *) Value) = SC_get_ARDF(stmt)->size_of_rowset;
			len = 4;
			break;
		case SQL_ATTR_APP_ROW_DESC:		/* 10010 */
		case SQL_ATTR_APP_PARAM_DESC:	/* 10011 */
		case SQL_ATTR_IMP_ROW_DESC:		/* 10012 */
		case SQL_ATTR_IMP_PARAM_DESC:	/* 10013 */
			/* Descriptor handles */
			len = 4;
			*((HSTMT *) Value) = descHandleFromStatementHandle(StatementHandle, Attribute);
			break;

		case SQL_ATTR_CURSOR_SCROLLABLE:		/* -1 */
			/* Whether cursor is scrollable */
			len = 4;
			if (SQL_CURSOR_FORWARD_ONLY == stmt->options.cursor_type)
				*((SQLUINTEGER *) Value) = SQL_NONSCROLLABLE;
			else
				*((SQLUINTEGER *) Value) = SQL_SCROLLABLE;
			break;
		case SQL_ATTR_CURSOR_SENSITIVITY:		/* -2 */
			/* Cursor sensitivity to changes made by others */
			len = 4;
			if (SQL_CONCUR_READ_ONLY == stmt->options.scroll_concurrency)
				*((SQLUINTEGER *) Value) = SQL_INSENSITIVE;
			else
				*((SQLUINTEGER *) Value) = SQL_UNSPECIFIED;
			break;
		case SQL_ATTR_METADATA_ID:		/* 10014 */
			/* Whether identifiers are quoted */
			*((SQLUINTEGER *) Value) = stmt->options.metadata_id;
			break;
		case SQL_ATTR_ENABLE_AUTO_IPD:	/* 15 */
			/* Whether automatic population of IPD is supported */
			*((SQLUINTEGER *) Value) = SQL_FALSE;
			break;
		case SQL_ATTR_AUTO_IPD:	/* 10001 */
			/* Unsupported attributes */
			SC_set_error(stmt, DESC_INVALID_OPTION_IDENTIFIER, "Unsupported statement option (Get)", func);
			return SQL_ERROR;
		default:
			/* For backward compatibility with ODBC 2.0 */
			ret = PGAPI_GetStmtOption(StatementHandle, (SQLSMALLINT) Attribute, Value, &len, BufferLength);
	}
	if (ret == SQL_SUCCESS && StringLength)
		*StringLength = len;
	return ret;
}

/*	SQLSetConnectOption -> SQLSetConnectAttr */
RETCODE		SQL_API
PGAPI_SetConnectAttr(HDBC ConnectionHandle,
					 SQLINTEGER Attribute, PTR Value,
					 SQLINTEGER StringLength)
{
	CSTR	func = "PGAPI_SetConnectAttr";
	ConnectionClass *conn = (ConnectionClass *) ConnectionHandle;
	RETCODE	ret = SQL_SUCCESS;
	BOOL	unsupported = FALSE;
	int	newValue;

	MYLOG(0, "entering for %p: " FORMAT_INTEGER " %p\n", ConnectionHandle, Attribute, Value);
	switch (Attribute)
	{
		case SQL_ATTR_METADATA_ID:
			conn->stmtOptions.metadata_id = CAST_UPTR(SQLUINTEGER, Value);
			break;
		case SQL_ATTR_ANSI_APP:
			if (SQL_AA_FALSE != CAST_PTR(SQLINTEGER, Value))
			{
				MYLOG(0, "the application is ansi\n");
				if (CC_is_in_unicode_driver(conn)) /* the driver is unicode */
					CC_set_in_ansi_app(conn); /* but the app is ansi */
			}
			else
			{
				MYLOG(0, "the application is unicode\n");
			}
			/*return SQL_ERROR;*/
			return SQL_SUCCESS;
		case SQL_ATTR_ENLIST_IN_DTC:
#ifdef	WIN32
#ifdef	_HANDLE_ENLIST_IN_DTC_
			MYLOG(0, "SQL_ATTR_ENLIST_IN_DTC %p request received\n", Value);
			if (conn->connInfo.xa_opt != 0)
			{
				/*
				 *	When a new global transaction is about
				 *	to begin, isolate the existent global
				 *	transaction.
				 */
				if (NULL != Value && CC_is_in_global_trans(conn))
					CALL_IsolateDtcConn(conn, TRUE);
				return CALL_EnlistInDtc(conn, Value, conn->connInfo.xa_opt);
			}
#endif /* _HANDLE_ENLIST_IN_DTC_ */
#endif /* WIN32 */
			unsupported = TRUE;
			break;
		case SQL_ATTR_AUTO_IPD:
			if (SQL_FALSE != Value)
				unsupported = TRUE;
			break;
		case SQL_ATTR_ASYNC_ENABLE:
		case SQL_ATTR_CONNECTION_DEAD:
		case SQL_ATTR_CONNECTION_TIMEOUT:
			unsupported = TRUE;
			break;
		case SQL_ATTR_PGOPT_DEBUG:
			newValue = CAST_UPTR(SQLCHAR, Value);
			if (newValue > 0)
			{
				logs_on_off(-1, conn->connInfo.drivers.debug, 0);
				conn->connInfo.drivers.debug = newValue;
				logs_on_off(1, conn->connInfo.drivers.debug, 0);
				MYLOG(0, "debug => %d\n", conn->connInfo.drivers.debug);
			}
			else if (newValue == 0 && conn->connInfo.drivers.debug > 0)
			{
				MYLOG(0, "debug => %d\n", newValue);
				logs_on_off(-1, conn->connInfo.drivers.debug, 0);
				conn->connInfo.drivers.debug = newValue;
				logs_on_off(1, 0, 0);
			}
			break;
		case SQL_ATTR_PGOPT_COMMLOG:
			newValue = CAST_UPTR(SQLCHAR, Value);
			if (newValue > 0)
			{
				logs_on_off(-1, 0, conn->connInfo.drivers.commlog);
				conn->connInfo.drivers.commlog = newValue;
				logs_on_off(1, 0, conn->connInfo.drivers.commlog);
				MYLOG(0, "commlog => %d\n", conn->connInfo.drivers.commlog);
			}
			else if (newValue == 0 && conn->connInfo.drivers.commlog > 0)
			{
				MYLOG(0, "commlog => %d\n", newValue);
				logs_on_off(-1, 0, conn->connInfo.drivers.commlog);
				conn->connInfo.drivers.debug = newValue;
				logs_on_off(1, 0, 0);
			}
			break;
		case SQL_ATTR_PGOPT_PARSE:
			conn->connInfo.drivers.parse = CAST_UPTR(SQLCHAR, Value);
			MYLOG(0, "parse => %d\n", conn->connInfo.drivers.parse);
			break;
		case SQL_ATTR_PGOPT_USE_DECLAREFETCH:
			conn->connInfo.drivers.use_declarefetch = CAST_UPTR(SQLCHAR, Value);
			ci_updatable_cursors_set(&conn->connInfo);
			MYLOG(0, "declarefetch => %d\n", conn->connInfo.drivers.use_declarefetch);
			break;
		case SQL_ATTR_PGOPT_SERVER_SIDE_PREPARE:
			conn->connInfo.use_server_side_prepare = CAST_UPTR(SQLCHAR, Value);
			MYLOG(0, "server_side_prepare => %d\n", conn->connInfo.use_server_side_prepare);
			break;
		case SQL_ATTR_PGOPT_FETCH:
			conn->connInfo.drivers.fetch_max = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "fetch => %d\n", conn->connInfo.drivers.fetch_max);
			break;
		case SQL_ATTR_PGOPT_UNKNOWNSIZES:
			conn->connInfo.drivers.unknown_sizes = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "unknown_sizes => %d\n", conn->connInfo.drivers.unknown_sizes);
			break;
		case SQL_ATTR_PGOPT_TEXTASLONGVARCHAR:
			conn->connInfo.drivers.text_as_longvarchar = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "text_as_longvarchar => %d\n", conn->connInfo.drivers.text_as_longvarchar);
			break;
		case SQL_ATTR_PGOPT_UNKNOWNSASLONGVARCHAR:
			conn->connInfo.drivers.unknowns_as_longvarchar = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "unknowns_as_long_varchar => %d\n", conn->connInfo.drivers.unknowns_as_longvarchar);
			break;
		case SQL_ATTR_PGOPT_BOOLSASCHAR:
			conn->connInfo.drivers.bools_as_char = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "bools_as_char => %d\n", conn->connInfo.drivers.bools_as_char);
			break;
		case SQL_ATTR_PGOPT_MAXVARCHARSIZE:
			conn->connInfo.drivers.max_varchar_size = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "max_varchar_size => %d\n", conn->connInfo.drivers.max_varchar_size);
			break;
		case SQL_ATTR_PGOPT_MAXLONGVARCHARSIZE:
			conn->connInfo.drivers.max_longvarchar_size = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "max_longvarchar_size => %d\n", conn->connInfo.drivers.max_longvarchar_size);
			break;
		case SQL_ATTR_PGOPT_WCSDEBUG:
			conn->connInfo.wcs_debug = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "wcs_debug => %d\n", conn->connInfo.wcs_debug);
			break;
		case SQL_ATTR_PGOPT_MSJET:
			conn->ms_jet = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "ms_jet => %d\n", conn->ms_jet);
			break;
		case SQL_ATTR_PGOPT_BATCHSIZE:
			conn->connInfo.batch_size = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "batch size => %d\n", conn->connInfo.batch_size);
			break;
		case SQL_ATTR_PGOPT_IGNORETIMEOUT:
			conn->connInfo.ignore_timeout = CAST_PTR(SQLINTEGER, Value);
			MYLOG(0, "ignore_timeout => %d\n", conn->connInfo.ignore_timeout);
			break;
		default:
			if (Attribute < 65536)
				ret = PGAPI_SetConnectOption(ConnectionHandle, (SQLUSMALLINT) Attribute, (SQLLEN) Value);
			else
				unsupported = TRUE;
	}
	if (unsupported)
	{
		char	msg[64];
		SPRINTF_FIXED(msg, "Couldn't set unsupported connect attribute " FORMAT_INTEGER, Attribute);
		CC_set_error(conn, CONN_OPTION_NOT_FOR_THE_DRIVER, msg, func);
		return SQL_ERROR;
	}
	return ret;
}

/*
 * PGAPI_GetDescField
 *
 * Description:
 *      Core implementation function that retrieves the current setting or value of a single field
 *      of a descriptor record. This function is called by both SQLGetDescField (ANSI) and 
 *      SQLGetDescFieldW (Unicode) functions.
 *
 * Parameters:
 *      DescriptorHandle - Handle to the descriptor
 *      RecNumber - The descriptor record number (1-based, 0 for header fields)
 *      FieldIdentifier - The field identifier (SQL_DESC_* constant)
 *      Value - Buffer to store the field value
 *      BufferLength - Length of the Value buffer in bytes
 *      StringLength - Pointer to store the actual length of string data
 *
 * Returns:
 *      SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE
 *
 * Implementation Strategy:
 *      1. Cast the generic SQLHDESC handle to the internal DescriptorClass structure
 *      2. Determine the descriptor type (ARD, APD, IRD, IPD)
 *      3. Call the appropriate type-specific handler function
 *      4. Handle any errors that occur during processing
 */
RETCODE		SQL_API
PGAPI_GetDescField(SQLHDESC DescriptorHandle,
				   SQLSMALLINT RecNumber, SQLSMALLINT FieldIdentifier,
				   PTR Value, SQLINTEGER BufferLength,
				   SQLINTEGER *StringLength)
{
	CSTR func = "PGAPI_GetDescField";
	RETCODE		ret = SQL_SUCCESS;
	/* Cast the generic ODBC handle to our internal descriptor structure */
	DescriptorClass *desc = (DescriptorClass *) DescriptorHandle;

	/* Log function entry with important parameters */
	MYLOG(0, "entering h=%p rec=" FORMAT_SMALLI " field=" FORMAT_SMALLI " blen=" FORMAT_INTEGER "\n", DescriptorHandle, RecNumber, FieldIdentifier, BufferLength);

	/* Route the request to the appropriate handler based on descriptor type */
	switch (DC_get_desc_type(desc))
	{
		case SQL_ATTR_APP_ROW_DESC:
			/* Application Row Descriptor - handles column bindings */
			ret = ARDGetField(desc, RecNumber, FieldIdentifier, Value, BufferLength, StringLength);
			break;
		case SQL_ATTR_APP_PARAM_DESC:
			/* Application Parameter Descriptor - handles parameter bindings */
			ret = APDGetField(desc, RecNumber, FieldIdentifier, Value, BufferLength, StringLength);
			break;
		case SQL_ATTR_IMP_ROW_DESC:
			/* Implementation Row Descriptor - handles result set metadata */
			ret = IRDGetField(desc, RecNumber, FieldIdentifier, Value, BufferLength, StringLength);
			break;
		case SQL_ATTR_IMP_PARAM_DESC:
			/* Implementation Parameter Descriptor - handles parameter metadata */
			ret = IPDGetField(desc, RecNumber, FieldIdentifier, Value, BufferLength, StringLength);
			break;
		default:
			/* Unknown descriptor type - return error */
			ret = SQL_ERROR;
			DC_set_error(desc, DESC_INTERNAL_ERROR, "Error not implemented");
	}

	/* Handle error cases by setting appropriate error messages */
	if (ret == SQL_ERROR)
	{
		if (!DC_get_errormsg(desc))
		{
			switch (DC_get_errornumber(desc))
			{
				case DESC_INVALID_DESCRIPTOR_IDENTIFIER:
					DC_set_errormsg(desc, "can't SQLGetDescField for this descriptor identifier");
					break;
				case DESC_INVALID_COLUMN_NUMBER_ERROR:
					DC_set_errormsg(desc, "can't SQLGetDescField for this column number");
					break;
				case DESC_BAD_PARAMETER_NUMBER_ERROR:
					DC_set_errormsg(desc, "can't SQLGetDescField for this parameter number");
					break;
			}
		}
		/* Log the error for debugging purposes */
		DC_log_error(func, "", desc);
	}
	return ret;
}

/*
 * PGAPI_SetDescField
 *
 * Description:
 *      Core implementation function that sets the value of a single field of a descriptor record.
 *      This function is called by both SQLSetDescField (ANSI) and SQLSetDescFieldW (Unicode) functions.
 *
 * Parameters:
 *      DescriptorHandle - Handle to the descriptor
 *      RecNumber - The descriptor record number (1-based, 0 for header fields)
 *      FieldIdentifier - The field identifier (SQL_DESC_* constant)
 *      Value - The value to set for the field
 *      BufferLength - Length of the Value buffer in bytes (for string data)
 *
 * Returns:
 *      SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE
 *
 * Implementation Strategy:
 *      1. Cast the generic SQLHDESC handle to the internal DescriptorClass structure
 *      2. Determine the descriptor type (ARD, APD, IRD, IPD)
 *      3. Call the appropriate type-specific handler function
 *      4. Handle any errors that occur during processing
 *
 * Notes:
 *      - Implementation Row Descriptor (IRD) fields are generally read-only
 *      - Some Implementation Parameter Descriptor (IPD) fields are read-only
 *      - Setting certain fields may affect other related fields automatically
 */
RETCODE		SQL_API
PGAPI_SetDescField(SQLHDESC DescriptorHandle,
				   SQLSMALLINT RecNumber, SQLSMALLINT FieldIdentifier,
				   PTR Value, SQLINTEGER BufferLength)
{
	CSTR func = "PGAPI_SetDescField";
	RETCODE		ret = SQL_SUCCESS;
	/* Cast the generic ODBC handle to our internal descriptor structure */
	DescriptorClass *desc = (DescriptorClass *) DescriptorHandle;

	/* Log function entry with important parameters */
	MYLOG(0, "entering h=%p(%d) rec=" FORMAT_SMALLI " field=" FORMAT_SMALLI " val=%p," FORMAT_INTEGER "\n", DescriptorHandle, DC_get_desc_type(desc), RecNumber, FieldIdentifier, Value, BufferLength);

	/* Route the request to the appropriate handler based on descriptor type */
	switch (DC_get_desc_type(desc))
	{
		case SQL_ATTR_APP_ROW_DESC:
			/* Application Row Descriptor - handles column bindings */
			ret = ARDSetField(desc, RecNumber, FieldIdentifier, Value, BufferLength);
			break;
		case SQL_ATTR_APP_PARAM_DESC:
			/* Application Parameter Descriptor - handles parameter bindings */
			ret = APDSetField(desc, RecNumber, FieldIdentifier, Value, BufferLength);
			break;
		case SQL_ATTR_IMP_ROW_DESC:
			/* Implementation Row Descriptor - handles result set metadata (mostly read-only) */
			ret = IRDSetField(desc, RecNumber, FieldIdentifier, Value, BufferLength);
			break;
		case SQL_ATTR_IMP_PARAM_DESC:
			/* Implementation Parameter Descriptor - handles parameter metadata (partially read-only) */
			ret = IPDSetField(desc, RecNumber, FieldIdentifier, Value, BufferLength);
			break;
		default:
			/* Unknown descriptor type - return error */
			ret = SQL_ERROR;
			DC_set_error(desc, DESC_INTERNAL_ERROR, "Error not implemented");
	}

	/* Handle error cases by setting appropriate error messages */
	if (ret == SQL_ERROR)
	{
		if (!DC_get_errormsg(desc))
		{
			switch (DC_get_errornumber(desc))
			{
				case DESC_INVALID_DESCRIPTOR_IDENTIFIER:
					DC_set_errormsg(desc, "can't SQLSetDescField for this descriptor identifier");
					break;
				case DESC_INVALID_COLUMN_NUMBER_ERROR:
					DC_set_errormsg(desc, "can't SQLSetDescField for this column number");
					break;
				case DESC_BAD_PARAMETER_NUMBER_ERROR:
					DC_set_errormsg(desc, "can't SQLSetDescField for this parameter number");
					break;
				break;
			}
		}
		/* Log the error for debugging purposes */
		DC_log_error(func, "", desc);
	}
	return ret;
}

/*
 * PGAPI_SetDescRec
 *
 * Description:
 *      Core implementation function that sets multiple descriptor fields with a single call.
 *      This function is called by both SQLSetDescRec (ANSI) and SQLSetDescRecW (Unicode) functions.
 *
 * Parameters:
 *      DescriptorHandle - Handle to the descriptor
 *      RecNumber - The descriptor record number (1-based)
 *      Type - SQL data type
 *      SubType - Datetime or interval subcode
 *      Length - Maximum data length
 *      Precision - Precision of numeric types
 *      Scale - Scale of numeric types
 *      Data - Pointer to data buffer
 *      StringLength - Pointer to buffer length
 *      Indicator - Pointer to indicator variable
 *
 * Returns:
 *      SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE
 *
 * Implementation Strategy:
 *      This function sets multiple descriptor fields by making individual calls
 *      to PGAPI_SetDescField for each field. It's a convenience function that
 *      allows setting commonly used fields with a single function call.
 */
RETCODE		SQL_API
PGAPI_SetDescRec(SQLHDESC DescriptorHandle,
				 SQLSMALLINT RecNumber, SQLSMALLINT Type,
				 SQLSMALLINT SubType, SQLLEN Length,
				 SQLSMALLINT Precision, SQLSMALLINT Scale,
				 PTR Data, SQLLEN *StringLength, SQLLEN *Indicator)
{
	CSTR func = "PGAPI_SetDescRec";
	RETCODE		ret = SQL_SUCCESS;
	DescriptorClass *desc = (DescriptorClass *) DescriptorHandle;

	MYLOG(0, "entering h=%p(%d) rec=" FORMAT_SMALLI " type=" FORMAT_SMALLI " sub=" FORMAT_SMALLI " len=" FORMAT_LEN " prec=" FORMAT_SMALLI " scale=" FORMAT_SMALLI " data=%p\n", 
			DescriptorHandle, DC_get_desc_type(desc), RecNumber, Type, SubType, Length, Precision, Scale, Data);
	MYLOG(0, "str=%p ind=%p\n", StringLength, Indicator);

	/* Descriptor handle must not be an IRD handle */
	if (DC_get_desc_type(desc) == SQL_ATTR_IMP_ROW_DESC)
	{
		DC_set_error(desc, DESC_INVALID_DESCRIPTOR_IDENTIFIER, "Invalid descriptor identifier");
		DC_log_error(func, "", desc);
		return SQL_ERROR;
	}

	/*
		Set following descriptor fields:

		- SQL_DESC_TYPE
		- SQL_DESC_DATETIME_INTERVAL_CODE
		- SQL_DESC_OCTET_LENGTH
		- SQL_DESC_PRECISION
		- SQL_DESC_SCALE
		- SQL_DESC_DATA_PTR
		- SQL_DESC_OCTET_LENGTH_PTR
		- SQL_DESC_INDICATOR_PTR
	*/
	ret = PGAPI_SetDescField(DescriptorHandle, RecNumber, SQL_DESC_TYPE, &Type, 0);
	if (ret != SQL_SUCCESS) return ret;

	/* If Type is SQL_DATETIME or SQL_INTERVAL, the value of SQL_DESC_DATETIME_INTERVAL_CODE is set to SubType. */
	if (Type == SQL_DATETIME || Type == SQL_INTERVAL) {
		ret = PGAPI_SetDescField(DescriptorHandle, RecNumber, SQL_DESC_DATETIME_INTERVAL_CODE, &SubType, 0);
		if (ret != SQL_SUCCESS) return ret;
	}

	ret = PGAPI_SetDescField(DescriptorHandle, RecNumber, SQL_DESC_OCTET_LENGTH, &Length, 0);
	if (ret != SQL_SUCCESS) return ret;

	ret = PGAPI_SetDescField(DescriptorHandle, RecNumber, SQL_DESC_PRECISION, &Precision, 0);
	if (ret != SQL_SUCCESS) return ret;

	ret = PGAPI_SetDescField(DescriptorHandle, RecNumber, SQL_DESC_SCALE, &Scale, 0);
	if (ret != SQL_SUCCESS) return ret;

	/* SQL_DESC_DATA_PTR is only for ARD or APD */
	if (DC_get_desc_type(desc) != SQL_ATTR_IMP_PARAM_DESC)
	{
		ret = PGAPI_SetDescField(DescriptorHandle, RecNumber, SQL_DESC_DATA_PTR, &Data, 0);
		if (ret != SQL_SUCCESS) return ret;
	}

	/* SQL_DESC_OCTET_LENGTH_PTR is only for ARD or APD */
	if (DC_get_desc_type(desc) != SQL_ATTR_IMP_PARAM_DESC)
	{
		ret = PGAPI_SetDescField(DescriptorHandle, RecNumber, SQL_DESC_OCTET_LENGTH_PTR, StringLength, 0);
		if (ret != SQL_SUCCESS) return ret;
	}

	/* SQL_DESC_INDICATOR_PTR is only for ARD or APD */
	if (DC_get_desc_type(desc) != SQL_ATTR_IMP_PARAM_DESC)
	{
		ret = PGAPI_SetDescField(DescriptorHandle, RecNumber, SQL_DESC_INDICATOR_PTR, Indicator, 0);
		if (ret != SQL_SUCCESS) return ret;
	}

	return SQL_SUCCESS;
}

/*
 * PGAPI_GetDescRec
 *
 * Description:
 *      Core implementation function that retrieves the current settings or values 
 *      of fields in a descriptor record. This function is called by both SQLGetDescRec 
 *      (ANSI) and SQLGetDescRecW (Unicode) functions.
 *
 * Parameters:
 *      DescriptorHandle - Handle to the descriptor
 *      RecNumber - The descriptor record number (1-based)
 *      Name - Buffer to store the descriptor name
 *      BufferLength - Length of the Name buffer in bytes
 *      StringLength - Pointer to store the actual length of the name
 *      Type - Pointer to store the SQL data type
 *      SubType - Pointer to store the data type subcode
 *      Length - Pointer to store the data length
 *      Precision - Pointer to store the numeric precision
 *      Scale - Pointer to store the numeric scale
 *      Nullable - Pointer to store the nullability attribute
 *
 * Returns:
 *      SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE
 *
 * Implementation Strategy:
 *      This function retrieves multiple descriptor fields by making individual calls
 *      to PGAPI_GetDescField for each requested attribute. It handles the complexity
 *      of retrieving all the necessary descriptor information in one function call.
 */
RETCODE		SQL_API
PGAPI_GetDescRec(SQLHDESC DescriptorHandle,
			SQLSMALLINT RecNumber, SQLCHAR *Name,
			SQLSMALLINT BufferLength, SQLSMALLINT *StringLength,
			SQLSMALLINT *Type, SQLSMALLINT *SubType,
			SQLLEN *Length, SQLSMALLINT *Precision,
			SQLSMALLINT *Scale, SQLSMALLINT *Nullable)
{
	RETCODE		ret = SQL_SUCCESS;
	SQLSMALLINT strlen, typ, subtyp, prec, scal, null;
	SQLLEN len;

	/* Log function entry with important parameters */
	MYLOG(0, "entering h=%p(%d) rec=" FORMAT_SMALLI " name=%p blen=" FORMAT_SMALLI "\n", DescriptorHandle, DC_get_desc_type((DescriptorClass *) DescriptorHandle), RecNumber, Name, BufferLength);
	MYLOG(0, "str=%p type=%p sub=%p len=%p prec=%p scale=%p null=%p\n", StringLength, Type, SubType, Length, Precision, Scale, Nullable);

	/*
		Get following descriptor fields:

		- SQL_DESC_TYPE
		- SQL_DESC_DATETIME_INTERVAL_CODE
		- SQL_DESC_OCTET_LENGTH
		- SQL_DESC_PRECISION
		- SQL_DESC_SCALE
		- SQL_DESC_NULLABLE
		- SQL_DESC_NAME
	*/

	/* Retrieve SQL data type if requested */
	if (Type != NULL)
	{
		ret = PGAPI_GetDescField(DescriptorHandle, RecNumber, SQL_DESC_TYPE, &typ, 0, NULL);
		if (ret != SQL_SUCCESS) return ret;
		*Type = typ;
	}

	/* For datetime and interval types, retrieve the subtype code if requested */
	if (SubType != NULL && (typ == SQL_DATETIME || typ == SQL_INTERVAL))
	{
		ret = PGAPI_GetDescField(DescriptorHandle, RecNumber, SQL_DESC_DATETIME_INTERVAL_CODE, &subtyp, 0, NULL);
		if (ret != SQL_SUCCESS) return ret;
		*SubType = subtyp;
	}

	/* Retrieve octet length (buffer size needed) if requested */
	if (Length != NULL)
	{
		ret = PGAPI_GetDescField(DescriptorHandle, RecNumber, SQL_DESC_OCTET_LENGTH, &len, 0, NULL);
		if (ret != SQL_SUCCESS) return ret;
		*Length = (SQLLEN) len;
	}

	/* Retrieve numeric precision if requested */
	if (Precision != NULL)
	{
		ret = PGAPI_GetDescField(DescriptorHandle, RecNumber, SQL_DESC_PRECISION, &prec, 0, NULL);
		if (ret != SQL_SUCCESS) return ret;
		*Precision = prec;
	}

	/* Retrieve numeric scale if requested */
	if (Scale != NULL)
	{
		ret = PGAPI_GetDescField(DescriptorHandle, RecNumber, SQL_DESC_SCALE, &scal, 0, NULL);
		if (ret != SQL_SUCCESS) return ret;
		*Scale = scal;
	}

	/* Retrieve nullability information if requested (only for implementation descriptors) */
	if (Nullable != NULL && (DC_get_desc_type((DescriptorClass *) DescriptorHandle) == SQL_ATTR_IMP_ROW_DESC 
		|| DC_get_desc_type((DescriptorClass *) DescriptorHandle) == SQL_ATTR_IMP_PARAM_DESC))
	{
		ret = PGAPI_GetDescField(DescriptorHandle, RecNumber, SQL_DESC_NULLABLE, &null, 0, NULL);
		if (ret != SQL_SUCCESS) return ret;
		*Nullable = null;
	}

	/* Retrieve column/parameter name if requested (only for implementation descriptors) */
	if (Name != NULL && (DC_get_desc_type((DescriptorClass *) DescriptorHandle) == SQL_ATTR_IMP_ROW_DESC 
		|| DC_get_desc_type((DescriptorClass *) DescriptorHandle) == SQL_ATTR_IMP_PARAM_DESC))
	{
		ret = PGAPI_GetDescField(DescriptorHandle, RecNumber, SQL_DESC_NAME, Name, BufferLength, (SQLINTEGER *) &strlen);
		if (ret != SQL_SUCCESS) return ret;
		if (StringLength != NULL)
			*StringLength = strlen;
	}

	return SQL_SUCCESS;
}

/*
 * PGAPI_SetStmtAttr
 *
 * Description:
 *      Core implementation function that sets the value of a statement attribute.
 *      This function is called by both SQLSetStmtAttr (ANSI) and SQLSetStmtAttrW (Unicode) functions.
 *
 * Parameters:
 *      StatementHandle - Handle to the statement
 *      Attribute - The attribute to set (SQL_ATTR_* constant)
 *      Value - The value to set for the attribute
 *      StringLength - Length of the Value buffer in bytes (for string data)
 *
 * Returns:
 *      SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE
 *
 * Implementation Strategy:
 *      This function uses a switch statement to handle different attribute types.
 *      Most attributes are stored directly in the statement structure or its descriptors.
 *      For unsupported attributes, it returns an error.
 *      For backward compatibility with ODBC 2.0, it calls PGAPI_SetStmtOption for some attributes.
 */
RETCODE		SQL_API
PGAPI_SetStmtAttr(HSTMT StatementHandle,
				  SQLINTEGER Attribute, PTR Value,
				  SQLINTEGER StringLength)
{
	RETCODE	ret = SQL_SUCCESS;
	CSTR func = "PGAPI_SetStmtAttr";
	StatementClass *stmt = (StatementClass *) StatementHandle;

	MYLOG(0, "entering Handle=%p " FORMAT_INTEGER "," FORMAT_ULEN "(%p)\n", StatementHandle, Attribute, (SQLULEN) Value, Value);
	switch (Attribute)
	{
		case SQL_ATTR_ENABLE_AUTO_IPD:	/* 15 */
			/* Whether automatic population of IPD is supported */
			if (SQL_FALSE == Value)
				break;
		case SQL_ATTR_CURSOR_SCROLLABLE:		/* -1 */
		case SQL_ATTR_CURSOR_SENSITIVITY:		/* -2 */
		case SQL_ATTR_AUTO_IPD:	/* 10001 */
			/* Unsupported attributes */
			SC_set_error(stmt, DESC_OPTION_NOT_FOR_THE_DRIVER, "Unsupported statement option (Set)", func);
			return SQL_ERROR;
		/* case SQL_ATTR_ROW_BIND_TYPE: ** == SQL_BIND_TYPE(ODBC2.0) */
		case SQL_ATTR_IMP_ROW_DESC:	/* 10012 (read-only) */
		case SQL_ATTR_IMP_PARAM_DESC:	/* 10013 (read-only) */
			/* Read-only attributes */
			SC_set_error(stmt, DESC_INVALID_OPTION_IDENTIFIER, "Unsupported statement option (Set)", func);
			return SQL_ERROR;

		case SQL_ATTR_METADATA_ID:		/* 10014 */
			/* Whether identifiers are quoted */
			stmt->options.metadata_id = CAST_UPTR(SQLUINTEGER, Value);
			break;
		case SQL_ATTR_APP_ROW_DESC:		/* 10010 */
			/* Application Row Descriptor */
			/* Application Row Descriptor */
			if (SQL_NULL_HDESC == Value)
			{
				stmt->ard = &(stmt->ardi);
			}
			else
			{
				stmt->ard = (DescriptorClass *) Value;
				MYLOG(DETAIL_LOG_LEVEL, "set ard=%p\n", stmt->ard);
			}
			break;
		case SQL_ATTR_APP_PARAM_DESC:	/* 10011 */
			/* Application Parameter Descriptor */
			if (SQL_NULL_HDESC == Value)
			{
				stmt->apd = &(stmt->apdi);
			}
			else
			{
				stmt->apd = (DescriptorClass *) Value;
			}
			break;
		case SQL_ATTR_FETCH_BOOKMARK_PTR:		/* 16 */
			/* Pointer to bookmark value */
			stmt->options.bookmark_ptr = Value;
			break;
		case SQL_ATTR_PARAM_BIND_OFFSET_PTR:	/* 17 */
			/* Pointer to parameter binding offset */
			SC_get_APDF(stmt)->param_offset_ptr = (SQLULEN *) Value;
			break;
		case SQL_ATTR_PARAM_BIND_TYPE:	/* 18 */
			/* Parameter binding type */
			SC_get_APDF(stmt)->param_bind_type = CAST_UPTR(SQLUINTEGER, Value);
			break;
		case SQL_ATTR_PARAM_OPERATION_PTR:		/* 19 */
			/* Pointer to parameter operation array */
			SC_get_APDF(stmt)->param_operation_ptr = Value;
			break;
		case SQL_ATTR_PARAM_STATUS_PTR:			/* 20 */
			/* Pointer to parameter status array */
			SC_get_IPDF(stmt)->param_status_ptr = (SQLUSMALLINT *) Value;
			break;
		case SQL_ATTR_PARAMS_PROCESSED_PTR:		/* 21 */
			/* Pointer to count of processed parameters */
			SC_get_IPDF(stmt)->param_processed_ptr = (SQLULEN *) Value;
			break;
		case SQL_ATTR_PARAMSET_SIZE:	/* 22 */
			/* Number of sets of parameters */
			SC_get_APDF(stmt)->paramset_size = CAST_UPTR(SQLULEN, Value);
			break;
		case SQL_ATTR_ROW_BIND_OFFSET_PTR:		/* 23 */
			/* Pointer to row binding offset */
			SC_get_ARDF(stmt)->row_offset_ptr = (SQLULEN *) Value;
			break;
		case SQL_ATTR_ROW_OPERATION_PTR:		/* 24 */
			/* Pointer to row operation array */
			SC_get_ARDF(stmt)->row_operation_ptr = Value;
			break;
		case SQL_ATTR_ROW_STATUS_PTR:	/* 25 */
			/* Pointer to row status array */
			SC_get_IRDF(stmt)->rowStatusArray = (SQLUSMALLINT *) Value;
			break;
		case SQL_ATTR_ROWS_FETCHED_PTR: /* 26 */
			/* Pointer to count of fetched rows */
			SC_get_IRDF(stmt)->rowsFetched = (SQLULEN *) Value;
			break;
		case SQL_ATTR_ROW_ARRAY_SIZE:	/* 27 */
			/* Number of rows to fetch */
			SC_get_ARDF(stmt)->size_of_rowset = CAST_UPTR(SQLULEN, Value);
			break;
		default:
			/* For backward compatibility with ODBC 2.0 */
			return PGAPI_SetStmtOption(StatementHandle, (SQLUSMALLINT) Attribute, (SQLULEN) Value);
	}
	return ret;
}

/*	SQL_NEED_DATA callback for PGAPI_BulkOperations */
typedef struct
{
	StatementClass	*stmt;
	SQLSMALLINT	operation;
	char		need_data_callback;
	char		auto_commit_needed;
	ARDFields	*opts;
	int		idx, processed;
}	bop_cdata;

static
RETCODE	bulk_ope_callback(RETCODE retcode, void *para)
{
	CSTR func = "bulk_ope_callback";
	RETCODE	ret = retcode;
	bop_cdata *s = (bop_cdata *) para;
	SQLULEN		global_idx;
	ConnectionClass	*conn;
	QResultClass	*res;
	IRDFields	*irdflds;
	PG_BM		pg_bm;

	if (s->need_data_callback)
	{
		MYLOG(0, "entering in\n");
		s->processed++;
		s->idx++;
	}
	else
	{
		s->idx = s->processed = 0;
	}
	s->need_data_callback = FALSE;
	res = SC_get_Curres(s->stmt);
	for (; SQL_ERROR != ret && s->idx < s->opts->size_of_rowset; s->idx++)
	{
		if (SQL_ADD != s->operation)
		{
			pg_bm = SC_Resolve_bookmark(s->opts, s->idx);
			QR_get_last_bookmark(res, s->idx, &pg_bm.keys);
			global_idx = pg_bm.index;
		}
		/* Note opts->row_operation_ptr is ignored */
		switch (s->operation)
		{
			case SQL_ADD:
				ret = SC_pos_add(s->stmt, (UWORD) s->idx);
				break;
			case SQL_UPDATE_BY_BOOKMARK:
				ret = SC_pos_update(s->stmt, (UWORD) s->idx, global_idx, &(pg_bm.keys));
				break;
			case SQL_DELETE_BY_BOOKMARK:
				ret = SC_pos_delete(s->stmt, (UWORD) s->idx, global_idx, &(pg_bm.keys));
				break;
		}
		if (SQL_NEED_DATA == ret)
		{
			bop_cdata *cbdata = (bop_cdata *) malloc(sizeof(bop_cdata));
			if (!cbdata)
			{
				SC_set_error(s->stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for cbdata.", func);
				return SQL_ERROR;
			}
			memcpy(cbdata, s, sizeof(bop_cdata));
			cbdata->need_data_callback = TRUE;
			if (0 == enqueueNeedDataCallback(s->stmt, bulk_ope_callback, cbdata))
				ret = SQL_ERROR;
			return ret;
		}
		s->processed++;
	}
	conn = SC_get_conn(s->stmt);
	if (s->auto_commit_needed)
		CC_set_autocommit(conn, TRUE);
	irdflds = SC_get_IRDF(s->stmt);
	if (irdflds->rowsFetched)
		*(irdflds->rowsFetched) = s->processed;

	if (res)
		res->recent_processed_row_count = s->stmt->diag_row_count = s->processed;
	return ret;
}

RETCODE	SQL_API
PGAPI_BulkOperations(HSTMT hstmt, SQLSMALLINT operationX)
{
	CSTR func = "PGAPI_BulkOperations";
	bop_cdata	s;
	RETCODE		ret;
	ConnectionClass	*conn;
	BindInfoClass	*bookmark;

	MYLOG(0, "entering operation = %d\n", operationX);
	s.stmt = (StatementClass *) hstmt;
	s.operation = operationX;
	SC_clear_error(s.stmt);
	s.opts = SC_get_ARDF(s.stmt);

	s.auto_commit_needed = FALSE;
	if (SQL_FETCH_BY_BOOKMARK != s.operation)
	{
		conn = SC_get_conn(s.stmt);
		if (s.auto_commit_needed = (char) CC_does_autocommit(conn), s.auto_commit_needed)
			CC_set_autocommit(conn, FALSE);
	}
	if (SQL_ADD != s.operation)
	{
		if (!(bookmark = s.opts->bookmark) || !(bookmark->buffer))
		{
			SC_set_error(s.stmt, DESC_INVALID_OPTION_IDENTIFIER, "bookmark isn't specified", func);
			return SQL_ERROR;
		}
	}

	/* StartRollbackState(s.stmt); */
	if (SQL_FETCH_BY_BOOKMARK == operationX)
		ret = SC_fetch_by_bookmark(s.stmt);
	else
	{
		s.need_data_callback = FALSE;
		ret = bulk_ope_callback(SQL_SUCCESS, &s);
	}
	return ret;
}