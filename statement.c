/*-------
 * Module:			statement.c
 *
 * Description:		This module contains functions related to creating
 *					and manipulating a statement.
 *
 * Classes:			StatementClass (Functions prefix: "SC_")
 *
 * API functions:	SQLAllocStmt, SQLFreeStmt
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#ifdef	WIN_MULTITHREAD_SUPPORT
#ifndef	_WIN32_WINNT
#define	_WIN32_WINNT	0x0400
#endif /* _WIN32_WINNT */
#endif /* WIN_MULTITHREAD_SUPPORT */

#include "statement.h"

#include "bind.h"
#include "connection.h"
#include "multibyte.h"
#include "qresult.h"
#include "convert.h"
#include "environ.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pgapifunc.h"


#define PRN_NULLCHECK

/*	Map sql commands to statement types */
static struct
{
	int			type;
	char	   *s;
}	Statement_Type[] =

{
	{
		STMT_TYPE_SELECT, "SELECT"
	}
	,{
		STMT_TYPE_INSERT, "INSERT"
	}
	,{
		STMT_TYPE_UPDATE, "UPDATE"
	}
	,{
		STMT_TYPE_DELETE, "DELETE"
	}
	,{
		STMT_TYPE_PROCCALL, "{"
	}
	,{
		STMT_TYPE_SET, "SET"
	}
	,{
		STMT_TYPE_RESET, "RESET"
	}
	,{
		STMT_TYPE_CREATE, "CREATE"
	}
	,{
		STMT_TYPE_DECLARE, "DECLARE"
	}
	,{
		STMT_TYPE_FETCH, "FETCH"
	}
	,{
		STMT_TYPE_MOVE, "MOVE"
	}
	,{
		STMT_TYPE_CLOSE, "CLOSE"
	}
	,{
		STMT_TYPE_PREPARE, "PREPARE"
	}
	,{
		STMT_TYPE_EXECUTE, "EXECUTE"
	}
	,{
		STMT_TYPE_DEALLOCATE, "DEALLOCATE"
	}
	,{
		STMT_TYPE_DROP, "DROP"
	}
	,{
		STMT_TYPE_START, "BEGIN"
	}
	,{
		STMT_TYPE_START, "START"
	}
	,{
		STMT_TYPE_TRANSACTION, "SAVEPOINT"
	}
	,{
		STMT_TYPE_TRANSACTION, "RELEASE"
	}
	,{
		STMT_TYPE_TRANSACTION, "COMMIT"
	}
	,{
		STMT_TYPE_TRANSACTION, "END"
	}
	,{
		STMT_TYPE_TRANSACTION, "ROLLBACK"
	}
	,{
		STMT_TYPE_TRANSACTION, "ABORT"
	}
	,{
		STMT_TYPE_LOCK, "LOCK"
	}
	,{
		STMT_TYPE_ALTER, "ALTER"
	}
	,{
		STMT_TYPE_GRANT, "GRANT"
	}
	,{
		STMT_TYPE_REVOKE, "REVOKE"
	}
	,{
		STMT_TYPE_COPY, "COPY"
	}
	,{
		STMT_TYPE_ANALYZE, "ANALYZE"
	}
	,{
		STMT_TYPE_NOTIFY, "NOTIFY"
	}
	,{
		STMT_TYPE_EXPLAIN, "EXPLAIN"
	}
	,{
		STMT_TYPE_SPECIAL, "VACUUM"
	}
	,{
		STMT_TYPE_SPECIAL, "REINDEX"
	}
	,{
		STMT_TYPE_SPECIAL, "CLUSTER"
	}
	,{
		STMT_TYPE_SPECIAL, "CHECKPOINT"
	}
	,{
		STMT_TYPE_WITH, "WITH"
	}
	,{
		0, NULL
	}
};


RETCODE		SQL_API
PGAPI_AllocStmt(HDBC hdbc,
				HSTMT FAR * phstmt, UDWORD flag)
{
	CSTR func = "PGAPI_AllocStmt";
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	StatementClass *stmt;
	ARDFields	*ardopts;
	BindInfoClass	*bookmark;

	mylog("%s: entering...\n", func);

	if (!conn)
	{
		CC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	stmt = SC_Constructor(conn);

	mylog("**** PGAPI_AllocStmt: hdbc = %p, stmt = %p\n", hdbc, stmt);

	if (!stmt)
	{
		CC_set_error(conn, CONN_STMT_ALLOC_ERROR, "No more memory to allocate a further SQL-statement", func);
		*phstmt = SQL_NULL_HSTMT;
		return SQL_ERROR;
	}

	if (!CC_add_statement(conn, stmt))
	{
		CC_set_error(conn, CONN_STMT_ALLOC_ERROR, "Maximum number of statements exceeded.", func);
		SC_Destructor(stmt);
		*phstmt = SQL_NULL_HSTMT;
		return SQL_ERROR;
	}

	*phstmt = (HSTMT) stmt;

	stmt->iflag = flag;
	/* Copy default statement options based from Connection options */
	if (0 != (PODBC_INHERIT_CONNECT_OPTIONS & flag))
	{
		stmt->options = stmt->options_orig = conn->stmtOptions;
		stmt->ardi.ardopts = conn->ardOptions;
	}
	else
	{
		InitializeStatementOptions(&stmt->options_orig);
		stmt->options = stmt->options_orig;
		InitializeARDFields(&stmt->ardi.ardopts);
	}
	ardopts = SC_get_ARDF(stmt);
	bookmark = ARD_AllocBookmark(ardopts);

	stmt->stmt_size_limit = CC_get_max_query_len(conn);
	/* Save the handle for later */
	stmt->phstmt = phstmt;

	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_FreeStmt(HSTMT hstmt,
			   SQLUSMALLINT fOption)
{
	CSTR func = "PGAPI_FreeStmt";
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("%s: entering...hstmt=%p, fOption=%hi\n", func, hstmt, fOption);

	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}
	SC_clear_error(stmt);

	if (fOption == SQL_DROP)
	{
		ConnectionClass *conn = stmt->hdbc;

		/* Remove the statement from the connection's statement list */
		if (conn)
		{
			QResultClass	*res;

			if (STMT_EXECUTING == stmt->status)
			{
				SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Statement is currently executing a transaction.", func);
				return SQL_ERROR; /* stmt may be executing a transaction */
			}
			/*
			 *	Before dropping the statement, sync and discard
			 *	the response from the server for the pending
			 *	extended query.
			 */
			if (NULL != conn->sock && stmt == conn->stmt_in_extquery)
				QR_Destructor(SendSyncAndReceive(stmt, NULL, "finish the pending query"));
			conn->stmt_in_extquery = NULL; /* for safety */
			/*
			 * Free any cursors and discard any result info.
			 * Don't detach the statement from the connection
			 * before freeing the associated cursors. Otherwise
			 * CC_cursor_count() would get wrong results.
			 */
			res = SC_get_Result(stmt);
			QR_Destructor(res);
			SC_init_Result(stmt);
			if (!CC_remove_statement(conn, stmt))
			{
				SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Statement is currently executing a transaction.", func);
				return SQL_ERROR;		/* stmt may be executing a
										 * transaction */
			}
		}

		if (stmt->execute_delegate)
		{
			PGAPI_FreeStmt(stmt->execute_delegate, SQL_DROP);
			stmt->execute_delegate = NULL;
		}
		if (stmt->execute_parent)
			stmt->execute_parent->execute_delegate = NULL;
		/* Destroy the statement and free any results, cursors, etc. */
		SC_Destructor(stmt);
	}
	else if (fOption == SQL_UNBIND)
		SC_unbind_cols(stmt);
	else if (fOption == SQL_CLOSE)
	{
		/*
		 * this should discard all the results, but leave the statement
		 * itself in place (it can be executed again)
		 */
		stmt->transition_status = STMT_TRANSITION_ALLOCATED;
		if (stmt->execute_delegate)
		{
			PGAPI_FreeStmt(stmt->execute_delegate, SQL_DROP);
			stmt->execute_delegate = NULL;
		}
		if (!SC_recycle_statement(stmt))
		{
			return SQL_ERROR;
		}
	}
	else if (fOption == SQL_RESET_PARAMS)
		SC_free_params(stmt, STMT_FREE_PARAMS_ALL);
	else
	{
		SC_set_error(stmt, STMT_OPTION_OUT_OF_RANGE_ERROR, "Invalid option passed to PGAPI_FreeStmt.", func);
		return SQL_ERROR;
	}

	return SQL_SUCCESS;
}


/*
 * StatementClass implementation
 */
void
InitializeStatementOptions(StatementOptions *opt)
{
	memset(opt, 0, sizeof(StatementOptions));
	opt->maxRows = 0;		/* driver returns all rows */
	opt->maxLength = 0;		/* driver returns all data for char/binary */
	opt->keyset_size = 0;		/* fully keyset driven is the default */
	opt->scroll_concurrency = SQL_CONCUR_READ_ONLY;
	opt->cursor_type = SQL_CURSOR_FORWARD_ONLY;
	opt->retrieve_data = SQL_RD_ON;
	opt->use_bookmarks = SQL_UB_OFF;
#if (ODBCVER >= 0x0300)
	opt->metadata_id = SQL_FALSE;
#endif /* ODBCVER */
}

static void SC_clear_parse_status(StatementClass *self, ConnectionClass *conn)
{
	self->parse_status = STMT_PARSE_NONE;
	if (PG_VERSION_LT(conn, 7.2))
	{
		SC_set_checked_hasoids(self, TRUE);
		self->num_key_fields = PG_NUM_NORMAL_KEYS;
	}
}

static void SC_init_discard_output_params(StatementClass *self)
{
	ConnectionClass	*conn = SC_get_conn(self);

	if (!conn)	return;
	self->discard_output_params = 0;
	if (!conn->connInfo.use_server_side_prepare)
		self->discard_output_params = 1;
}

static void SC_init_parse_method(StatementClass *self)
{
	ConnectionClass	*conn = SC_get_conn(self);

	self->parse_method = 0;
	if (!conn)	return;
	if (0 == (PODBC_EXTERNAL_STATEMENT & self->iflag))	return;
	if (self->catalog_result) return;
	if (conn->connInfo.drivers.parse)
		SC_set_parse_forced(self);
	if (self->multi_statement <= 0 && conn->connInfo.disallow_premature)
		SC_set_parse_tricky(self);
}

StatementClass *
SC_Constructor(ConnectionClass *conn)
{
	StatementClass *rv;

	rv = (StatementClass *) malloc(sizeof(StatementClass));
	if (rv)
	{
		rv->hdbc = conn;
		rv->phstmt = NULL;
		rv->result = NULL;
		rv->curres = NULL;
		rv->catalog_result = FALSE;
		rv->prepare = NON_PREPARE_STATEMENT;
		rv->prepared = NOT_YET_PREPARED;
		rv->status = STMT_ALLOCATED;
		rv->internal = FALSE;
		rv->iflag = 0;
		rv->plan_name = NULL;
		rv->transition_status = STMT_TRANSITION_UNALLOCATED;
		rv->multi_statement = -1; /* unknown */
		rv->num_params = -1; /* unknown */

		rv->__error_message = NULL;
		rv->__error_number = 0;
		rv->pgerror = NULL;

		rv->statement = NULL;
		rv->stmt_with_params = NULL;
		rv->load_statement = NULL;
		rv->execute_statement = NULL;
		rv->stmt_size_limit = -1;
		rv->statement_type = STMT_TYPE_UNKNOWN;

		rv->currTuple = -1;
		SC_set_rowset_start(rv, -1, FALSE);
		rv->current_col = -1;
		rv->bind_row = 0;
		rv->from_pos = rv->where_pos = -1;
		rv->last_fetch_count = rv->last_fetch_count_include_ommitted = 0;
		rv->save_rowset_size = -1;

		rv->data_at_exec = -1;
		rv->current_exec_param = -1;
		rv->exec_start_row = -1;
		rv->exec_end_row = -1;
		rv->exec_current_row = -1;
		rv->put_data = FALSE;
		rv->ref_CC_error = FALSE;
		rv->lock_CC_for_rb = 0;
		rv->join_info = 0;
		rv->curr_param_result = 0;
		SC_init_parse_method(rv);

		rv->lobj_fd = -1;
		INIT_NAME(rv->cursor_name);

		/* Parse Stuff */
		rv->ti = NULL;
		rv->ntab = 0;
		rv->num_key_fields = -1; /* unknown */
		SC_clear_parse_status(rv, conn);
		rv->proc_return = -1;
		SC_init_discard_output_params(rv);
		rv->cancel_info = 0;

		/* Clear Statement Options -- defaults will be set in AllocStmt */
		memset(&rv->options, 0, sizeof(StatementOptions));
		InitializeEmbeddedDescriptor((DescriptorClass *)&(rv->ardi),
				rv, SQL_ATTR_APP_ROW_DESC);
		InitializeEmbeddedDescriptor((DescriptorClass *)&(rv->apdi),
				rv, SQL_ATTR_APP_PARAM_DESC);
		InitializeEmbeddedDescriptor((DescriptorClass *)&(rv->irdi),
				rv, SQL_ATTR_IMP_ROW_DESC);
		InitializeEmbeddedDescriptor((DescriptorClass *)&(rv->ipdi),
				rv, SQL_ATTR_IMP_PARAM_DESC);

		rv->pre_executing = FALSE;
		rv->inaccurate_result = FALSE;
		rv->miscinfo = 0;
		rv->rbonerr = 0;
		SC_reset_updatable(rv);
		rv->diag_row_count = 0;
		rv->stmt_time = 0;
		rv->execute_delegate = NULL;
		rv->execute_parent = NULL;
		rv->allocated_callbacks = 0;
		rv->num_callbacks = 0;
		rv->callbacks = NULL;
		GetDataInfoInitialize(SC_get_GDTI(rv));
		PutDataInfoInitialize(SC_get_PDTI(rv));
		INIT_STMT_CS(rv);
	}
	return rv;
}

char
SC_Destructor(StatementClass *self)
{
	CSTR func	= "SC_Destrcutor";
	QResultClass	*res = SC_get_Result(self);

	if (!self)	return FALSE;
	mylog("SC_Destructor: self=%p, self->result=%p, self->hdbc=%p\n", self, res, self->hdbc);
	SC_clear_error(self);
	if (STMT_EXECUTING == self->status)
	{
		SC_set_error(self, STMT_SEQUENCE_ERROR, "Statement is currently executing a transaction.", func);
		return FALSE;
	}

	if (res)
	{
		if (!self->hdbc)
			res->conn = NULL;	/* prevent any dbase activity */

		QR_Destructor(res);
	}

	SC_initialize_stmts(self, TRUE);

        /* Free the parsed table information */
	SC_initialize_cols_info(self, FALSE, TRUE);

	NULL_THE_NAME(self->cursor_name);
	/* Free the parsed field information */
	DC_Destructor((DescriptorClass *) SC_get_ARDi(self));
	DC_Destructor((DescriptorClass *) SC_get_APDi(self));
	DC_Destructor((DescriptorClass *) SC_get_IRDi(self));
	DC_Destructor((DescriptorClass *) SC_get_IPDi(self));
	GDATA_unbind_cols(SC_get_GDTI(self), TRUE);
	PDATA_free_params(SC_get_PDTI(self), STMT_FREE_PARAMS_ALL);
	
	if (self->__error_message)
		free(self->__error_message);
	if (self->pgerror)
		ER_Destructor(self->pgerror);
	cancelNeedDataState(self);
	if (self->callbacks)
		free(self->callbacks);

	DELETE_STMT_CS(self);
	free(self);

	mylog("SC_Destructor: EXIT\n");

	return TRUE;
}


/*
 *	Free parameters and free the memory from the
 *	data-at-execution parameters that was allocated in SQLPutData.
 */
void
SC_free_params(StatementClass *self, char option)
{
	if (option != STMT_FREE_PARAMS_DATA_AT_EXEC_ONLY)
	{
		APD_free_params(SC_get_APDF(self), option);
		IPD_free_params(SC_get_IPDF(self), option);
	}
	PDATA_free_params(SC_get_PDTI(self), option);
	self->data_at_exec = -1;
	self->current_exec_param = -1;
	self->put_data = FALSE;
	if (option == STMT_FREE_PARAMS_ALL)
	{
		self->exec_start_row = -1;
		self->exec_end_row = -1;
		self->exec_current_row = -1;
	}
}


int
statement_type(const char *statement)
{
	int			i;

	/* ignore leading whitespace in query string */
	while (*statement && (isspace((UCHAR) *statement) || *statement == '('))
		statement++;

	for (i = 0; Statement_Type[i].s; i++)
		if (!strnicmp(statement, Statement_Type[i].s, strlen(Statement_Type[i].s)))
			return Statement_Type[i].type;

	return STMT_TYPE_OTHER;
}

void
SC_set_planname(StatementClass *stmt, const char *plan_name)
{
	if (stmt->plan_name)
		free(stmt->plan_name);
	if (plan_name && plan_name[0])
		stmt->plan_name = strdup(plan_name);
	else
		stmt->plan_name = NULL;
}

void
SC_set_rowset_start(StatementClass *stmt, SQLLEN start, BOOL valid_base)
{
	QResultClass	*res = SC_get_Curres(stmt);
	SQLLEN		incr = start - stmt->rowset_start;

inolog("%p->SC_set_rowstart " FORMAT_LEN "->" FORMAT_LEN "(%s) ", stmt, stmt->rowset_start, start, valid_base ? "valid" : "unknown");
	if (res != NULL)
	{
		BOOL	valid = QR_has_valid_base(res);
inolog(":(%p)QR is %s", res, QR_has_valid_base(res) ? "valid" : "unknown");

		if (valid)
		{
			if (valid_base)
				QR_inc_rowstart_in_cache(res, incr);
			else
				QR_set_no_valid_base(res);
		}
		else if (valid_base)
		{	
			QR_set_has_valid_base(res);
			if (start < 0)
				QR_set_rowstart_in_cache(res, -1);
			else
				QR_set_rowstart_in_cache(res, start);
		}
		if (!QR_get_cursor(res))
			res->key_base = start;
inolog(":(%p)QR result=" FORMAT_LEN "(%s)", res, QR_get_rowstart_in_cache(res), QR_has_valid_base(res) ? "valid" : "unknown");
	}
	stmt->rowset_start = start;
inolog(":stmt result=" FORMAT_LEN "\n", stmt->rowset_start);
}
void
SC_inc_rowset_start(StatementClass *stmt, SQLLEN inc)
{
	SQLLEN	start = stmt->rowset_start + inc;
	
	SC_set_rowset_start(stmt, start, TRUE);
}
int
SC_set_current_col(StatementClass *stmt, int col)
{
	if (col == stmt->current_col)
		return col;
	if (col >= 0)
		reset_a_getdata_info(SC_get_GDTI(stmt), col + 1);
	stmt->current_col = col;

	return stmt->current_col;
}

void
SC_set_prepared(StatementClass *stmt, BOOL prepared)
{
	if (prepared == stmt->prepared)
		;
	else if (NOT_YET_PREPARED == prepared && PREPARED_PERMANENTLY == stmt->prepared)
	{
		ConnectionClass *conn = SC_get_conn(stmt);

		if (conn && CONN_CONNECTED == conn->status)
		{
			if (CC_is_in_error_trans(conn))
			{
				CC_mark_a_object_to_discard(conn, 's',  stmt->plan_name);
			}
			else
			{
				QResultClass	*res;
				char dealloc_stmt[128];

				sprintf(dealloc_stmt, "DEALLOCATE \"%s\"", stmt->plan_name);
				res = CC_send_query(conn, dealloc_stmt, NULL, IGNORE_ABORT_ON_CONN | ROLLBACK_ON_ERROR, NULL);
				QR_Destructor(res);
			} 
		}
	}
	if (NOT_YET_PREPARED == prepared)
		SC_set_planname(stmt, NULL);
	stmt->prepared = prepared;
}

/*
 *	Initialize stmt_with_params, load_statement and execute_statement
 *		member pointer deallocating corresponding prepared plan.
 *	Also initialize statement member pointer if specified.
 */
RETCODE
SC_initialize_stmts(StatementClass *self, BOOL initializeOriginal)
{
	ConnectionClass *conn = SC_get_conn(self);

	if (self->lock_CC_for_rb > 0)
	{
		while (self->lock_CC_for_rb > 0)
		{
			LEAVE_CONN_CS(conn);
			self->lock_CC_for_rb--;
		}
	}
	if (initializeOriginal)
	{
		if (self->statement)
		{
			free(self->statement);
			self->statement = NULL;
		}
		if (self->execute_statement)
		{
			free(self->execute_statement);
			self->execute_statement = NULL;
		}
		self->prepare = NON_PREPARE_STATEMENT;
		SC_set_prepared(self, NOT_YET_PREPARED);
		self->statement_type = STMT_TYPE_UNKNOWN; /* unknown */
		self->multi_statement = -1; /* unknown */
		self->num_params = -1; /* unknown */
		self->proc_return = -1; /* unknown */
		self->join_info = 0;
		SC_init_parse_method(self);
		SC_init_discard_output_params(self);
	}
	if (self->stmt_with_params)
	{
		free(self->stmt_with_params);
		self->stmt_with_params = NULL;
	}
	if (self->load_statement)
	{
		free(self->load_statement);
		self->load_statement = NULL;
	}

	return 0;
}

BOOL	SC_opencheck(StatementClass *self, const char *func)
{
	QResultClass	*res;

	if (!self)
		return FALSE;
	if (self->status == STMT_EXECUTING)
	{
        	SC_set_error(self, STMT_SEQUENCE_ERROR, "Statement is currently executing a transaction.", func);
		return TRUE;
	}
	/*
	 * We can dispose the result of PREMATURE execution any time.
	 */
	if (self->prepare && self->status == STMT_PREMATURE)
	{
		mylog("SC_opencheck: self->prepare && self->status == STMT_PREMATURE\n");
		return FALSE;
	}
	if (res = SC_get_Curres(self), NULL != res)
	{
		if (QR_command_maybe_successful(res) && res->backend_tuples)
		{
        		SC_set_error(self, STMT_SEQUENCE_ERROR, "The cursor is open.", func);
			return TRUE;
		}
	}

	return	FALSE;
}

RETCODE
SC_initialize_and_recycle(StatementClass *self)
{
	SC_initialize_stmts(self, TRUE);
	if (!SC_recycle_statement(self))
		return SQL_ERROR;

	return SQL_SUCCESS;
}

/*
 *	Called from SQLPrepare if STMT_PREMATURE, or
 *	from SQLExecute if STMT_FINISHED, or
 *	from SQLFreeStmt(SQL_CLOSE)
 */
char
SC_recycle_statement(StatementClass *self)
{
	CSTR	func = "SC_recycle_statement";
	ConnectionClass *conn;
	QResultClass	*res;

	mylog("%s: self= %p\n", func, self);

	SC_clear_error(self);
	/* This would not happen */
	if (self->status == STMT_EXECUTING)
	{
		SC_set_error(self, STMT_SEQUENCE_ERROR, "Statement is currently executing a transaction.", func);
		return FALSE;
	}

	conn = SC_get_conn(self);
	switch (self->status)
	{
		case STMT_ALLOCATED:
			/* this statement does not need to be recycled */
			return TRUE;

		case STMT_READY:
			break;

		case STMT_PREMATURE:

			/*
			 * Premature execution of the statement might have caused the
			 * start of a transaction. If so, we have to rollback that
			 * transaction.
			 */
			if (CC_loves_visible_trans(conn) && CC_is_in_trans(conn))
			{
				if (SC_is_pre_executable(self) && !SC_is_parse_tricky(self))
					/* CC_abort(conn) */;
			}
			break;

		case STMT_FINISHED:
			break;

		default:
			SC_set_error(self, STMT_INTERNAL_ERROR, "An internal error occured while recycling statements", func);
			return FALSE;
	}

	switch (self->prepared)
	{
		case NOT_YET_PREPARED:
		case ONCE_DESCRIBED:
        		/* Free the parsed table/field information */
			SC_initialize_cols_info(self, TRUE, TRUE);

inolog("SC_clear_parse_status\n");
			SC_clear_parse_status(self, conn);
			break;
	}

	/* Free any cursors */
	if (res = SC_get_Result(self), res)
	{
		switch (self->prepared)
		{
			case PREPARED_PERMANENTLY:
			case PREPARED_TEMPORARILY:
				QR_close_result(res, FALSE);
				break;
			default:
				QR_Destructor(res);
				SC_init_Result(self);
				break;
		}
	}
	self->inaccurate_result = FALSE;
	self->miscinfo = 0;
	/* self->rbonerr = 0; Never clear the bits here */

	/*
	 * Reset only parameters that have anything to do with results
	 */
	self->status = STMT_READY;
	self->catalog_result = FALSE;	/* not very important */

	self->currTuple = -1;
	SC_set_rowset_start(self, -1, FALSE);
	SC_set_current_col(self, -1);
	self->bind_row = 0;
inolog("%s statement=%p ommitted=0\n", func, self);
	self->last_fetch_count = self->last_fetch_count_include_ommitted = 0;

	self->__error_message = NULL;
	self->__error_number = 0;

	self->lobj_fd = -1;

	/*
	 * Free any data at exec params before the statement is executed
	 * again.  If not, then there will be a memory leak when the next
	 * SQLParamData/SQLPutData is called.
	 */
	SC_free_params(self, STMT_FREE_PARAMS_DATA_AT_EXEC_ONLY);
	SC_initialize_stmts(self, FALSE);
	cancelNeedDataState(self);
	self->cancel_info = 0;
	/*
	 *	reset the current attr setting to the original one.
	 */
	self->options.scroll_concurrency = self->options_orig.scroll_concurrency;
	self->options.cursor_type = self->options_orig.cursor_type;
	self->options.keyset_size = self->options_orig.keyset_size;
	self->options.maxLength = self->options_orig.maxLength;
	self->options.maxRows = self->options_orig.maxRows;

	return TRUE;
}

/*
 *	Scan the query wholly or partially (if the next_cmd param specified).
 *	Also count the number of parameters respectviely.
 */
void
SC_scanQueryAndCountParams(const char *query, const ConnectionClass *conn,
		Int4 *next_cmd, SQLSMALLINT * pcpar,
		po_ind_t *multi_st, po_ind_t *proc_return)
{
	CSTR func = "SC_scanQueryAndCountParams";
	char	literal_quote = LITERAL_QUOTE, identifier_quote = IDENTIFIER_QUOTE, dollar_quote = DOLLAR_QUOTE;
	const	char *sptr, *tstr, *tag = NULL;
	size_t	taglen = 0;
	char	tchar, bchar, escape_in_literal = '\0';
	char	in_literal = FALSE, in_identifier = FALSE,
		in_dollar_quote = FALSE, in_escape = FALSE,
		in_line_comment = FALSE, del_found = FALSE;
	int	comment_level = 0;
	po_ind_t multi = FALSE;
	SQLSMALLINT	num_p;
	encoded_str	encstr;

	mylog("%s: entering...\n", func);
	num_p = 0;
	if (proc_return)
		*proc_return = 0;
	if (next_cmd)
		*next_cmd = -1;
	tstr = query;
	make_encoded_str(&encstr, conn, tstr);
	for (sptr = tstr, bchar = '\0'; *sptr; sptr++)
	{
		tchar = encoded_nextchar(&encstr);
		if (ENCODE_STATUS(encstr) != 0) /* multibyte char */
		{
			if ((UCHAR) tchar >= 0x80)
				bchar = tchar;
			continue;
		}
		if (!multi && del_found)
		{
			if (!isspace(tchar))
			{
				multi = TRUE;
				if (next_cmd)
					break;
			}
		}
		if (in_dollar_quote)
		{
			if (tchar == dollar_quote)
			{
				if (strncmp(sptr, tag, taglen) == 0)
				{
					in_dollar_quote = FALSE;
					tag = NULL;
					sptr += taglen;
					sptr--;
					encoded_position_shift(&encstr, taglen - 1);
				}
			}
		}
		else if (in_literal)
		{
			if (in_escape)
				in_escape = FALSE;
			else if (tchar == escape_in_literal)
				in_escape = TRUE;
			else if (tchar == literal_quote)
				in_literal = FALSE;
		}
		else if (in_identifier)
		{
			if (tchar == identifier_quote)
				in_identifier = FALSE;
		}
		else if (in_line_comment)
		{
			if (PG_LINEFEED == tchar)
				in_line_comment = FALSE;
		}
		else if (comment_level > 0)
		{
			if ('/' == tchar && '*' == sptr[1])
			{
				encoded_nextchar(&encstr);
				sptr++;
				comment_level++;
			}
			else if ('*' == tchar && '/' == sptr[1])
			{
				encoded_nextchar(&encstr);
				sptr++;
				comment_level--;
			}
		}
		else
		{
			if (tchar == '?')
			{
				if (0 == num_p && bchar == '{')
				{
					if (proc_return)
						*proc_return = 1;
				}
				num_p++;
			}
			else if (tchar == ';')
			{
				del_found = TRUE;
				if (next_cmd)
					*next_cmd = sptr - query;
			}
			else if (tchar == dollar_quote)
			{
				taglen = findTag(sptr, dollar_quote, encstr.ccsc);
				if (taglen > 0)
				{
					in_dollar_quote = TRUE;
					tag = sptr;
					sptr += (taglen - 1);
					encoded_position_shift(&encstr, taglen - 1);
				}
				else
					num_p++;
			}
			else if (tchar == literal_quote)
			{
				in_literal = TRUE;
				escape_in_literal = CC_get_escape(conn);
				if (!escape_in_literal)
				{
					if (LITERAL_EXT == sptr[-1])
						escape_in_literal = ESCAPE_IN_LITERAL;
				}
			}
			else if (tchar == identifier_quote)
				in_identifier = TRUE;
			else if ('-' == tchar)
			{
				if ('-' == sptr[1])
				{
					encoded_nextchar(&encstr);
					sptr++;
					in_line_comment = TRUE;
				}
			}
			else if ('/' == tchar)
			{
				if ('*' == sptr[1])
				{
					encoded_nextchar(&encstr);
					sptr++;
					comment_level++;
				}
			}
			if (!isspace(tchar))
				bchar = tchar;
		}
	}
	if (pcpar)
		*pcpar = num_p;
	if (multi_st)
		*multi_st = multi;
}

/*
 * Pre-execute a statement (for SQLPrepare/SQLDescribeCol) 
 */
Int4	/* returns # of fields if successful */
SC_pre_execute(StatementClass *self)
{
	Int4		num_fields = -1;
	QResultClass	*res;
	mylog("SC_pre_execute: status = %d\n", self->status);

	res = SC_get_Curres(self);
	if (NULL != res)
	{
		num_fields = QR_NumResultCols(res);
		if (num_fields > 0 ||
		    NULL != QR_get_command(res))
			return num_fields;
	}
	if (self->status == STMT_READY)
	{
		mylog("              preprocess: status = READY\n");

		self->miscinfo = 0;
		if (SC_can_req_colinfo(self))
		{
			char		old_pre_executing = self->pre_executing;

			decideHowToPrepare(self, FALSE);
			self->inaccurate_result = FALSE;
			switch (SC_get_prepare_method(self))
			{
				case NAMED_PARSE_REQUEST:
				case PARSE_TO_EXEC_ONCE:
					if (SQL_SUCCESS != prepareParameters(self, TRUE))
						return num_fields;
					break;
				case PARSE_REQ_FOR_INFO:
					if (SQL_SUCCESS != prepareParameters(self, TRUE))
						return num_fields;
					self->status = STMT_PREMATURE;
					self->inaccurate_result = TRUE;
					break;
				default:
					self->pre_executing = TRUE;
					PGAPI_Execute(self, 0);

					self->pre_executing = old_pre_executing;

					if (self->status == STMT_FINISHED)
					{
						mylog("              preprocess: after status = FINISHED, so set PREMATURE\n");
						self->status = STMT_PREMATURE;
					}
			}
			if (res = SC_get_Curres(self), NULL != res)
			{
				num_fields = QR_NumResultCols(res);
				return num_fields;
			}
		}
		if (!SC_is_pre_executable(self))
		{
			SC_set_Result(self, QR_Constructor());
			QR_set_rstatus(SC_get_Result(self), PORES_TUPLES_OK);
			self->inaccurate_result = TRUE;
			self->status = STMT_PREMATURE;
			num_fields = 0;
		}
	}
	return num_fields;
}


/* This is only called from SQLFreeStmt(SQL_UNBIND) */
char
SC_unbind_cols(StatementClass *self)
{
	ARDFields	*opts = SC_get_ARDF(self);
	GetDataInfo	*gdata = SC_get_GDTI(self);
	BindInfoClass	*bookmark;

	ARD_unbind_cols(opts, FALSE);
	GDATA_unbind_cols(gdata, FALSE);
	if (bookmark = opts->bookmark, bookmark != NULL)
	{
		bookmark->buffer = NULL;
		bookmark->used = NULL;
	}

	return 1;
}


void
SC_clear_error(StatementClass *self)
{
	QResultClass	*res;

	self->__error_number = 0;
	if (self->__error_message)
	{
		free(self->__error_message);
		self->__error_message = NULL;
	}
	if (self->pgerror)
	{
		ER_Destructor(self->pgerror);
		self->pgerror = NULL;
	}
	self->diag_row_count = 0;
	if (res = SC_get_Curres(self), res)
	{
		QR_set_message(res, NULL);
		QR_set_notice(res, NULL);
		res->sqlstate[0] = '\0';
	}
	self->stmt_time = 0;
	SC_unref_CC_error(self);
}


/*
 *	This function creates an error info which is the concatenation
 *	of the result, statement, connection, and socket messages.
 */

/*	Map sql commands to statement types */
static struct
{
	int	number;
	const	char	* ver3str;
	const	char	* ver2str;
}	Statement_sqlstate[] =

{
	{ STMT_ERROR_IN_ROW, "01S01", "01S01" },
	{ STMT_OPTION_VALUE_CHANGED, "01S02", "01S02" },
	{ STMT_ROW_VERSION_CHANGED,  "01001", "01001" }, /* data changed */
	{ STMT_POS_BEFORE_RECORDSET, "01S06", "01S06" },
	{ STMT_TRUNCATED, "01004", "01004" }, /* data truncated */
	{ STMT_INFO_ONLY, "00000", "00000" }, /* just an information that is returned, no error */

	{ STMT_OK,  "00000", "00000" }, /* OK */
	{ STMT_EXEC_ERROR, "HY000", "S1000" }, /* also a general error */
	{ STMT_STATUS_ERROR, "HY010", "S1010" },
	{ STMT_SEQUENCE_ERROR, "HY010", "S1010" }, /* Function sequence error */
	{ STMT_NO_MEMORY_ERROR, "HY001", "S1001" }, /* memory allocation failure */
	{ STMT_COLNUM_ERROR, "07009", "S1002" }, /* invalid column number */
	{ STMT_NO_STMTSTRING, "HY001", "S1001" }, /* having no stmtstring is also a malloc problem */
	{ STMT_ERROR_TAKEN_FROM_BACKEND, "HY000", "S1000" }, /* general error */
	{ STMT_INTERNAL_ERROR, "HY000", "S1000" }, /* general error */
	{ STMT_STILL_EXECUTING, "HY010", "S1010" },
	{ STMT_NOT_IMPLEMENTED_ERROR, "HYC00", "S1C00" }, /* == 'driver not 
							  * capable' */
	{ STMT_BAD_PARAMETER_NUMBER_ERROR, "07009", "S1093" },
	{ STMT_OPTION_OUT_OF_RANGE_ERROR, "HY092", "S1092" },
	{ STMT_INVALID_COLUMN_NUMBER_ERROR, "07009", "S1002" },
	{ STMT_RESTRICTED_DATA_TYPE_ERROR, "07006", "07006" },
	{ STMT_INVALID_CURSOR_STATE_ERROR, "07005", "24000" },
	{ STMT_CREATE_TABLE_ERROR, "42S01", "S0001" }, /* table already exists */
	{ STMT_NO_CURSOR_NAME, "S1015", "S1015" },
	{ STMT_INVALID_CURSOR_NAME, "34000", "34000" },
	{ STMT_INVALID_ARGUMENT_NO, "HY024", "S1009" }, /* invalid argument value */
	{ STMT_ROW_OUT_OF_RANGE, "HY107", "S1107" },
	{ STMT_OPERATION_CANCELLED, "HY008", "S1008" },
	{ STMT_INVALID_CURSOR_POSITION, "HY109", "S1109" },
	{ STMT_VALUE_OUT_OF_RANGE, "HY019", "22003" },
	{ STMT_OPERATION_INVALID, "HY011", "S1011" },
	{ STMT_PROGRAM_TYPE_OUT_OF_RANGE, "?????", "?????" }, 
	{ STMT_BAD_ERROR, "08S01", "08S01" }, /* communication link failure */
	{ STMT_INVALID_OPTION_IDENTIFIER, "HY092", "HY092" },
	{ STMT_RETURN_NULL_WITHOUT_INDICATOR, "22002", "22002" },
	{ STMT_INVALID_DESCRIPTOR_IDENTIFIER, "HY091", "HY091" },
	{ STMT_OPTION_NOT_FOR_THE_DRIVER, "HYC00", "HYC00" },
	{ STMT_FETCH_OUT_OF_RANGE, "HY106", "S1106" },
	{ STMT_COUNT_FIELD_INCORRECT, "07002", "07002" },
	{ STMT_INVALID_NULL_ARG, "HY009", "S1009" },
	{ STMT_NO_RESPONSE, "08S01", "08S01" },
	{ STMT_COMMUNICATION_ERROR, "08S01", "08S01" }
};

static PG_ErrorInfo *
SC_create_errorinfo(const StatementClass *self)
{
	QResultClass *res = SC_get_Curres(self);
	ConnectionClass *conn = SC_get_conn(self);
	Int4	errornum;
	size_t		pos;
	BOOL		resmsg = FALSE, detailmsg = FALSE, msgend = FALSE;
	BOOL		looponce, loopend;
	char		msg[4096], *wmsg;
	char		*ermsg = NULL, *sqlstate = NULL;
	PG_ErrorInfo	*pgerror;

	if (self->pgerror)
		return self->pgerror;
	errornum = self->__error_number;
	if (errornum == 0)
		return	NULL;

	looponce = (SC_get_Result(self) != res);
	msg[0] = '\0';
	for (loopend = FALSE; (NULL != res) && !loopend; res = res->next)
	{
		if (looponce)
			loopend = TRUE;
		if ('\0' != res->sqlstate[0])
		{
			if (NULL != sqlstate && strnicmp(res->sqlstate, "00", 2) == 0)
				continue;
			sqlstate = res->sqlstate;
			if ('0' != sqlstate[0] ||
			    '1' < sqlstate[1])
				loopend = TRUE;
		}
		if (NULL != res->message)
		{
			strncpy_null(msg, res->message, sizeof(msg));
			detailmsg = resmsg = TRUE;
		}
		else if (NULL != res->messageref)
		{
			strncpy_null(msg, res->messageref, sizeof(msg));
			detailmsg = resmsg = TRUE;
		}
		if (msg[0])
			ermsg = msg;
		else if (QR_get_notice(res))
		{
			char *notice = QR_get_notice(res);
			size_t	len = strlen(notice);
			if (len < sizeof(msg))
			{
				memcpy(msg, notice, len);
				msg[len] = '\0';
				ermsg = msg;
			}
			else
			{
				ermsg = notice;
				msgend = TRUE;
			}
		}
	}
	if (!msgend && (wmsg = SC_get_errormsg(self)) && wmsg[0])
	{
		pos = strlen(msg);

		if (detailmsg)
		{
			msg[pos++] = ';';
			msg[pos++] = '\n';
		}
		strncpy_null(msg + pos, wmsg, sizeof(msg) - pos);
		ermsg = msg;
		detailmsg = TRUE;
	}
	if (!self->ref_CC_error)
		msgend = TRUE;

	if (conn && !msgend)
	{
		SocketClass *sock = conn->sock;
		const char *sockerrmsg;

		if (!resmsg && (wmsg = CC_get_errormsg(conn)) && wmsg[0] != '\0')
		{
			pos = strlen(msg);
			snprintf(&msg[pos], sizeof(msg) - pos, ";\n%s", CC_get_errormsg(conn));
		}

		if (sock && NULL != (sockerrmsg = SOCK_get_errmsg(sock)) && '\0' != sockerrmsg[0])
		{
			pos = strlen(msg);
			snprintf(&msg[pos], sizeof(msg) - pos, ";\n%s", sockerrmsg);
		}
		ermsg = msg;
	}
	pgerror = ER_Constructor(self->__error_number, ermsg);
	if (sqlstate)
		strcpy(pgerror->sqlstate, sqlstate);
	else if (conn)
	{
		if (!msgend && conn->sqlstate[0])
			strcpy(pgerror->sqlstate, conn->sqlstate);
		else
		{
        		EnvironmentClass *env = (EnvironmentClass *) CC_get_env(conn);

			errornum -= LOWEST_STMT_ERROR;
        		if (errornum < 0 ||
				errornum >= sizeof(Statement_sqlstate) / sizeof(Statement_sqlstate[0]))
				errornum = 1 - LOWEST_STMT_ERROR;
        		strcpy(pgerror->sqlstate, EN_is_odbc3(env) ?
				Statement_sqlstate[errornum].ver3str : 
				Statement_sqlstate[errornum].ver2str);
		}
	} 

	return pgerror;
}


StatementClass *SC_get_ancestor(StatementClass *stmt)
{
	StatementClass	*child = stmt, *parent;

inolog("SC_get_ancestor in stmt=%p\n", stmt);
	for (child = stmt, parent = child->execute_parent; parent; child = parent, parent = child->execute_parent)
	{
		inolog("parent=%p\n", parent);
	}
	return child;
}
void SC_reset_delegate(RETCODE retcode, StatementClass *stmt)
{
	StatementClass	*delegate = stmt->execute_delegate;

	if (!delegate)
		return;
	PGAPI_FreeStmt(delegate, SQL_DROP);
}

void
SC_set_error(StatementClass *self, int number, const char *message, const char *func)
{
	if (self->__error_message)
		free(self->__error_message);
	self->__error_number = number;
	self->__error_message = message ? strdup(message) : NULL;
	if (func && number != STMT_OK && number != STMT_INFO_ONLY)
		SC_log_error(func, "", self);
}


void
SC_set_errormsg(StatementClass *self, const char *message)
{
	if (self->__error_message)
		free(self->__error_message);
	self->__error_message = message ? strdup(message) : NULL;
}


void
SC_replace_error_with_res(StatementClass *self, int number, const char *message, const QResultClass *from_res, BOOL check)
{
	QResultClass	*self_res;
	BOOL	repstate;

inolog("SC_set_error_from_res %p->%p check=%i\n", from_res ,self, check);
	if (check)
	{
		if (0 == number)			return;
		if (0 > number &&		/* SQL_SUCCESS_WITH_INFO */
		    0 < self->__error_number)
			return;
	}
	self->__error_number = number;
	if (!check || message)
	{
		if (self->__error_message)
			free(self->__error_message);
		self->__error_message = message ? strdup(message) : NULL;
	}
	if (self->pgerror)
	{
		ER_Destructor(self->pgerror);
		self->pgerror = NULL;
	}
	self_res = SC_get_Curres(self);
	if (!self_res) return;
	if (self_res == from_res)	return;
	QR_add_message(self_res, QR_get_message(from_res));
	QR_add_notice(self_res, QR_get_notice(from_res));
	repstate = FALSE;
	if (!check)
		repstate = TRUE;
	else if (from_res->sqlstate[0])
	{
		if (!self_res->sqlstate[0] || strncmp(self_res->sqlstate, "00", 2) == 0)
			repstate = TRUE;
		else if (strncmp(from_res->sqlstate, "01", 2) >= 0)
			repstate = TRUE;
	}
	if (repstate)
		strcpy(self_res->sqlstate, from_res->sqlstate);
}

void
SC_error_copy(StatementClass *self, const StatementClass *from, BOOL check)
{
	QResultClass	*self_res, *from_res;
	BOOL	repstate;

inolog("SC_error_copy %p->%p check=%i\n", from ,self, check);
	if (self == from)	return;
	if (check)
	{
		if (0 == from->__error_number)	/* SQL_SUCCESS */
			return;
		if (0 > from->__error_number &&	/* SQL_SUCCESS_WITH_INFO */
		    0 < self->__error_number)
			return;
	}
	self->__error_number = from->__error_number;
	if (!check || from->__error_message)
	{
		if (self->__error_message)
			free(self->__error_message);
		self->__error_message = from->__error_message ? strdup(from->__error_message) : NULL;
	}
	if (self->pgerror)
	{
		ER_Destructor(self->pgerror);
		self->pgerror = NULL;
	}
	self_res = SC_get_Curres(self);
	from_res = SC_get_Curres(from);
	if (!self_res || !from_res)
		return;
	QR_add_message(self_res, QR_get_message(from_res));
	QR_add_notice(self_res, QR_get_notice(from_res));
	repstate = FALSE;
	if (!check)
		repstate = TRUE;
	else if (from_res->sqlstate[0])
	{
		if (!self_res->sqlstate[0] || strncmp(self_res->sqlstate, "00", 2) == 0)
			repstate = TRUE;
		else if (strncmp(from_res->sqlstate, "01", 2) >= 0)
			repstate = TRUE;
	}
	if (repstate)
		strcpy(self_res->sqlstate, from_res->sqlstate);
}


void
SC_full_error_copy(StatementClass *self, const StatementClass *from, BOOL allres)
{
	PG_ErrorInfo		*pgerror;

inolog("SC_full_error_copy %p->%p\n", from ,self);
	if (self->__error_message)
	{
		free(self->__error_message);
		self->__error_message = NULL;
	}
	if (from->__error_message)
		self->__error_message = strdup(from->__error_message);
	self->__error_number = from->__error_number;
	if (from->pgerror)
	{
		if (self->pgerror)
			ER_Destructor(self->pgerror);
		self->pgerror = ER_Dup(from->pgerror);
		return;
	}
	else if (!allres)
		return;
	pgerror = SC_create_errorinfo(from);
	if (!pgerror->__error_message[0])
	{
		ER_Destructor(pgerror);
		return;
	} 
	if (self->pgerror)
		ER_Destructor(self->pgerror);
	self->pgerror = pgerror;
}

/*              Returns the next SQL error information. */
RETCODE         SQL_API
PGAPI_StmtError(	SQLHSTMT	hstmt,
		SQLSMALLINT RecNumber,
		SQLCHAR FAR * szSqlState,
		SQLINTEGER FAR * pfNativeError,
		SQLCHAR FAR * szErrorMsg,
		SQLSMALLINT cbErrorMsgMax,
		SQLSMALLINT FAR * pcbErrorMsg,
		UWORD flag)
{
	/* CC: return an error of a hdesc  */
	StatementClass *stmt = (StatementClass *) hstmt;

	stmt->pgerror = SC_create_errorinfo(stmt);
	return ER_ReturnError(&(stmt->pgerror), RecNumber, szSqlState,
		pfNativeError, szErrorMsg, cbErrorMsgMax,
		pcbErrorMsg, flag);
}

time_t
SC_get_time(StatementClass *stmt)
{
	if (!stmt)
		return time(NULL);
	if (0 == stmt->stmt_time)
		stmt->stmt_time = time(NULL);
	return stmt->stmt_time;
}
/*
 *	Currently, the driver offers very simple bookmark support -- it is
 *	just the current row number.  But it could be more sophisticated
 *	someday, such as mapping a key to a 32 bit value
 */
SQLULEN
SC_get_bookmark(StatementClass *self)
{
	return SC_make_bookmark(self->currTuple);
}


RETCODE
SC_fetch(StatementClass *self)
{
	CSTR func = "SC_fetch";
	QResultClass *res = SC_get_Curres(self);
	ARDFields	*opts;
	GetDataInfo	*gdata;
	int		retval;
	RETCODE		result;

	Int2		num_cols,
				lf;
	OID			type;
	int		atttypmod;
	char	   *value;
	ColumnInfoClass *coli;
	BindInfoClass	*bookmark;
	BOOL		useCursor;

	/* TupleField *tupleField; */

inolog("%s statement=%p res=%x ommitted=0\n", func, self, res);
	self->last_fetch_count = self->last_fetch_count_include_ommitted = 0;
	if (!res)
		return SQL_ERROR;
	coli = QR_get_fields(res);	/* the column info */

	mylog("fetch_cursor=%d, %p->total_read=%d\n", SC_is_fetchcursor(self), res, res->num_total_read);

	useCursor = (SC_is_fetchcursor(self) && (NULL != QR_get_cursor(res)));
	if (!useCursor)
	{
		if (self->currTuple >= (Int4) QR_get_num_total_tuples(res) - 1 ||
			(self->options.maxRows > 0 && self->currTuple == self->options.maxRows - 1))
		{
			/*
			 * if at the end of the tuples, return "no data found" and set
			 * the cursor past the end of the result set
			 */
			self->currTuple = QR_get_num_total_tuples(res);
			return SQL_NO_DATA_FOUND;
		}

		mylog("**** %s: non-cursor_result\n", func);
		(self->currTuple)++;
	}
	else
	{
		int	lastMessageType;

		/* read from the cache or the physical next tuple */
		retval = QR_next_tuple(res, self, &lastMessageType);
		if (retval < 0)
		{
			mylog("**** %s: end_tuples\n", func);
			if (QR_get_cursor(res) &&
			    SQL_CURSOR_FORWARD_ONLY == self->options.cursor_type &&
			    QR_once_reached_eof(res))
				QR_close(res);
			return SQL_NO_DATA_FOUND;
		}
		else if (retval > 0)
			(self->currTuple)++;	/* all is well */
		else
		{
			ConnectionClass *conn = SC_get_conn(self);

			mylog("%s: error\n", func);
			switch (conn->status)
			{
				case CONN_NOT_CONNECTED:
				case CONN_DOWN:
					SC_set_error(self, STMT_BAD_ERROR, "Error fetching next row", func);
					break;
				default:
					switch (QR_get_rstatus(res))
					{
						case PORES_NO_MEMORY_ERROR:
							SC_set_error(self, STMT_NO_MEMORY_ERROR, NULL, __FUNCTION__);
							break;
						case PORES_BAD_RESPONSE:
							SC_set_error(self, STMT_COMMUNICATION_ERROR, "communication error occured", __FUNCTION__);
							break;
						default:
							SC_set_error(self, STMT_EXEC_ERROR, "Error fetching next row", __FUNCTION__);
							break;
					}
			}
			return SQL_ERROR;
		}
	}
	if (QR_haskeyset(res))
	{
		SQLLEN	kres_ridx;

		kres_ridx = GIdx2KResIdx(self->currTuple, self, res);
		if (kres_ridx >= 0 && kres_ridx < res->num_cached_keys)
		{
			UWORD	pstatus = res->keyset[kres_ridx].status;
inolog("SC_ pstatus[%d]=%hx fetch_count=" FORMAT_LEN "\n", kres_ridx, pstatus, self->last_fetch_count);
			if (0 != (pstatus & (CURS_SELF_DELETING | CURS_SELF_DELETED)))
				return SQL_SUCCESS_WITH_INFO;
			if (SQL_ROW_DELETED != (pstatus & KEYSET_INFO_PUBLIC) &&
		    		0 != (pstatus & CURS_OTHER_DELETED))
				return SQL_SUCCESS_WITH_INFO;
			if (0 != (CURS_NEEDS_REREAD & pstatus))
			{
				UWORD	qcount;

				result = SC_pos_reload(self, self->currTuple, &qcount, 0);
				if (SQL_ERROR == result)
					return result;
				pstatus &= ~CURS_NEEDS_REREAD;
			}
		}
	}

	num_cols = QR_NumPublicResultCols(res);

	result = SQL_SUCCESS;
	self->last_fetch_count++;
inolog("%s: stmt=%p ommitted++\n", func, self);
	self->last_fetch_count_include_ommitted++;

	opts = SC_get_ARDF(self);
	/*
	 * If the bookmark column was bound then return a bookmark. Since this
	 * is used with SQLExtendedFetch, and the rowset size may be greater
	 * than 1, and an application can use row or column wise binding, use
	 * the code in copy_and_convert_field() to handle that.
	 */
	if ((bookmark = opts->bookmark) && bookmark->buffer)
	{
		char		buf[32];
		SQLLEN	offset = opts->row_offset_ptr ? *opts->row_offset_ptr : 0;

		sprintf(buf, FORMAT_ULEN, SC_get_bookmark(self));
		SC_set_current_col(self, -1);
		result = copy_and_convert_field(self, 0, PG_UNSPECIFIED, buf,
			 SQL_C_ULONG, 0, bookmark->buffer + offset, 0,
			LENADDR_SHIFT(bookmark->used, offset),
			LENADDR_SHIFT(bookmark->used, offset));
	}

	if (self->options.retrieve_data == SQL_RD_OFF)		/* data isn't required */
		return SQL_SUCCESS;
	/* The following adjustment would be needed after SQLMoreResults() */
	if (opts->allocated < num_cols)
		extend_column_bindings(opts, num_cols);
	gdata = SC_get_GDTI(self);
	if (gdata->allocated != opts->allocated)
		extend_getdata_info(gdata, opts->allocated, TRUE);
	for (lf = 0; lf < num_cols; lf++)
	{
		mylog("fetch: cols=%d, lf=%d, opts = %p, opts->bindings = %p, buffer[] = %p\n", num_cols, lf, opts, opts->bindings, opts->bindings[lf].buffer);

		/* reset for SQLGetData */
		gdata->gdata[lf].data_left = -1;

		if (NULL == opts->bindings)
			continue;
		if (opts->bindings[lf].buffer != NULL)
		{
			/* this column has a binding */

			/* type = QR_get_field_type(res, lf); */
			type = CI_get_oid(coli, lf);		/* speed things up */
			atttypmod = CI_get_atttypmod(coli, lf);	/* speed things up */

			mylog("type = %d, atttypmod = %d\n", type, atttypmod);

			if (useCursor)
				value = QR_get_value_backend(res, lf);
			else
			{
				SQLLEN	curt = GIdx2CacheIdx(self->currTuple, self, res);
inolog("%p->base=%d curr=%d st=%d valid=%d\n", res, QR_get_rowstart_in_cache(res), self->currTuple, SC_get_rowset_start(self), QR_has_valid_base(res));
inolog("curt=%d\n", curt);
				value = QR_get_value_backend_row(res, curt, lf);
			}

			mylog("value = '%s'\n", (value == NULL) ? "<NULL>" : value);

			retval = copy_and_convert_field_bindinfo(self, type, atttypmod, value, lf);

			mylog("copy_and_convert: retval = %d\n", retval);

			switch (retval)
			{
				case COPY_OK:
					break;		/* OK, do next bound column */

				case COPY_UNSUPPORTED_TYPE:
					SC_set_error(self, STMT_RESTRICTED_DATA_TYPE_ERROR, "Received an unsupported type from Postgres.", func);
					result = SQL_ERROR;
					break;

				case COPY_UNSUPPORTED_CONVERSION:
					SC_set_error(self, STMT_RESTRICTED_DATA_TYPE_ERROR, "Couldn't handle the necessary data type conversion.", func);
					result = SQL_ERROR;
					break;

				case COPY_RESULT_TRUNCATED:
					SC_set_error(self, STMT_TRUNCATED, "Fetched item was truncated.", func);
					qlog("The %dth item was truncated\n", lf + 1);
					qlog("The buffer size = %d", opts->bindings[lf].buflen);
					qlog(" and the value is '%s'\n", value);
					result = SQL_SUCCESS_WITH_INFO;
					break;

					/* error msg already filled in */
				case COPY_GENERAL_ERROR:
					result = SQL_ERROR;
					break;

					/* This would not be meaningful in SQLFetch. */
				case COPY_NO_DATA_FOUND:
					break;

				default:
					SC_set_error(self, STMT_INTERNAL_ERROR, "Unrecognized return value from copy_and_convert_field.", func);
					result = SQL_ERROR;
					break;
			}
		}
	}

	return result;
}


#include "dlg_specific.h"
RETCODE
SC_execute(StatementClass *self)
{
	CSTR func = "SC_execute";
	CSTR fetch_cmd = "fetch";
	ConnectionClass *conn;
	IPDFields	*ipdopts;
	char		was_ok, was_nonfatal;
	QResultClass	*res = NULL;
	Int2		oldstatus,
				numcols;
	QueryInfo	qi;
	ConnInfo   *ci;
	UDWORD		qflag = 0;
	BOOL		is_in_trans, issue_begin, has_out_para;
	BOOL		use_extended_protocol;
	int		func_cs_count = 0, i;
	BOOL		useCursor;

	conn = SC_get_conn(self);
	ci = &(conn->connInfo);

	/* Begin a transaction if one is not already in progress */

	/*
	 * Basically we don't have to begin a transaction in autocommit mode
	 * because Postgres backend runs in autocomit mode. We issue "BEGIN"
	 * in the following cases. 1) we use declare/fetch and the statement
	 * is SELECT (because declare/fetch must be called in a transaction).
	 * 2) we are in autocommit off state and the statement isn't of type
	 * OTHER.
	 */
#define	return	DONT_CALL_RETURN_FROM_HERE???
	ENTER_INNER_CONN_CS(conn, func_cs_count);
	oldstatus = conn->status;
	if (CONN_EXECUTING == conn->status)
	{
		SC_set_error(self, STMT_SEQUENCE_ERROR, "Connection is already in use.", func);
		mylog("%s: problem with connection\n", func);
		goto cleanup;
	}
	is_in_trans = CC_is_in_trans(conn);
	if (useCursor = SC_is_fetchcursor(self))
	{
		QResultClass *curres = SC_get_Curres(self);

		if (NULL != curres)
			useCursor = (NULL != QR_get_cursor(curres));
	} 
	/* issue BEGIN ? */
	issue_begin = TRUE;
	if (self->internal)
		issue_begin = FALSE;
	else if (is_in_trans)
	{
		issue_begin = FALSE;
		if (STMT_TYPE_START == self->statement_type &&
		    CC_does_autocommit(conn))
		{
			CC_commit(conn);
			is_in_trans = CC_is_in_trans(conn);
		}
	}
	else if (CC_does_autocommit(conn) &&
		 (!useCursor
	    /* || SC_is_with_hold(self) thiw would lose the performance */
		 ))
		issue_begin = FALSE;
	else
	{
		switch (self->statement_type)
		{
			case STMT_TYPE_START:
			case STMT_TYPE_SPECIAL:
				issue_begin = FALSE;
				break;
		}
	}
	if (issue_begin)
	{
		mylog("   about to begin a transaction on statement = %p\n", self);
		if (PG_VERSION_GE(conn, 7.1))
			qflag |= GO_INTO_TRANSACTION;
                else if (!CC_begin(conn))
                {
			SC_set_error(self, STMT_EXEC_ERROR, "Could not begin a transaction", func);
			goto cleanup;
                }
	}

	/* self->status = STMT_EXECUTING; */
	if (!SC_SetExecuting(self, TRUE))
	{
		SC_set_error(self, STMT_OPERATION_CANCELLED, "Cancel Reuest Accepted", func);
		goto cleanup;
	}
	conn->status = CONN_EXECUTING;

	/* If it's a SELECT statement, use a cursor. */

	/*
	 * Note that the declare cursor has already been prepended to the
	 * statement
	 */
	/* in copy_statement... */
	use_extended_protocol = FALSE;
	switch (self->prepared)
	{
		case PREPARING_PERMANENTLY:
		case PREPARED_PERMANENTLY:
	    		if (PROTOCOL_74(ci))
				use_extended_protocol = TRUE;
			break;
		case PREPARING_TEMPORARILY:
		case PREPARED_TEMPORARILY:
			if (!issue_begin)
			{
				switch (SC_get_prepare_method(self))
				{
#ifndef	BYPASS_ONESHOT_PLAN_EXECUTION
					case PARSE_TO_EXEC_ONCE:
#endif /* BYPASS_ONESHOT_PLAN_EXECUTION */
					case NAMED_PARSE_REQUEST:
						use_extended_protocol = TRUE;
				}
			}
			if (!use_extended_protocol)
			{
				SC_forget_unnamed(self);
				SC_set_Result(self, NULL); /* discard the parsed information */
			}
	}
	if (use_extended_protocol)
	{
		char	*plan_name = self->plan_name;

		if (issue_begin)
			CC_begin(conn);
		if (!plan_name)
			plan_name = "";
		if (!SendBindRequest(self, plan_name))
		{
			if (SC_get_errornumber(self) <= 0)
				SC_set_error(self, STMT_EXEC_ERROR, "Bind request error", func);
			goto cleanup;
		}
		if (!SendExecuteRequest(self, plan_name, 0))
		{
			if (SC_get_errornumber(self) <= 0)
				SC_set_error(self, STMT_EXEC_ERROR, "Execute request error", func);
			goto cleanup;
		}
		for (res = SC_get_Result(self); NULL != res && NULL != res->next; res = res->next) ;
inolog("get_Result=%p %p %d\n", res, SC_get_Result(self), self->curr_param_result);
		if (!(res = SendSyncAndReceive(self, self->curr_param_result ? res : NULL, "bind_and_execute")))
		{
			if (SC_get_errornumber(self) <= 0)
				SC_set_error(self, STMT_NO_RESPONSE, "Could not receive the response, communication down ??", func);
			CC_on_abort(conn, CONN_DEAD);
			goto cleanup;
		}
	}
	else if (self->statement_type == STMT_TYPE_SELECT ||
		 self->statement_type == STMT_TYPE_PROCCALL)
	{
		char		fetch[128];
		const char *appendq = NULL;
		QueryInfo	*qryi = NULL;
 
		qflag |= (SQL_CONCUR_READ_ONLY != self->options.scroll_concurrency ? CREATE_KEYSET : 0);
		mylog("       Sending SELECT statement on stmt=%p, cursor_name='%s' qflag=%d,%d\n", self, SC_cursor_name(self), qflag, self->options.scroll_concurrency);

		/* send the declare/select */
		if (useCursor)
		{
			qi.result_in = NULL;
			qi.cursor = SC_cursor_name(self);
			qi.row_size = ci->drivers.fetch_max;
			sprintf(fetch, "%s " FORMAT_LEN " in \"%s\"", fetch_cmd, qi.row_size, SC_cursor_name(self));
			qryi = &qi;
			appendq = fetch;
			if (0 != (ci->extra_opts & BIT_IGNORE_ROUND_TRIP_TIME))
				qflag |= IGNORE_ROUND_TRIP;
		}
		res = CC_send_query_append(conn, self->stmt_with_params, qryi, qflag, SC_get_ancestor(self), appendq);
		if (useCursor && QR_command_maybe_successful(res))
		{
			if (appendq)
			{
				QResultClass	*qres, *nres;

				for (qres = res; qres;)
				{
					if (qres->command && strnicmp(qres->command, fetch_cmd, 5) == 0)
					{
						res = qres;
						break;
					}
					nres = qres->next;
					qres->next = NULL;
					QR_Destructor(qres);
					qres = nres;
				}
			}	
			if (SC_is_with_hold(self))
				QR_set_withhold(res);
		}
		mylog("     done sending the query:\n");
	}
	else
	{
		/* not a SELECT statement so don't use a cursor */
		mylog("      it's NOT a select statement: stmt=%p\n", self);
		res = CC_send_query(conn, self->stmt_with_params, NULL, qflag, SC_get_ancestor(self));

		/*
		 * We shouldn't send COMMIT. Postgres backend does the autocommit
		 * if neccessary. (Zoltan, 04/26/2000)
		 */

		/*
		 * Above seems wrong. Even in case of autocommit, started
		 * transactions must be committed. (Hiroshi, 02/11/2001)
		 */
		if (CC_is_in_trans(conn))
		{
			if (!is_in_trans)
				CC_set_in_manual_trans(conn);
			if (!self->internal && CC_does_autocommit(conn))
				CC_commit(conn);
		}
	}
	SC_forget_unnamed(self);

	if (CONN_DOWN != conn->status)
		conn->status = oldstatus;
	self->status = STMT_FINISHED;
	LEAVE_INNER_CONN_CS(func_cs_count, conn);

	/* Check the status of the result */
	if (res)
	{
		was_ok = QR_command_successful(res);
		was_nonfatal = QR_command_nonfatal(res);

		if (was_ok)
			SC_set_errornumber(self, STMT_OK);
		else if (0 < SC_get_errornumber(self))
			;
		else if (was_nonfatal)
			SC_set_errornumber(self, STMT_INFO_ONLY);
		else 
		{
			switch (QR_get_rstatus(res))
			{
				case PORES_NO_MEMORY_ERROR:
					SC_set_errornumber(self, STMT_NO_MEMORY_ERROR);
					break;
				case PORES_BAD_RESPONSE:
					SC_set_errornumber(self, STMT_COMMUNICATION_ERROR);
					break;
				case PORES_INTERNAL_ERROR:
					SC_set_errornumber(self, STMT_INTERNAL_ERROR);
					break;
				default:
					SC_set_errornumber(self, STMT_ERROR_TAKEN_FROM_BACKEND);
			}
		}
		/* set cursor before the first tuple in the list */
		self->currTuple = -1;
		SC_set_current_col(self, -1);
		SC_set_rowset_start(self, -1, FALSE);

		/* issue "ABORT" when query aborted */
		if (QR_get_aborted(res))
		{
#ifdef	_LEGACY_MODE_
			if (!self->internal)
				CC_abort(conn);
#endif /* _LEGACY_MODE */
		}
		else
		{
			QResultClass	*tres;

			/* see if the query did return any result columns */
			for (tres = res, numcols = 0; !numcols && tres; tres = tres->next)
			{
				numcols = QR_NumResultCols(tres);
			}
			/* now allocate the array to hold the binding info */
			if (numcols > 0)
			{
				ARDFields	*opts = SC_get_ARDF(self);
				extend_column_bindings(opts, numcols);
				if (opts->bindings == NULL)
				{
					QR_Destructor(res);
					SC_set_error(self, STMT_NO_MEMORY_ERROR,"Could not get enough free memory to store the binding information", func);
					goto cleanup;
				}
			}

inolog("!!%p->SC_is_concat_pre=%x res=%p\n", self, self->miscinfo, res);
			/*
			 * special handling of result for keyset driven cursors. 			 * Use the columns info of the 1st query and
			 * user the keyset info of the 2nd query.
			 */
			if (SQL_CURSOR_KEYSET_DRIVEN == self->options.cursor_type &&
			    SQL_CONCUR_READ_ONLY != self->options.scroll_concurrency &&
			    !useCursor)
			{
				if (tres = res->next, tres)
				{
					if (tres->fields)
						CI_Destructor(tres->fields);
					tres->fields = res->fields;
					res->fields = NULL;
					tres->num_fields = res->num_fields;
					res->next = NULL;
					QR_Destructor(res);
					SC_init_Result(self);
					SC_set_Result(self, tres);
					res = tres;
				}
			}
			/* skip the result of PREPARE in 'PREPARE ..:EXECUTE ..' call */
			else if (SC_is_concat_prepare_exec(self))
			{
				tres = res->next;
inolog("res->next=%p\n", tres);
				res->next = NULL;
				if (res != SC_get_Result(self))
					QR_Destructor(res);
				SC_set_Result(self, tres);
				res = tres;
				SC_set_prepared(self, PREPARED_PERMANENTLY);
				SC_no_concat_prepare_exec(self);
			}
		}
	}
	else
	{
		/* Bad Error -- The error message will be in the Connection */
		if (!conn->sock)
			SC_set_error(self, STMT_BAD_ERROR, CC_get_errormsg(conn), func);
		else if (self->statement_type == STMT_TYPE_CREATE)
		{
			SC_set_error(self, STMT_CREATE_TABLE_ERROR, "Error creating the table", func);

			/*
			 * This would allow the table to already exists, thus
			 * appending rows to it.  BUT, if the table didn't have the
			 * same attributes, it would fail. return
			 * SQL_SUCCESS_WITH_INFO;
			 */
		}
		else
		{
			SC_set_error(self, STMT_EXEC_ERROR, CC_get_errormsg(conn), func);
		}

#ifdef	_LEGACY_MODE_
		if (!self->internal)
			CC_abort(conn);
#endif /* _LEGACY_MODE_ */
	}
	if (!SC_get_Result(self))
		SC_set_Result(self, res);
	else
	{
		QResultClass	*last;

		for (last = SC_get_Result(self); NULL != last->next; last = last->next)
		{
			if (last == res)
				break;
		}
		if (last != res)
			last->next = res;
		self->curr_param_result = 1;
	}

	ipdopts = SC_get_IPDF(self);
	has_out_para = FALSE;
	if (self->statement_type == STMT_TYPE_PROCCALL &&
		(SC_get_errornumber(self) == STMT_OK ||
		 SC_get_errornumber(self) == STMT_INFO_ONLY))
	{
		Int2	io, out;
		has_out_para = (CountParameters(self, NULL, &io, &out) > 0);
/*
 *	I'm not sure if the following REFCUR_SUPPORT stuff is valuable
 *	or not.
 */
#ifdef	REFCUR_SUPPORT

inolog("!!! numfield=%d field_type=%u\n", QR_NumResultCols(res), QR_get_field_type(res, 0));
		if (!has_out_para &&
		    0 < QR_NumResultCols(res) &&
		    PG_TYPE_REFCURSOR == QR_get_field_type(res, 0))
		{
			char	fetch[128];
			int	stmt_type = self->statement_type;

			STR_TO_NAME(self->cursor_name, QR_get_value_backend_text(res, 0, 0));
			QR_Destructor(res);
			SC_init_Result(self);
			SC_set_fetchcursor(self); 
			qi.result_in = NULL;
			qi.cursor = SC_cursor_name(self);
			qi.row_size = ci->drivers.fetch_max;
			snprintf(fetch, sizeof(fetch), "%s " FORMAT_LEN " in \"%s\"", fetch_cmd, qi.row_size, SC_cursor_name(self));
			if (0 != (ci->extra_opts & BIT_IGNORE_ROUND_TRIP_TIME))
				qflag |= IGNORE_ROUND_TRIP;
			if (res = CC_send_query_append(conn, fetch, &qi, qflag, SC_get_ancestor(self), NULL), NULL != res)
				SC_set_Result(self, res);
		}
#endif	/* REFCUR_SUPPORT */
	}
	if (has_out_para)
	{	/* get the return value of the procedure call */
		RETCODE		ret;
		HSTMT		hstmt = (HSTMT) self;

		self->bind_row = 0;
		ret = SC_fetch(hstmt);
inolog("!!SC_fetch return =%d\n", ret);
		if (SQL_SUCCEEDED(ret))
		{
			APDFields	*apdopts = SC_get_APDF(self);
			SQLULEN		offset = apdopts->param_offset_ptr ? *apdopts->param_offset_ptr : 0;
			ARDFields	*ardopts = SC_get_ARDF(self);
			const ParameterInfoClass	*apara;
			const ParameterImplClass	*ipara;
			int	save_bind_size = ardopts->bind_size, gidx, num_p;

			ardopts->bind_size = apdopts->param_bind_type;
			num_p = self->num_params;
			if (ipdopts->allocated < num_p)
				num_p = ipdopts->allocated;
			for (i = 0, gidx = 0; i < num_p; i++)
			{
				ipara = ipdopts->parameters + i;
				if (ipara->paramType == SQL_PARAM_OUTPUT ||
				    ipara->paramType == SQL_PARAM_INPUT_OUTPUT)
				{
					apara = apdopts->parameters + i;	
					ret = PGAPI_GetData(hstmt, gidx + 1, apara->CType, apara->buffer + offset, apara->buflen, apara->used ? LENADDR_SHIFT(apara->used, offset) : NULL);
					if (!SQL_SUCCEEDED(ret))
					{
						SC_set_error(self, STMT_EXEC_ERROR, "GetData to Procedure return failed.", func);
						break;
					}
					gidx++;
				}
			}
			ardopts->bind_size = save_bind_size; /* restore */
		}
		else
		{
			SC_set_error(self, STMT_EXEC_ERROR, "SC_fetch to get a Procedure return failed.", func);
		}
	}
cleanup:
#undef	return
	SC_SetExecuting(self, FALSE);
	CLEANUP_FUNC_CONN_CS(func_cs_count, conn);
	if (CONN_DOWN != conn->status)
		conn->status = oldstatus;
	/* self->status = STMT_FINISHED; */
	if (SC_get_errornumber(self) == STMT_OK)
		return SQL_SUCCESS;
	else if (SC_get_errornumber(self) < STMT_OK)
		return SQL_SUCCESS_WITH_INFO;
	else
	{
		if (!SC_get_errormsg(self) || !SC_get_errormsg(self)[0])
		{
			if (STMT_NO_MEMORY_ERROR != SC_get_errornumber(self))
				SC_set_errormsg(self, "Error while executing the query");
			SC_log_error(func, NULL, self);
		}
		return SQL_ERROR;
	}
}

#define	CALLBACK_ALLOC_ONCE	4
int enqueueNeedDataCallback(StatementClass *stmt, NeedDataCallfunc func, void *data)
{
	if (stmt->num_callbacks >= stmt->allocated_callbacks)
	{
		SC_REALLOC_return_with_error(stmt->callbacks, NeedDataCallback,
			sizeof(NeedDataCallback) * (stmt->allocated_callbacks +
				CALLBACK_ALLOC_ONCE), stmt, "NeedDataCallback enqueue error", 0);
		stmt->allocated_callbacks += CALLBACK_ALLOC_ONCE;
	}
	stmt->callbacks[stmt->num_callbacks].func = func;
	stmt->callbacks[stmt->num_callbacks].data = data;
	stmt->num_callbacks++;

inolog("enqueueNeedDataCallack stmt=%p, func=%p, count=%d\n", stmt, func, stmt->num_callbacks);
	return stmt->num_callbacks;
}

RETCODE dequeueNeedDataCallback(RETCODE retcode, StatementClass *stmt)
{
	RETCODE			ret;
	NeedDataCallfunc	func;
	void			*data;
	int			i, cnt;

	mylog("dequeueNeedDataCallback ret=%d count=%d\n", retcode, stmt->num_callbacks);
	if (SQL_NEED_DATA == retcode)
		return retcode;
	if (stmt->num_callbacks <= 0)
		return retcode;
	func = stmt->callbacks[0].func;
	data = stmt->callbacks[0].data;
	for (i = 1; i < stmt->num_callbacks; i++)
		stmt->callbacks[i - 1] = stmt->callbacks[i];
	cnt = --stmt->num_callbacks;
	ret = (*func)(retcode, data);
	free(data);
	if (SQL_NEED_DATA != ret && cnt > 0)
		ret = dequeueNeedDataCallback(ret, stmt);
	return ret;
}

void	cancelNeedDataState(StatementClass *stmt)
{
	int	cnt = stmt->num_callbacks, i;

	stmt->num_callbacks = 0;
	for (i = 0; i < cnt; i++)
	{
		if (stmt->callbacks[i].data)
			free(stmt->callbacks[i].data);
	}
	SC_reset_delegate(SQL_ERROR, stmt);
}

void
SC_log_error(const char *func, const char *desc, const StatementClass *self)
{
	const	char *head;
#ifdef PRN_NULLCHECK
#define nullcheck(a) (a ? a : "(NULL)")
#endif
	if (self)
	{
		QResultClass *res = SC_get_Result(self);
		const ARDFields	*opts = SC_get_ARDF(self);
		const APDFields	*apdopts = SC_get_APDF(self);
		SQLLEN	rowsetSize;

#if (ODBCVER >= 0x0300)
		rowsetSize = (STMT_TRANSITION_EXTENDED_FETCH == self->transition_status ? opts->size_of_rowset_odbc2 : opts->size_of_rowset);
#else
		rowsetSize = opts->size_of_rowset_odbc2;
#endif /* ODBCVER */
		if (SC_get_errornumber(self) <= 0)
			head = "STATEMENT WARNING";
		else
		{
			head = "STATEMENT ERROR";
			qlog("%s: func=%s, desc='%s', errnum=%d, errmsg='%s'\n",head, func, desc, self->__error_number, nullcheck(self->__error_message));
		}
		mylog("%s: func=%s, desc='%s', errnum=%d, errmsg='%s'\n", head, func, desc, self->__error_number, nullcheck(self->__error_message));
		if (SC_get_errornumber(self) > 0)
		{
			qlog("                 ------------------------------------------------------------\n");
			qlog("                 hdbc=%p, stmt=%p, result=%p\n", self->hdbc, self, res);
			qlog("                 prepare=%d, internal=%d\n", self->prepare, self->internal);
			qlog("                 bindings=%p, bindings_allocated=%d\n", opts->bindings, opts->allocated);
			qlog("                 parameters=%p, parameters_allocated=%d\n", apdopts->parameters, apdopts->allocated);
			qlog("                 statement_type=%d, statement='%s'\n", self->statement_type, nullcheck(self->statement));
			qlog("                 stmt_with_params='%s'\n", nullcheck(self->stmt_with_params));
			qlog("                 data_at_exec=%d, current_exec_param=%d, put_data=%d\n", self->data_at_exec, self->current_exec_param, self->put_data);
			qlog("                 currTuple=%d, current_col=%d, lobj_fd=%d\n", self->currTuple, self->current_col, self->lobj_fd);
			qlog("                 maxRows=%d, rowset_size=%d, keyset_size=%d, cursor_type=%d, scroll_concurrency=%d\n", self->options.maxRows, rowsetSize, self->options.keyset_size, self->options.cursor_type, self->options.scroll_concurrency);
			qlog("                 cursor_name='%s'\n", SC_cursor_name(self));

			qlog("                 ----------------QResult Info -------------------------------\n");

			if (res)
			{
				qlog("                 fields=%p, backend_tuples=%p, tupleField=%d, conn=%p\n", res->fields, res->backend_tuples, res->tupleField, res->conn);
				qlog("                 fetch_count=%d, num_total_rows=%d, num_fields=%d, cursor='%s'\n", res->fetch_number, QR_get_num_total_tuples(res), res->num_fields, nullcheck(QR_get_cursor(res)));
				qlog("                 message='%s', command='%s', notice='%s'\n", nullcheck(QR_get_message(res)), nullcheck(res->command), nullcheck(res->notice));
				qlog("                 status=%d, inTuples=%d\n", QR_get_rstatus(res), QR_is_fetching_tuples(res));
			}

			/* Log the connection error if there is one */
			CC_log_error(func, desc, self->hdbc);
		}
	}
	else
	{
		qlog("INVALID STATEMENT HANDLE ERROR: func=%s, desc='%s'\n", func, desc);
		mylog("INVALID STATEMENT HANDLE ERROR: func=%s, desc='%s'\n", func, desc);
	}
#undef PRN_NULLCHECK
}

/*
 *	Extended Query 
 */

static BOOL
RequestStart(StatementClass *stmt, ConnectionClass *conn, const char *func)
{
	BOOL	ret = TRUE;

	if (SC_accessed_db(stmt))
		return TRUE;
	if (SQL_ERROR == SetStatementSvp(stmt))
	{
		char	emsg[128];

		snprintf(emsg, sizeof(emsg), "internal savepoint error in %s", func);
		SC_set_error(stmt, STMT_INTERNAL_ERROR, emsg, func);
		return FALSE;
	}
	if (!CC_is_in_trans(conn) && CC_loves_visible_trans(conn))
	{
		if (ret = CC_begin(conn), !ret)
			return ret;
	}
	return ret;
}

BOOL
SendBindRequest(StatementClass *stmt, const char *plan_name)
{
	CSTR	func = "SendBindRequest";
	ConnectionClass	*conn = SC_get_conn(stmt);

	mylog("%s: plan_name=%s\n", func, plan_name);
	if (!RequestStart(stmt, conn, func))
		return FALSE;
	if (!BuildBindRequest(stmt, plan_name))
		return FALSE;
	conn->stmt_in_extquery = stmt;

	return TRUE;
}

QResultClass *SendSyncAndReceive(StatementClass *stmt, QResultClass *res, const char *comment)
{
	CSTR func = "SendSyncAndReceive";
	ConnectionClass	*conn = SC_get_conn(stmt);
	SocketClass	*sock = conn->sock;
	char		id;
	Int4		response_length;
	UInt4		oid;
	int		num_p, num_io_params;
	int		i, pidx;
	Int2		num_discard_params, paramType;
	BOOL		rcvend = FALSE, loopend = FALSE, msg_truncated;
	char		msgbuffer[ERROR_MSG_LENGTH + 1];
	IPDFields	*ipdopts;
	QResultClass	*newres = NULL;

	if (!RequestStart(stmt, conn, func))
		return NULL;

	SOCK_put_char(sock, 'S');	/* Sync command */
	SOCK_put_int(sock, 4, 4);	/* length */
	SOCK_flush_output(sock);

	if (!res)
		newres = res = QR_Constructor();
	for (;!loopend;)
	{
		id = SOCK_get_id(sock);
		if ((SOCK_get_errcode(sock) != 0) || (id == EOF))
			break;
inolog("desc id=%c", id);
		response_length = SOCK_get_response_length(sock);
		if (0 != SOCK_get_errcode(sock))
			break;
inolog(" response_length=%d\n", response_length);
		switch (id)
		{
			case 'C':
				SOCK_get_string(sock, msgbuffer, sizeof(msgbuffer));
				mylog("command response=%s\n", msgbuffer);
				QR_set_command(res, msgbuffer);
				if (QR_is_fetching_tuples(res))
				{
					res->dataFilled = TRUE;
					QR_set_no_fetching_tuples(res);
					/* in case of FETCH, Portal Suspend never arrives */
					if (strnicmp(msgbuffer, "SELECT", 6) == 0)
					{
						mylog("%s: reached eof now\n", func);
						QR_set_reached_eof(res);
					}
					else
					{
						int	ret1, ret2;

						ret1 = ret2 = 0;
						if (sscanf(msgbuffer, "%*s %d %d", &ret1, &ret2) > 1)
							res->recent_processed_row_count = ret2;
						else
							res->recent_processed_row_count = ret1;
					}
				}
				break;
			case 'E': /* ErrorMessage */
				msg_truncated = handle_error_message(conn, msgbuffer, sizeof(msgbuffer), res->sqlstate, comment, res);

				break;
			case 'N': /* Notice */
				msg_truncated = handle_notice_message(conn, msgbuffer, sizeof(msgbuffer), res->sqlstate, comment, res);
				break;
			case '1': /* ParseComplete */
				if (stmt->plan_name)
					SC_set_prepared(stmt, PREPARED_PERMANENTLY);
				else
					SC_set_prepared(stmt, PREPARED_TEMPORARILY);
				break;
			case '2': /* BindComplete */
				QR_set_fetching_tuples(res);
				break;
			case '3': /* CloseComplete */
				QR_set_no_fetching_tuples(res);
				break;
			case 'Z': /* ReadyForQuery */
				loopend = rcvend = TRUE;
				EatReadyForQuery(conn);
				break;
			case 't': /* ParameterDesription */
				num_p = SOCK_get_int(sock, 2);
inolog("num_params=%d info=%d\n", stmt->num_params, num_p);
				num_discard_params = 0;
				if (stmt->discard_output_params)
					CountParameters(stmt, NULL, NULL, &num_discard_params);
				if (num_discard_params < stmt->proc_return)
					num_discard_params = stmt->proc_return;
				if (num_p + num_discard_params != (int) stmt->num_params)
				{
					mylog("ParamInfo unmatch num_params(=%d) != info(=%d)+discard(=%d)\n", stmt->num_params, num_p, num_discard_params);
					/* stmt->num_params = (Int2) num_p + num_discard_params; it's possible in case of multi command queries */
				}
				ipdopts = SC_get_IPDF(stmt);
				extend_iparameter_bindings(ipdopts, stmt->num_params);
#ifdef	NOT_USED
				if (stmt->discard_output_params)
				{
					for (i = 0, pidx = stmt->proc_return; i < num_p && pidx < stmt->num_params; pidx++)
					{
						paramType = ipdopts->parameters[pidx].paramType;
						if (SQL_PARAM_OUTPUT == paramType)
						{
							i++;
							continue;
						}
						oid = SOCK_get_int(sock, 4);
						PIC_set_pgtype(ipdopts->parameters[pidx], oid);
					}
				}
				else
				{
					for (i = 0, pidx = stmt->proc_return; i < num_p; i++, pidx++)
					{
						paramType = ipdopts->parameters[pidx].paramType;	
						oid = SOCK_get_int(sock, 4);
						if (SQL_PARAM_OUTPUT != paramType ||
						    PG_TYPE_VOID != oid)
							PIC_set_pgtype(ipdopts->parameters[pidx], oid);
					}
				}
#endif /* NOT_USED */
				pidx = stmt->current_exec_param;
				if (pidx >= 0)
					pidx--;
				for (i = 0; i < num_p; i++)
				{
					SC_param_next(stmt, &pidx, NULL, NULL);
					if (pidx >= stmt->num_params)
					{
						mylog("%dth parameter's position(%d) is out of bound[%d]\n", i, pidx, stmt->num_params);
						break;
					}
					oid = SOCK_get_int(sock, 4);
					paramType = ipdopts->parameters[pidx].paramType;	
					if (SQL_PARAM_OUTPUT != paramType ||
					    PG_TYPE_VOID != oid)
						PIC_set_pgtype(ipdopts->parameters[pidx], oid);
				}
				break;
			case 'T': /* RowDesription */
				QR_set_conn(res, conn);
				if (CI_read_fields(QR_get_fields(res), conn))
				{
					Int2	dummy1, dummy2;
					int	cidx;

					QR_set_rstatus(res, PORES_FIELDS_OK);
					res->num_fields = CI_get_num_fields(res->fields);
					if (QR_haskeyset(res))
						res->num_fields -= res->num_key_fields;
					num_io_params = CountParameters(stmt, NULL, &dummy1, &dummy2);
					if (stmt->proc_return > 0 ||
					    num_io_params > 0)
					{
						ipdopts = SC_get_IPDF(stmt);
						extend_iparameter_bindings(ipdopts, stmt->num_params);
						for (i = 0, cidx = 0; i < stmt->num_params; i++)
						{
							if (i < stmt->proc_return)
								ipdopts->parameters[i].paramType = SQL_PARAM_OUTPUT;
							paramType =ipdopts->parameters[i].paramType;
 							if (SQL_PARAM_OUTPUT == paramType ||
							    SQL_PARAM_INPUT_OUTPUT == paramType)
							{
inolog("!![%d].PGType %u->%u\n", i, PIC_get_pgtype(ipdopts->parameters[i]), CI_get_oid(res->fields, cidx));
								PIC_set_pgtype(ipdopts->parameters[i], CI_get_oid(res->fields, cidx));
								cidx++;
							}
						}
					}
				}
				else
				{
					if (NULL == QR_get_fields(res)->coli_array)
					{
						QR_set_rstatus(res, PORES_NO_MEMORY_ERROR);
						QR_set_messageref(res, "Out of memory while reading field information");
					}
					else
					{
						QR_set_rstatus(res, PORES_BAD_RESPONSE);
						QR_set_message(res, "Error reading field information");
					}
					loopend = rcvend = TRUE;
				}
				break;
			case 'B': /* Binary data */
			case 'D': /* ASCII data */
				if (!QR_get_tupledata(res, id == 'B'))
				{
					loopend = TRUE;
				}
				break;
			case 'S': /* parameter status */
				getParameterValues(conn);
				break;
			case 's':	/* portal suspend */
				QR_set_no_fetching_tuples(res);
				res->dataFilled = TRUE;
				break;
			default:
				break;
		}
	}
	if (!rcvend && 0 == SOCK_get_errcode(sock) && EOF != id)
	{
		for (;;)
		{
			id = SOCK_get_id(sock);
			if ((SOCK_get_errcode(sock) != 0) || (id == EOF))
				break;
			response_length = SOCK_get_response_length(sock);
			if (0 != SOCK_get_errcode(sock))
				break;
			if ('Z' == id)
			{
				EatReadyForQuery(conn);
				qlog("%s Discarded data until ReadyForQuery comes\n", __FUNCTION__);
				break;
			}
		}
	}
	if (0 != SOCK_get_errcode(sock) || EOF == id)
	{
		SC_set_error(stmt, STMT_NO_RESPONSE, "No response rom the backend", func);

		mylog("%s: 'id' - %s\n", func, SC_get_errormsg(stmt));
		CC_on_abort(conn, CONN_DEAD);
		res = NULL;
	}
	if (res != newres &&
	    NULL != newres)
		QR_Destructor(newres);
	conn->stmt_in_extquery = NULL;
	return res;
}

BOOL
SendParseRequest(StatementClass *stmt, const char *plan_name, const char *query, Int4 qlen, Int2 num_params)
{
	CSTR	func = "SendParseRequest";
	ConnectionClass	*conn = SC_get_conn(stmt);
	SocketClass	*sock = conn->sock;
	Int4		sta_pidx = -1, end_pidx = -1;
	size_t		pileng, leng;

	mylog("%s: plan_name=%s query=%s\n", func, plan_name, query);
	qlog("%s: plan_name=%s query=%s\n", func, plan_name, query);
	if (!RequestStart(stmt, conn, func))
		return FALSE;

	SOCK_put_char(sock, 'P'); /* Parse command */
	if (SOCK_get_errcode(sock) != 0)
	{
		CC_set_error(conn, CONNECTION_COULD_NOT_SEND, "Could not send P request to backend", func);
		CC_on_abort(conn, CONN_DEAD);
		return FALSE;
	}

	pileng = sizeof(Int2);
	if (stmt->discard_output_params)
		num_params = 0;
	else if (num_params != 0)
	{
#ifdef	NOT_USED
		sta_pidx += stmt->proc_return;
#endif /* NOT_USED */
		int	pidx;

		sta_pidx = stmt->current_exec_param;
		if (num_params < 0)
			end_pidx = stmt->num_params - 1;
		else
			end_pidx = sta_pidx + num_params - 1;
#ifdef	NOT_USED
		num_params = end_pidx - sta_pidx + 1;
#endif /* NOT_USED */
		for (num_params = 0, pidx = sta_pidx - 1;;)
		{
			SC_param_next(stmt, &pidx, NULL, NULL);
			if (pidx > end_pidx)
				break;
			else if (pidx < end_pidx)
				num_params++;
			else
			{
				num_params++;
				break;
			}
		}
mylog("sta_pidx=%d end_pidx=%d num_p=%d\n", sta_pidx, end_pidx, num_params);
		pileng += (sizeof(UInt4) * num_params);
	}
	qlen = (SQL_NTS == qlen) ? strlen(query) : qlen; 
	leng = strlen(plan_name) + 1 + qlen + 1 + pileng;
	SOCK_put_int(sock, (Int4) (leng + 4), 4); /* length */
inolog("parse leng=" FORMAT_SIZE_T "\n", leng);
	SOCK_put_string(sock, plan_name);
	SOCK_put_n_char(sock, query, qlen);
	SOCK_put_char(sock, '\0');
	SOCK_put_int(sock, num_params, sizeof(Int2)); /* number of parameters specified */
	if (num_params > 0)
	{
		int	i;
		IPDFields	*ipdopts = SC_get_IPDF(stmt);

		for (i = sta_pidx; i <= end_pidx; i++)
		{
			if (i < ipdopts->allocated &&
			    SQL_PARAM_OUTPUT == ipdopts->parameters[i].paramType)
				SOCK_put_int(sock, PG_TYPE_VOID, sizeof(UInt4));
			else
				SOCK_put_int(sock, 0, sizeof(UInt4));
		}
	}
	conn->stmt_in_extquery = stmt;

	return TRUE;
}

BOOL	SyncParseRequest(ConnectionClass *conn)
{
	StatementClass *stmt = conn->stmt_in_extquery;
	QResultClass	*res, *last;
	BOOL	ret = FALSE;

	if (!stmt)
		return TRUE;

	res = SC_get_Result(stmt);
	for (last = res; last && last->next; last = last->next)
		;
	if (!(res = SendSyncAndReceive(stmt, stmt->curr_param_result ? last : NULL, __FUNCTION__)))
	{
		if (SC_get_errornumber(stmt) <= 0)
			SC_set_error(stmt, STMT_NO_RESPONSE, "Could not receive the response, communication down ??", __FUNCTION__);
		CC_on_abort(conn, CONN_DEAD);
		goto cleanup;
	}

	if (!last)
		SC_set_Result(stmt, res);
	else
	{
		if (res != last)
			last->next = res;
		stmt->curr_param_result = 1;
	}
	if (!QR_command_maybe_successful(res))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "Error while syncing parse reuest", __FUNCTION__);
		goto cleanup;
	}
	ret = TRUE;
cleanup:
	return ret;
}

BOOL
SendDescribeRequest(StatementClass *stmt, const char *plan_name, BOOL paramAlso)
{
	CSTR	func = "SendDescribeRequest";
	ConnectionClass	*conn = SC_get_conn(stmt);
	SocketClass	*sock = conn->sock;
	size_t		leng;
	BOOL		sockerr = FALSE;

	mylog("%s:plan_name=%s\n", func, plan_name);
	if (!RequestStart(stmt, conn, func))
		return FALSE;

	SOCK_put_char(sock, 'D'); /* Describe command */
	if (SOCK_get_errcode(sock) != 0)
		sockerr = TRUE;
	if (!sockerr)
	{
		leng = 1 + strlen(plan_name) + 1;
		SOCK_put_int(sock, (Int4) (leng + 4), 4); /* length */
		if (SOCK_get_errcode(sock) != 0)
			sockerr = TRUE;
	}
	if (!sockerr)
	{
inolog("describe leng=%d\n", leng);
		SOCK_put_char(sock, paramAlso ? 'S' : 'P'); /* describe a prepared statement */
		if (SOCK_get_errcode(sock) != 0)
			sockerr = TRUE;
	}
	if (!sockerr)
	{
		SOCK_put_string(sock, plan_name);
		if (SOCK_get_errcode(sock) != 0)
			sockerr = TRUE;
	}
	if (sockerr)
	{
		CC_set_error(conn, CONNECTION_COULD_NOT_SEND, "Could not send D Request to backend", func);
		CC_on_abort(conn, CONN_DEAD);
		return FALSE;
	}
	conn->stmt_in_extquery = stmt;

	return TRUE;
}

BOOL
SendExecuteRequest(StatementClass *stmt, const char *plan_name, UInt4 count)
{
	CSTR	func = "SendExecuteRequest";
	ConnectionClass	*conn;
	SocketClass	*sock;
	size_t		leng;

	if (!stmt)	return FALSE;
	if (conn = SC_get_conn(stmt), !conn)	return FALSE;
	if (sock = conn->sock, !sock)	return FALSE;

	mylog("%s: plan_name=%s count=%d\n", func, plan_name, count);
	qlog("%s: plan_name=%s count=%d\n", func, plan_name, count);
	if (!SC_is_fetchcursor(stmt))
	{
		switch (stmt->prepared)
		{
			case NOT_YET_PREPARED:
			case ONCE_DESCRIBED:
				SC_set_error(stmt, STMT_EXEC_ERROR, "about to execute a non-prepared statement", func);
				return FALSE;
		}
	}
	if (!RequestStart(stmt, conn, func))
		return FALSE;

	SOCK_put_char(sock, 'E'); /* Execute command */
	SC_forget_unnamed(stmt); /* unnamed plans are unavailable */
	if (SOCK_get_errcode(sock) != 0)
	{
		CC_set_error(conn, CONNECTION_COULD_NOT_SEND, "Could not send E Request to backend", func);
		CC_on_abort(conn, CONN_DEAD);
		return FALSE;
	}

	leng = strlen(plan_name) + 1 + 4;
	SOCK_put_int(sock, (Int4) (leng + 4), sizeof(Int4)); /* length */
inolog("execute leng=%d\n", leng);
	SOCK_put_string(sock, plan_name);	/* portal name == plan name */
	SOCK_put_int(sock, count, sizeof(Int4));
	if (0 == count) /* will send a Close portal command */
	{
		SOCK_put_char(sock, 'C');	/* Close command */
		if (SOCK_get_errcode(sock) != 0)
		{
			CC_set_error(conn, CONNECTION_COULD_NOT_SEND, "Could not send C Request to backend", func);
			CC_on_abort(conn, CONN_DEAD);
			return FALSE;
		}

		leng = 1 + strlen(plan_name) + 1;
		SOCK_put_int(sock, (Int4) (leng + 4), 4); /* length */
inolog("Close leng=%d\n", leng);
		SOCK_put_char(sock, 'P');	/* Portal */
		SOCK_put_string(sock, plan_name);
	}
	conn->stmt_in_extquery = stmt;

	return TRUE;
}

BOOL	SendSyncRequest(ConnectionClass *conn)
{
	SocketClass	*sock = conn->sock;

	SOCK_put_char(sock, 'S');	/* Sync command */
	SOCK_put_int(sock, 4, 4);
	SOCK_flush_output(sock);
	conn->stmt_in_extquery = NULL;

	return TRUE;
}

enum {
	CancelRequestSet	= 1L
	,CancelRequestAccepted	= (1L << 1)
	,CancelCompleted	= (1L << 2)
};
/*	commonly used for short term lock */
#if defined(WIN_MULTITHREAD_SUPPORT)
extern  CRITICAL_SECTION        common_cs;
#elif defined(POSIX_MULTITHREAD_SUPPORT)
extern  pthread_mutex_t         common_cs;
#endif /* WIN_MULTITHREAD_SUPPORT */
BOOL	SC_IsExecuting(const StatementClass *self)
{
	BOOL	ret;
	ENTER_COMMON_CS; /* short time blocking */
	ret = (STMT_EXECUTING == self->status);
	LEAVE_COMMON_CS;
	return ret;
}
BOOL	SC_SetExecuting(StatementClass *self, BOOL on)
{
	BOOL	exeSet = FALSE;	
	ENTER_COMMON_CS; /* short time blocking */
	if (on)
	{
		if (0 == (self->cancel_info & CancelRequestSet))
		{
			self->status = STMT_EXECUTING;
			exeSet = TRUE;
		}
	}
	else
	{
		self->cancel_info = 0;
		self->status = STMT_FINISHED;
		exeSet = TRUE;
	}	
	LEAVE_COMMON_CS;
	return exeSet;
}

#ifdef	NOT_USED
BOOL	SC_SetCancelRequest(StatementClass *self)
{
	BOOL	enteredCS = FALSE;

	ENTER_COMMON_CS;
	if (0 != (self->cancel_info & CancelCompleted))
		;
	else if (STMT_EXECUTING == self->status)
	{
		self->cancel_info |= CancelRequestSet;
	}
	else
	{
		/* try to acquire */
		if (TRY_ENTER_STMT_CS(self))
			enteredCS = TRUE;
		else
			self->cancel_info |= CancelRequestSet;
	}	
	LEAVE_COMMON_CS;
	return enteredCS;
}
#endif /* NOT_USED */

BOOL	SC_AcceptedCancelRequest(const StatementClass *self)
{
	BOOL	shouldCancel = FALSE;
	ENTER_COMMON_CS;
	if (0 != (self->cancel_info & (CancelRequestSet | CancelRequestAccepted | CancelCompleted)))
		shouldCancel = TRUE;
	LEAVE_COMMON_CS;
	return shouldCancel;
}
