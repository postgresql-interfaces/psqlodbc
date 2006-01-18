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

#include "statement.h"

#include "bind.h"
#include "connection.h"
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
		STMT_TYPE_EXECUTE, "DEALLOCATE"
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
		0, NULL
	}
};


RETCODE		SQL_API
PGAPI_AllocStmt(HDBC hdbc,
				HSTMT FAR * phstmt)
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

	mylog("**** PGAPI_AllocStmt: hdbc = %x, stmt = %x\n", hdbc, stmt);

	if (!stmt)
	{
		CC_set_error(conn, CONN_STMT_ALLOC_ERROR, "No more memory to allocate a further SQL-statement", func);
		*phstmt = SQL_NULL_HSTMT;
		return SQL_ERROR;
	}

	if (!CC_add_statement(conn, stmt))
	{
		CC_set_error(conn, CONN_STMT_ALLOC_ERROR, "Maximum number of connections exceeded.", func);
		SC_Destructor(stmt);
		*phstmt = SQL_NULL_HSTMT;
		return SQL_ERROR;
	}

	*phstmt = (HSTMT) stmt;

	/* Copy default statement options based from Connection options */
	stmt->options = stmt->options_orig = conn->stmtOptions;
	stmt->ardi.ardopts = conn->ardOptions;
	ardopts = SC_get_ARDF(stmt);
	bookmark = ARD_AllocBookmark(ardopts);

	stmt->stmt_size_limit = CC_get_max_query_len(conn);
	/* Save the handle for later */
	stmt->phstmt = phstmt;

	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_FreeStmt(HSTMT hstmt,
			   UWORD fOption)
{
	CSTR func = "PGAPI_FreeStmt";
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("%s: entering...hstmt=%x, fOption=%d\n", func, hstmt, fOption);

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

			if (!CC_remove_statement(conn, stmt))
			{
				SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Statement is currently executing a transaction.", func);
				return SQL_ERROR;		/* stmt may be executing a
										 * transaction */
			}

			/* Free any cursors and discard any result info */
			res = SC_get_Result(stmt);
			QR_Destructor(res);
			SC_init_Result(stmt);
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
		stmt->transition_status = 0;
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
		rv->plan_name = NULL;
		rv->transition_status = 0;
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
		rv->last_fetch_count = rv->last_fetch_count_include_ommitted = 0;
		rv->save_rowset_size = -1;

		rv->data_at_exec = -1;
		rv->current_exec_param = -1;
		rv->exec_start_row = -1;
		rv->exec_end_row = -1;
		rv->exec_current_row = -1;
		rv->put_data = FALSE;

		rv->lobj_fd = -1;
		INIT_NAME(rv->cursor_name);

		/* Parse Stuff */
		rv->ti = NULL;
		rv->ntab = 0;
		rv->num_key_fields = -1; /* unknown */
		SC_clear_parse_status(rv, conn);
		rv->proc_return = -1;
		SC_init_discard_output_params(rv);

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
		rv->updatable = FALSE;
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
	mylog("SC_Destructor: self=%x, self->result=%x, self->hdbc=%x\n", self, res, self->hdbc);
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
	if (self->ti)
	{
		TI_Destructor(self->ti, self->ntab);

		free(self->ti);
		self->ti = NULL;
	}

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
SC_set_rowset_start(StatementClass *stmt, int start, BOOL valid_base)
{
	QResultClass	*res = SC_get_Curres(stmt);
	Int4	incr = start - stmt->rowset_start;

inolog("%x->SC_set_rowstart %d->%d(%s) ", stmt, stmt->rowset_start, start, valid_base ? "valid" : "unknown");
	if (res != NULL)
	{
		BOOL	valid = QR_has_valid_base(res);
inolog(":QR is %s", QR_has_valid_base(res) ? "valid" : "unknown");

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
				QR_set_rowstart_in_cache(res, 0);
		}
		if (!QR_get_cursor(res))
			res->key_base = start;
inolog(":QR result=%d(%s)", QR_get_rowstart_in_cache(res), QR_has_valid_base(res) ? "valid" : "unknown");
	}
	stmt->rowset_start = start;
inolog(":stmt result=%d\n", stmt->rowset_start);
}
void
SC_inc_rowset_start(StatementClass *stmt, int inc)
{
	Int4	start = stmt->rowset_start + inc;
	
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
	if (res = SC_get_Result(self), NULL != res)
	{
		if (res->backend_tuples)
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

	mylog("%s: self= %x\n", func, self);

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
			if (!CC_is_in_autocommit(conn) && CC_is_in_trans(conn))
			{
				if (SC_is_pre_executable(self) && !conn->connInfo.disallow_premature)
					CC_abort(conn);
			}
			break;

		case STMT_FINISHED:
			break;

		default:
			SC_set_error(self, STMT_INTERNAL_ERROR, "An internal error occured while recycling statements", func);
			return FALSE;
	}

	if (NOT_YET_PREPARED == self->prepared)
	{
        	/* Free the parsed table information */
		if (self->ti)
		{
			TI_Destructor(self->ti, self->ntab);
			free(self->ti);
			self->ti = NULL;
			self->ntab = 0;
		}
		/* Free the parsed field information */
		DC_Destructor((DescriptorClass *) SC_get_IRD(self));

inolog("SC_clear_parse_status\n");
		SC_clear_parse_status(self, conn);
		self->updatable = FALSE;
	}

	/* Free any cursors */
	if (res = SC_get_Result(self), res)
	{
		if (PREPARED_PERMANENTLY == self->prepared)
			QR_close_result(res, FALSE);
		else
		{
			QR_Destructor(res);
			SC_init_Result(self);
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
inolog("%s statement=%x ommitted=0\n", func, self);
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
 * Pre-execute a statement (for SQLPrepare/SQLDescribeCol) 
 */
Int4	/* returns # of fields if successful */
SC_pre_execute(StatementClass *self)
{
	Int4		num_fields = -1;
	QResultClass	*res;
	mylog("SC_pre_execute: status = %d\n", self->status);

	res = SC_get_Curres(self);
	if (res && (num_fields = QR_NumResultCols(res)) > 0)
		return num_fields;
	if (self->status == STMT_READY)
	{
		mylog("              preprocess: status = READY\n");

		self->miscinfo = 0;
		if (self->statement_type == STMT_TYPE_SELECT)
		{
			char		old_pre_executing = self->pre_executing;

			decideHowToPrepare(self);
			self->inaccurate_result = TRUE;
			switch (SC_get_prepare_method(self))
			{
				case USING_PARSE_REQUEST:
					if (SQL_SUCCESS != prepareParameters(self))
						return num_fields;
					break;
				case USING_UNNAMED_PARSE_REQUEST:
					if (SQL_SUCCESS != prepareParameters(self))
						return num_fields;
					self->status = STMT_PREMATURE;
					break;
				default:
					self->pre_executing = TRUE;
					self->inaccurate_result = FALSE;
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
			QR_set_rstatus(SC_get_Result(self), PGRES_TUPLES_OK);
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
	{ STMT_INFO_ONLY, "00000", "00000" }, /* just information that is returned, no error */

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
	{ STMT_INVALID_NULL_ARG, "HY009", "S1009" }
};

static PG_ErrorInfo *
SC_create_errorinfo(const StatementClass *self)
{
	QResultClass *res = SC_get_Curres(self);
	ConnectionClass *conn = SC_get_conn(self);
	Int4	errornum;
	int		pos;
	BOOL		resmsg = FALSE, detailmsg = FALSE, msgend = FALSE;
	char		msg[4096], *wmsg;
	char		*ermsg = NULL, *sqlstate = NULL;
	PG_ErrorInfo	*pgerror;

	if (self->pgerror)
		return self->pgerror;
	errornum = self->__error_number;
	if (errornum == 0)
		return	NULL;

	msg[0] = '\0';
	if (res)
	{
		if (res->sqlstate[0])
			sqlstate = res->sqlstate;
		if (res->message)
		{
			strncpy(msg, res->message, sizeof(msg));
			detailmsg = resmsg = TRUE;
		}
		if (msg[0])
			ermsg = msg;
		else if (QR_get_notice(res))
		{
			char *notice = QR_get_notice(res);
			int len = strlen(notice);
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
		strncpy(msg + pos, wmsg, sizeof(msg) - pos);
		ermsg = msg;
		detailmsg = TRUE;
	}

	if (conn && !msgend)
	{
		SocketClass *sock = conn->sock;

		if (!resmsg && (wmsg = CC_get_errormsg(conn)) && wmsg[0] != '\0')
		{
			pos = strlen(msg);
			snprintf(&msg[pos], sizeof(msg) - pos, ";\n%s", CC_get_errormsg(conn));
		}

		if (sock && sock->errormsg && sock->errormsg[0] != '\0')
		{
			pos = strlen(msg);
			snprintf(&msg[pos], sizeof(msg) - pos, ";\n%s", sock->errormsg);
		}
		ermsg = msg;
	}
	pgerror = ER_Constructor(self->__error_number, ermsg);
	if (sqlstate)
		strcpy(pgerror->sqlstate, sqlstate);
	else if (conn)
	{
		if (conn->sqlstate[0])
			strcpy(pgerror->sqlstate, conn->sqlstate);
		else
		{
        		EnvironmentClass *env = (EnvironmentClass *) conn->henv;

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

inolog("SC_get_ancestor in stmt=%x\n", stmt);
	for (child = stmt, parent = child->execute_parent; parent; child = parent, parent = child->execute_parent)
	{
		inolog("parent=%x\n", parent);
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

inolog("SC_set_error_from_res %x->%x check=%d\n", from_res ,self, check);
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

inolog("SC_error_copy %x->%x check=%d\n", from ,self, check);
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

inolog("SC_full_error_copy %x->%x\n", from ,self);
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
		SWORD	RecNumber,
		UCHAR FAR * szSqlState,
		SDWORD FAR * pfNativeError,
		UCHAR FAR * szErrorMsg,
		SWORD cbErrorMsgMax,
		SWORD FAR * pcbErrorMsg,
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
	if (!stmt->stmt_time)
		stmt->stmt_time = time(NULL);
	return stmt->stmt_time;
}
/*
 *	Currently, the driver offers very simple bookmark support -- it is
 *	just the current row number.  But it could be more sophisticated
 *	someday, such as mapping a key to a 32 bit value
 */
UInt4
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
	Oid			type;
	char	   *value;
	ColumnInfoClass *coli;
	BindInfoClass	*bookmark;

	/* TupleField *tupleField; */
	ConnInfo   *ci = &(SC_get_conn(self)->connInfo);

inolog("%s statement=%x ommitted=0\n", func, self);
	self->last_fetch_count = self->last_fetch_count_include_ommitted = 0;
	coli = QR_get_fields(res);	/* the column info */

	mylog("fetch_cursor=%d, %x->total_read=%d\n", SC_is_fetchcursor(self), res, res->num_total_read);

	if (!SC_is_fetchcursor(self))
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

		mylog("**** SC_fetch: non-cursor_result\n");
		(self->currTuple)++;
	}
	else
	{
		/* read from the cache or the physical next tuple */
		retval = QR_next_tuple(res, self);
		if (retval < 0)
		{
			mylog("**** SC_fetch: end_tuples\n");
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
			mylog("SC_fetch: error\n");
			SC_set_error(self, STMT_EXEC_ERROR, "Error fetching next row", func);
			return SQL_ERROR;
		}
	}
	if (QR_haskeyset(res))
	{
		Int4	kres_ridx;

		kres_ridx = GIdx2KResIdx(self->currTuple, self, res);
		if (kres_ridx >= 0 && kres_ridx < res->num_cached_keys)
		{
			UWORD	pstatus = res->keyset[kres_ridx].status;
inolog("SC_ pstatus[%d]=%x fetch_count=%d\n", kres_ridx, pstatus, self->last_fetch_count);
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
inolog("%s: stmt=%x ommitted++\n", func, self);
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
		UInt4	offset = opts->row_offset_ptr ? *opts->row_offset_ptr : 0;

		sprintf(buf, "%ld", SC_get_bookmark(self));
		SC_set_current_col(self, -1);
		result = copy_and_convert_field(self, 0, buf,
			 SQL_C_ULONG, bookmark->buffer + offset, 0,
			bookmark->used ? bookmark->used + (offset >> 2) : NULL);
	}

	if (self->options.retrieve_data == SQL_RD_OFF)		/* data isn't required */
		return SQL_SUCCESS;
	gdata = SC_get_GDTI(self);
	if (gdata->allocated != opts->allocated)
		extend_getdata_info(gdata, opts->allocated, TRUE);
	for (lf = 0; lf < num_cols; lf++)
	{
		mylog("fetch: cols=%d, lf=%d, opts = %x, opts->bindings = %x, buffer[] = %x\n", num_cols, lf, opts, opts->bindings, opts->bindings[lf].buffer);

		/* reset for SQLGetData */
		gdata->gdata[lf].data_left = -1;

		if (opts->bindings[lf].buffer != NULL)
		{
			/* this column has a binding */

			/* type = QR_get_field_type(res, lf); */
			type = CI_get_oid(coli, lf);		/* speed things up */

			mylog("type = %d\n", type);

			if (SC_is_fetchcursor(self))
				value = QR_get_value_backend(res, lf);
			else
			{
				int	curt = GIdx2CacheIdx(self->currTuple, self, res);
inolog("base=%d curr=%d st=%d\n", QR_get_rowstart_in_cache(res), self->currTuple, SC_get_rowset_start(self));
inolog("curt=%d\n", curt);
				value = QR_get_value_backend_row(res, curt, lf);
			}

			mylog("value = '%s'\n", (value == NULL) ? "<NULL>" : value);

			retval = copy_and_convert_field_bindinfo(self, type, value, lf);

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
	UDWORD		qflag = 0;
	BOOL		is_in_trans, issue_begin, has_out_para;
	int		func_cs_count = 0, i;

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
	if (CONN_EXECUTING == conn->status)
	{
		SC_set_error(self, STMT_SEQUENCE_ERROR, "Connection is already in use.", func);
		mylog("%s: problem with connection\n", func);
		goto cleanup;
	}
	is_in_trans = CC_is_in_trans(conn);
	/* issue BEGIN ? */
	issue_begin = TRUE;
	if (self->internal || is_in_trans)
		issue_begin = FALSE;
	else if (CC_is_in_autocommit(conn) &&
		 (!SC_is_fetchcursor(self)
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
		mylog("   about to begin a transaction on statement = %x\n", self);
		if (PG_VERSION_GE(conn, 7.1))
			qflag |= GO_INTO_TRANSACTION;
                else if (!CC_begin(conn))
                {
			SC_set_error(self, STMT_EXEC_ERROR, "Could not begin a transaction", func);
			goto cleanup;
                }
	}

	oldstatus = conn->status;
	conn->status = CONN_EXECUTING;
	self->status = STMT_EXECUTING;

	/* If it's a SELECT statement, use a cursor. */

	/*
	 * Note that the declare cursor has already been prepended to the
	 * statement
	 */
	/* in copy_statement... */
	if (PREPARED_PERMANENTLY == self->prepared &&
	    PROTOCOL_74(ci))
	{
		char	*plan_name = self->plan_name;

		if (issue_begin)
			CC_begin(conn);
		res = SC_get_Result(self);
inolog("get_Result=%x\n", res);
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
		if (!(res = SendSyncAndReceive(self, res, "bind_and_execute")))
		{
			if (SC_get_errornumber(self) <= 0)
				SC_set_error(self, STMT_EXEC_ERROR, "Could not receive he response, communication down ??", func);
			CC_on_abort(conn, CONN_DEAD);
			goto cleanup;
		}
	}
	else if (self->statement_type == STMT_TYPE_SELECT)
	{
		char		fetch[128];
		qflag |= (SQL_CONCUR_READ_ONLY != self->options.scroll_concurrency ? CREATE_KEYSET : 0); 

		mylog("       Sending SELECT statement on stmt=%x, cursor_name='%s' qflag=%d,%d\n", self, SC_cursor_name(self), qflag, self->options.scroll_concurrency);

		/* send the declare/select */
		res = CC_send_query(conn, self->stmt_with_params, NULL, qflag, SC_get_ancestor(self));
		if (SC_is_fetchcursor(self) && res != NULL &&
			QR_command_maybe_successful(res))
		{
			QR_Destructor(res);
			qflag &= (~ GO_INTO_TRANSACTION);

			/*
			 * That worked, so now send the fetch to start getting data
			 * back
			 */
			qi.result_in = NULL;
			qi.cursor = SC_cursor_name(self);
			qi.row_size = ci->drivers.fetch_max;

			/*
			 * Most likely the rowset size will not be set by the
			 * application until after the statement is executed, so might
			 * as well use the cache size. The qr_next_tuple() function
			 * will correct for any discrepancies in sizes and adjust the
			 * cache accordingly.
			 */
			sprintf(fetch, "fetch %d in \"%s\"", qi.row_size, SC_cursor_name(self));

			res = CC_send_query(conn, fetch, &qi, qflag, SC_get_ancestor(self));
			if (SC_is_with_hold(self))
				QR_set_withhold(res);
		}
		mylog("     done sending the query:\n");
	}
	else
	{
		/* not a SELECT statement so don't use a cursor */
		mylog("      it's NOT a select statement: stmt=%x\n", self);
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
			if (!self->internal && CC_is_in_autocommit(conn) && !CC_is_in_manual_trans(conn))
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

		if (was_ok)
			SC_set_errornumber(self, STMT_OK);
		else
			SC_set_errornumber(self, was_nonfatal ? STMT_INFO_ONLY : STMT_ERROR_TAKEN_FROM_BACKEND);

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

inolog("!!%x->SC_is_concat_pre=%x res=%x\n", self, self->miscinfo, res);
			/*
			 * special handling of result for keyset driven cursors. 			 * Use the columns info of the 1st query and
			 * user the keyset info of the 2nd query.
			 */
			if (SQL_CURSOR_KEYSET_DRIVEN == self->options.cursor_type &&
			    SQL_CONCUR_READ_ONLY != self->options.scroll_concurrency &&
			    !SC_is_fetchcursor(self))
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
inolog("res->next=%x\n", tres);
				res->next = NULL;
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
	else if (res == SC_get_Result(self))
		;
	else
	{
		QResultClass	*last;
		for (last = SC_get_Result(self); last->next; last = last->next)
			;
		last->next = res;
	}

	ipdopts = SC_get_IPDF(self);
	has_out_para = FALSE;
	if (self->statement_type == STMT_TYPE_PROCCALL &&
		(SC_get_errornumber(self) == STMT_OK ||
		 SC_get_errornumber(self) == STMT_INFO_ONLY))
	{
		Int2	io, out;
		has_out_para = (CountParameters(self, NULL, &io, &out) > 0);
	}
	if (has_out_para)
	{	/* get the return value of the procedure call */
		RETCODE		ret;
		HSTMT		hstmt = (HSTMT) self;

		self->bind_row = 0;
		ret = SC_fetch(hstmt);
inolog("!!SC_fetch return =%d\n", ret);
		if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		{
			APDFields	*apdopts = SC_get_APDF(self);
			UInt4	offset = apdopts->param_offset_ptr ? *apdopts->param_offset_ptr : 0;
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
					ret = PGAPI_GetData(hstmt, gidx + 1, apara->CType, apara->buffer + offset, apara->buflen, apara->used ? apara->used + (offset >> 2) : NULL);
					if (ret != SQL_SUCCESS)
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
	CLEANUP_FUNC_CONN_CS(func_cs_count, conn);
	if (CONN_DOWN != conn->status)
		conn->status = oldstatus;
	self->status = STMT_FINISHED;
	if (SC_get_errornumber(self) == STMT_OK)
		return SQL_SUCCESS;
	else if (SC_get_errornumber(self) == STMT_INFO_ONLY)
		return SQL_SUCCESS_WITH_INFO;
	else
	{
		if (!SC_get_errormsg(self) || !SC_get_errormsg(self)[0])
		{
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
		stmt->callbacks = (NeedDataCallback *) realloc(stmt->callbacks,
			sizeof(NeedDataCallback) * (stmt->allocated_callbacks +
				CALLBACK_ALLOC_ONCE));
		stmt->allocated_callbacks += CALLBACK_ALLOC_ONCE;
	}
	stmt->callbacks[stmt->num_callbacks].func = func;
	stmt->callbacks[stmt->num_callbacks].data = data;
	stmt->num_callbacks++;

inolog("enqueueNeedDataCallack stmt=%x, func=%x, count=%d\n", stmt, func, stmt->num_callbacks);
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
		int	rowsetSize;

#if (ODBCVER >= 0x0300)
		rowsetSize = (7 == self->transition_status ? opts->size_of_rowset_odbc2 : opts->size_of_rowset);
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
			qlog("                 hdbc=%x, stmt=%x, result=%x\n", self->hdbc, self, res);
			qlog("                 prepare=%d, internal=%d\n", self->prepare, self->internal);
			qlog("                 bindings=%x, bindings_allocated=%d\n", opts->bindings, opts->allocated);
			qlog("                 parameters=%x, parameters_allocated=%d\n", apdopts->parameters, apdopts->allocated);
			qlog("                 statement_type=%d, statement='%s'\n", self->statement_type, nullcheck(self->statement));
			qlog("                 stmt_with_params='%s'\n", nullcheck(self->stmt_with_params));
			qlog("                 data_at_exec=%d, current_exec_param=%d, put_data=%d\n", self->data_at_exec, self->current_exec_param, self->put_data);
			qlog("                 currTuple=%d, current_col=%d, lobj_fd=%d\n", self->currTuple, self->current_col, self->lobj_fd);
			qlog("                 maxRows=%d, rowset_size=%d, keyset_size=%d, cursor_type=%d, scroll_concurrency=%d\n", self->options.maxRows, rowsetSize, self->options.keyset_size, self->options.cursor_type, self->options.scroll_concurrency);
			qlog("                 cursor_name='%s'\n", SC_cursor_name(self));

			qlog("                 ----------------QResult Info -------------------------------\n");

			if (res)
			{
				qlog("                 fields=%x, backend_tuples=%x, tupleField=%d, conn=%x\n", res->fields, res->backend_tuples, res->tupleField, res->conn);
				qlog("                 fetch_count=%d, num_total_rows=%d, num_fields=%d, cursor='%s'\n", res->fetch_number, QR_get_num_total_tuples(res), res->num_fields, nullcheck(QR_get_cursor(res)));
				qlog("                 message='%s', command='%s', notice='%s'\n", nullcheck(res->message), nullcheck(res->command), nullcheck(res->notice));
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

BOOL
SendBindRequest(StatementClass *stmt, const char *plan_name)
{
	CSTR	func = "SendBindRequest";
	ConnectionClass	*conn = SC_get_conn(stmt);
	SocketClass	*sock = conn->sock;

	mylog("%s: plan_name=%s\n", func, plan_name);
	if (!BuildBindRequest(stmt, plan_name))
		return FALSE;

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
	int		i;
	Int2		num_discard_params, paramType;
	BOOL		rcvend = FALSE, msg_truncated;
	char		msgbuffer[ERROR_MSG_LENGTH + 1];
	IPDFields	*ipdopts;
	QResultClass	*newres = NULL;

	if (CC_is_in_trans(conn) && !SC_accessed_db(stmt))
	{ 
		if (SQL_ERROR == SetStatementSvp(stmt))
		{
			SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal savepoint error in SendSynAndReceive", func);
			return FALSE;
		}
	}

	SOCK_put_char(sock, 'S');	/* Sync message */
	SOCK_put_int(sock, 4, 4);
	SOCK_flush_output(sock);

	if (!res)
		newres = res = QR_Constructor();
	for (;!rcvend;)
	{
		id = SOCK_get_id(sock);
		if ((SOCK_get_errcode(sock) != 0) || (id == EOF))
		{
			SC_set_error(stmt, CONNECTION_NO_RESPONSE, "No response rom the backend", func);

			mylog("%s: 'id' - %s\n", func, SC_get_errormsg(stmt));
			CC_on_abort(conn, CONN_DEAD);
			QR_Destructor(newres);
			return NULL;
		}
inolog("desc id=%c", id);
		response_length = SOCK_get_response_length(sock);
inolog(" response_length=%d\n", response_length);
		switch (id)
		{
			case 'C':
				SOCK_get_string(sock, msgbuffer, sizeof(msgbuffer));
				mylog("command response=%s\n", msgbuffer);
				QR_set_command(res, msgbuffer);
				if (QR_is_fetching_tuples(res))
				{
					QR_set_no_fetching_tuples(res);
					/* in case of FETCH, Portal Suspend never arrives */
					if (strnicmp(msgbuffer, "SELECT", 6) == 0)
					{
						mylog("%s: reached eof now\n", func);
						QR_set_reached_eof(res);
					}
				}
				break;
			case 'E': /* ErrorMessage */
				msg_truncated = handle_error_message(conn, msgbuffer, sizeof(msgbuffer), res->sqlstate, comment, res);

				rcvend = TRUE;
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
				rcvend = TRUE;
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
					mylog("ParamInfo unmatch num_params=%d! info=%d+discard=%d\n", stmt->num_params, num_p, num_discard_params);
					stmt->num_params = (Int2) num_p + num_discard_params;
				}
				ipdopts = SC_get_IPDF(stmt);
				extend_iparameter_bindings(ipdopts, stmt->num_params);
				if (stmt->discard_output_params)
				{
					for (i = stmt->proc_return; i < stmt->num_params; i++)
					{
						paramType = ipdopts->parameters[i].paramType;
						if (SQL_PARAM_OUTPUT == paramType)
							continue;
						oid = SOCK_get_int(sock, 4);
						ipdopts->parameters[i].PGType = oid;
					}
				}
				else
				{
					for (i = 0; i < num_p; i++)
					{
						paramType = ipdopts->parameters[i].paramType;	
						oid = SOCK_get_int(sock, 4);
						if (SQL_PARAM_OUTPUT != paramType ||
						    PG_TYPE_VOID != oid)
							ipdopts->parameters[i + stmt->proc_return].PGType = oid;
					}
				}
				break;
			case 'T': /* RowDesription */
				QR_set_conn(res, conn);
				if (CI_read_fields(res->fields, conn))
				{
					Int2	dummy1, dummy2;
					int	cidx;

					QR_set_rstatus(res, PGRES_FIELDS_OK);
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
inolog("!![%d].PGType %u->%u\n", i, ipdopts->parameters[i].PGType, res->fields->adtid[cidx]);
								ipdopts->parameters[i].PGType = res->fields->adtid[cidx];
								cidx++;
							}
						}
					}
				}
				else
				{
					QR_set_rstatus(res, PGRES_BAD_RESPONSE);
					QR_set_message(res, "Error reading field information");
					rcvend = TRUE;
				}
				break;
			case 'B': /* Binary data */
			case 'D': /* ASCII data */
				QR_get_tupledata(res, id == 'B');
				break;
			case 'S': /* parameter status */
				getParameterValues(conn);
				break;
			case 's':	/* portal suspend */
				QR_set_no_fetching_tuples(res);
				break;
			default:
				break;
		}
	}
	return res;
}

BOOL
SendParseRequest(StatementClass *stmt, const char *plan_name, const char *query)
{
	CSTR	func = "SendParseRequest";
	ConnectionClass	*conn = SC_get_conn(stmt);
	SocketClass	*sock = conn->sock;
	unsigned long	pileng, leng;

	mylog("%s: plan_name=%s query=%s\n", func, plan_name, query);
	if (CC_is_in_trans(conn) && !SC_accessed_db(stmt))
	{ 
		if (SQL_ERROR == SetStatementSvp(stmt))
		{
			SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal savepoint error in SendParseRequest", func);
			return FALSE;
		}
	}
	SOCK_put_char(sock, 'P');
	if (SOCK_get_errcode(sock) != 0)
	{
		CC_set_error(conn, CONNECTION_COULD_NOT_SEND, "Could not send P request to backend", func);
		CC_on_abort(conn, CONN_DEAD);
		return FALSE;
	}

	pileng = sizeof(Int2);
	if (!stmt->discard_output_params)
		pileng += sizeof(UInt4) * (stmt->num_params - stmt->proc_return); 
	leng = strlen(plan_name) + 1 + strlen(query) + 1 + pileng;
	SOCK_put_int(sock, leng + 4, 4);
inolog("parse leng=%d\n", leng);
	SOCK_put_string(sock, plan_name);
	SOCK_put_string(sock, query);
	SOCK_put_int(sock, stmt->num_params - stmt->proc_return, sizeof(Int2)); /* number of parameters unspecified */
	if (!stmt->discard_output_params)
	{
		int	i;
		IPDFields	*ipdopts = SC_get_IPDF(stmt);

		for (i = stmt->proc_return; i < stmt->num_params; i++)
		{
			if (i < ipdopts->allocated &&
			    SQL_PARAM_OUTPUT == ipdopts->parameters[i].paramType)
				SOCK_put_int(sock, PG_TYPE_VOID, sizeof(UInt4));
			else
				SOCK_put_int(sock, 0, sizeof(UInt4));
		}
	}

	return TRUE;
}

BOOL
SendDescribeRequest(StatementClass *stmt, const char *plan_name)
{
	CSTR	func = "SendDescribeRequest";
	ConnectionClass	*conn = SC_get_conn(stmt);
	SocketClass	*sock = conn->sock;
	unsigned long	leng;
	BOOL		sockerr = FALSE;

	mylog("%s:plan_name=%s\n", func, plan_name);
	if (CC_is_in_trans(conn) && !SC_accessed_db(stmt))
	{
		if (SQL_ERROR == SetStatementSvp(stmt))
		{
			SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal savepoint error", func);
			return FALSE;
		}
	}
	SOCK_put_char(sock, 'D');
	if (SOCK_get_errcode(sock) != 0)
		sockerr = TRUE;
	if (!sockerr)
	{
		leng = 1 + strlen(plan_name) + 1;
		SOCK_put_int(sock, leng + 4, 4);
		if (SOCK_get_errcode(sock) != 0)
			sockerr = TRUE;
	}
	if (!sockerr)
	{
inolog("describe leng=%d\n", leng);
		SOCK_put_char(sock, 'S');
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

	return TRUE;
}

BOOL
SendExecuteRequest(StatementClass *stmt, const char *plan_name, UInt4 count)
{
	CSTR	func = "SendExecuteRequest";
	ConnectionClass	*conn;
	SocketClass	*sock;
	unsigned long	leng;

	if (!stmt)	return FALSE;
	if (conn = SC_get_conn(stmt), !conn)	return FALSE;
	if (sock = conn->sock, !sock)	return FALSE;

	mylog("%s: plan_name=%s count=%d\n", func, plan_name, count);
	if (CC_is_in_trans(conn) && !SC_accessed_db(stmt))
	{
		if (SQL_ERROR == SetStatementSvp(stmt))
		{
			SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal savepoint error", func);
			return FALSE;
		}
	}
	SOCK_put_char(sock, 'E');
	if (SOCK_get_errcode(sock) != 0)
	{
		CC_set_error(conn, CONNECTION_COULD_NOT_SEND, "Could not send D Request to backend", func);
		CC_on_abort(conn, CONN_DEAD);
		return FALSE;
	}

	leng = strlen(plan_name) + 1 + 4;
	SOCK_put_int(sock, leng + 4, 4);
inolog("execute leng=%d\n", leng);
	SOCK_put_string(sock, plan_name);
	SOCK_put_int(sock, count, 4);

	return TRUE;
}

BOOL	SendSyncRequest(ConnectionClass *conn)
{
	SocketClass	*sock = conn->sock;

	SOCK_put_char(sock, 'S');	/* Sync message */
	SOCK_put_int(sock, 4, 4);
	SOCK_flush_output(sock);

	return TRUE;
}
