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
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#include "psqlodbc.h"

#include <stdio.h>
#include <string.h>

#include "environ.h"
#include "connection.h"
#include "statement.h"
#include "qresult.h"
#include "convert.h"
#include "bind.h"
#include "pgtypes.h"
#include "lobj.h"
#include "pgapifunc.h"

/*extern GLOBAL_VALUES globals;*/


/*		Perform a Prepare on the SQL statement */
RETCODE		SQL_API
PGAPI_Prepare(HSTMT hstmt,
			  const SQLCHAR FAR * szSqlStr,
			  SQLINTEGER cbSqlStr)
{
	CSTR func = "PGAPI_Prepare";
	StatementClass *self = (StatementClass *) hstmt;
	RETCODE	retval = SQL_SUCCESS;

	mylog("%s: entering...\n", func);

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(self); */
	if (!self)
	{
		SC_log_error(func, "", NULL);
		retval = SQL_INVALID_HANDLE;
		goto cleanup;
	}

	/*
	 * According to the ODBC specs it is valid to call SQLPrepare multiple
	 * times. In that case, the bound SQL statement is replaced by the new
	 * one
	 */

	SC_set_prepared(self, NOT_YET_PREPARED);
	switch (self->status)
	{
		case STMT_PREMATURE:
			mylog("**** PGAPI_Prepare: STMT_PREMATURE, recycle\n");
			SC_recycle_statement(self); /* recycle the statement, but do
										 * not remove parameter bindings */
			break;

		case STMT_FINISHED:
			mylog("**** PGAPI_Prepare: STMT_FINISHED, recycle\n");
			SC_recycle_statement(self); /* recycle the statement, but do
										 * not remove parameter bindings */
			break;

		case STMT_ALLOCATED:
			mylog("**** PGAPI_Prepare: STMT_ALLOCATED, copy\n");
			self->status = STMT_READY;
			break;

		case STMT_READY:
			mylog("**** PGAPI_Prepare: STMT_READY, change SQL\n");
			break;

		case STMT_EXECUTING:
			mylog("**** PGAPI_Prepare: STMT_EXECUTING, error!\n");

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
inolog("SQLPrepare return=%d\n", retval);
	if (self->internal)
		retval = DiscardStatementSvp(self, retval, FALSE);
	return retval;
}


/*		Performs the equivalent of SQLPrepare, followed by SQLExecute. */
RETCODE		SQL_API
PGAPI_ExecDirect(
				 HSTMT hstmt,
				 const SQLCHAR FAR * szSqlStr,
				 SQLINTEGER cbSqlStr,
				 UWORD flag)
{
	StatementClass *stmt = (StatementClass *) hstmt;
	RETCODE		result;
	CSTR func = "PGAPI_ExecDirect";
	const ConnectionClass	*conn = SC_get_conn(stmt);
	const ConnInfo	*ci = &(conn->connInfo);

	mylog("%s: entering...%x\n", func, flag);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	/*
	 * keep a copy of the un-parametrized statement, in case they try to
	 * execute this statement again
	 */
	stmt->statement = make_string(szSqlStr, cbSqlStr, NULL, 0);
inolog("a2\n");
	if (!stmt->statement)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "No memory available to store statement", func);
		return SQL_ERROR;
	}

	mylog("**** %s: hstmt=%x, statement='%s'\n", func, hstmt, stmt->statement);

	if (0 != (flag & PODBC_WITH_HOLD))
		SC_set_with_hold(stmt);

	/*
	 * If an SQLPrepare was performed prior to this, but was left in the
	 * premature state because an error occurred prior to SQLExecute then
	 * set the statement to finished so it can be recycled.
	 */
	if (stmt->status == STMT_PREMATURE)
		stmt->status = STMT_FINISHED;

	stmt->statement_type = statement_type(stmt->statement);

	/* Check if connection is onlyread (only selects are allowed) */
	if (CC_is_onlyread(conn) && STMT_UPDATE(stmt))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "Connection is readonly, only select statements are allowed.", func);
		return SQL_ERROR;
	}

	mylog("%s: calling PGAPI_Execute...\n", func);

	flag = SC_is_with_hold(stmt) ? PODBC_WITH_HOLD : 0;
	result = PGAPI_Execute(hstmt, flag);

	mylog("%s: returned %hd from PGAPI_Execute\n", func, result);
	return result;
}

void
decideHowToPrepare(StatementClass *stmt)
{
	ConnectionClass	*conn;
	ConnInfo	*ci;

	if (PREPARE_STATEMENT != stmt->prepare) /* not a prepare statement or already decided */
		return;
	if (stmt->inaccurate_result)
		return;
	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
	if (!ci->use_server_side_prepare ||
		PG_VERSION_LT(conn, 7.3))
	{
		/* Do prepare operations by the driver itself */
		stmt->prepare |= PREPARE_BY_THE_DRIVER;
		goto cleanup;
	}
	if (NOT_YET_PREPARED == stmt->prepared)
	{
		SQLSMALLINT	num_params;

		if (STMT_TYPE_DECLARE == stmt->statement_type &&
		    PG_VERSION_LT(conn, 8.0))
		{
			stmt->prepare |= PREPARE_BY_THE_DRIVER;
			goto cleanup;
		}
		if (stmt->multi_statement < 0)
			PGAPI_NumParams(stmt, &num_params);
		if (stmt->multi_statement > 0) /* server-side prepare mechanism coundn' handle multi statement */
			stmt->prepare |= PREPARE_BY_THE_DRIVER;
		else if (PROTOCOL_74(ci))
		{
			if (STMT_TYPE_SELECT == stmt->statement_type)
			{
				if (ci->drivers.use_declarefetch)
					/* stmt->prepare |= PREPARE_BY_THE_DRIVER; */
					stmt->prepare |= USING_UNNAMED_PARSE_REQUEST;
				else if (SQL_CURSOR_FORWARD_ONLY != stmt->options.cursor_type)
					stmt->prepare |= USING_UNNAMED_PARSE_REQUEST;
				else
					stmt->prepare |= USING_PARSE_REQUEST;
			}
			else
				stmt->prepare |= USING_PARSE_REQUEST;
		}
		else
		{
			if (STMT_TYPE_SELECT == stmt->statement_type &&
			    (SQL_CURSOR_FORWARD_ONLY != stmt->options.cursor_type ||
			    ci->drivers.use_declarefetch))
				stmt->prepare |= PREPARE_BY_THE_DRIVER;
			else
				stmt->prepare |= USING_PREPARE_COMMAND;
		}
	}

cleanup:
	if (PREPARE_BY_THE_DRIVER == stmt->prepare)
		stmt->discard_output_params = 1;
	return;
}

/*
 *	The execution after all parameters were resolved.
 */
static
RETCODE	Exec_with_parameters_resolved(StatementClass *stmt, BOOL *exec_end)
{
	CSTR func = "Exec_with_parameters_resolved";
	RETCODE		retval;
	int		end_row, cursor_type, scroll_concurrency;
	ConnectionClass	*conn;
	QResultClass	*res;
	APDFields	*apdopts;
	IPDFields	*ipdopts;
	BOOL		prepare_before_exec = FALSE;

	*exec_end = FALSE;
	conn = SC_get_conn(stmt);
	mylog("%s: copying statement params: trans_status=%d, len=%d, stmt='%s'\n", func, conn->transact_status, strlen(stmt->statement), stmt->statement);

	/* save the cursor's info before the execution */
	cursor_type = stmt->options.cursor_type;
	scroll_concurrency = stmt->options.scroll_concurrency;
	/* Prepare the statement if possible at backend side */
	if (!stmt->inaccurate_result)
	{
		decideHowToPrepare(stmt);
		switch (SC_get_prepare_method(stmt))
		{
			case USING_PREPARE_COMMAND:
			case USING_PARSE_REQUEST:
				prepare_before_exec = TRUE;
		}
	}
inolog("prepare_before_exec=%d srv=%d\n", prepare_before_exec, conn->connInfo.use_server_side_prepare);
	/* Create the statement with parameters substituted. */
	retval = copy_statement_with_parameters(stmt, prepare_before_exec);
	stmt->current_exec_param = -1;
	if (retval != SQL_SUCCESS)
	{
		stmt->exec_current_row = -1;
		*exec_end = TRUE;
		return retval; /* error msg is passed from the above */
	}

	mylog("   stmt_with_params = '%s'\n", stmt->stmt_with_params);

	/*
	 *	Dummy exection to get the column info.
	 */ 
	if (stmt->inaccurate_result && conn->connInfo.disallow_premature)
	{
		BOOL		in_trans = CC_is_in_trans(conn);
		BOOL		issued_begin = FALSE,
					begin_included = FALSE;
		QResultClass *curres;

		stmt->exec_current_row = -1;
		*exec_end = TRUE;
		if (!SC_is_pre_executable(stmt))
			return SQL_SUCCESS;
		if (strnicmp(stmt->stmt_with_params, "BEGIN;", 6) == 0)
			begin_included = TRUE;
		else if (!in_trans)
		{
			if (issued_begin = CC_begin(conn), !issued_begin)
			{
				SC_set_error(stmt, STMT_EXEC_ERROR,  "Handle prepare error", func);
				return SQL_ERROR;
			}
		}
		/* we are now in a transaction */
		res = CC_send_query(conn, stmt->stmt_with_params, NULL, 0, SC_get_ancestor(stmt));
		if (!QR_command_maybe_successful(res))
		{
#ifndef	_LEGACY_MODE_
			if (PG_VERSION_LT(conn, 8.0))
				CC_abort(conn);
#endif /* LEGACY_MODE_ */
			SC_set_error(stmt, STMT_EXEC_ERROR, "Handle prepare error", func);
			QR_Destructor(res);
			return SQL_ERROR;
		}
		SC_set_Result(stmt, res);
		for (curres = res; !curres->num_fields; curres = curres->next)
			;
		SC_set_Curres(stmt, curres);
		if (CC_is_in_autocommit(conn))
		{
			if (issued_begin)
				CC_commit(conn);
		}
		stmt->status = STMT_FINISHED;
		return SQL_SUCCESS;
	}
	/*
	 *	The real execution.
	 */
	retval = SC_execute(stmt);
	if (retval == SQL_ERROR)
	{
		stmt->exec_current_row = -1;
		*exec_end = TRUE;
		return retval;
	}
	res = SC_get_Result(stmt);
	/* special handling of result for keyset driven cursors */
	if (SQL_CURSOR_KEYSET_DRIVEN == stmt->options.cursor_type &&
	    SQL_CONCUR_READ_ONLY != stmt->options.scroll_concurrency)
	{
		QResultClass	*kres;

		if (kres = res->next, kres)
		{
			if (kres->fields)
				CI_Destructor(kres->fields);
			kres->fields = res->fields;
			res->fields = NULL;
			kres->num_fields = res->num_fields;
			res->next = NULL;
			SC_set_Result(stmt, kres);
			res = kres;
		}
	}
#ifdef	NOT_USED
	else if (SC_is_concat_prepare_exec(stmt))
	{
		if (res && QR_command_maybe_successful(res))
		{
			QResultClass	*kres;
		
			kres = res->next;
inolog("res->next=%x\n", kres);
			res->next = NULL;
			SC_set_Result(stmt, kres);
			res = kres;
			SC_set_prepared(stmt, PREPARED_PERMANENTLY);
		}
		else
		{
			retval = SQL_ERROR;
			if (stmt->execute_statement)
				free(stmt->execute_statement);
			stmt->execute_statement = NULL;
		}
	}
#endif /* NOT_USED */
#if (ODBCVER >= 0x0300)
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
#endif /* ODBCVER */
	if (end_row = stmt->exec_end_row, end_row < 0)
	{
		apdopts = SC_get_APDF(stmt);
		end_row = apdopts->paramset_size - 1;
	}
	if (stmt->inaccurate_result ||
	    stmt->exec_current_row >= end_row)
	{
		*exec_end = TRUE;
		stmt->exec_current_row = -1;
	}
	else
		stmt->exec_current_row++;
	if (res)
	{
#if (ODBCVER >= 0x0300)
		EnvironmentClass *env = (EnvironmentClass *) (conn->henv);
		const char *cmd = QR_get_command(res);

		if (retval == SQL_SUCCESS && cmd && EN_is_odbc3(env))
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
#endif /* ODBCVER */
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
	return retval;
}

int
StartRollbackState(StatementClass *stmt)
{
	CSTR	func = "StartRollbackState";
	int	ret;
	ConnectionClass	*conn;
	ConnInfo	*ci = NULL;
	
inolog("%s:%x->internal=%d\n", func, stmt, stmt->internal);
	conn = SC_get_conn(stmt);
	if (conn)
		ci = &conn->connInfo;
	ret = 0;
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

/*
 *	Must be in a transaction or the subsequent execution
 *	invokes a transaction.
 */
RETCODE
SetStatementSvp(StatementClass *stmt)
{
	CSTR	func = "SetStatementSvp";
	char	esavepoint[32], cmd[64];
	ConnectionClass	*conn = SC_get_conn(stmt);
	QResultClass *res;
	RETCODE	ret = SQL_SUCCESS_WITH_INFO;

	if (CC_is_in_error_trans(conn))
		return ret;

	switch (stmt->statement_type)
	{
		case STMT_TYPE_SPECIAL:
		case STMT_TYPE_TRANSACTION:
			return ret;
	}
	if (!SC_accessed_db(stmt))
	{
		if (stmt->internal)
		{
			if (PG_VERSION_GE(conn, 8.0))
				SC_start_rb_stmt(stmt);
			else
				SC_start_tc_stmt(stmt);
		}
		if (SC_is_rb_stmt(stmt) && CC_is_in_trans(conn))
		{
			sprintf(esavepoint, "_EXEC_SVP_%08x", stmt);
			snprintf(cmd, sizeof(cmd), "SAVEPOINT %s", esavepoint);
			res = CC_send_query(conn, cmd, NULL, 0, NULL);
			if (QR_command_maybe_successful(res))
			{
				SC_set_accessed_db(stmt);
				SC_start_rbpoint(stmt);
				ret = SQL_SUCCESS;
			}
			else
			{
				SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal SAVEPOINT failed", func);
				ret = SQL_ERROR;
			}
			QR_Destructor(res);
		}
		else
			SC_set_accessed_db(stmt);
	}
inolog("%s:%x->accessed=%d\n", func, stmt, SC_accessed_db(stmt));
	return ret;
}

RETCODE
DiscardStatementSvp(StatementClass *stmt, RETCODE ret, BOOL errorOnly)
{
	CSTR	func = "DiscardStatementSvp";
	char	esavepoint[32], cmd[64];
	ConnectionClass	*conn = SC_get_conn(stmt);
	QResultClass *res;
	BOOL	cmd_success, start_stmt = FALSE;	

inolog("%s:%x->accessed=%d is_in=%d is_rb=%d is_tc=%d\n", func, stmt, SC_accessed_db(stmt),
CC_is_in_trans(conn), SC_is_rb_stmt(stmt), SC_is_tc_stmt(stmt));
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
	if (!SC_accessed_db(stmt) || !CC_is_in_trans(conn))
		goto cleanup;
	if (!SC_is_rb_stmt(stmt) && !SC_is_tc_stmt(stmt))
		goto cleanup;
	sprintf(esavepoint, "_EXEC_SVP_%08x", stmt);
	if (SQL_ERROR == ret)
	{
		if (SC_started_rbpoint(stmt))
		{
			snprintf(cmd, sizeof(cmd), "ROLLBACK to %s", esavepoint);
			res = CC_send_query(conn, cmd, NULL, IGNORE_ABORT_ON_CONN, NULL);
			cmd_success = QR_command_maybe_successful(res);
			QR_Destructor(res);
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
inolog("ret=%d\n", ret);
	if (SQL_NEED_DATA != ret && SC_started_rbpoint(stmt))
	{
		snprintf(cmd, sizeof(cmd), "RELEASE %s", esavepoint);
		res = CC_send_query(conn, cmd, NULL, IGNORE_ABORT_ON_CONN, NULL);
		cmd_success = QR_command_maybe_successful(res);
		QR_Destructor(res);
		if (!cmd_success)
		{
			SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal RELEASE failed", func);
			ret = SQL_ERROR;
		}
	}
cleanup:
	if (start_stmt || SQL_ERROR == ret)
		SC_start_stmt(stmt);
	return ret;
}

static void
SetInsertTable(StatementClass *stmt)
{
	const char *cmd = stmt->statement, *ptr;
	ConnectionClass	*conn;
	size_t	len;

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
	while (isspace((UCHAR) *(++cmd)));
	if (!*cmd)
        	return;
	conn = SC_get_conn(stmt);
	NULL_THE_NAME(conn->schemaIns);
	NULL_THE_NAME(conn->tableIns);
	ptr = NULL;
	if (IDENTIFIER_QUOTE == *cmd)
	{
		if (ptr = strchr(cmd + 1, IDENTIFIER_QUOTE), NULL == ptr)
			return;
		if ('.' == ptr[1])
		{
			len = ptr - cmd - 1;
			STRN_TO_NAME(conn->schemaIns, cmd + 1, len);
			cmd = ptr + 2;
			ptr = NULL;
		}
	}
	else
	{
		if (ptr = strchr(cmd + 1, '.'), NULL != ptr)
		{
			len = ptr - cmd - 1;
			STRN_TO_NAME(conn->schemaIns, cmd, len);
			cmd = ptr + 1;
			ptr = NULL; 
		}
	}
	if (IDENTIFIER_QUOTE == *cmd && NULL == ptr)
	{
		if (ptr = strchr(cmd + 1, IDENTIFIER_QUOTE), NULL == ptr)
			return;
	}
	if (IDENTIFIER_QUOTE == *cmd)
	{
		len = ptr - cmd - 1;
		STRN_TO_NAME(conn->tableIns, cmd + 1, len);
	}
	else
	{
		ptr = cmd;
		while (*ptr && !isspace((UCHAR) *ptr)) ptr++;
		len = ptr - cmd;
		STRN_TO_NAME(conn->tableIns, cmd, len);
	}
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
	int			i, start_row, end_row;
	BOOL	exec_end, recycled = FALSE, recycle = TRUE;
	SQLSMALLINT	num_params;

	mylog("%s: entering...%x\n", func, flag);

	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		mylog("%s: NULL statement so return SQL_INVALID_HANDLE\n", func);
		return SQL_INVALID_HANDLE;
	}

	conn = SC_get_conn(stmt);
	apdopts = SC_get_APDF(stmt);
	/*
	 * If the statement is premature, it means we already executed it from
	 * an SQLPrepare/SQLDescribeCol type of scenario.  So just return
	 * success.
	 */
	if (stmt->prepare && stmt->status == STMT_PREMATURE)
	{
		if (stmt->inaccurate_result)
		{
			stmt->exec_current_row = -1;
			SC_recycle_statement(stmt);
		}
		else
		{
			stmt->status = STMT_FINISHED;
			if (NULL == SC_get_errormsg(stmt))
			{
				mylog("%s: premature statement but return SQL_SUCCESS\n", func);
				retval = SQL_SUCCESS;
				goto cleanup;
			}
			else
			{
				SC_set_error(stmt,STMT_INTERNAL_ERROR, "premature statement so return SQL_ERROR", func);
				retval = SQL_ERROR;
				goto cleanup;
			}
		}
	}

	mylog("%s: clear errors...\n", func);

	SC_clear_error(stmt);
	if (!stmt->statement)
	{
		SC_set_error(stmt, STMT_NO_STMTSTRING, "This handle does not have a SQL statement stored in it", func);
		mylog("%s: problem with handle\n", func);
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
	else if (PREPARED_PERMANENTLY == stmt->prepared)
	{
		QResultClass	*res;

		/*
		 * re-executing an prepared statement.
		 * Don't recycle the statement but
		 * discard the old result.
		 */
		recycle = FALSE;
		if (res = SC_get_Result(stmt), res)
        		QR_close_result(res, FALSE);
	}
	/*
	 * If SQLExecute is being called again, recycle the statement. Note
	 * this should have been done by the application in a call to
	 * SQLFreeStmt(SQL_CLOSE) or SQLCancel.
	 */
	else if (stmt->status == STMT_FINISHED)
	{
		mylog("%s: recycling statement (should have been done by app)...\n", func);
/******** Is this really NEEDED ? ******/
		SC_recycle_statement(stmt);
		recycled = TRUE;
	}
	/* Check if the statement is in the correct state */
	else if ((stmt->prepare && stmt->status != STMT_READY) ||
		(stmt->status != STMT_ALLOCATED && stmt->status != STMT_READY))
	{
		SC_set_error(stmt, STMT_STATUS_ERROR, "The handle does not point to a statement that is ready to be executed", func);
		mylog("%s: problem with statement\n", func);
		retval = SQL_ERROR;
		goto cleanup;
	}

	if (start_row = stmt->exec_start_row, start_row < 0)
		start_row = 0; 
	if (end_row = stmt->exec_end_row, end_row < 0)
		end_row = apdopts->paramset_size - 1; 
	if (stmt->exec_current_row < 0)
		stmt->exec_current_row = start_row;
	ipdopts = SC_get_IPDF(stmt);
	num_params = stmt->num_params;
	if (num_params < 0)
		PGAPI_NumParams(stmt, &num_params);
	if (stmt->exec_current_row == start_row)
	{
		ConnInfo *ci = &(conn->connInfo);
		if (NOT_YET_PREPARED == stmt->prepared &&
		    PROTOCOL_74(ci) &&
		    ci->use_server_side_prepare &&
		    ci->bytea_as_longvarbinary) /* both lo and bytea are LO */
		{
			SQLSMALLINT	num_params = stmt->num_params;
			if (num_params < 0)
				PGAPI_NumParams(stmt, &num_params);
			if (num_params > 0)
			{
				int	param_number = -1;
				ParameterImplClass *ipara;
				BOOL	bCallPrepare = FALSE;

				while (1)
				{
					SC_param_next(stmt, &param_number, NULL, &ipara);
					if (!ipara)
						break;
					if (SQL_LONGVARBINARY == ipara->SQLType)
					{
						bCallPrepare = TRUE;
						break;
					}
				}
				if (bCallPrepare)
				{
					if (retval = prepareParameters(stmt), SQL_ERROR == retval)
						goto cleanup;
				}
			}
		}

		if (ipdopts->param_processed_ptr)
			*ipdopts->param_processed_ptr = 0;
#if (ODBCVER >= 0x0300)
		/*
	 	 *	Initialize the param_status_ptr 
	 	 */
		if (ipdopts->param_status_ptr)
		{
			for (i = 0; i <= end_row; i++)
				ipdopts->param_status_ptr[i] = SQL_PARAM_UNUSED;
		}
#endif /* ODBCVER */
		if (recycle && !recycled)
			SC_recycle_statement(stmt);
	}

	/* StartRollbackState must be called after SC_recycle_statement */
	/* StartRollbackState(stmt); */
next_param_row:
#if (ODBCVER >= 0x0300)
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
#endif /* ODBCVER */
	/*
	 * Check if statement has any data-at-execute parameters when it is
	 * not in SC_pre_execute.
	 */
	if (!stmt->pre_executing)
	{
		/*
		 * The bound parameters could have possibly changed since the last
		 * execute of this statement?  Therefore check for params and
		 * re-copy.
		 */
		UInt4	offset = apdopts->param_offset_ptr ? *apdopts->param_offset_ptr : 0;
		Int4	bind_size = apdopts->param_bind_type;
		Int4	current_row = stmt->exec_current_row < 0 ? 0 : stmt->exec_current_row;
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
					pcVal = (SQLLEN *)((char *)pcVal + offset + bind_size * current_row);
				else
					pcVal = (SQLLEN *)((char *)pcVal + offset + sizeof(SDWORD) * current_row);
				if (*pcVal == SQL_DATA_AT_EXEC || *pcVal <= SQL_LEN_DATA_AT_EXEC_OFFSET)
					apdopts->parameters[i].data_at_exec = TRUE;
			}
			/* Check for data at execution parameters */
			if (apdopts->parameters[i].data_at_exec)
			{
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
		goto next_param_row;
cleanup:
mylog("retval=%d\n", retval);
	if (STMT_TYPE_INSERT == stmt->statement_type &&
	    CC_fake_mss(conn) &&
	    (SQL_SUCCESS == retval ||
	     SQL_SUCCESS_WITH_INFO == retval)
	   )
	{
		SetInsertTable(stmt);
	}
#undef	return
	if (stmt->internal)
		retval = DiscardStatementSvp(stmt, retval, FALSE);
	return retval;
}


RETCODE		SQL_API
PGAPI_Transact(
			   HENV henv,
			   HDBC hdbc,
			   SQLUSMALLINT fType)
{
	CSTR func = "PGAPI_Transact";
	extern ConnectionClass *conns[];
	ConnectionClass *conn;
	QResultClass *res;
	char		ok,
			   *stmt_string;
	int			lf;

	mylog("entering %s: hdbc=%x, henv=%x\n", func, hdbc, henv);

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
		for (lf = 0; lf < MAX_CONNECTIONS; lf++)
		{
			conn = conns[lf];

			if (conn && conn->henv == henv)
				if (PGAPI_Transact(henv, (HDBC) conn, fType) != SQL_SUCCESS)
					return SQL_ERROR;
		}
		return SQL_SUCCESS;
	}

	conn = (ConnectionClass *) hdbc;

	if (fType == SQL_COMMIT)
		stmt_string = "COMMIT";
	else if (fType == SQL_ROLLBACK)
		stmt_string = "ROLLBACK";
	else
	{
		CC_set_error(conn, CONN_INVALID_ARGUMENT_NO, "PGAPI_Transact can only be called with SQL_COMMIT or SQL_ROLLBACK as parameter", func);
		return SQL_ERROR;
	}

	/* If manual commit and in transaction, then proceed. */
	if (!CC_is_in_autocommit(conn) && CC_is_in_trans(conn))
	{
		mylog("PGAPI_Transact: sending on conn %d '%s'\n", conn, stmt_string);

		res = CC_send_query(conn, stmt_string, NULL, 0, NULL);
		ok = QR_command_maybe_successful(res);
		QR_Destructor(res);
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
PGAPI_Cancel(
			 HSTMT hstmt)		/* Statement to cancel. */
{
	CSTR func = "PGAPI_Cancel";
	StatementClass *stmt = (StatementClass *) hstmt, *estmt;
	ConnectionClass *conn;
	RETCODE		ret = SQL_SUCCESS;
	BOOL	entered_cs = FALSE;
	ConnInfo   *ci;

	mylog("%s: entering...\n", func);

	/* Check if this can handle canceling in the middle of a SQLPutData? */
	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}
	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */

	if (stmt->execute_delegate)
		estmt = stmt->execute_delegate;
	else
		estmt = stmt;
	/*
	 * Not in the middle of SQLParamData/SQLPutData so cancel like a
	 * close.
	 */
	if (estmt->data_at_exec < 0)
	{
		/*
		 * Tell the Backend that we're cancelling this request
		 */
		if (estmt->status == STMT_EXECUTING)
		{
			if (!CC_send_cancel_request(conn))
			{
				ret = SQL_ERROR;
			}
			goto cleanup;
		}
		/*
		 * MAJOR HACK for Windows to reset the driver manager's cursor
		 * state: Because of what seems like a bug in the Odbc driver
		 * manager, SQLCancel does not act like a SQLFreeStmt(CLOSE), as
		 * many applications depend on this behavior.  So, this brute
		 * force method calls the driver manager's function on behalf of
		 * the application.
		 */

#if (ODBCVER < 0x0350)
#ifdef WIN32
		if (ci->drivers.cancel_as_freestmt)
		{
			HMODULE		hmodule;
			FARPROC		addr;

			hmodule = GetModuleHandle("ODBC32");
			addr = GetProcAddress(hmodule, "SQLFreeStmt");
			ret = addr((char *) (stmt->phstmt) - 96, SQL_CLOSE);
		}
		else
#endif /* WIN32 */
		{
			ENTER_STMT_CS(stmt);
			entered_cs = TRUE;
			SC_clear_error(hstmt);
			ret = PGAPI_FreeStmt(hstmt, SQL_CLOSE);
		}

		mylog("PGAPI_Cancel:  PGAPI_FreeStmt returned %d\n", ret);
#endif /* ODBCVER */
		goto cleanup;
	}

	/* In the middle of SQLParamData/SQLPutData, so cancel that. */
	/*
	 * Note, any previous data-at-exec buffers will be freed in the
	 * recycle
	 */
	/* if they call SQLExecDirect or SQLExecute again. */

	ENTER_STMT_CS(stmt);
	entered_cs = TRUE;
	SC_clear_error(stmt);
	estmt->data_at_exec = -1;
	estmt->current_exec_param = -1;
	estmt->put_data = FALSE;
	cancelNeedDataState(estmt);

cleanup:
#undef	return
	if (entered_cs)
	{
		if (stmt->internal)
			ret = DiscardStatementSvp(stmt, ret, FALSE);
		LEAVE_STMT_CS(stmt);
	}
	return ret;
}


/*
 *	Returns the SQL string as modified by the driver.
 *	Currently, just copy the input string without modification
 *	observing buffer limits and truncation.
 */
RETCODE		SQL_API
PGAPI_NativeSql(
				HDBC hdbc,
				const SQLCHAR FAR * szSqlStrIn,
				SQLINTEGER cbSqlStrIn,
				SQLCHAR FAR * szSqlStr,
				SQLINTEGER cbSqlStrMax,
				SQLINTEGER FAR * pcbSqlStr)
{
	CSTR func = "PGAPI_NativeSql";
	int			len = 0;
	char	   *ptr;
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	RETCODE		result;

	mylog("%s: entering...cbSqlStrIn=%d\n", func, cbSqlStrIn);

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
		strncpy_null(szSqlStr, ptr, cbSqlStrMax);

		if (len >= cbSqlStrMax)
		{
			result = SQL_SUCCESS_WITH_INFO;
			CC_set_error(conn, CONN_TRUNCATED, "The buffer was too small for the NativeSQL.", func);
		}
	}

	if (pcbSqlStr)
		*pcbSqlStr = len;

	if (cbSqlStrIn)
		free(ptr);

	return result;
}


/*
 *	Supplies parameter data at execution time.
 *	Used in conjuction with SQLPutData.
 */
RETCODE		SQL_API
PGAPI_ParamData(
				HSTMT hstmt,
				PTR FAR * prgbValue)
{
	CSTR func = "PGAPI_ParamData";
	StatementClass *stmt = (StatementClass *) hstmt, *estmt;
	APDFields	*apdopts;
	IPDFields	*ipdopts;
	RETCODE		retval;
	int		i;
	Int2		num_p;
	ConnInfo   *ci;

	mylog("%s: entering...\n", func);

	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		retval = SQL_INVALID_HANDLE;
		goto cleanup;
	}
	ci = &(SC_get_conn(stmt)->connInfo);

	estmt = stmt->execute_delegate ? stmt->execute_delegate : stmt;
	apdopts = SC_get_APDF(estmt);
	mylog("%s: data_at_exec=%d, params_alloc=%d\n", func, estmt->data_at_exec, apdopts->allocated);

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
		ConnectionClass	*conn = SC_get_conn(estmt);

		odbc_lo_close(conn, estmt->lobj_fd);

		/* commit transaction if needed */
		if (!CC_cursor_count(conn) && CC_is_in_autocommit(conn))
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
inolog("ipdopts=%x\n", ipdopts);
	if (estmt->data_at_exec == 0)
	{
		BOOL	exec_end;

		retval = Exec_with_parameters_resolved(estmt, &exec_end);
		if (exec_end)
		{
			/**SC_reset_delegate(retval, stmt);**/
			retval = dequeueNeedDataCallback(retval, stmt);
			goto cleanup;
		}
		if (retval = PGAPI_Execute(estmt, 0), SQL_NEED_DATA != retval)
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
inolog("i=%d allocated=%d num_p=%d\n", i, apdopts->allocated, num_p);
	if (num_p > apdopts->allocated)
		num_p = apdopts->allocated;
	/* At least 1 data at execution parameter, so Fill in the token value */
	for (; i < num_p; i++)
	{
inolog("i=%d", i);
		if (apdopts->parameters[i].data_at_exec)
		{
inolog(" at exec buffer=%x", apdopts->parameters[i].buffer);
			estmt->data_at_exec--;
			estmt->current_exec_param = i;
			estmt->put_data = FALSE;
			if (prgbValue)
			{
				/* returns token here */
				if (stmt->execute_delegate)
				{
					UInt4	offset = apdopts->param_offset_ptr ? *apdopts->param_offset_ptr : 0;
					UInt4	perrow = apdopts->param_bind_type > 0 ? apdopts->param_bind_type : apdopts->parameters[i].buflen; 

inolog(" offset=%d perrow=%d", offset, perrow);
					*prgbValue = apdopts->parameters[i].buffer + offset + estmt->exec_current_row * perrow;
				}
				else
					*prgbValue = apdopts->parameters[i].buffer;
			}
			break;
		}
inolog("\n");
	}

	retval = SQL_NEED_DATA;
inolog("return SQL_NEED_DATA\n");
cleanup:
#undef	return
	if (stmt->internal)
		retval = DiscardStatementSvp(stmt, retval, FALSE);
	mylog("%s: returning %d\n", func, retval);
	return retval;
}


/*
 *	Supplies parameter data at execution time.
 *	Used in conjunction with SQLParamData.
 */
RETCODE		SQL_API
PGAPI_PutData(
			  HSTMT hstmt,
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
	int			old_pos;
	ParameterInfoClass *current_param;
	ParameterImplClass *current_iparam;
	PutDataClass	*current_pdata;
	char	   *buffer, *putbuf, *allocbuf = NULL;
	Int2		ctype;
	SQLLEN		putlen;
	BOOL		lenset = FALSE;

	mylog("%s: entering...\n", func);

#define	return	DONT_CALL_RETURN_FROM_HERE???
	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		retval = SQL_INVALID_HANDLE;
		goto cleanup;
	}
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
		if (SQL_C_WCHAR == ctype &&
		    (CC_is_in_ansi_app(conn)
			|| conn->ms_jet	/* not only but for any other ? */
		   ))
			ctype = SQL_C_CHAR;
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
	if (current_iparam->PGType == conn->lobj_type && SQL_C_CHAR == ctype)
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
		mylog("PGAPI_PutData: (1) cbValue = %d\n", cbValue);

		estmt->put_data = TRUE;

		current_pdata->EXEC_used = (SDWORD *) malloc(sizeof(SDWORD));
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
		if (current_iparam->PGType == conn->lobj_type)
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

			retval = odbc_lo_write(conn, estmt->lobj_fd, putbuf, putlen);
			mylog("lo_write: cbValue=%d, wrote %d bytes\n", putlen, retval);
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
		mylog("PGAPI_PutData: (>1) cbValue = %d\n", cbValue);

		/* if (current_iparam->SQLType == SQL_LONGVARBINARY) */
		if (current_iparam->PGType == conn->lobj_type)
		{
			/* the large object fd is in EXEC_buffer */
			retval = odbc_lo_write(conn, estmt->lobj_fd, putbuf, putlen);
			mylog("lo_write(2): cbValue = %d, wrote %d bytes\n", putlen, retval);

			*current_pdata->EXEC_used += putlen;
		}
		else
		{
			buffer = current_pdata->EXEC_buffer;
			old_pos = *current_pdata->EXEC_used;
			if (putlen > 0)
			{
				SQLLEN	used = *current_pdata->EXEC_used + putlen, allocsize;
				for (allocsize = (1 << 4); allocsize <= used; allocsize <<= 1) ;
				mylog("        cbValue = %d, old_pos = %d, *used = %d\n", putlen, old_pos, used);

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
	if (stmt->internal)
		retval = DiscardStatementSvp(stmt, retval, TRUE);

	return retval;
}
