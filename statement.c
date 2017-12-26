/*------
 * Module:			statement.c
 *
 * Description:		This module contains functions related to creating
 *					and manipulating a statement.
 *
 * Classes:			StatementClass (Functions prefix: "SC_")
 *
 * API functions:	SQLAllocStmt, SQLFreeStmt
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */

#ifdef	WIN_MULTITHREAD_SUPPORT
#ifndef	_WIN32_WINNT
#define	_WIN32_WINNT	0x0400
#endif /* _WIN32_WINNT */
#endif /* WIN_MULTITHREAD_SUPPORT */

#include "statement.h"
#include "misc.h" // strncpy_null

#include "bind.h"
#include "connection.h"
#include "multibyte.h"
#include "qresult.h"
#include "convert.h"
#include "environ.h"
#include "loadlib.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pgapifunc.h"


/*	Map sql commands to statement types */
static const struct
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

	/*
	 * Special-commands that cannot be run in a transaction block. This isn't
	 * as granular as it could be. VACUUM can never be run in a transaction
	 * block, but some variants of REINDEX and CLUSTER can be. CHECKPOINT
	 * doesn't throw an error if you do, but it cannot be rolled back so
	 * there's no point in beginning a new transaction for it.
	 */
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

static QResultClass *libpq_bind_and_exec(StatementClass *stmt);
static void SC_set_errorinfo(StatementClass *self, QResultClass *res, int errkind);
static void SC_set_error_if_not_set(StatementClass *self, int errornumber, const char *errmsg, const char *func);


RETCODE		SQL_API
PGAPI_AllocStmt(HDBC hdbc,
				HSTMT * phstmt, UDWORD flag)
{
	CSTR func = "PGAPI_AllocStmt";
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	StatementClass *stmt;
	ARDFields	*ardopts;

	MYLOG(0, "entering...\n");

	if (!conn)
	{
		CC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	stmt = SC_Constructor(conn);

	MYLOG(0, "**** : hdbc = %p, stmt = %p\n", hdbc, stmt);

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
		stmt->ardi.ardf = conn->ardOptions;
	}
	else
	{
		InitializeStatementOptions(&stmt->options_orig);
		stmt->options = stmt->options_orig;
		InitializeARDFields(&stmt->ardi.ardf);
	}
	ardopts = SC_get_ARDF(stmt);
	ARD_AllocBookmark(ardopts);

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

	MYLOG(0, "entering...hstmt=%p, fOption=%hi\n", hstmt, fOption);

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
			if (conn->unnamed_prepared_stmt == stmt)
				conn->unnamed_prepared_stmt = NULL;

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
		SC_set_Curres(stmt, NULL);
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
	opt->scroll_concurrency = SQL_CONCUR_READ_ONLY;
	opt->cursor_type = SQL_CURSOR_FORWARD_ONLY;
	opt->retrieve_data = SQL_RD_ON;
	opt->use_bookmarks = SQL_UB_OFF;
	opt->metadata_id = SQL_FALSE;
}

static void SC_clear_parse_status(StatementClass *self, ConnectionClass *conn)
{
	self->parse_status = STMT_PARSE_NONE;
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
		rv->external = FALSE;
		rv->iflag = 0;
		rv->plan_name = NULL;
		rv->transition_status = STMT_TRANSITION_UNALLOCATED;
		rv->multi_statement = -1; /* unknown */
		rv->num_params = -1; /* unknown */
		rv->processed_statements = NULL;

		rv->__error_message = NULL;
		rv->__error_number = 0;
		rv->pgerror = NULL;

		rv->statement = NULL;
		rv->stmt_with_params = NULL;
		rv->load_statement = NULL;
		rv->statement_type = STMT_TYPE_UNKNOWN;

		rv->currTuple = -1;
		rv->rowset_start = 0;
		SC_set_rowset_start(rv, -1, FALSE);
		rv->current_col = -1;
		rv->bind_row = 0;
		rv->from_pos = rv->load_from_pos = rv->where_pos = -1;
		rv->last_fetch_count = rv->last_fetch_count_include_ommitted = 0;
		rv->save_rowset_size = -1;

		rv->data_at_exec = -1;
		rv->current_exec_param = -1;
		rv->exec_start_row = -1;
		rv->exec_end_row = -1;
		rv->exec_current_row = -1;
		rv->put_data = FALSE;
		rv->ref_CC_error = FALSE;
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

		rv->miscinfo = 0;
		rv->execinfo = 0;
		rv->rb_or_tc = 0;
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
	CSTR func	= "SC_Destructor";
	QResultClass	*res = SC_get_Result(self);

	MYLOG(0, "entering self=%p, self->result=%p, self->hdbc=%p\n", self, res, self->hdbc);
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

	MYLOG(0, "leaving\n");

	return TRUE;
}

void
SC_init_Result(StatementClass *self)
{
	self->result = self->curres = NULL;
	self->curr_param_result = 0;
	MYLOG(0, "leaving(%p)\n", self);
}

void
SC_set_Result(StatementClass *self, QResultClass *res)
{
	if (res != self->result)
	{
		MYLOG(0, "(%p, %p)\n", self, res);
		QR_Destructor(self->result);
		self->result = self->curres = res;
		if (NULL != res)
			self->curr_param_result = 1;
	}
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

MYLOG(DETAIL_LOG_LEVEL, "%p->SC_set_rowstart " FORMAT_LEN "->" FORMAT_LEN "(%s) ", stmt, stmt->rowset_start, start, valid_base ? "valid" : "unknown");
	if (res != NULL)
	{
		BOOL	valid = QR_has_valid_base(res);
MYPRINTF(DETAIL_LOG_LEVEL, ":(%p)QR is %s", res, QR_has_valid_base(res) ? "valid" : "unknown");

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
MYPRINTF(DETAIL_LOG_LEVEL, ":(%p)QR result=" FORMAT_LEN "(%s)", res, QR_get_rowstart_in_cache(res), QR_has_valid_base(res) ? "valid" : "unknown");
	}
	stmt->rowset_start = start;
MYPRINTF(DETAIL_LOG_LEVEL, ":stmt result=" FORMAT_LEN "\n", stmt->rowset_start);
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
SC_set_prepared(StatementClass *stmt, int prepared)
{
	if (prepared == stmt->prepared)
		;
	else if (NOT_YET_PREPARED == prepared && PREPARED_PERMANENTLY == stmt->prepared)
	{
		ConnectionClass *conn = SC_get_conn(stmt);

		if (conn)
		{
			ENTER_CONN_CS(conn);
			if (CONN_CONNECTED == conn->status)
			{
				if (CC_is_in_error_trans(conn))
				{
					CC_mark_a_object_to_discard(conn, 's',  stmt->plan_name);
				}
				else
				{
					QResultClass	*res;
					char dealloc_stmt[128];

					SPRINTF_FIXED(dealloc_stmt, "DEALLOCATE \"%s\"", stmt->plan_name);
					res = CC_send_query(conn, dealloc_stmt, NULL, IGNORE_ABORT_ON_CONN | ROLLBACK_ON_ERROR, NULL);
					QR_Destructor(res);
				}
			}
			LEAVE_CONN_CS(conn);
		}
	}
	if (NOT_YET_PREPARED == prepared)
		SC_set_planname(stmt, NULL);
	stmt->prepared = prepared;
}

/*
 * Initialize stmt_with_params and load_statement member pointer
 * deallocating corresponding prepared plan. Also initialize
 * statement member pointer if specified.
 */
RETCODE
SC_initialize_stmts(StatementClass *self, BOOL initializeOriginal)
{
	ProcessedStmt *pstmt;
	ProcessedStmt *next_pstmt;

	if (initializeOriginal)
	{
		if (self->statement)
		{
			free(self->statement);
			self->statement = NULL;
		}

		pstmt = self->processed_statements;
		while (pstmt)
		{
			if (pstmt->query)
				free(pstmt->query);
			next_pstmt = pstmt->next;
			free(pstmt);
			pstmt = next_pstmt;
		}
		self->processed_statements = NULL;

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
	 * We can dispose the result of Describe-only any time.
	 */
	if (self->prepare && self->status == STMT_DESCRIBED)
	{
		MYLOG(0, "self->prepare && self->status == STMT_DESCRIBED\n");
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

void
SC_reset_result_for_rerun(StatementClass *self)
{
	QResultClass	*res;
	ColumnInfoClass	*flds;

	if (!self)	return;
	if (res = SC_get_Result(self), NULL == res)
		return;
	flds = QR_get_fields(res);
	if (NULL == flds ||
	    0 == CI_get_num_fields(flds))
		SC_set_Result(self, NULL);
	else
	{
		QR_reset_for_re_execute(res);
		self->curr_param_result = 1;
		SC_set_Curres(self, NULL);
	}
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

	MYLOG(0, "entering self=%p\n", self);

	SC_clear_error(self);
	/* This would not happen */
	if (self->status == STMT_EXECUTING)
	{
		SC_set_error(self, STMT_SEQUENCE_ERROR, "Statement is currently executing a transaction.", func);
		return FALSE;
	}

	if (SC_get_conn(self)->unnamed_prepared_stmt == self)
		SC_get_conn(self)->unnamed_prepared_stmt = NULL;

	conn = SC_get_conn(self);
	switch (self->status)
	{
		case STMT_ALLOCATED:
			/* this statement does not need to be recycled */
			return TRUE;

		case STMT_READY:
			break;

		case STMT_DESCRIBED:
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
		case PREPARED_TEMPORARILY:
			/* Free the parsed table/field information */
			SC_initialize_cols_info(self, TRUE, TRUE);

MYLOG(DETAIL_LOG_LEVEL, "SC_clear_parse_status\n");
			SC_clear_parse_status(self, conn);
			break;
	}

	/* Free any cursors */
	if (SC_get_Result(self))
		SC_set_Result(self, NULL);
	self->miscinfo = 0;
	self->execinfo = 0;
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
MYLOG(DETAIL_LOG_LEVEL, "statement=%p ommitted=0\n", self);
	self->last_fetch_count = self->last_fetch_count_include_ommitted = 0;

	self->__error_message = NULL;
	self->__error_number = 0;

	self->lobj_fd = -1;

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
		ssize_t *next_cmd, SQLSMALLINT * pcpar,
		po_ind_t *multi_st, po_ind_t *proc_return)
{
	const	char *tstr, *tag = NULL;
	size_t	taglen = 0;
	char	tchar, bchar, escape_in_literal = '\0';
	char	in_literal = FALSE, in_ident_keyword = FALSE,
		in_dquote_identifier = FALSE,
		in_dollar_quote = FALSE, in_escape = FALSE,
		in_line_comment = FALSE, del_found = FALSE;
	int	comment_level = 0;
	po_ind_t multi = FALSE;
	SQLSMALLINT	num_p;
	encoded_str	encstr;

	MYLOG(0, "entering...\n");
	num_p = 0;
	if (proc_return)
		*proc_return = 0;
	if (next_cmd)
		*next_cmd = -1;
	tstr = query;
	make_encoded_str(&encstr, conn, tstr);
	for (bchar = '\0', tchar = encoded_nextchar(&encstr); tchar; tchar = encoded_nextchar(&encstr))
	{
		if (MBCS_NON_ASCII(encstr)) /* multibyte char */
		{
			if ((UCHAR) tchar >= 0x80)
				bchar = tchar;
			if (in_dquote_identifier ||
			    in_literal ||
			    in_dollar_quote ||
			    in_escape ||
			    in_line_comment ||
			    comment_level > 0)
				;
			else
				in_ident_keyword = TRUE;

			continue;
		}
		if (!multi && del_found)
		{
			if (IS_NOT_SPACE(tchar))
			{
				multi = TRUE;
				if (next_cmd)
					break;
			}
		}
		if (in_ident_keyword)
		{
			if (isalnum(tchar) ||
			    DOLLAR_QUOTE == tchar ||
			    '_' == tchar)
			{
				bchar = tchar;
				continue;
			}
			in_ident_keyword = FALSE;
		}

		if (in_dollar_quote)
		{
			if (tchar == DOLLAR_QUOTE)
			{
				if (strncmp((const char *) ENCODE_PTR(encstr), tag, taglen) == 0)
				{
					in_dollar_quote = FALSE;
					tag = NULL;
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
			else if (tchar == LITERAL_QUOTE)
				in_literal = FALSE;
		}
		else if (in_dquote_identifier)
		{
			if (tchar == IDENTIFIER_QUOTE)
				in_dquote_identifier = FALSE;
		}
		else if (in_line_comment)
		{
			if (PG_LINEFEED == tchar)
				in_line_comment = FALSE;
		}
		else if (comment_level > 0)
		{
			if ('/' == tchar && '*' == ENCODE_PTR(encstr)[1])
			{
				tchar = encoded_nextchar(&encstr);
				comment_level++;
			}
			else if ('*' == tchar && '/' == ENCODE_PTR(encstr)[1])
			{
				tchar = encoded_nextchar(&encstr);
				comment_level--;
			}
		}
		else if (isalnum(tchar))
			in_ident_keyword = TRUE;
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
					*next_cmd = encstr.pos;
			}
			else if (tchar == DOLLAR_QUOTE)
			{
				const char *ptr = (const char *) ENCODE_PTR(encstr);
				taglen = findTag(ptr, encstr.ccsc);
				if (taglen > 0)
				{
					in_dollar_quote = TRUE;
					tag = ptr;
					encoded_position_shift(&encstr, taglen - 1);
				}
			}
			else if (tchar == LITERAL_QUOTE)
			{
				in_literal = TRUE;
				escape_in_literal = CC_get_escape(conn);
				if (!escape_in_literal)
				{
					if (LITERAL_EXT == ENCODE_PTR(encstr)[-1])
						escape_in_literal = ESCAPE_IN_LITERAL;
				}
			}
			else if (tchar == IDENTIFIER_QUOTE)
				in_dquote_identifier = TRUE;
			else if ('-' == tchar)
			{
				if ('-' == ENCODE_PTR(encstr)[1])
				{
					tchar = encoded_nextchar(&encstr);
					in_line_comment = TRUE;
				}
			}
			else if ('/' == tchar)
			{
				if ('*' == ENCODE_PTR(encstr)[1])
				{
					tchar = encoded_nextchar(&encstr);
					comment_level++;
				}
			}
			if (IS_NOT_SPACE(tchar))
				bchar = tchar;
		}
	}
	if (pcpar)
		*pcpar = num_p;
	if (multi_st)
		*multi_st = multi;

	MYLOG(0, "leaving...num_p=%d multi=%d\n", num_p, multi);
}

/*
 * Describe the result set a statement will produce (for
 * SQLPrepare/SQLDescribeCol)
 *
 * returns # of fields if successful, -1 on error.
 */
Int4
SC_describe(StatementClass *self)
{
	Int4		num_fields = -1;
	QResultClass	*res;
	MYLOG(0, "entering status = %d\n", self->status);

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
MYLOG(0, "              preprocess: status = READY\n");

		self->miscinfo = 0;
		self->execinfo = 0;

		decideHowToPrepare(self, FALSE);
		switch (SC_get_prepare_method(self))
		{
			case NAMED_PARSE_REQUEST:
			case PARSE_TO_EXEC_ONCE:
				if (SQL_SUCCESS != prepareParameters(self, FALSE))
					return num_fields;
				break;
			case PARSE_REQ_FOR_INFO:
				if (SQL_SUCCESS != prepareParameters(self, FALSE))
					return num_fields;
				self->status = STMT_DESCRIBED;
				break;
			default:
				if (SQL_SUCCESS != prepareParameters(self, TRUE))
					return num_fields;
				self->status = STMT_DESCRIBED;
				break;
		}
		if (res = SC_get_Curres(self), NULL != res)
		{
			num_fields = QR_NumResultCols(res);
			return num_fields;
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
	memset(&self->localtime, 0, sizeof(self->localtime));
	self->localtime.tm_sec = -1;
	SC_unref_CC_error(self);
}


/*
 *	This function creates an error info which is the concatenation
 *	of the result, statement, connection, and socket messages.
 */

/*	Map sql commands to statement types */
static const struct
{
	int		number;
	const char ver3str[6];
	const char ver2str[6];
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
SC_create_errorinfo(const StatementClass *self, PG_ErrorInfo *pgerror_fail_safe)
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
			STRCPY_FIXED(msg, res->message);
			detailmsg = resmsg = TRUE;
		}
		else if (NULL != res->messageref)
		{
			STRCPY_FIXED(msg, res->messageref);
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

		snprintf(&msg[pos], sizeof(msg) - pos, "%s%s",
				 detailmsg ? ";\n" : "",
				 wmsg);
		ermsg = msg;
		detailmsg = TRUE;
	}
	if (!self->ref_CC_error)
		msgend = TRUE;

	if (conn && !msgend)
	{
		if (!resmsg && (wmsg = CC_get_errormsg(conn)) && wmsg[0] != '\0')
		{
			pos = strlen(msg);
			snprintf(&msg[pos], sizeof(msg) - pos,
					 ";\n%s", CC_get_errormsg(conn));
		}

		ermsg = msg;
	}
	pgerror = ER_Constructor(self->__error_number, ermsg);
	if (!pgerror)
	{
		if (pgerror_fail_safe)
		{
			memset(pgerror_fail_safe, 0, sizeof(*pgerror_fail_safe));
			pgerror = pgerror_fail_safe;
			pgerror->status = self->__error_number;
			pgerror->errorsize = sizeof(pgerror->__error_message);
			STRCPY_FIXED(pgerror->__error_message, ermsg); 
			pgerror->recsize = -1;
		}
		else
			return NULL;
	}
	if (sqlstate)
		STRCPY_FIXED(pgerror->sqlstate, sqlstate);
	else if (conn)
	{
		if (!msgend && conn->sqlstate[0])
			STRCPY_FIXED(pgerror->sqlstate, conn->sqlstate);
		else
		{
			EnvironmentClass *env = (EnvironmentClass *) CC_get_env(conn);

			errornum -= LOWEST_STMT_ERROR;
			if (errornum < 0 ||
				errornum >= sizeof(Statement_sqlstate) / sizeof(Statement_sqlstate[0]))
			{
				errornum = 1 - LOWEST_STMT_ERROR;
			}
			STRCPY_FIXED(pgerror->sqlstate, EN_is_odbc3(env) ?
				   Statement_sqlstate[errornum].ver3str :
				   Statement_sqlstate[errornum].ver2str);
		}
	}

	return pgerror;
}


StatementClass *SC_get_ancestor(StatementClass *stmt)
{
	StatementClass	*child = stmt, *parent;

MYLOG(DETAIL_LOG_LEVEL, "entering stmt=%p\n", stmt);
	for (child = stmt, parent = child->execute_parent; parent; child = parent, parent = child->execute_parent)
	{
		MYLOG(DETAIL_LOG_LEVEL, "parent=%p\n", parent);
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

MYLOG(DETAIL_LOG_LEVEL, "entering %p->%p check=%i\n", from_res ,self, check);
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
		STRCPY_FIXED(self_res->sqlstate, from_res->sqlstate);
}

void
SC_error_copy(StatementClass *self, const StatementClass *from, BOOL check)
{
	QResultClass	*self_res, *from_res;
	BOOL	repstate;

MYLOG(DETAIL_LOG_LEVEL, "entering %p->%p check=%i\n", from ,self, check);
	if (!from)		return;	/* for safety */
	if (self == from)	return; /* for safety */
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
		STRCPY_FIXED(self_res->sqlstate, from_res->sqlstate);
}


void
SC_full_error_copy(StatementClass *self, const StatementClass *from, BOOL allres)
{
	PG_ErrorInfo		*pgerror;

MYLOG(DETAIL_LOG_LEVEL, "entering %p->%p\n", from ,self);
	if (!from)		return;	/* for safety */
	if (self == from)	return; /* for safety */
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
	pgerror = SC_create_errorinfo(from, NULL);
	if (!pgerror || !pgerror->__error_message[0])
	{
		ER_Destructor(pgerror);
		return;
	}
	if (self->pgerror)
		ER_Destructor(self->pgerror);
	self->pgerror = pgerror;
}

/* Returns the next SQL error information. */
RETCODE		SQL_API
PGAPI_StmtError(SQLHSTMT	hstmt,
				SQLSMALLINT RecNumber,
				SQLCHAR * szSqlState,
				SQLINTEGER * pfNativeError,
				SQLCHAR * szErrorMsg,
				SQLSMALLINT cbErrorMsgMax,
				SQLSMALLINT * pcbErrorMsg,
				UWORD flag)
{
	/* CC: return an error of a hdesc  */
	PG_ErrorInfo		*pgerror, error;
	StatementClass *stmt = (StatementClass *) hstmt;
	int errnum = SC_get_errornumber(stmt);

	if (pgerror = SC_create_errorinfo(stmt, &error), NULL == pgerror)
		return SQL_NO_DATA_FOUND;
	if (pgerror != &error)
		stmt->pgerror = pgerror;
	if (STMT_NO_MEMORY_ERROR == errnum &&
	    !pgerror->__error_message[0])
		STRCPY_FIXED(pgerror->__error_message, "Memory Allocation Error??");
	return ER_ReturnError(pgerror, RecNumber, szSqlState,
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

struct tm *
SC_get_localtime(StatementClass *stmt)
{
#ifndef HAVE_LOCALTIME_R
	struct tm * tim;
#endif /* HAVE_LOCALTIME_R */

	if (stmt->localtime.tm_sec < 0)
	{
		SC_get_time(stmt);
#ifdef HAVE_LOCALTIME_R
		localtime_r(&stmt->stmt_time, &(stmt->localtime));
#else
		tim = localtime(&stmt->stmt_time);
		stmt->localtime = *tim;
#endif /* HAVE_LOCALTIME_R */
	}

	return &(stmt->localtime);
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
	KeySet		*keyset = NULL;

	/* TupleField *tupleField; */

MYLOG(DETAIL_LOG_LEVEL, "entering statement=%p res=%p ommitted=0\n", self, res);
	self->last_fetch_count = self->last_fetch_count_include_ommitted = 0;
	if (!res)
		return SQL_ERROR;
	coli = QR_get_fields(res);	/* the column info */

	MYLOG(0, "fetch_cursor=%d, %p->total_read=" FORMAT_LEN "\n", SC_is_fetchcursor(self), res, res->num_total_read);

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

		MYLOG(0, "**** : non-cursor_result\n");
		(self->currTuple)++;
	}
	else
	{
		/* read from the cache or the physical next tuple */
		retval = QR_next_tuple(res, self);
		if (retval < 0)
		{
			MYLOG(0, "**** : end_tuples\n");
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
			SC_set_errorinfo(self, res, 1);
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
MYLOG(DETAIL_LOG_LEVEL, "SC_ pstatus[" FORMAT_LEN "]=%hx fetch_count=" FORMAT_LEN "\n", kres_ridx, pstatus, self->last_fetch_count);
			if (0 != (pstatus & (CURS_SELF_DELETING | CURS_SELF_DELETED)))
				return SQL_SUCCESS_WITH_INFO;
			if (SQL_ROW_DELETED != (pstatus & KEYSET_INFO_PUBLIC) &&
				0 != (pstatus & CURS_OTHER_DELETED))
			{
				return SQL_SUCCESS_WITH_INFO;
			}
			if (0 != (CURS_NEEDS_REREAD & pstatus))
			{
				UWORD	qcount;

				result = SC_pos_reload(self, self->currTuple, &qcount, 0);
				if (SQL_ERROR == result)
					return result;
				pstatus &= ~CURS_NEEDS_REREAD;
			}
			keyset =  res->keyset + kres_ridx;
		}
	}

	num_cols = QR_NumPublicResultCols(res);

	result = SQL_SUCCESS;
	self->last_fetch_count++;
MYLOG(DETAIL_LOG_LEVEL, "stmt=%p ommitted++\n", self);
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
		SC_set_current_col(self, -1);
		SC_Create_bookmark(self, bookmark, self->bind_row, self->currTuple, keyset);
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
		MYLOG(0, "fetch: cols=%d, lf=%d, opts = %p, opts->bindings = %p, buffer[] = %p\n", num_cols, lf, opts, opts->bindings, opts->bindings[lf].buffer);

		/* reset for SQLGetData */
		GETDATA_RESET(gdata->gdata[lf]);

		if (NULL == opts->bindings)
			continue;
		if (opts->bindings[lf].buffer != NULL)
		{
			/* this column has a binding */

			/* type = QR_get_field_type(res, lf); */
			type = CI_get_oid(coli, lf);		/* speed things up */
			atttypmod = CI_get_atttypmod(coli, lf);	/* speed things up */

			MYLOG(0, "type = %d, atttypmod = %d\n", type, atttypmod);

			if (useCursor)
				value = QR_get_value_backend(res, lf);
			else
			{
				SQLLEN	curt = GIdx2CacheIdx(self->currTuple, self, res);
MYLOG(DETAIL_LOG_LEVEL, "%p->base=" FORMAT_LEN " curr=" FORMAT_LEN " st=" FORMAT_LEN " valid=%d\n", res, QR_get_rowstart_in_cache(res), self->currTuple, SC_get_rowset_start(self), QR_has_valid_base(res));
MYLOG(DETAIL_LOG_LEVEL, "curt=" FORMAT_LEN "\n", curt);
				value = QR_get_value_backend_row(res, curt, lf);
			}

			MYLOG(0, "value = '%s'\n", (value == NULL) ? "<NULL>" : value);

			retval = copy_and_convert_field_bindinfo(self, type, atttypmod, value, lf);

			MYLOG(0, "copy_and_convert: retval = %d\n", retval);

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
					MYLOG(DETAIL_LOG_LEVEL, "The %dth item was truncated\n", lf + 1);
					MYLOG(DETAIL_LOG_LEVEL, "The buffer size = " FORMAT_LEN, opts->bindings[lf].buflen);
					MYLOG(DETAIL_LOG_LEVEL, " and the value is '%s'\n", value);
					result = SQL_SUCCESS_WITH_INFO;
					break;

				case COPY_INVALID_STRING_CONVERSION:    /* invalid string */
					SC_set_error(self, STMT_STRING_CONVERSION_ERROR, "invalid string conversion occured.", func);
					result = SQL_ERROR;
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
	ConnectionClass *conn;
	IPDFields	*ipdopts;
	char		was_ok, was_nonfatal;
	QResultClass	*res = NULL;
	Int2		oldstatus,
				numcols;
	QueryInfo	qi;
	ConnInfo   *ci;
	unsigned int	qflag = 0;
	BOOL		is_in_trans, issue_begin, has_out_para;
	BOOL		use_extended_protocol;
	int		func_cs_count = 0, i;
	BOOL		useCursor, isSelectType;

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
		MYLOG(0, "problem with connection\n");
		goto cleanup;
	}
	is_in_trans = CC_is_in_trans(conn);
	if ((useCursor = SC_is_fetchcursor(self)))
	{
		QResultClass *curres = SC_get_Curres(self);

		if (NULL != curres &&
		    curres->dataFilled)
			useCursor = (NULL != QR_get_cursor(curres));
	}
	/* issue BEGIN ? */
	issue_begin = TRUE;
	if (!self->external)
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
	else if (self->statement_type == STMT_TYPE_SPECIAL)
	{
		/*
		 * Some utility commands like VACUUM cannot be run in a transaction
		 * block, so don't begin one even if auto-commit mode is disabled.
		 *
		 * An application should never issue an explicit BEGIN when
		 * auto-commit mode is disabled (probably not even when it's enabled,
		 * actually). We used to also suppress the implicit BEGIN when the
		 * statement was of STMT_TYPE_START type, ie. if the application
		 * issued an explicit BEGIN, but that actually seems like a bad idea.
		 * First of all, if you issue a BEGIN twice the backend will give a
		 * warning which can be helpful to spot mistakes in the application
		 * (because it shouldn't be doing that).
		 */
		issue_begin = FALSE;
	}
	if (issue_begin)
	{
		MYLOG(0, "   about to begin a transaction on statement = %p\n", self);
		qflag |= GO_INTO_TRANSACTION;
	}

	/*
	 * If the session query timeout setting differs from the statement one,
	 * change it.
	 */
	if (conn->stmt_timeout_in_effect != self->options.stmt_timeout)
	{
		char query[64];

		SPRINTF_FIXED(query, "SET statement_timeout = %d",
				 (int) self->options.stmt_timeout * 1000);
		res = CC_send_query(conn, query, NULL, 0, NULL);
		if (QR_command_maybe_successful(res))
			conn->stmt_timeout_in_effect = self->options.stmt_timeout;
		QR_Destructor(res);
	}

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
	if (self->stmt_with_params)
		use_extended_protocol = FALSE;
	else
	{
		use_extended_protocol = TRUE;
	}
	isSelectType = (SC_may_use_cursor(self) || self->statement_type == STMT_TYPE_PROCCALL);
	if (use_extended_protocol)
	{
		if (issue_begin)
			CC_begin(conn);

		res = libpq_bind_and_exec(self);
		if (!res)
		{
			if (SC_get_errornumber(self) <= 0)
			{
				SC_set_error(self, STMT_NO_RESPONSE, "Could not receive the response, communication down ??", func);
			}
			goto cleanup;
		}
	}
	else if (isSelectType)
	{
		char		fetch[128];
		const char *appendq = NULL;
		QueryInfo	*qryi = NULL;

		qflag |= (SQL_CONCUR_READ_ONLY != self->options.scroll_concurrency ? CREATE_KEYSET : 0);
		MYLOG(0, "       Sending SELECT statement on stmt=%p, cursor_name='%s' qflag=%d,%d\n", self, SC_cursor_name(self), qflag, self->options.scroll_concurrency);

		/* send the declare/select */
		if (useCursor)
		{
			qi.result_in = NULL;
			qi.cursor = SC_cursor_name(self);
			qi.fetch_size = qi.row_size = ci->drivers.fetch_max;
			SPRINTF_FIXED(fetch,
					 "fetch " FORMAT_LEN " in \"%s\"",
					 qi.fetch_size, SC_cursor_name(self));
			qryi = &qi;
			appendq = fetch;
			qflag &= (~READ_ONLY_QUERY); /* must be a SAVEPOINT after DECLARE */
		}
		res = SC_get_Result(self);
		if (self->curr_param_result && res)
			SC_set_Result(self, res->next);
		res = CC_send_query_append(conn, self->stmt_with_params, qryi, qflag, SC_get_ancestor(self), appendq);
		if (useCursor && QR_command_maybe_successful(res))
		{
			/*
			 * If we sent a DECLARE CURSOR + FETCH, throw away the result of
			 * the DECLARE CURSOR statement, and only return the result of the
			 * FETCH to the caller. However, if we received any NOTICEs as
			 * part of the DECLARE CURSOR, carry those over.
			 */
			if (appendq)
			{
				QResultClass	*qres, *nres;

				for (qres = res; qres;)
				{
					if (qres->command && strnicmp(qres->command, "fetch", 5) == 0)
					{
						break;
					}
					nres = qres->next;
					if (nres && QR_get_notice(qres) != NULL)
					{
						if (QR_command_successful(nres) &&
							QR_command_nonfatal(qres))
						{
							QR_set_rstatus(nres, PORES_NONFATAL_ERROR);
						}
						QR_add_notice(nres, QR_get_notice(qres));
					}
					qres->next = NULL;
					QR_Destructor(qres);
					qres = nres;

					/*
					 * If we received fewer rows than requested, there are no
					 * more rows to fetch.
					 */
					if (qres->num_cached_rows < qi.row_size)
						QR_set_reached_eof(qres);
				}
				res = qres;
			}
			if (res && SC_is_with_hold(self))
				QR_set_withhold(res);
		}
		MYLOG(0, "     done sending the query:\n");
	}
	else
	{
		/* not a SELECT statement so don't use a cursor */
		MYLOG(0, "      it's NOT a select statement: stmt=%p\n", self);
		res = CC_send_query(conn, self->stmt_with_params, NULL, qflag, SC_get_ancestor(self));
	}

	if (!isSelectType)
	{
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
			if (self->external && CC_does_autocommit(conn))
				CC_commit(conn);
		}
	}

	if (CONN_DOWN != conn->status)
		conn->status = oldstatus;
	self->status = STMT_FINISHED;
	LEAVE_INNER_CONN_CS(func_cs_count, conn);

	/* Check the status of the result */
	if (res)
	{
		was_ok = QR_command_successful(res);
		was_nonfatal = QR_command_nonfatal(res);

		if (0 < SC_get_errornumber(self))
			;
		else if (was_ok)
			SC_set_errornumber(self, STMT_OK);
		else if (was_nonfatal)
			SC_set_errornumber(self, STMT_INFO_ONLY);
		else
			SC_set_errorinfo(self, res, 0);
		/* set cursor before the first tuple in the list */
		self->currTuple = -1;
		SC_set_current_col(self, -1);
		SC_set_rowset_start(self, -1, FALSE);

		/* issue "ABORT" when query aborted */
		if (QR_get_aborted(res))
		{
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

MYLOG(DETAIL_LOG_LEVEL, "!!%p->miscinfo=%x res=%p\n", self, self->miscinfo, res);
			/*
			 * special handling of result for keyset driven cursors.
			 * Use the columns info of the 1st query and
			 * user the keyset info of the 2nd query.
			 */
			if (SQL_CURSOR_KEYSET_DRIVEN == self->options.cursor_type &&
			    SQL_CONCUR_READ_ONLY != self->options.scroll_concurrency &&
			    !useCursor)
			{
				if (tres = res->next, tres)
				{
					QR_set_fields(tres, QR_get_fields(res));
					QR_set_fields(res,  NULL);
					tres->num_fields = res->num_fields;
					res->next = NULL;
					QR_Destructor(res);
					SC_init_Result(self);
					SC_set_Result(self, tres);
					res = tres;
				}
			}
		}
	}
	else
	{
		/* Bad Error -- The error message will be in the Connection */
		if (!conn->pqconn)
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
	if (NULL == SC_get_Curres(self))
		SC_set_Curres(self, SC_get_Result(self));

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

MYLOG(DETAIL_LOG_LEVEL, "!!! numfield=%d field_type=%u\n", QR_NumResultCols(res), QR_get_field_type(res, 0));
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
			qi.cache_size = qi.row_size = ci->drivers.fetch_max;
			SPRINTF_FIXED(fetch, "fetch " FORMAT_LEN " in \"%s\"", qi.fetch_size, SC_cursor_name(self));
			res = CC_send_query(conn, fetch, &qi, qflag | READ_ONLY_QUERY, SC_get_ancestor(self));
			if (NULL != res)
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
MYLOG(DETAIL_LOG_LEVEL, "!!SC_fetch return =%d\n", ret);
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

MYLOG(DETAIL_LOG_LEVEL, "stmt=%p, func=%p, count=%d\n", stmt, func, stmt->num_callbacks);
	return stmt->num_callbacks;
}

RETCODE dequeueNeedDataCallback(RETCODE retcode, StatementClass *stmt)
{
	RETCODE			ret;
	NeedDataCallfunc	func;
	void			*data;
	int			i, cnt;

	MYLOG(0, "entering ret=%d count=%d\n", retcode, stmt->num_callbacks);
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
#define NULLCHECK(a) (a ? a : "(NULL)")
	if (self)
	{
		QResultClass *res = SC_get_Result(self);
		const ARDFields	*opts = SC_get_ARDF(self);
		const APDFields	*apdopts = SC_get_APDF(self);
		SQLLEN	rowsetSize;
		const int level = 9;

		rowsetSize = (STMT_TRANSITION_EXTENDED_FETCH == self->transition_status ? opts->size_of_rowset_odbc2 : opts->size_of_rowset);
		if (SC_get_errornumber(self) <= 0)
			head = "STATEMENT WARNING";
		else
		{
			head = "STATEMENT ERROR";
			QLOG(level, "%s: func=%s, desc='%s', errnum=%d, errmsg='%s'\n",head, func, desc, self->__error_number, NULLCHECK(self->__error_message));
		}
		MYLOG(0, "%s: func=%s, desc='%s', errnum=%d, errmsg='%s'\n", head, func, desc, self->__error_number, NULLCHECK(self->__error_message));
		if (SC_get_errornumber(self) > 0)
		{
			QLOG(level, "                 ------------------------------------------------------------\n");
			QLOG(level, "                 hdbc=%p, stmt=%p, result=%p\n", self->hdbc, self, res);
			QLOG(level, "                 prepare=%d, external=%d\n", self->prepare, self->external);
			QLOG(level, "                 bindings=%p, bindings_allocated=%d\n", opts->bindings, opts->allocated);
			QLOG(level, "                 parameters=%p, parameters_allocated=%d\n", apdopts->parameters, apdopts->allocated);
			QLOG(level, "                 statement_type=%d, statement='%s'\n", self->statement_type, NULLCHECK(self->statement));
			QLOG(level, "                 stmt_with_params='%s'\n", NULLCHECK(self->stmt_with_params));
			QLOG(level, "                 data_at_exec=%d, current_exec_param=%d, put_data=%d\n", self->data_at_exec, self->current_exec_param, self->put_data);
			QLOG(level, "                 currTuple=" FORMAT_LEN ", current_col=%d, lobj_fd=%d\n", self->currTuple, self->current_col, self->lobj_fd);
			QLOG(level, "                 maxRows=" FORMAT_LEN ", rowset_size=" FORMAT_LEN ", keyset_size=" FORMAT_LEN ", cursor_type=%u, scroll_concurrency=%d\n", self->options.maxRows, rowsetSize, self->options.keyset_size, self->options.cursor_type, self->options.scroll_concurrency);
			QLOG(level, "                 cursor_name='%s'\n", SC_cursor_name(self));

			QLOG(level, "                 ----------------QResult Info -------------------------------\n");

			if (res)
			{
				QLOG(level, "                 fields=%p, backend_tuples=%p, tupleField=%p, conn=%p\n", QR_get_fields(res), res->backend_tuples, res->tupleField, res->conn);
				QLOG(level, "                 fetch_count=" FORMAT_LEN ", num_total_rows=" FORMAT_ULEN ", num_fields=%d, cursor='%s'\n", res->fetch_number, QR_get_num_total_tuples(res), res->num_fields, NULLCHECK(QR_get_cursor(res)));
				QLOG(level, "                 message='%s', command='%s', notice='%s'\n", NULLCHECK(QR_get_message(res)), NULLCHECK(res->command), NULLCHECK(res->notice));
				QLOG(level, "                 status=%d\n", QR_get_rstatus(res));
			}

			/* Log the connection error if there is one */
			CC_log_error(func, desc, self->hdbc);
		}
	}
	else
	{
		MYLOG(0, "INVALID STATEMENT HANDLE ERROR: func=%s, desc='%s'\n", func, desc);
	}
}

/*
 *	Extended Query
 */

static BOOL
RequestStart(StatementClass *stmt, ConnectionClass *conn, const char *func)
{
	BOOL	ret = TRUE;
	unsigned int	svpopt = 0;

#ifdef	_HANDLE_ENLIST_IN_DTC_
	if (conn->asdum)
		CALL_IsolateDtcConn(conn, TRUE);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
	if (NULL == conn->pqconn)
        {
		SC_set_error(stmt, STMT_COMMUNICATION_ERROR, "The connection has been lost", __FUNCTION__);
		return SQL_ERROR;
	}
	if (CC_started_rbpoint(conn))
		return TRUE;
	if (SC_is_readonly(stmt))
		svpopt |= SVPOPT_RDONLY;
	if (SQL_ERROR == SetStatementSvp(stmt, svpopt))
	{
		char	emsg[128];

		SPRINTF_FIXED(emsg, "internal savepoint error in %s", func);
		SC_set_error_if_not_set(stmt, STMT_INTERNAL_ERROR, emsg, func);
		return FALSE;
	}

	/*
	 * In auto-commit mode, begin a new transaction implicitly if no
	 * transaction is in progress yet. However, some special statements like
	 * VACUUM and CLUSTER cannot be run in a transaction block.
	 */
	if (!CC_is_in_trans(conn) && CC_loves_visible_trans(conn) &&
		stmt->statement_type != STMT_TYPE_SPECIAL)
	{
		ret = CC_begin(conn);
	}
	return ret;
}

static void log_params(int nParams, const Oid *paramTypes, const UCHAR * const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat)
{
	int	i, j;
	BOOL	isBinary;

	for (i = 0; i < nParams; i++)
	{
		isBinary = paramFormats ? paramFormats[i] : FALSE;
		if (!paramValues[i])
			QLOG(TUPLE_LOG_LEVEL, "\t%c (null) OID=%u\n", isBinary ? 'b' : 't', paramTypes ? paramTypes[i] : 0);
		else if (isBinary)
		{
			QLOG(TUPLE_LOG_LEVEL, "\tb '");
			for (j = 0; j < paramLengths[i]; j++)
				QPRINTF(TUPLE_LOG_LEVEL, "%02x", paramValues[i][j]);
			QPRINTF(TUPLE_LOG_LEVEL, " OID=%u\n", paramTypes ? paramTypes[i] : 0);
		}
		else
			QLOG(TUPLE_LOG_LEVEL, "\tt '%s' OID=%u\n", paramValues[i], paramTypes ? paramTypes[i] : 0);
	}
}

static QResultClass *
libpq_bind_and_exec(StatementClass *stmt)
{
	CSTR		func = "libpq_bind_and_exec";
	ConnectionClass	*conn = SC_get_conn(stmt);
	int			nParams;
	Oid		   *paramTypes = NULL;
	char	  **paramValues = NULL;
	int		   *paramLengths = NULL;
	int		   *paramFormats = NULL;
	int			resultFormat;
	PGresult   *pgres = NULL;
	int			pgresstatus;
	QResultClass	*newres = NULL;
	QResultClass *res = NULL;
	char	   *cmdtag;
	char	   *rowcount;

	if (!RequestStart(stmt, conn, func))
		return NULL;

#ifdef	NOT_USED
	if (CC_is_in_trans(conn) && !CC_started_rbpoint(conn))
	{
		if (SQL_ERROR == SetStatementSvp(stmt, 0))
		{
			SC_set_error_if_not_set(stmt, STMT_INTERNAL_ERROR, "internal savepoint error in build_libpq_bind_params", func);
			return NULL;
		}
	}
#endif /* NOT_USED */

	/* 1. Bind */
	MYLOG(0, "bind stmt=%p\n", stmt);
	if (!build_libpq_bind_params(stmt,
								 &nParams,
								 &paramTypes,
								 &paramValues,
								 &paramLengths, &paramFormats,
								 &resultFormat))
	{
		if (SC_get_errornumber(stmt) <= 0)
			SC_set_errornumber(stmt, STMT_NO_MEMORY_ERROR);
		goto cleanup;
	}

	/* 2. Execute */
	MYLOG(0, "execute stmt=%p\n", stmt);
	if (!SC_is_fetchcursor(stmt))
	{
		if (stmt->prepared == NOT_YET_PREPARED ||
			(stmt->prepared == PREPARED_TEMPORARILY && conn->unnamed_prepared_stmt != stmt))
		{
			SC_set_error(stmt, STMT_EXEC_ERROR, "about to execute a non-prepared statement", func);
			goto cleanup;
		}
	}

	/* 2.5 Prepare and Describe if needed */
	if (stmt->prepared == PREPARING_TEMPORARILY ||
		(stmt->prepared == PREPARED_TEMPORARILY && conn->unnamed_prepared_stmt != stmt))
	{
		ProcessedStmt *pstmt;

		if (!stmt->processed_statements)
		{
			if (prepareParametersNoDesc(stmt, FALSE, EXEC_PARAM_CAST) == SQL_ERROR)
				goto cleanup;
		}

		pstmt = stmt->processed_statements;
		QLOG(0, "PQexecParams: %p '%s' nParams=%d\n", conn->pqconn, pstmt->query, nParams);
		log_params(nParams, paramTypes, (const UCHAR * const *) paramValues, paramLengths, paramFormats, resultFormat);
		pgres = PQexecParams(conn->pqconn,
							 pstmt->query,
							 nParams,
							 paramTypes,
							 (const char **) paramValues,
							 paramLengths,
							 paramFormats,
							 resultFormat);
	}
	else
	{
		const char *plan_name;

		if (stmt->prepared == PREPARING_PERMANENTLY)
		{
			if (prepareParameters(stmt, FALSE) == SQL_ERROR)
				goto cleanup;
		}

		/* prepareParameters() set plan name, so don't fetch this earlier */
		plan_name = stmt->plan_name ? stmt->plan_name : NULL_STRING;

		/* already prepared */
		QLOG(0, "PQexecPrepared: %p plan=%s nParams=%d\n", conn->pqconn, plan_name, nParams);
		log_params(nParams, paramTypes, (const UCHAR * const *) paramValues, paramLengths, paramFormats, resultFormat);
		pgres = PQexecPrepared(conn->pqconn,
							   plan_name, 	/* portal name == plan name */
							   nParams,
							   (const char **) paramValues, paramLengths, paramFormats,
							   resultFormat);
	}
	if (stmt->curr_param_result)
	{
		for (res = SC_get_Result(stmt); NULL != res && NULL != res->next; res = res->next) ;
	}
	else
		res = NULL;

	if (!res)
	{
		newres = res = QR_Constructor();
		if (!res)
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Out of memory while allocating result set", func);
			goto cleanup;
		}
	}

	/* 3. Receive results */
MYLOG(DETAIL_LOG_LEVEL, "get_Result=%p %p %d\n", res, SC_get_Result(stmt), stmt->curr_param_result);
	pgresstatus = PQresultStatus(pgres);
	switch (pgresstatus)
	{
		case PGRES_COMMAND_OK:
			/* portal query command, no tuples returned */
			/* read in the return message from the backend */
			cmdtag = PQcmdStatus(pgres);
			QLOG(0, "\tok: - 'C' - %s\n", cmdtag);
			QR_set_command(res, cmdtag);
			if (QR_command_successful(res))
				QR_set_rstatus(res, PORES_COMMAND_OK);

			/* get rowcount */
			rowcount = PQcmdTuples(pgres);
			if (rowcount && rowcount[0])
				res->recent_processed_row_count = atoi(rowcount);
			else
				res->recent_processed_row_count = -1;
			break;

		case PGRES_EMPTY_QUERY:
			/* We return the empty query */
			QR_set_rstatus(res, PORES_EMPTY_QUERY);
			break;
		case PGRES_NONFATAL_ERROR:
			handle_pgres_error(conn, pgres, "libpq_bind_and_exec", res, FALSE);
			break;

		case PGRES_BAD_RESPONSE:
		case PGRES_FATAL_ERROR:
			handle_pgres_error(conn, pgres, "libpq_bind_and_exec", res, TRUE);
			break;
		case PGRES_TUPLES_OK:
			if (!QR_from_PGresult(res, stmt, conn, NULL, &pgres))
				goto cleanup;
			if (res->rstatus == PORES_TUPLES_OK && res->notice)
				QR_set_rstatus(res, PORES_NONFATAL_ERROR);
			break;
		case PGRES_COPY_OUT:
		case PGRES_COPY_IN:
		case PGRES_COPY_BOTH:
		default:
			/* skip the unexpected response if possible */
			QR_set_rstatus(res, PORES_BAD_RESPONSE);
			CC_set_error(conn, CONNECTION_BACKEND_CRAZY, "Unexpected protocol character from backend (send_query)", func);
			CC_on_abort(conn, CONN_DEAD);

			QLOG(0, "PQexecXxxx error: - (%d) - %s\n", pgresstatus, CC_get_errormsg(conn));
			break;
	}

	if (res != newres && NULL != newres)
		QR_Destructor(newres);

cleanup:
	if (pgres)
		PQclear(pgres);
	if (paramValues)
	{
		int			i;
		for (i = 0; i < nParams; i++)
		{
			if (paramValues[i] != NULL)
				free(paramValues[i]);
		}
		free(paramValues);
	}
	if (paramTypes)
		free(paramTypes);
	if (paramLengths)
		free(paramLengths);
	if (paramFormats)
		free(paramFormats);

	return res;
}

/*
 * Parse a query using libpq.
 *
 * 'res' is only passed here for error reporting purposes. If an error is
 * encountered, it is set in 'res', and the function returns FALSE.
 */
static BOOL
ParseWithLibpq(StatementClass *stmt, const char *plan_name,
			   const char *query,
			   Int2 num_params, const char *comment, QResultClass *res)
{
	CSTR	func = "ParseWithLibpq";
	ConnectionClass	*conn = SC_get_conn(stmt);
	Int4		sta_pidx = -1, end_pidx = -1;
	const char	*cstatus;
	Oid		   *paramTypes = NULL;
	BOOL		retval = FALSE;
	PGresult   *pgres = NULL;

	MYLOG(0, "entering plan_name=%s query=%s\n", plan_name, query);
	if (!RequestStart(stmt, conn, func))
		return FALSE;

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
			{
				if (0 == num_params)
					sta_pidx = pidx;
				num_params++;
			}
			else
			{
				num_params++;
				break;
			}
		}
MYLOG(0, "sta_pidx=%d end_pidx=%d num_p=%d\n", sta_pidx, end_pidx, num_params);
	}

	/*
	 * We let the server deduce the right datatype for the parameters, except
	 * for out parameters, which are sent as VOID.
	 */
	if (num_params > 0)
	{
		int	i;
		int j;
		IPDFields	*ipdopts = SC_get_IPDF(stmt);

		paramTypes = malloc(sizeof(Oid) * num_params);
		if (paramTypes == NULL)
		{
			SC_set_errornumber(stmt, STMT_NO_MEMORY_ERROR);
			goto cleanup;
		}

		MYLOG(0, "ipdopts->allocated: %d\n", ipdopts->allocated);
		j = 0;
		for (i = sta_pidx; i <= end_pidx; i++)
		{
			if (i < ipdopts->allocated)
			{
				if (SQL_PARAM_OUTPUT == ipdopts->parameters[i].paramType)
					paramTypes[j++] = PG_TYPE_VOID;
				else
					paramTypes[j++] = sqltype_to_bind_pgtype(conn,
															 ipdopts->parameters[i].SQLType);
			}
			else
			{
				/* Unknown type of parameter. Let the server decide */
				paramTypes[j++] = 0;
			}
		}
	}

	if (plan_name == NULL || plan_name[0] == '\0')
		conn->unnamed_prepared_stmt = NULL;

	/* Prepare */
	QLOG(0, "PQprepare: %p '%s' plan=%s nParams=%d\n", conn->pqconn, query, plan_name, num_params);
	pgres = PQprepare(conn->pqconn, plan_name, query, num_params, paramTypes);
	if (PQresultStatus(pgres) != PGRES_COMMAND_OK)
	{
		handle_pgres_error(conn, pgres, "ParseWithlibpq", res, TRUE);
		goto cleanup;
	}
	cstatus = PQcmdStatus(pgres);
	QLOG(0, "\tok: - 'C' - %s\n", cstatus);
	if (stmt->plan_name)
		SC_set_prepared(stmt, PREPARED_PERMANENTLY);
	else
		SC_set_prepared(stmt, PREPARED_TEMPORARILY);

	if (plan_name == NULL || plan_name[0] == '\0')
		conn->unnamed_prepared_stmt = stmt;

	retval = TRUE;

cleanup:
	if (paramTypes)
		free(paramTypes);

	if (pgres)
		PQclear(pgres);

	return retval;
}


/*
 * Parse and describe a query using libpq.
 *
 * Returns an empty result set that has the column information, or error code
 * and message, filled in. If 'res' is not NULL, it is the result set
 * returned, otherwise a new one is allocated.
 *
 * NB: The caller must set stmt->current_exec_param before calling this
 * function!
 */
QResultClass *
ParseAndDescribeWithLibpq(StatementClass *stmt, const char *plan_name,
						  const char *query_param,
						  Int2 num_params, const char *comment,
						  QResultClass *res)
{
	CSTR	func = "ParseAndDescribeWithLibpq";
	ConnectionClass	*conn = SC_get_conn(stmt);
	PGresult   *pgres = NULL;
	int			num_p;
	Int2		num_discard_params;
	IPDFields	*ipdopts;
	int			pidx;
	int			i;
	Oid			oid;
	SQLSMALLINT paramType;

	MYLOG(0, "entering plan_name=%s query=%s\n", plan_name, query_param);
	if (!RequestStart(stmt, conn, func))
		return NULL;

	if (!res)
		res = QR_Constructor();
	if (!res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for query", func);
		return NULL;
	}

	/*
	 * We need to do Prepare + Describe as two different round-trips to the
	 * server, while before we switched to use libpq, we used to send a Parse
	 * and Describe message followed by a single Sync.
	 */
	if (!ParseWithLibpq(stmt, plan_name, query_param, num_params, comment, res))
		goto cleanup;

	/* Describe */
	QLOG(0, "\tPQdescribePrepared: %p plan_name=%s\n", conn->pqconn, plan_name);

	pgres = PQdescribePrepared(conn->pqconn, plan_name);
	switch (PQresultStatus(pgres))
	{
		case PGRES_COMMAND_OK:
			QLOG(0, "\tok: - 'C' - %s\n", PQcmdStatus(pgres));
			/* expected */
			break;
		case PGRES_NONFATAL_ERROR:
			handle_pgres_error(conn, pgres, "ParseAndDescribeWithLibpq", res, FALSE);
			goto cleanup;
		case PGRES_FATAL_ERROR:
			handle_pgres_error(conn, pgres, "ParseAndDescribeWithLibpq", res, TRUE);
			goto cleanup;
		default:
			/* skip the unexpected response if possible */
			CC_set_error(conn, CONNECTION_BACKEND_CRAZY, "Unexpected result from PQdescribePrepared", func);
			CC_on_abort(conn, CONN_DEAD);

			MYLOG(0, "PQdescribePrepared: error - %s\n", CC_get_errormsg(conn));
			goto cleanup;
	}

	/* Extract parameter information from the result set */
	num_p = PQnparams(pgres);
MYLOG(DETAIL_LOG_LEVEL, "num_params=%d info=%d\n", stmt->num_params, num_p);
	if (get_qlog() > 0 || get_mylog() > 0)
	{
		int	i;

		QLOG(0, "\tnParams=%d", num_p);
		for (i = 0; i < num_p; i++)
			QPRINTF(0, " %u", PQparamtype(pgres, i));
		QPRINTF(0, "\n");
	}
	num_discard_params = 0;
	if (stmt->discard_output_params)
		CountParameters(stmt, NULL, NULL, &num_discard_params);
	if (num_discard_params < stmt->proc_return)
		num_discard_params = stmt->proc_return;
	if (num_p + num_discard_params != (int) stmt->num_params)
	{
		MYLOG(0, "ParamInfo unmatch num_params(=%d) != info(=%d)+discard(=%d)\n", stmt->num_params, num_p, num_discard_params);
		/* stmt->num_params = (Int2) num_p + num_discard_params; it's possible in case of multi command queries */
	}
	ipdopts = SC_get_IPDF(stmt);
	extend_iparameter_bindings(ipdopts, stmt->num_params);
	pidx = stmt->current_exec_param;
	if (pidx >= 0)
		pidx--;
	for (i = 0; i < num_p; i++)
	{
		SC_param_next(stmt, &pidx, NULL, NULL);
		if (pidx >= stmt->num_params)
		{
			MYLOG(0, "%dth parameter's position(%d) is out of bound[%d]\n", i, pidx, stmt->num_params);
			break;
		}
		oid = PQparamtype(pgres, i);
		paramType = ipdopts->parameters[pidx].paramType;
		if (SQL_PARAM_OUTPUT != paramType ||
			PG_TYPE_VOID != oid)
			PIC_set_pgtype(ipdopts->parameters[pidx], oid);
	}

	/* Extract Portal information */
	QR_set_conn(res, conn);

	if (CI_read_fields_from_pgres(QR_get_fields(res), pgres))
	{
		Int2	dummy1, dummy2;
		int	cidx;
		int num_io_params;

		QR_set_rstatus(res, PORES_FIELDS_OK);
		res->num_fields = CI_get_num_fields(QR_get_fields(res));
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
					MYLOG(DETAIL_LOG_LEVEL, "!![%d].PGType %u->%u\n", i, PIC_get_pgtype(ipdopts->parameters[i]), CI_get_oid(QR_get_fields(res), cidx));
					PIC_set_pgtype(ipdopts->parameters[i], CI_get_oid(QR_get_fields(res), cidx));
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
	}

cleanup:
	if (pgres)
		PQclear(pgres);

	return res;
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

static void
SC_set_error_if_not_set(StatementClass *self, int errornumber, const char *errmsg, const char *func)
{
	int	errnum = SC_get_errornumber(self);

	if (errnum <= 0)
	{
		const char *emsg = SC_get_errormsg(self);

		if (emsg && 0 == errnum)
			SC_set_errornumber(self, errornumber);
		else
			SC_set_error(self, errornumber, errmsg, func);
	}
}

static void
SC_set_errorinfo(StatementClass *self, QResultClass *res, int errkind)
{
	ConnectionClass *conn = SC_get_conn(self);

	if (CC_not_connected(conn))
	{
		SC_set_error_if_not_set(self, STMT_COMMUNICATION_ERROR, "The connection has been lost", __FUNCTION__);
		return;
	}

	switch (QR_get_rstatus(res))
	{
		case PORES_NO_MEMORY_ERROR:
			SC_set_error_if_not_set(self, STMT_NO_MEMORY_ERROR, "memory allocation error???", __FUNCTION__);
			break;
		case PORES_BAD_RESPONSE:
			SC_set_error_if_not_set(self, STMT_COMMUNICATION_ERROR, "communication error occured", __FUNCTION__);
			break;
		case PORES_INTERNAL_ERROR:
			SC_set_error_if_not_set(self, STMT_INTERNAL_ERROR, "Internal error fetching next row", __FUNCTION__);
			break;
		default:
			switch (errkind)
			{
				case 1:
					SC_set_error_if_not_set(self, STMT_EXEC_ERROR, "Error while fetching the next result", __FUNCTION__);
					break;
				default:
					SC_set_error_if_not_set(self, STMT_EXEC_ERROR, "Error while executing the query", __FUNCTION__);
					break;
			}
			break;
	}
}

/*
 *	Before 9.6, the driver offered very simple bookmark support -- it is just
 *	the current row number.
 *	Now the driver offers more verbose bookmarks which contain KeySet informations
 *	(CTID (+ OID)). Though they consume 12bytes(row number + CTID) or 16bytes
 *	(row number + CTID + OID), they are useful in declare/fetch mode.
 */

PG_BM	SC_Resolve_bookmark(const ARDFields *opts, Int4 idx)
{
	BindInfoClass	*bookmark;
	SQLLEN		*used;
	SQLULEN		offset;
	SQLUINTEGER	bind_size;
	size_t	cpylen = sizeof(Int4);
	PG_BM	pg_bm;

	bookmark = opts->bookmark;
	offset = opts->row_offset_ptr ? *(opts->row_offset_ptr) : 0;
	bind_size = opts->bind_size;
	memset(&pg_bm, 0, sizeof(pg_bm));
	if (used = bookmark->used, used != NULL)
	{
		used = LENADDR_SHIFT(used, offset);
		if (bind_size > 0)
			used = LENADDR_SHIFT(used, idx * bind_size);
		else
			used = LENADDR_SHIFT(used, idx * sizeof(SQLLEN));
		if (*used >= sizeof(pg_bm))
			cpylen = sizeof(pg_bm);
		else if (*used >= 12)
			cpylen = 12;
		MYLOG(0, "used=" FORMAT_LEN " cpylen=" FORMAT_SIZE_T "\n", *used, cpylen);
	}
	memcpy(&pg_bm, CALC_BOOKMARK_ADDR(bookmark, offset, bind_size, idx), cpylen);
MYLOG(0, "index=%d block=%d off=%d\n", pg_bm.index, pg_bm.keys.blocknum, pg_bm.keys.offset);
	pg_bm.index = SC_resolve_int4_bookmark(pg_bm.index);

	return pg_bm;
}

int	SC_Create_bookmark(StatementClass *self, BindInfoClass *bookmark, Int4 bind_row, Int4 currTuple, const KeySet *keyset)
{
	ARDFields	*opts = SC_get_ARDF(self);
	SQLUINTEGER	bind_size = opts->bind_size;
	SQLULEN		offset = opts->row_offset_ptr ? *opts->row_offset_ptr : 0;
	size_t		cvtlen = sizeof(Int4);
	PG_BM		pg_bm;

MYLOG(0, "entering type=%d buflen=" FORMAT_LEN " buf=%p\n", bookmark->returntype, bookmark->buflen, bookmark->buffer);
	memset(&pg_bm, 0, sizeof(pg_bm));
	if (SQL_C_BOOKMARK == bookmark->returntype)
		;
	else if (bookmark->buflen >= sizeof(pg_bm))
		cvtlen = sizeof(pg_bm);
	else if (bookmark->buflen >= 12)
		cvtlen = 12;
	pg_bm.index = SC_make_int4_bookmark(currTuple);
	if (keyset)
		pg_bm.keys  = *keyset;
	memcpy(CALC_BOOKMARK_ADDR(bookmark, offset, bind_size, bind_row), &pg_bm, cvtlen);
	if (bookmark->used)
	{
		SQLLEN *used = LENADDR_SHIFT(bookmark->used, offset);

		if (bind_size > 0)
			used = LENADDR_SHIFT(used, bind_row * bind_size);
		else
			used = LENADDR_SHIFT(used, bind_row * sizeof(SQLLEN));
		*used = cvtlen;
	}
MYLOG(0, "leaving cvtlen=" FORMAT_SIZE_T " ix(bl,of)=%d(%d,%d)\n", cvtlen, pg_bm.index, pg_bm.keys.blocknum, pg_bm.keys.offset);

	return COPY_OK;
}
