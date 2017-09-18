/*-------
 * Module:			execute.c
 *
 * Description:		This module contains routines related to
 *					preparing and executing an SQL statement.
 *
 * Classes:			n/a
 *
 * API functions:	SQLPrepare, SQLExecute, SQLExecDirect, SQLTransact,
 *					SQLCancel, SQLNativeSql, SQLParamData, SQLPutData
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */

#include "psqlodbc.h"
#include "misc.h"

#include <stdio.h>
#include <string.h>

#ifndef	WIN32
#include <ctype.h>
#endif /* WIN32 */

#include "environ.h"
#include "connection.h"
#include "statement.h"
#include "qresult.h"
#include "convert.h"
#include "bind.h"
#include "pgtypes.h"
#include "lobj.h"
#include "pgapifunc.h"

/*		Perform a Prepare on the SQL statement */
RETCODE		SQL_API
PGAPI_Prepare(HSTMT hstmt,
			  const SQLCHAR * szSqlStr,
			  SQLINTEGER cbSqlStr)
{
	CSTR func = "PGAPI_Prepare";
	StatementClass *self = (StatementClass *) hstmt;
	RETCODE	retval = SQL_SUCCESS;
	BOOL	prepared;

	MYLOG(0, "entering...\n");

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(self); */

	/*
	 * According to the ODBC specs it is valid to call SQLPrepare multiple
	 * times. In that case, the bound SQL statement is replaced by the new
	 * one
	 */

	prepared = self->prepared;
	SC_set_prepared(self, NOT_YET_PREPARED);
	switch (self->status)
	{
		case STMT_DESCRIBED:
			MYLOG(0, "**** STMT_DESCRIBED, recycle\n");
			SC_recycle_statement(self); /* recycle the statement, but do
										 * not remove parameter bindings */
			break;

		case STMT_FINISHED:
			MYLOG(0, "**** STMT_FINISHED, recycle\n");
			SC_recycle_statement(self); /* recycle the statement, but do
										 * not remove parameter bindings */
			break;

		case STMT_ALLOCATED:
			MYLOG(0, "**** STMT_ALLOCATED, copy\n");
			self->status = STMT_READY;
			break;

		case STMT_READY:
			MYLOG(0, "**** STMT_READY, change SQL\n");
			if (NOT_YET_PREPARED != prepared)
				SC_recycle_statement(self); /* recycle the statement */
			break;

		case STMT_EXECUTING:
			MYLOG(0, "**** STMT_EXECUTING, error!\n");

			SC_set_error(self, STMT_SEQUENCE_ERROR, "PGAPI_Prepare(): The handle does not point to a statement that is ready to be executed", func);

			retval = SQL_ERROR;
			goto cleanup;

		default:
			SC_set_error(self, STMT_INTERNAL_ERROR, "An Internal Error has occured -- Unknown statement status.", func);
			retval = SQL_ERROR;
			goto cleanup;
	}

	SC_initialize_stmts(self, TRUE);

	if (!szSqlStr)
	{
		SC_set_error(self, STMT_NO_MEMORY_ERROR, "the query is NULL", func);
		retval = SQL_ERROR;
		goto cleanup;
	}
	if (!szSqlStr[0])
		self->statement = strdup("");
	else
		self->statement = make_string(szSqlStr, cbSqlStr, NULL, 0);
	if (!self->statement)
	{
		SC_set_error(self, STMT_NO_MEMORY_ERROR, "No memory available to store statement", func);
		retval = SQL_ERROR;
		goto cleanup;
	}

	self->prepare = PREPARE_STATEMENT;
	self->statement_type = statement_type(self->statement);

	/* Check if connection is onlyread (only selects are allowed) */
	if (CC_is_onlyread(SC_get_conn(self)) && STMT_UPDATE(self))
	{
		SC_set_error(self, STMT_EXEC_ERROR, "Connection is readonly, only select statements are allowed.", func);
		retval = SQL_ERROR;
		goto cleanup;
	}

cleanup:
#undef	return
MYLOG(1, "leaving %d\n", retval);
	return retval;
}


/*		Performs the equivalent of SQLPrepare, followed by SQLExecute. */
RETCODE		SQL_API
PGAPI_ExecDirect(HSTMT hstmt,
				 const SQLCHAR * szSqlStr,
				 SQLINTEGER cbSqlStr,
				 UWORD flag)
{
	StatementClass *stmt = (StatementClass *) hstmt;
	RETCODE		result;
	CSTR func = "PGAPI_ExecDirect";
	const ConnectionClass	*conn = SC_get_conn(stmt);

	MYLOG(0, "entering...%x\n", flag);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	/*
	 * keep a copy of the un-parametrized statement, in case they try to
	 * execute this statement again
	 */
	stmt->statement = make_string(szSqlStr, cbSqlStr, NULL, 0);
MYLOG(1, "a2\n");
	if (!stmt->statement)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "No memory available to store statement", func);
		return SQL_ERROR;
	}

	MYLOG(0, "**** hstmt=%p, statement='%s'\n", hstmt, stmt->statement);

	if (0 != (flag & PODBC_WITH_HOLD))
		SC_set_with_hold(stmt);
	if (0 != (flag & PODBC_RDONLY))
		SC_set_readonly(stmt);

	/*
	 * If an SQLPrepare was performed prior to this, but was left in the
	 * described state because an error occurred prior to SQLExecute then
	 * set the statement to finished so it can be recycled.
	 */
	if (stmt->status == STMT_DESCRIBED)
		stmt->status = STMT_FINISHED;

	stmt->statement_type = statement_type(stmt->statement);

	/* Check if connection is onlyread (only selects are allowed) */
	if (CC_is_onlyread(conn) && STMT_UPDATE(stmt))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "Connection is readonly, only select statements are allowed.", func);
		return SQL_ERROR;
	}

	MYLOG(0, "calling PGAPI_Execute...\n");

	result = PGAPI_Execute(hstmt, flag);

	MYLOG(0, "leaving %hd\n", result);
	return result;
}

static int
inquireHowToPrepare(const StatementClass *stmt)
{
	ConnectionClass	*conn;
	ConnInfo	*ci;
	int		ret = 0;

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
	if (!ci->use_server_side_prepare)
	{
		/* Do prepare operations by the driver itself */
		return PREPARE_BY_THE_DRIVER;
	}
	if (NOT_YET_PREPARED == stmt->prepared)
	{
		SQLSMALLINT	num_params;

		if (STMT_TYPE_DECLARE == stmt->statement_type &&
		    PG_VERSION_LT(conn, 8.0))
		{
			return PREPARE_BY_THE_DRIVER;
		}
		if (stmt->multi_statement < 0)
			PGAPI_NumParams((StatementClass *) stmt, &num_params);
		if (stmt->multi_statement > 0)
		{
			/*
			 * divide the query into multiple commands and apply V3 parse
			 * requests for each of them
			 */
			ret = PARSE_REQ_FOR_INFO;
		}
		else
		{
			if (SC_may_use_cursor(stmt))
			{
				if (ci->drivers.use_declarefetch)
					return PARSE_REQ_FOR_INFO;
				else if (SQL_CURSOR_FORWARD_ONLY != stmt->options.cursor_type)
					ret = PARSE_REQ_FOR_INFO;
				else
					ret = PARSE_TO_EXEC_ONCE;
			}
			else
				ret = PARSE_TO_EXEC_ONCE;
		}
	}
	if (SC_is_prepare_statement(stmt) && (PARSE_TO_EXEC_ONCE == ret))
		ret = NAMED_PARSE_REQUEST;

	return ret;
}

int
decideHowToPrepare(StatementClass *stmt, BOOL force)
{
	int	method = SC_get_prepare_method(stmt);

	if (0 != method) /* a method was already determined */
		return method;
	switch (stmt->prepare)
	{
		case NON_PREPARE_STATEMENT: /* not a prepare statement */
			if (!force)
				return method;
			break;
	}
	method = inquireHowToPrepare(stmt);
	stmt->prepare |= method;
	if (PREPARE_BY_THE_DRIVER == method)
		stmt->discard_output_params = 1;
	return method;
}

/* dont/should/can send Parse request ? */
enum {
	 doNothing = 0
	,allowParse
	,preferParse
	,shouldParse
	,usingCommand
};

#define	ONESHOT_CALL_PARSE		allowParse
#define	NOPARAM_ONESHOT_CALL_PARSE	doNothing

static
int HowToPrepareBeforeExec(StatementClass *stmt, BOOL checkOnly)
{
	SQLSMALLINT	num_params = stmt->num_params;
	ConnectionClass	*conn = SC_get_conn(stmt);
	ConnInfo *ci = &(conn->connInfo);
	int		nCallParse = doNothing, how_to_prepare = 0;
	BOOL		bNeedsTrans = FALSE;

	if (num_params < 0)
		PGAPI_NumParams(stmt, &num_params);
	how_to_prepare = decideHowToPrepare(stmt, checkOnly);
	if (checkOnly)
	{
		if (num_params <= 0)
			return doNothing;
	}
	else
	{
		switch (how_to_prepare)
		{
			case NAMED_PARSE_REQUEST:
				return shouldParse;
			case PARSE_TO_EXEC_ONCE:
				switch (stmt->prepared)
				{
					case PREPARED_TEMPORARILY:
						nCallParse = preferParse;
						break;
					default:
						if (num_params <= 0)
							nCallParse = NOPARAM_ONESHOT_CALL_PARSE;
						else
							nCallParse = ONESHOT_CALL_PARSE;
				}
				break;
			default:
				return doNothing;
		}
	}

	if (num_params > 0)
	{
		int	param_number = -1;
		ParameterInfoClass *apara;
		ParameterImplClass *ipara;
		OID	pgtype;

		while (TRUE)
		{
			SC_param_next(stmt, &param_number, &apara, &ipara);
			if (!ipara || !apara)
				break;
			pgtype = PIC_get_pgtype(*ipara);
			if (checkOnly)
			{
				switch (ipara->SQLType)
				{
					case SQL_LONGVARBINARY:
						if (0 == pgtype)
						{
							if (ci->bytea_as_longvarbinary &&
							    0 != conn->lobj_type)
								nCallParse = shouldParse;
						}
						break;
					case SQL_CHAR:
						if (ci->cvt_null_date_string)
							nCallParse = shouldParse;
						break;
					case SQL_VARCHAR:
						if (ci->drivers.bools_as_char &&
						    PG_WIDTH_OF_BOOLS_AS_CHAR == ipara->column_size)
							nCallParse = shouldParse;
						break;
				}
			}
			else
			{
				BOOL	bBytea = FALSE;

				switch (ipara->SQLType)
				{
					case SQL_LONGVARBINARY:
						if (conn->lobj_type == pgtype || PG_TYPE_OID == pgtype)
							bNeedsTrans = TRUE;
						else if (PG_TYPE_BYTEA == pgtype)
							bBytea = TRUE;
						else if (0 == pgtype)
						{
							if (ci->bytea_as_longvarbinary)
								bBytea = TRUE;
							else
								bNeedsTrans = TRUE;
						}
						if (bBytea)
							if (nCallParse < preferParse)
								nCallParse = preferParse;
						break;
				}
			}
		}
	}
	if (bNeedsTrans &&
	    PARSE_TO_EXEC_ONCE == how_to_prepare)
	{
		if (!CC_is_in_trans(conn) && CC_does_autocommit(conn))
			nCallParse = doNothing;
	}
	return nCallParse;
}

static
const char *GetSvpName(const ConnectionClass *conn, char *wrk, int wrksize)
{
	snprintf(wrk, wrksize, "_EXEC_SVP_%p", conn);
	return wrk;
}

/*
 *	The execution after all parameters were resolved.
 */
static
RETCODE	Exec_with_parameters_resolved(StatementClass *stmt, BOOL *exec_end)
{
	CSTR func = "Exec_with_parameters_resolved";
	RETCODE		retval;
	SQLLEN		end_row;
	SQLINTEGER	cursor_type, scroll_concurrency;
	ConnectionClass	*conn;
	QResultClass	*res;
	APDFields	*apdopts;
	IPDFields	*ipdopts;
	BOOL		prepare_before_exec = FALSE;

	*exec_end = FALSE;
	conn = SC_get_conn(stmt);
	MYLOG(0, "copying statement params: trans_status=%d, len=" FORMAT_SIZE_T ", stmt='%s'\n", conn->transact_status, strlen(stmt->statement), stmt->statement);

#define	return	DONT_CALL_RETURN_FROM_HERE???
#define	RETURN(code)	{ retval = code; goto cleanup; }
	ENTER_CONN_CS(conn);
	/* save the cursor's info before the execution */
	cursor_type = stmt->options.cursor_type;
	scroll_concurrency = stmt->options.scroll_concurrency;
	/* Prepare the statement if possible at backend side */
	if (HowToPrepareBeforeExec(stmt, FALSE) >= allowParse)
		prepare_before_exec = TRUE;

MYLOG(1, "prepare_before_exec=%d srv=%d\n", prepare_before_exec, conn->connInfo.use_server_side_prepare);
	/* Create the statement with parameters substituted. */
	retval = copy_statement_with_parameters(stmt, prepare_before_exec);
	stmt->current_exec_param = -1;
	if (retval != SQL_SUCCESS)
	{
		stmt->exec_current_row = -1;
		*exec_end = TRUE;
		RETURN(retval) /* error msg is passed from the above */
	}

	MYLOG(0, "   stmt_with_params = '%s'\n", stmt->stmt_with_params);

	/*
	 *	The real execution.
	 */
MYLOG(0, "about to begin SC_execute\n");
	retval = SC_execute(stmt);
	if (retval == SQL_ERROR)
	{
		stmt->exec_current_row = -1;
		*exec_end = TRUE;
		RETURN(retval)
	}
	res = SC_get_Result(stmt);
	/* special handling of result for keyset driven cursors */
	if (SQL_CURSOR_KEYSET_DRIVEN == stmt->options.cursor_type &&
	    SQL_CONCUR_READ_ONLY != stmt->options.scroll_concurrency)
	{
		QResultClass	*kres;

		if (kres = res->next, kres)
		{
			QR_set_fields(kres, QR_get_fields(res));
			QR_set_fields(res,  NULL);
			kres->num_fields = res->num_fields;
			res->next = NULL;
			SC_set_Result(stmt, kres);
			res = kres;
		}
	}
	ipdopts = SC_get_IPDF(stmt);
	if (ipdopts->param_status_ptr)
	{
		switch (retval)
		{
			case SQL_SUCCESS:
				ipdopts->param_status_ptr[stmt->exec_current_row] = SQL_PARAM_SUCCESS;
				break;
			case SQL_SUCCESS_WITH_INFO:
				ipdopts->param_status_ptr[stmt->exec_current_row] = SQL_PARAM_SUCCESS_WITH_INFO;
				break;
			default:
				ipdopts->param_status_ptr[stmt->exec_current_row] = SQL_PARAM_ERROR;
				break;
		}
	}
	if (end_row = stmt->exec_end_row, end_row < 0)
	{
		apdopts = SC_get_APDF(stmt);
		end_row = (SQLINTEGER) apdopts->paramset_size - 1;
	}
	if (stmt->exec_current_row >= end_row)
	{
		*exec_end = TRUE;
		stmt->exec_current_row = -1;
	}
	else
		stmt->exec_current_row++;
	if (res)
	{
		EnvironmentClass *env = (EnvironmentClass *) CC_get_env(conn);
		const char *cmd = QR_get_command(res);
		SQLLEN	start_row;
		if (start_row = stmt->exec_start_row, start_row < 0)
			start_row = 0;

		if (retval == SQL_SUCCESS &&
		    NULL != cmd &&
		    start_row >= end_row &&
		    NULL != env &&
		    EN_is_odbc3(env))
		{
			int     count;

			if (sscanf(cmd , "UPDATE %d", &count) == 1)
				;
			else if (sscanf(cmd , "DELETE %d", &count) == 1)
				;
			else
				count = -1;
			if (0 == count)
				retval = SQL_NO_DATA;
		}
		stmt->diag_row_count = res->recent_processed_row_count;
	}
	/*
	 *	The cursor's info was changed ?
	 */
	if (retval == SQL_SUCCESS &&
	    (stmt->options.cursor_type != cursor_type ||
	     stmt->options.scroll_concurrency != scroll_concurrency))
	{
		SC_set_error(stmt, STMT_OPTION_VALUE_CHANGED, "cursor updatability changed", func);
		retval = SQL_SUCCESS_WITH_INFO;
	}

cleanup:
#undef	RETURN
#undef	return
	LEAVE_CONN_CS(conn);
	return retval;
}

int
StartRollbackState(StatementClass *stmt)
{
	int			ret;
	ConnectionClass	*conn;
	ConnInfo	*ci = NULL;

MYLOG(1, "entering %p->external=%d\n", stmt, stmt->external);
	conn = SC_get_conn(stmt);
	if (conn)
		ci = &conn->connInfo;

	if (!ci || ci->rollback_on_error < 0) /* default */
	{
		if (conn && PG_VERSION_GE(conn, 8.0))
			ret = 2; /* statement rollback */
		else
			ret = 1; /* transaction rollback */
	}
	else
	{
		ret = ci->rollback_on_error;
		if (2 == ret && PG_VERSION_LT(conn, 8.0))
			ret = 1;
	}

	switch (ret)
	{
		case 1:
			SC_start_tc_stmt(stmt);
			break;
		case 2:
			SC_start_rb_stmt(stmt);
			break;
	}
	return	ret;
}

int
GenerateSvpCommand(ConnectionClass *conn, int type, char *cmd, int buflen)
{
	char esavepoint[50];
	int	rtn = -1;

	cmd[0] = '\0';
	switch (type)
	{
		case INTERNAL_SAVEPOINT_OPERATION:	/* savepoint */
#ifdef	_RELEASE_INTERNAL_SAVEPOINT
			if (conn->internal_svp)
				rtn = snprintf(cmd, buflen, "RELEASE %s;", GetSvpName(conn, esavepoint, sizeof(esavepoint)));
#endif /* _RELEASE_INTERNAL_SAVEPOINT */
			rtn = snprintfcat(cmd, buflen, "SAVEPOINT %s", GetSvpName(conn, esavepoint, sizeof(esavepoint)));
			break;
		case INTERNAL_ROLLBACK_OPERATION: /* rollback */
			if (conn->internal_svp)
				rtn = snprintf(cmd, buflen, "ROLLBACK TO %s", GetSvpName(conn, esavepoint, sizeof(esavepoint)));
			else
				rtn = snprintf(cmd, buflen, "ROLLBACK");

			break;
	}

	return rtn;
}

/*
 *	Must be in a transaction or the subsequent execution
 *	invokes a transaction.
 */
RETCODE
SetStatementSvp(StatementClass *stmt, unsigned int option)
{
	CSTR	func = "SetStatementSvp";
	char	cmd[128];
	ConnectionClass	*conn = SC_get_conn(stmt);
	QResultClass *res;
	RETCODE	ret = SQL_SUCCESS_WITH_INFO;

	if (NULL == conn->pqconn)
	{
		SC_set_error(stmt, STMT_COMMUNICATION_ERROR, "The connection has been lost", __FUNCTION__);
		return SQL_ERROR;
	}
	if (CC_is_in_error_trans(conn))
		return ret;

	if (0 == conn->lock_CC_for_rb)
	{
		ENTER_CONN_CS(conn);
		conn->lock_CC_for_rb++;
	}
MYLOG(1, " %p->accessed=%d opt=%u in_progress=%u prev=%u\n", conn, CC_accessed_db(conn), option, conn->opt_in_progress, conn->opt_previous);
	conn->opt_in_progress &= option;
	switch (stmt->statement_type)
	{
		case STMT_TYPE_SPECIAL:
		case STMT_TYPE_TRANSACTION:
			return ret;
	}
	/* If rbpoint is not yet started and the previous statement was not read-only */
	if (!CC_started_rbpoint(conn) && 0 == (conn->opt_previous & SVPOPT_RDONLY))
	{
		BOOL	need_savep = FALSE;

		if (SC_is_rb_stmt(stmt))
		{
			if (CC_is_in_trans(conn)) /* needless to issue SAVEPOINT before the 1st command */
			{
				need_savep = TRUE;
			}
		}
		if (need_savep)
		{
			if (0 != (option & SVPOPT_REDUCE_ROUNDTRIP))
			{
				conn->internal_op = PREPEND_IN_PROGRESS;
				CC_set_accessed_db(conn);
				return ret;
			}
			GenerateSvpCommand(conn, INTERNAL_SAVEPOINT_OPERATION, cmd, sizeof(cmd));
			conn->internal_op = SAVEPOINT_IN_PROGRESS;
			res = CC_send_query(conn, cmd, NULL, 0, NULL);
			conn->internal_op = 0;
			if (QR_command_maybe_successful(res))
				ret = SQL_SUCCESS;
			else
			{
				SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal SAVEPOINT failed", func);
				ret = SQL_ERROR;
			}
			QR_Destructor(res);
		}
	}
	CC_set_accessed_db(conn);
MYLOG(1, "leaving %p->accessed=%d\n", conn, CC_accessed_db(conn));
	return ret;
}

RETCODE
DiscardStatementSvp(StatementClass *stmt, RETCODE ret, BOOL errorOnly)
{
	CSTR	func = "DiscardStatementSvp";
	ConnectionClass	*conn = SC_get_conn(stmt);
	BOOL	start_stmt = FALSE;

MYLOG(1, "entering %p->accessed=%d is_in=%d is_rb=%d is_tc=%d\n", conn, CC_accessed_db(conn),
CC_is_in_trans(conn), SC_is_rb_stmt(stmt), SC_is_tc_stmt(stmt));
	if (conn->lock_CC_for_rb > 0)
		MYLOG(0, "in_progress=%u previous=%d\n", conn->opt_in_progress, conn->opt_previous);
	switch (ret)
	{
		case SQL_NEED_DATA:
			break;
		case SQL_ERROR:
			start_stmt = TRUE;
			break;
		default:
			if (!errorOnly)
				start_stmt = TRUE;
			break;
	}
	if (!CC_accessed_db(conn) || !CC_is_in_trans(conn))
		goto cleanup;
	if (!SC_is_rb_stmt(stmt) && !SC_is_tc_stmt(stmt))
		goto cleanup;
	if (SQL_ERROR == ret)
	{
		if (CC_started_rbpoint(conn) && conn->internal_svp)
		{
			int	cmd_success = CC_internal_rollback(conn, PER_STATEMENT_ROLLBACK, FALSE);

			if (!cmd_success)
			{
				SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal ROLLBACK failed", func);
				goto cleanup;
			}
		}
		else
		{
			CC_abort(conn);
			goto cleanup;
		}
	}
	else if (errorOnly)
		return ret;
MYLOG(1, "\tret=%d\n", ret);
cleanup:
#ifdef NOT_USED
	if (!SC_is_prepare_statement(stmt) && ONCE_DESCRIBED == stmt->prepared)
		SC_set_prepared(stmt, NOT_YET_PREPARED);
#endif
	if (start_stmt || SQL_ERROR == ret)
	{
		stmt->execinfo = 0;
		if (SQL_ERROR != ret && CC_accessed_db(conn))
		{
			conn->opt_previous = conn->opt_in_progress;
			CC_init_opt_in_progress(conn);
		}
		while (conn->lock_CC_for_rb > 0)
		{
			LEAVE_CONN_CS(conn);
			conn->lock_CC_for_rb--;
			MYLOG(1, " release conn_lock\n");
		}
		CC_start_stmt(conn);
	}
	MYLOG(1, "leaving %d\n", ret);
	return ret;
}

/*
 * Given a SQL statement, see if it is an INSERT INTO statement and extract
 * the name of the table (with schema) of the table that was inserted to.
 * (It is needed to resolve any @@identity references in the future.)
 */
void
SC_setInsertedTable(StatementClass *stmt, RETCODE retval)
{
	const char *cmd = stmt->statement;
	ConnectionClass	*conn;
	size_t	len;

	if (STMT_TYPE_INSERT != stmt->statement_type)
		return;
	if (!SQL_SUCCEEDED(retval))
		return;
	conn = SC_get_conn(stmt);
#ifdef	NOT_USED /* give up the use of lastval() */
	if (PG_VERSION_GE(conn, 8.1)) /* lastval() is available */
		return;
#endif /* NOT_USED */
	/*if (!CC_fake_mss(conn))
		return;*/

	/*
	 * Parse a statement that was just executed. If it looks like an INSERT INTO
	 * statement, try to extract the table name (and schema) of the table that
	 * we inserted into.
	 *
	 * This is by no means fool-proof, we don't implement the whole backend
	 * lexer and grammar here, but should handle most simple INSERT statements.
	 */
	while (isspace((UCHAR) *cmd)) cmd++;
	if (!*cmd)
		return;
	len = 6;
	if (strnicmp(cmd, "insert", len))
		return;
	cmd += len;
	while (isspace((UCHAR) *(++cmd)));
	if (!*cmd)
		return;
	len = 4;
	if (strnicmp(cmd, "into", len))
		return;
	cmd += len;
	while (isspace((UCHAR) *cmd)) cmd++;
	if (!*cmd)
		return;
	NULL_THE_NAME(conn->schemaIns);
	NULL_THE_NAME(conn->tableIns);

	eatTableIdentifiers((const UCHAR *) cmd, conn->ccsc, &conn->tableIns, &conn->schemaIns);
	if (!NAME_IS_VALID(conn->tableIns))
		NULL_THE_NAME(conn->schemaIns);
}

/*	Execute a prepared SQL statement */
RETCODE		SQL_API
PGAPI_Execute(HSTMT hstmt, UWORD flag)
{
	CSTR func = "PGAPI_Execute";
	StatementClass *stmt = (StatementClass *) hstmt;
	RETCODE		retval = SQL_SUCCESS;
	ConnectionClass	*conn;
	APDFields	*apdopts;
	IPDFields	*ipdopts;
	SQLLEN		i, start_row, end_row;
	BOOL	exec_end, recycled = FALSE, recycle = TRUE;
	SQLSMALLINT	num_params;

	MYLOG(0, "entering...%x\n", flag);

	conn = SC_get_conn(stmt);
	apdopts = SC_get_APDF(stmt);

	/*
	 * If the statement was previously described, just recycle the old result
	 * set that contained just the column information.
	 */
	if (stmt->prepare && stmt->status == STMT_DESCRIBED)
	{
		stmt->exec_current_row = -1;
		SC_recycle_statement(stmt);
	}

	MYLOG(0, "clear errors...\n");

	SC_clear_error(stmt);
	if (!stmt->statement)
	{
		SC_set_error(stmt, STMT_NO_STMTSTRING, "This handle does not have a SQL statement stored in it", func);
		MYLOG(0, "problem with handle\n");
		return SQL_ERROR;
	}

#define	return	DONT_CALL_RETURN_FROM_HERE???

	if (stmt->exec_current_row > 0)
	{
		/*
		 * executing an array of parameters.
		 * Don't recycle the statement.
		 */
		recycle = FALSE;
	}
	else if (PREPARED_PERMANENTLY == stmt->prepared ||
		 PREPARED_TEMPORARILY == stmt->prepared)
	{
		/*
		 * re-executing an prepared statement.
		 * Don't recycle the statement but
		 * discard the old result.
		 */
		recycle = FALSE;
		SC_reset_result_for_rerun(stmt);
	}
	/*
	 * If SQLExecute is being called again, recycle the statement. Note
	 * this should have been done by the application in a call to
	 * SQLFreeStmt(SQL_CLOSE) or SQLCancel.
	 */
	else if (stmt->status == STMT_FINISHED)
	{
		MYLOG(0, "recycling statement (should have been done by app)...\n");
/******** Is this really NEEDED ? ******/
		SC_recycle_statement(stmt);
		recycled = TRUE;
	}
	/* Check if the statement is in the correct state */
	else if ((stmt->prepare && stmt->status != STMT_READY) ||
		(stmt->status != STMT_ALLOCATED && stmt->status != STMT_READY))
	{
		SC_set_error(stmt, STMT_STATUS_ERROR, "The handle does not point to a statement that is ready to be executed", func);
		MYLOG(0, "problem with statement\n");
		retval = SQL_ERROR;
		goto cleanup;
	}

	if (start_row = stmt->exec_start_row, start_row < 0)
		start_row = 0;
	if (end_row = stmt->exec_end_row, end_row < 0)
		end_row = (SQLINTEGER) apdopts->paramset_size - 1;
	if (stmt->exec_current_row < 0)
		stmt->exec_current_row = start_row;
	ipdopts = SC_get_IPDF(stmt);
	num_params = stmt->num_params;
	if (num_params < 0)
		PGAPI_NumParams(stmt, &num_params);
	if (stmt->exec_current_row == start_row)
	{
		/*
		   We sometimes need to know about the PG type of binding
		   parameters even in case of non-prepared statements.
		 */
		int	nCallParse = doNothing;

		if (NOT_YET_PREPARED == stmt->prepared)
		{
			switch (nCallParse = HowToPrepareBeforeExec(stmt, TRUE))
			{
				case shouldParse:
					if (retval = prepareParameters(stmt, FALSE), SQL_ERROR == retval)
						goto cleanup;
					break;
			}
		}
MYLOG(0, "prepareParameters was %s called, prepare state:%d\n", shouldParse == nCallParse ? "" : "not", stmt->prepare);

		if (shouldParse == nCallParse &&
		    PREPARE_BY_THE_DRIVER == stmt->prepare)
		{
			SC_set_Result(stmt, NULL);
		}
		if (ipdopts->param_processed_ptr)
			*ipdopts->param_processed_ptr = 0;
		/*
		 * Initialize param_status_ptr
		 */
		if (ipdopts->param_status_ptr)
		{
			for (i = 0; i <= end_row; i++)
				ipdopts->param_status_ptr[i] = SQL_PARAM_UNUSED;
		}
		if (recycle && !recycled)
			SC_recycle_statement(stmt);
		if (isSqlServr() &&
		    stmt->external &&
		    0 != stmt->prepare &&
		    PG_VERSION_LT(conn, 8.4) &&
		    SC_can_parse_statement(stmt))
			parse_sqlsvr(stmt);
	}

next_param_row:
	if (apdopts->param_operation_ptr)
	{
		while (apdopts->param_operation_ptr[stmt->exec_current_row] == SQL_PARAM_IGNORE)
		{
			if (stmt->exec_current_row >= end_row)
			{
				stmt->exec_current_row = -1;
				retval = SQL_SUCCESS;
				goto cleanup;
			}
			++stmt->exec_current_row;
		}
	}
	/*
	 *	Initialize the current row status
	 */
	if (ipdopts->param_status_ptr)
		ipdopts->param_status_ptr[stmt->exec_current_row] = SQL_PARAM_ERROR;

	/*
	 * Check if statement has any data-at-execute parameters when it is
	 * not in SC_pre_execute.
	 */
	{
		/*
		 * The bound parameters could have possibly changed since the last
		 * execute of this statement?  Therefore check for params and
		 * re-copy.
		 */
		SQLULEN	offset = apdopts->param_offset_ptr ? *apdopts->param_offset_ptr : 0;
		SQLINTEGER	bind_size = apdopts->param_bind_type;
		SQLLEN		current_row = stmt->exec_current_row < 0 ? 0 : stmt->exec_current_row;
		Int4	num_p = num_params < apdopts->allocated ? num_params : apdopts->allocated;

		/*
		 *	Increment the  number of currently processed rows
		 */
		if (ipdopts->param_processed_ptr)
			(*ipdopts->param_processed_ptr)++;
		stmt->data_at_exec = -1;
		for (i = 0; i < num_p; i++)
		{
			SQLLEN	   *pcVal = apdopts->parameters[i].used;

			apdopts->parameters[i].data_at_exec = FALSE;
			if (pcVal)
			{
				if (bind_size > 0)
					pcVal = LENADDR_SHIFT(pcVal, offset + bind_size * current_row);
				else
					pcVal = LENADDR_SHIFT(pcVal, offset) + current_row;
				if (*pcVal == SQL_DATA_AT_EXEC || *pcVal <= SQL_LEN_DATA_AT_EXEC_OFFSET)
					apdopts->parameters[i].data_at_exec = TRUE;
			}
			/* Check for data at execution parameters */
			if (apdopts->parameters[i].data_at_exec)
			{
				MYLOG(0, "The " FORMAT_LEN "th parameter of " FORMAT_LEN "-row is data at exec(" FORMAT_LEN ")\n", i, current_row, pcVal ? (*pcVal) : -1);
				if (stmt->data_at_exec < 0)
					stmt->data_at_exec = 1;
				else
					stmt->data_at_exec++;
			}
		}

		/*
		 * If there are some data at execution parameters, return need
		 * data
		 */

		/*
		 * SQLParamData and SQLPutData will be used to send params and
		 * execute the statement.
		 */
		if (stmt->data_at_exec > 0)
		{
			retval = SQL_NEED_DATA;
			goto cleanup;
		}
	}

	if (0 != (flag & PODBC_WITH_HOLD))
		SC_set_with_hold(stmt);
	retval = Exec_with_parameters_resolved(stmt, &exec_end);
	if (!exec_end)
	{
		stmt->curr_param_result = 0;
		goto next_param_row;
	}
cleanup:
MYLOG(0, "leaving retval=%d\n", retval);
	SC_setInsertedTable(stmt, retval);
#undef	return
	return retval;
}


RETCODE		SQL_API
PGAPI_Transact(HENV henv,
			   HDBC hdbc,
			   SQLUSMALLINT fType)
{
	CSTR func = "PGAPI_Transact";
	ConnectionClass *conn;
	char		ok;
	int			lf;

	MYLOG(0, "entering hdbc=%p, henv=%p\n", hdbc, henv);

	if (hdbc == SQL_NULL_HDBC && henv == SQL_NULL_HENV)
	{
		CC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	/*
	 * If hdbc is null and henv is valid, it means transact all
	 * connections on that henv.
	 */
	if (hdbc == SQL_NULL_HDBC && henv != SQL_NULL_HENV)
	{
		ConnectionClass * const *conns = getConnList();
		const int	conn_count = getConnCount();
		for (lf = 0; lf < conn_count; lf++)
		{
			conn = conns[lf];

			if (conn && CC_get_env(conn) == henv)
				if (PGAPI_Transact(henv, (HDBC) conn, fType) != SQL_SUCCESS)
					return SQL_ERROR;
		}
		return SQL_SUCCESS;
	}

	conn = (ConnectionClass *) hdbc;

	if (fType != SQL_COMMIT &&
	    fType != SQL_ROLLBACK)
	{
		CC_set_error(conn, CONN_INVALID_ARGUMENT_NO, "PGAPI_Transact can only be called with SQL_COMMIT or SQL_ROLLBACK as parameter", func);
		return SQL_ERROR;
	}

	/* If manual commit and in transaction, then proceed. */
	if (CC_loves_visible_trans(conn) && CC_is_in_trans(conn))
	{
		MYLOG(0, "sending on conn %p '%d'\n", conn, fType);

		ok = (SQL_COMMIT == fType) ? CC_commit(conn) : CC_abort(conn);
		if (!ok)
		{
			/* error msg will be in the connection */
			CC_on_abort(conn, NO_TRANS);
			CC_log_error(func, "", conn);
			return SQL_ERROR;
		}
	}
	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_Cancel(HSTMT hstmt)		/* Statement to cancel. */
{
	CSTR func = "PGAPI_Cancel";
	StatementClass *stmt = (StatementClass *) hstmt, *estmt;
	ConnectionClass *conn;
	RETCODE		ret = SQL_SUCCESS;

	MYLOG(0, "entering...\n");

	/* Check if this can handle canceling in the middle of a SQLPutData? */
	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}
	conn = SC_get_conn(stmt);

	if (stmt->execute_delegate)
		estmt = stmt->execute_delegate;
	else
		estmt = stmt;

	/*
	 * SQLCancel works differently depending on what the statement is
	 * currently doing:
	 *
	 * 1. In the middle of SQLParamData / SQLPutData
	 *    -> cancel the statement
	 *
	 * 2. Running a query asynchronously. (asynchronous mode is not supported
	 *    by psqlODBC)
	 *
	 * 3. Busy running a function in another thread.
	 *    -> Send a query cancel request to the server
	 *
	 * 4. Not doing anything.
	 *    -> in ODBC version 2.0, same as SQLFreeStmt(SQL_CLOSE). In version
	 *       3.5, it has no effect.
	 *
	 * XXX: Checking for these conditions is racy. For example, we might
	 * see that the statement is waiting for SQLParamdata/SQLPutData, but
	 * before we acquire the lock on the statement, another thread has
	 * supplied the data and started executing. In that case, we'll block
	 * on the lock until the execution finishes.
	 */
	if (estmt->data_at_exec >= 0)
	{
		/* Waiting for more data from SQLParamData/SQLPutData. Cancel that. */
		/*
		 * Note, any previous data-at-exec buffers will be freed in the
		 * recycle
		 */
		/* if they call SQLExecDirect or SQLExecute again. */

		ENTER_STMT_CS(stmt);
		SC_clear_error(stmt);
		estmt->data_at_exec = -1;
		estmt->current_exec_param = -1;
		estmt->put_data = FALSE;
		cancelNeedDataState(estmt);
		LEAVE_STMT_CS(stmt);
		return ret;
	}
	else if (estmt->status == STMT_EXECUTING)
	{
		/*
		 * Busy executing in a different thread. Send a cancel request to
		 * the server.
		 */
		if (!CC_send_cancel_request(conn))
			return SQL_ERROR;
		else
			return SQL_SUCCESS;
	}
	else
	{
		/*
		 * The statement is not executing, and it's not waiting for params
		 * either. Looks like it's not doing anything.
		 */
		return SQL_SUCCESS;
	}
}


/*
 *	Returns the SQL string as modified by the driver.
 *	Currently, just copy the input string without modification
 *	observing buffer limits and truncation.
 */
RETCODE		SQL_API
PGAPI_NativeSql(HDBC hdbc,
				const SQLCHAR * szSqlStrIn,
				SQLINTEGER cbSqlStrIn,
				SQLCHAR * szSqlStr,
				SQLINTEGER cbSqlStrMax,
				SQLINTEGER * pcbSqlStr)
{
	CSTR func = "PGAPI_NativeSql";
	size_t		len = 0;
	char	   *ptr;
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	RETCODE		result;

	MYLOG(0, "entering...cbSqlStrIn=%d\n", cbSqlStrIn);

	ptr = (cbSqlStrIn == 0) ? "" : make_string(szSqlStrIn, cbSqlStrIn, NULL, 0);
	if (!ptr)
	{
		CC_set_error(conn, CONN_NO_MEMORY_ERROR, "No memory available to store native sql string", func);
		return SQL_ERROR;
	}

	result = SQL_SUCCESS;
	len = strlen(ptr);

	if (szSqlStr)
	{
		strncpy_null((char *) szSqlStr, ptr, cbSqlStrMax);

		if (len >= cbSqlStrMax)
		{
			result = SQL_SUCCESS_WITH_INFO;
			CC_set_error(conn, CONN_TRUNCATED, "The buffer was too small for the NativeSQL.", func);
		}
	}

	if (pcbSqlStr)
		*pcbSqlStr = (SQLINTEGER) len;

	if (cbSqlStrIn)
		free(ptr);

	return result;
}


/*
 *	Supplies parameter data at execution time.
 *	Used in conjuction with SQLPutData.
 */
RETCODE		SQL_API
PGAPI_ParamData(HSTMT hstmt,
				PTR * prgbValue)
{
	CSTR func = "PGAPI_ParamData";
	StatementClass *stmt = (StatementClass *) hstmt, *estmt;
	APDFields	*apdopts;
	IPDFields	*ipdopts;
	RETCODE		retval;
	int		i;
	Int2		num_p;
	ConnectionClass	*conn = NULL;

	MYLOG(0, "entering...\n");

	conn = SC_get_conn(stmt);

	estmt = stmt->execute_delegate ? stmt->execute_delegate : stmt;
	apdopts = SC_get_APDF(estmt);
	MYLOG(0, "\tdata_at_exec=%d, params_alloc=%d\n", estmt->data_at_exec, apdopts->allocated);

#define	return	DONT_CALL_RETURN_FROM_HERE???
	if (SC_AcceptedCancelRequest(stmt))
	{
		SC_set_error(stmt, STMT_OPERATION_CANCELLED, "Cancel the statement, sorry", func);
		retval = SQL_ERROR;
		goto cleanup;
	}
	if (estmt->data_at_exec < 0)
	{
		SC_set_error(stmt, STMT_SEQUENCE_ERROR, "No execution-time parameters for this statement", func);
		retval = SQL_ERROR;
		goto cleanup;
	}

	if (estmt->data_at_exec > apdopts->allocated)
	{
		SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Too many execution-time parameters were present", func);
		retval = SQL_ERROR;
		goto cleanup;
	}

	/* close the large object */
	if (estmt->lobj_fd >= 0)
	{
		odbc_lo_close(conn, estmt->lobj_fd);

		/* commit transaction if needed */
		if (!CC_cursor_count(conn) && CC_does_autocommit(conn))
		{
			if (!CC_commit(conn))
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Could not commit (in-line) a transaction", func);
				retval = SQL_ERROR;
				goto cleanup;
			}
		}
		estmt->lobj_fd = -1;
	}

	/* Done, now copy the params and then execute the statement */
	ipdopts = SC_get_IPDF(estmt);
MYLOG(1, "ipdopts=%p\n", ipdopts);
	if (estmt->data_at_exec == 0)
	{
		BOOL	exec_end;
		UWORD	flag = SC_is_with_hold(stmt) ? PODBC_WITH_HOLD : 0;

		retval = Exec_with_parameters_resolved(estmt, &exec_end);
		if (exec_end)
		{
			/**SC_reset_delegate(retval, stmt);**/
			retval = dequeueNeedDataCallback(retval, stmt);
			goto cleanup;
		}
		else
		{
			stmt->curr_param_result = 0;
		}

		if (retval = PGAPI_Execute(estmt, flag), SQL_NEED_DATA != retval)
		{
			goto cleanup;
		}
	}

	/*
	 * Set beginning param;  if first time SQLParamData is called , start
	 * at 0. Otherwise, start at the last parameter + 1.
	 */
	i = estmt->current_exec_param >= 0 ? estmt->current_exec_param + 1 : 0;

	num_p = estmt->num_params;
	if (num_p < 0)
		PGAPI_NumParams(estmt, &num_p);
MYLOG(1, "i=%d allocated=%d num_p=%d\n", i, apdopts->allocated, num_p);
	if (num_p > apdopts->allocated)
		num_p = apdopts->allocated;
	/* At least 1 data at execution parameter, so Fill in the token value */
	for (; i < num_p; i++)
	{
MYLOG(1, "i=%d", i);
		if (apdopts->parameters[i].data_at_exec)
		{
MYPRINTF(1, " at exec buffer=%p", apdopts->parameters[i].buffer);
			estmt->data_at_exec--;
			estmt->current_exec_param = i;
			estmt->put_data = FALSE;
			if (prgbValue)
			{
				/* returns token here */
				if (stmt->execute_delegate)
				{
					SQLULEN	offset = apdopts->param_offset_ptr ? *apdopts->param_offset_ptr : 0;
					SQLLEN	perrow = apdopts->param_bind_type > 0 ? apdopts->param_bind_type : apdopts->parameters[i].buflen;

MYPRINTF(1, " offset=" FORMAT_LEN " perrow=" FORMAT_LEN, offset, perrow);
					*prgbValue = apdopts->parameters[i].buffer + offset + estmt->exec_current_row * perrow;
				}
				else
					*prgbValue = apdopts->parameters[i].buffer;
			}
			break;
		}
MYPRINTF(1, "\n");
	}

	retval = SQL_NEED_DATA;
MYLOG(1, "return SQL_NEED_DATA\n");
cleanup:
#undef	return
	SC_setInsertedTable(stmt, retval);
	MYLOG(0, "leaving %d\n", retval);
	return retval;
}


/*
 *	Supplies parameter data at execution time.
 *	Used in conjunction with SQLParamData.
 */
RETCODE		SQL_API
PGAPI_PutData(HSTMT hstmt,
			  PTR rgbValue,
			  SQLLEN cbValue)
{
	CSTR func = "PGAPI_PutData";
	StatementClass *stmt = (StatementClass *) hstmt, *estmt;
	ConnectionClass *conn;
	RETCODE		retval = SQL_SUCCESS;
	APDFields	*apdopts;
	IPDFields	*ipdopts;
	PutDataInfo	*pdata;
	SQLLEN		old_pos;
	ParameterInfoClass *current_param;
	ParameterImplClass *current_iparam;
	PutDataClass	*current_pdata;
	char	   *putbuf, *allocbuf = NULL;
	Int2		ctype;
	SQLLEN		putlen;
	BOOL		lenset = FALSE, handling_lo = FALSE;

	MYLOG(0, "entering...\n");

#define	return	DONT_CALL_RETURN_FROM_HERE???
	if (SC_AcceptedCancelRequest(stmt))
	{
		SC_set_error(stmt, STMT_OPERATION_CANCELLED, "Cancel the statement, sorry.", func);
		retval = SQL_ERROR;
		goto cleanup;
	}

	estmt = stmt->execute_delegate ? stmt->execute_delegate : stmt;
	apdopts = SC_get_APDF(estmt);
	if (estmt->current_exec_param < 0)
	{
		SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Previous call was not SQLPutData or SQLParamData", func);
		retval = SQL_ERROR;
		goto cleanup;
	}

	current_param = &(apdopts->parameters[estmt->current_exec_param]);
	ipdopts = SC_get_IPDF(estmt);
	current_iparam = &(ipdopts->parameters[estmt->current_exec_param]);
	pdata = SC_get_PDTI(estmt);
	current_pdata = &(pdata->pdata[estmt->current_exec_param]);
	ctype = current_param->CType;

	conn = SC_get_conn(estmt);
	if (ctype == SQL_C_DEFAULT)
	{
		ctype = sqltype_to_default_ctype(conn, current_iparam->SQLType);
#ifdef	UNICODE_SUPPORT
		if (SQL_C_WCHAR == ctype &&
		    CC_default_is_c(conn))
			ctype = SQL_C_CHAR;
#endif
	}
	if (SQL_NTS == cbValue)
	{
#ifdef	UNICODE_SUPPORT
		if (SQL_C_WCHAR == ctype)
		{
			putlen = WCLEN * ucs2strlen((SQLWCHAR *) rgbValue);
			lenset = TRUE;
		}
		else
#endif /* UNICODE_SUPPORT */
		if (SQL_C_CHAR == ctype)
		{
			putlen = strlen(rgbValue);
			lenset = TRUE;
		}
	}
	if (!lenset)
	{
		if (cbValue < 0)
			putlen = cbValue;
		else
#ifdef	UNICODE_SUPPORT
		if (ctype == SQL_C_CHAR || ctype == SQL_C_BINARY || ctype == SQL_C_WCHAR)
#else
		if (ctype == SQL_C_CHAR || ctype == SQL_C_BINARY)
#endif /* UNICODE_SUPPORT */
			putlen = cbValue;
		else
			putlen = ctype_length(ctype);
	}
	putbuf = rgbValue;
	handling_lo = (PIC_dsp_pgtype(conn, *current_iparam) == conn->lobj_type);
	if (handling_lo && SQL_C_CHAR == ctype)
	{
		allocbuf = malloc(putlen / 2 + 1);
		if (allocbuf)
		{
			pg_hex2bin(rgbValue, allocbuf, putlen);
			putbuf = allocbuf;
			putlen /= 2;
		}
	}

	if (!estmt->put_data)
	{							/* first call */
		MYLOG(0, "(1) cbValue = " FORMAT_LEN "\n", cbValue);

		estmt->put_data = TRUE;

		current_pdata->EXEC_used = (SQLLEN *) malloc(sizeof(SQLLEN));
		if (!current_pdata->EXEC_used)
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Out of memory in PGAPI_PutData (1)", func);
			retval = SQL_ERROR;
			goto cleanup;
		}

		*current_pdata->EXEC_used = putlen;

		if (cbValue == SQL_NULL_DATA)
		{
			retval = SQL_SUCCESS;
			goto cleanup;
		}

		/* Handle Long Var Binary with Large Objects */
		/* if (current_iparam->SQLType == SQL_LONGVARBINARY) */
		if (handling_lo)
		{
			/* begin transaction if needed */
			if (!CC_is_in_trans(conn))
			{
				if (!CC_begin(conn))
				{
					SC_set_error(stmt, STMT_EXEC_ERROR, "Could not begin (in-line) a transaction", func);
					retval = SQL_ERROR;
					goto cleanup;
				}
			}

			/* store the oid */
			current_pdata->lobj_oid = odbc_lo_creat(conn, INV_READ | INV_WRITE);
			if (current_pdata->lobj_oid == 0)
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Couldnt create large object.", func);
				retval = SQL_ERROR;
				goto cleanup;
			}

			/*
			 * major hack -- to allow convert to see somethings there have
			 * to modify convert to handle this better
			 */
			/***current_param->EXEC_buffer = (char *) &current_param->lobj_oid;***/

			/* store the fd */
			estmt->lobj_fd = odbc_lo_open(conn, current_pdata->lobj_oid, INV_WRITE);
			if (estmt->lobj_fd < 0)
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Couldnt open large object for writing.", func);
				retval = SQL_ERROR;
				goto cleanup;
			}

			retval = odbc_lo_write(conn, estmt->lobj_fd, putbuf, (Int4) putlen);
			MYLOG(0, "lo_write: cbValue=" FORMAT_LEN ", wrote %d bytes\n", putlen, retval);
		}
		else
		{
			current_pdata->EXEC_buffer = malloc(putlen + 1);
			if (!current_pdata->EXEC_buffer)
			{
				SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Out of memory in PGAPI_PutData (2)", func);
				retval = SQL_ERROR;
				goto cleanup;
			}
			memcpy(current_pdata->EXEC_buffer, putbuf, putlen);
			current_pdata->EXEC_buffer[putlen] = '\0';
		}
	}
	else
	{
		/* calling SQLPutData more than once */
		MYLOG(0, "(>1) cbValue = " FORMAT_LEN "\n", cbValue);

		/* if (current_iparam->SQLType == SQL_LONGVARBINARY) */
		if (handling_lo)
		{
			/* the large object fd is in EXEC_buffer */
			retval = odbc_lo_write(conn, estmt->lobj_fd, putbuf, (Int4) putlen);
			MYLOG(0, "lo_write(2): cbValue = " FORMAT_LEN ", wrote %d bytes\n", putlen, retval);

			*current_pdata->EXEC_used += putlen;
		}
		else
		{
			old_pos = *current_pdata->EXEC_used;
			if (putlen > 0)
			{
				SQLLEN	used = *current_pdata->EXEC_used + putlen;
				SQLLEN allocsize;
				char *buffer;

				for (allocsize = (1 << 4); allocsize <= used; allocsize <<= 1) ;
				MYLOG(0, "        cbValue = " FORMAT_LEN ", old_pos = " FORMAT_LEN ", *used = " FORMAT_LEN "\n", putlen, old_pos, used);

				/* dont lose the old pointer in case out of memory */
				buffer = realloc(current_pdata->EXEC_buffer, allocsize);
				if (!buffer)
				{
					SC_set_error(stmt, STMT_NO_MEMORY_ERROR,"Out of memory in PGAPI_PutData (3)", func);
					retval = SQL_ERROR;
					goto cleanup;
				}

				memcpy(&buffer[old_pos], putbuf, putlen);
				buffer[used] = '\0';

				/* reassign buffer incase realloc moved it */
				*current_pdata->EXEC_used = used;
				current_pdata->EXEC_buffer = buffer;
			}
			else
			{
				SC_set_error(stmt, STMT_INTERNAL_ERROR, "bad cbValue", func);
				retval = SQL_ERROR;
				goto cleanup;
			}
		}
	}

	retval = SQL_SUCCESS;
cleanup:
#undef	return
	if (allocbuf)
		free(allocbuf);

	return retval;
}
