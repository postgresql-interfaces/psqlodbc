/*---------
 * Module:			qresult.c
 *
 * Description:		This module contains functions related to
 *			managing result information (i.e, fetching rows
 *			from the backend, managing the tuple cache, etc.)
 *			and retrieving it.	Depending on the situation, a
 *			QResultClass will hold either data from the backend
 *			or a manually built result.
 *
 * Classes:		QResultClass (Functions prefix: "QR_")
 *
 * API functions:	none
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *---------
 */

#include "qresult.h"
#include "statement.h"

#include <libpq-fe.h>

#include "misc.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

static BOOL QR_prepare_for_tupledata(QResultClass *self);
static BOOL QR_read_tuples_from_pgres(QResultClass *, PGresult **pgres);

/*
 *	Used for building a Manual Result only
 *	All info functions call this function to create the manual result set.
 */
void
QR_set_num_fields(QResultClass *self, int new_num_fields)
{
	if (!self)	return;
	MYLOG(0, "entering\n");

	CI_set_num_fields(QR_get_fields(self), new_num_fields);

	MYLOG(0, "leaving\n");
}


void
QR_set_position(QResultClass *self, SQLLEN pos)
{
	self->tupleField = self->backend_tuples + ((QR_get_rowstart_in_cache(self) + pos) * self->num_fields);
}


void
QR_set_cache_size(QResultClass *self, SQLLEN cache_size)
{
	self->cache_size = cache_size;
}


void
QR_set_reqsize(QResultClass *self, Int4 reqsize)
{
	self->rowset_size_include_ommitted = reqsize;
}

void
QR_set_cursor(QResultClass *self, const char *name)
{
	ConnectionClass	*conn = QR_get_conn(self);

	if (self->cursor_name)
	{
		if (name &&
		    0 == strcmp(name, self->cursor_name))
			return;
		free(self->cursor_name);
		if (conn)
		{
			CONNLOCK_ACQUIRE(conn);
			conn->ncursors--;
			CONNLOCK_RELEASE(conn);
		}
		self->cursTuple = -1;
		QR_set_no_cursor(self);
	}
	else if (NULL == name)
		return;
	if (name)
	{
		self->cursor_name = strdup(name);
		if (conn)
		{
			CONNLOCK_ACQUIRE(conn);
			conn->ncursors++;
			CONNLOCK_RELEASE(conn);
		}
	}
	else
	{
		QResultClass *res;

		self->cursor_name = NULL;
		for (res = self->next; NULL != res; res = res->next)
		{
			if (NULL != res->cursor_name)
				free(res->cursor_name);
			res->cursor_name = NULL;
		}
	}
}


void
QR_set_num_cached_rows(QResultClass *self, SQLLEN num_rows)
{
	self->num_cached_rows = num_rows;
	if (QR_synchronize_keys(self))
		self->num_cached_keys = self->num_cached_rows;
}

void
QR_set_rowstart_in_cache(QResultClass *self, SQLLEN start)
{
	if (QR_synchronize_keys(self))
		self->key_base = start;
	self->base = start;
}

void
QR_inc_rowstart_in_cache(QResultClass *self, SQLLEN base_inc)
{
	if (!QR_has_valid_base(self))
		MYLOG(0, " called while the cache is not ready\n");
	self->base += base_inc;
	if (QR_synchronize_keys(self))
		self->key_base = self->base;
}

void
QR_set_fields(QResultClass *self, ColumnInfoClass *fields)
{
	ColumnInfoClass	*curfields = QR_get_fields(self);

	if (curfields == fields)
		return;

	/*
	 * Unlink the old columninfo from this result set, freeing it if this
	 * was the last reference.
	 */
	if (NULL != curfields)
	{
		if (curfields->refcount > 1)
			curfields->refcount--;
		else
			CI_Destructor(curfields);
	}
	self->fields = fields;
	if (NULL != fields)
		fields->refcount++;
}

/*
 * CLASS QResult
 */
QResultClass *
QR_Constructor(void)
{
	QResultClass *rv;

	MYLOG(0, "entering\n");
	rv = (QResultClass *) malloc(sizeof(QResultClass));

	if (rv != NULL)
	{
		ColumnInfoClass	*fields;

		rv->rstatus = PORES_EMPTY_QUERY;
		rv->pstatus = 0;

		/* construct the column info */
		rv->fields = NULL;
		if (fields = CI_Constructor(), NULL == fields)
		{
			free(rv);
			return NULL;
		}
		QR_set_fields(rv, fields);
		rv->backend_tuples = NULL;
		rv->sqlstate[0] = '\0';
		rv->message = NULL;
		rv->messageref = NULL;
		rv->command = NULL;
		rv->notice = NULL;
		rv->conn = NULL;
		rv->next = NULL;
		rv->count_backend_allocated = 0;
		rv->count_keyset_allocated = 0;
		rv->num_total_read = 0;
		rv->num_cached_rows = 0;
		rv->num_cached_keys = 0;
		rv->fetch_number = 0;
		rv->flags = 0; /* must be cleared before calling QR_set_rowstart_in_cache() */
		QR_set_rowstart_in_cache(rv, -1);
		rv->key_base = -1;
		rv->recent_processed_row_count = -1;
		rv->cursTuple = -1;
		rv->move_offset = 0;
		rv->num_fields = 0;
		rv->num_key_fields = PG_NUM_NORMAL_KEYS; /* CTID + OID */
		rv->tupleField = NULL;
		rv->cursor_name = NULL;
		rv->aborted = FALSE;

		rv->cache_size = 0;
		rv->cmd_fetch_size = 0;
		rv->rowset_size_include_ommitted = 1;
		rv->move_direction = 0;
		rv->keyset = NULL;
		rv->reload_count = 0;
		rv->rb_alloc = 0;
		rv->rb_count = 0;
		rv->dataFilled = FALSE;
		rv->rollback = NULL;
		rv->ad_alloc = 0;
		rv->ad_count = 0;
		rv->added_keyset = NULL;
		rv->added_tuples = NULL;
		rv->up_alloc = 0;
		rv->up_count = 0;
		rv->updated = NULL;
		rv->updated_keyset = NULL;
		rv->updated_tuples = NULL;
		rv->dl_alloc = 0;
		rv->dl_count = 0;
		rv->deleted = NULL;
		rv->deleted_keyset = NULL;
	}

	MYLOG(0, "leaving\n");
	return rv;
}


void
QR_close_result(QResultClass *self, BOOL destroy)
{
	ConnectionClass	*conn;
	QResultClass *next;
	BOOL	top = TRUE;

	if (!self)	return;
	MYLOG(0, "entering\n");

	while(self)
	{
		/*
		 * If conn is defined, then we may have used "backend_tuples", so in
		 * case we need to, free it up.  Also, close the cursor.
		 */
		if ((conn = QR_get_conn(self)) && conn->pqconn)
		{
			if (CC_is_in_trans(conn) || QR_is_withhold(self))
			{
				if (!QR_close(self))	/* close the cursor if there is one */
				{
				}
			}
		}

		QR_free_memory(self);		/* safe to call anyway */

		/*
		 * Should have been freed in the close() but just in case...
		 * QR_set_cursor clears the cursor name of all the chained results too,
		 * so we only need to do this for the first result in the chain.
		 */
		if (top)
			QR_set_cursor(self, NULL);

		/* Free up column info */
		if (destroy)
			QR_set_fields(self, NULL);

		/* Free command info (this is from strdup()) */
		if (self->command)
		{
			free(self->command);
			self->command = NULL;
		}

		/* Free message info (this is from strdup()) */
		if (self->message)
		{
			free(self->message);
			self->message = NULL;
		}

		/* Free notice info (this is from strdup()) */
		if (self->notice)
		{
			free(self->notice);
			self->notice = NULL;
		}
		/* Destruct the result object in the chain */
		next = self->next;
		self->next = NULL;
		if (destroy)
			free(self);

		/* Repeat for the next result in the chain */
		self = next;
		destroy = TRUE; /* always destroy chained results */
		top = FALSE;
	}

	MYLOG(0, "leaving\n");
}

void
QR_reset_for_re_execute(QResultClass *self)
{
	MYLOG(0, "entering for %p\n", self);
	if (!self)	return;
	QR_close_result(self, FALSE);
	/* reset flags etc */
	self->flags = 0;
	QR_set_rowstart_in_cache(self, -1);
	self->recent_processed_row_count = -1;
	/* clear error info etc */
	self->rstatus = PORES_EMPTY_QUERY;
	self->aborted = FALSE;
	self->sqlstate[0] = '\0';
	self->messageref = NULL;

	MYLOG(0, "leaving\n");
}

void
QR_Destructor(QResultClass *self)
{
	MYLOG(0, "entering\n");
	if (!self)	return;
	QR_close_result(self, TRUE);

	MYLOG(0, "leaving\n");
}


void
QR_set_command(QResultClass *self, const char *msg)
{
	if (self->command)
		free(self->command);

	self->command = msg ? strdup(msg) : NULL;
}


void
QR_set_message(QResultClass *self, const char *msg)
{
	if (self->message)
		free(self->message);
	self->messageref = NULL;

	self->message = msg ? strdup(msg) : NULL;
}

void
QR_add_message(QResultClass *self, const char *msg)
{
	char	*message = self->message;
	size_t	alsize, pos, addlen;

	if (!msg || !msg[0])
		return;
	addlen = strlen(msg);
	if (message)
	{
		pos = strlen(message) + 1;
		alsize = pos + addlen + 1;
	}
	else
	{
		pos = 0;
		alsize = addlen + 1;
	}
	if (message = realloc(message, alsize), NULL == message)
		return;
	if (pos > 0)
		message[pos - 1] = ';';
	strncpy_null(message + pos, msg, addlen + 1);
	self->message = message;
}


void
QR_set_notice(QResultClass *self, const char *msg)
{
	if (self->notice)
		free(self->notice);

	self->notice = msg ? strdup(msg) : NULL;
}

void
QR_add_notice(QResultClass *self, const char *msg)
{
	char	*message = self->notice;
	size_t	alsize, pos, addlen;

	if (!msg || !msg[0])
		return;
	addlen = strlen(msg);
	if (message)
	{
		pos = strlen(message) + 1;
		alsize = pos + addlen + 1;
	}
	else
	{
		pos = 0;
		alsize = addlen + 1;
	}
	if (message = realloc(message, alsize), NULL == message)
		return;
	if (pos > 0)
		message[pos - 1] = ';';
	strncpy_null(message + pos, msg, addlen + 1);
	self->notice = message;
}


TupleField	*QR_AddNew(QResultClass *self)
{
	size_t	alloc;
	UInt4	num_fields;

	if (!self)	return	NULL;
MYLOG(DETAIL_LOG_LEVEL, FORMAT_ULEN "th row(%d fields) alloc=" FORMAT_LEN "\n", self->num_cached_rows, QR_NumResultCols(self), self->count_backend_allocated);
	if (num_fields = QR_NumResultCols(self), !num_fields)	return	NULL;
	if (self->num_fields <= 0)
	{
		self->num_fields = num_fields;
		QR_set_reached_eof(self);
	}
	alloc = self->count_backend_allocated;
	if (!self->backend_tuples)
	{
		self->num_cached_rows = 0;
		alloc = TUPLE_MALLOC_INC;
		QR_MALLOC_return_with_error(self->backend_tuples, TupleField, alloc * sizeof(TupleField) * num_fields, self, "Out of memory in QR_AddNew.", NULL);
	}
	else if (self->num_cached_rows >= self->count_backend_allocated)
	{
		alloc = self->count_backend_allocated * 2;
		QR_REALLOC_return_with_error(self->backend_tuples, TupleField, alloc * sizeof(TupleField) * num_fields, self, "Out of memory in QR_AddNew.", NULL);
	}
	self->count_backend_allocated = alloc;

	if (self->backend_tuples)
	{
		memset(self->backend_tuples + num_fields * self->num_cached_rows, 0, num_fields * sizeof(TupleField));
		self->num_cached_rows++;
		self->ad_count++;
	}
	return self->backend_tuples + num_fields * (self->num_cached_rows - 1);
}

void
QR_free_memory(QResultClass *self)
{
	SQLLEN		num_backend_rows = self->num_cached_rows;
	int		num_fields = self->num_fields;

	MYLOG(0, "entering fcount=" FORMAT_LEN "\n", num_backend_rows);

	if (self->backend_tuples)
	{
		ClearCachedRows(self->backend_tuples, num_fields, num_backend_rows);
		free(self->backend_tuples);
		self->count_backend_allocated = 0;
		self->backend_tuples = NULL;
		self->dataFilled = FALSE;
		self->tupleField = NULL;
	}
	if (self->keyset)
	{
		ConnectionClass	*conn = QR_get_conn(self);

		free(self->keyset);
		self->keyset = NULL;
		self->count_keyset_allocated = 0;
		if (self->reload_count > 0 && conn && conn->pqconn)
		{
			char	plannm[32];

			SPRINTF_FIXED(plannm, "_KEYSET_%p", self);
			if (CC_is_in_error_trans(conn))
			{
				CC_mark_a_object_to_discard(conn, 's',plannm);
			}
			else
			{
				QResultClass	*res;
				char		cmd[64];

				SPRINTF_FIXED(cmd, "DEALLOCATE \"%s\"", plannm);
				res = CC_send_query(conn, cmd, NULL, IGNORE_ABORT_ON_CONN | ROLLBACK_ON_ERROR, NULL);
				QR_Destructor(res);
			}
		}
		self->reload_count = 0;
	}
	if (self->rollback)
	{
		free(self->rollback);
		self->rb_alloc = 0;
		self->rb_count = 0;
		self->rollback = NULL;
	}
	if (self->deleted)
	{
		free(self->deleted);
		self->deleted = NULL;
	}
	if (self->deleted_keyset)
	{
		free(self->deleted_keyset);
		self->deleted_keyset = NULL;
	}
	self->dl_alloc = 0;
	self->dl_count = 0;
	/* clear added info */
	if (self->added_keyset)
	{
		free(self->added_keyset);
		self->added_keyset = NULL;
	}
	if (self->added_tuples)
	{
		ClearCachedRows(self->added_tuples, num_fields, self->ad_count);
		free(self->added_tuples);
		self->added_tuples = NULL;
	}
	self->ad_alloc = 0;
	self->ad_count = 0;
	/* clear updated info */
	if (self->updated)
	{
		free(self->updated);
		self->updated = NULL;
	}
	if (self->updated_keyset)
	{
		free(self->updated_keyset);
		self->updated_keyset = NULL;
	}
	if (self->updated_tuples)
	{
		ClearCachedRows(self->updated_tuples, num_fields, self->up_count);
		free(self->updated_tuples);
		self->updated_tuples = NULL;
	}
	self->up_alloc = 0;
	self->up_count = 0;

	self->num_total_read = 0;
	self->num_cached_rows = 0;
	self->num_cached_keys = 0;
	self->cursTuple = -1;
	self->pstatus = 0;

	MYLOG(0, "leaving\n");
}


BOOL
QR_from_PGresult(QResultClass *self, StatementClass *stmt, ConnectionClass *conn, const char *cursor, PGresult **pgres)
{
	int			num_io_params, num_cached_rows;
	int			i;
	Int2		paramType;
	IPDFields  *ipdopts;
	Int2		lf;
	int			new_num_fields;
	OID			new_adtid, new_relid = 0, new_attid = 0;
	Int2		new_adtsize;
	Int4		new_atttypmod = -1;
	char	   *new_field_name;
	Int2		dummy1, dummy2;
	int			cidx;
	BOOL		reached_eof_now = FALSE;

	if (NULL != conn)
		/* First, get column information */
		QR_set_conn(self, conn);

	/* at first read in the number of fields that are in the query */
	new_num_fields = PQnfields(*pgres);
	QLOG(0, "\tnFields: %d\n", new_num_fields);

	/* according to that allocate memory */
	QR_set_num_fields(self, new_num_fields);
	if (NULL == QR_get_fields(self)->coli_array)
		return FALSE;

	/* now read in the descriptions */
	for (lf = 0; lf < new_num_fields; lf++)
	{
		new_field_name = PQfname(*pgres, lf);
		new_relid = PQftable(*pgres, lf);
		new_attid = PQftablecol(*pgres, lf);
		new_adtid = (OID) PQftype(*pgres, lf);
		new_adtsize = (Int2) PQfsize(*pgres, lf);
		new_atttypmod = (Int4) PQfmod(*pgres, lf);

		/* Subtract the header length */
		switch (new_adtid)
		{
			case PG_TYPE_DATETIME:
			case PG_TYPE_TIMESTAMP_NO_TMZONE:
			case PG_TYPE_TIME:
			case PG_TYPE_TIME_WITH_TMZONE:
				break;
			default:
				new_atttypmod -= 4;
		}
		if (new_atttypmod < 0)
			new_atttypmod = -1;

		QLOG(0, "\tfieldname='%s', adtid=%d, adtsize=%d, atttypmod=%d (rel,att)=(%d,%d)\n", new_field_name, new_adtid, new_adtsize, new_atttypmod, new_relid, new_attid);

		CI_set_field_info(QR_get_fields(self), lf, new_field_name, new_adtid, new_adtsize, new_atttypmod, new_relid, new_attid);

		QR_set_rstatus(self, PORES_FIELDS_OK);
		self->num_fields = CI_get_num_fields(QR_get_fields(self));
		if (QR_haskeyset(self))
			self->num_fields -= self->num_key_fields;
		if (stmt && conn)
		{
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
MYLOG(DETAIL_LOG_LEVEL, "[%d].PGType %u->%u\n", i, PIC_get_pgtype(ipdopts->parameters[i]), CI_get_oid(QR_get_fields(self), cidx));
						PIC_set_pgtype(ipdopts->parameters[i], CI_get_oid(QR_get_fields(self), cidx));
						cidx++;
					}
				}
			}
		}
	}


	/* Then, get the data itself */
	num_cached_rows = self->num_cached_rows;
	if (!QR_read_tuples_from_pgres(self, pgres))
		return FALSE;

MYLOG(DETAIL_LOG_LEVEL, "!!%p->cursTup=" FORMAT_LEN " total_read=" FORMAT_ULEN "\n", self, self->cursTuple, self->num_total_read);
	if (!QR_once_reached_eof(self) && self->cursTuple >= (Int4) self->num_total_read)
		self->num_total_read = self->cursTuple + 1;
	/* EOF is 'fetched < fetch requested' */
	if (self->num_cached_rows - num_cached_rows < self->cmd_fetch_size)
	{
		MYLOG(0, "detect EOF " FORMAT_ULEN " - %d < " FORMAT_ULEN "\n", self->num_cached_rows, num_cached_rows, self->cmd_fetch_size);
		reached_eof_now = TRUE;
		QR_set_reached_eof(self);
	}
	if (reached_eof_now && self->cursTuple < (Int4) self->num_total_read)
		self->cursTuple = self->num_total_read;

	if (NULL != conn)
	{
		/* Force a read to occur in next_tuple */
		QR_set_next_in_cache(self, (SQLLEN) 0);
		QR_set_rowstart_in_cache(self, 0);
		self->key_base = 0;
	}

	/*
	 * Also fill in command tag. (Typically, it's SELECT, but can also be
	 * a FETCH.)
	 */
	QR_set_command(self, PQcmdStatus(*pgres));
	QR_set_cursor(self, cursor);
	if (NULL == cursor)
		QR_set_reached_eof(self);
	return TRUE;
}


/*
 *	Procedure needed when closing cursors.
 */
void
QR_on_close_cursor(QResultClass *self)
{
	QR_set_cursor(self, NULL);
}

/*
 *	Close the cursor and end the transaction (if no cursors left)
 *	We only close the cursor if other cursors are used.
 */
int
QR_close(QResultClass *self)
{
	ConnectionClass	*conn;
	QResultClass *res;
	int	ret = TRUE;

	conn = QR_get_conn(self);
	if (self && QR_get_cursor(self))
	{
		if (CC_is_in_error_trans(conn))
		{
			if (QR_is_withhold(self))
				CC_mark_a_object_to_discard(conn, 'p', QR_get_cursor(self));
		}
		else
		{
			BOOL		does_commit = FALSE;
			unsigned int	flag = 0;
			char		buf[64];

			flag = READ_ONLY_QUERY;
			if (QR_needs_survival_check(self))
				flag |= (ROLLBACK_ON_ERROR | IGNORE_ABORT_ON_CONN);

			SPRINTF_FIXED(buf, "close \"%s\"", QR_get_cursor(self));
			/* End the transaction if there are no cursors left on this conn */
			if (CC_is_in_trans(conn) &&
			    CC_does_autocommit(conn) &&
			    CC_cursor_count(conn) <= 1)
			{
				MYLOG(0, "End transaction on conn=%p\n", conn);
				if ((ROLLBACK_ON_ERROR & flag) == 0)
				{
					STRCAT_FIXED(buf, ";commit");
					flag |= END_WITH_COMMIT;
					QR_set_cursor(self, NULL);
				}
				else
					does_commit = TRUE;
			}

MYLOG(DETAIL_LOG_LEVEL, " Case I CC_send_query %s flag=%x\n", buf, flag);
			res = CC_send_query(conn, buf, NULL, flag, NULL);
			QR_Destructor(res);
			if (does_commit)
			{
				if (!CC_commit(conn))
				{
					QR_set_rstatus(self, PORES_FATAL_ERROR);
					QR_set_message(self, "Error ending transaction on autocommit.");
					ret = FALSE;
				}
			}
		}

		QR_on_close_cursor(self);
		if (!ret)
			return ret;

#ifdef	NOT_USED
		/* End the transaction if there are no cursors left on this conn */
		if (CC_does_autocommit(conn) && CC_cursor_count(conn) == 0)
		{
			MYLOG(0, "End transaction on conn=%p\n", conn);

			if (!CC_commit(conn))
			{
				QR_set_rstatus(self, PORES_FATAL_ERROR);
				QR_set_message(self, "Error ending transaction.");
				ret = FALSE;
			}
		}
#endif /* NOT_USED */
	}

	return ret;
}

/*
 * Allocate memory for receiving next tuple.
 */
static BOOL
QR_prepare_for_tupledata(QResultClass *self)
{
	BOOL	haskeyset = QR_haskeyset(self);
	SQLULEN num_total_rows = QR_get_num_total_tuples(self);

MYLOG(DETAIL_LOG_LEVEL, "entering %p->num_fields=%d\n", self, self->num_fields);
	if (!QR_get_cursor(self))
	{

		if (self->num_fields > 0 &&
		    num_total_rows >= self->count_backend_allocated)
		{
			SQLLEN	tuple_size = self->count_backend_allocated;

			MYLOG(0, "REALLOC: old_count = " FORMAT_LEN ", size = " FORMAT_SIZE_T "\n", tuple_size, self->num_fields * sizeof(TupleField) * tuple_size);
			if (tuple_size < 1)
				tuple_size = TUPLE_MALLOC_INC;
			else
				tuple_size *= 2;
			QR_REALLOC_return_with_error(self->backend_tuples, TupleField, tuple_size * self->num_fields * sizeof(TupleField), self, "Out of memory while reading tuples.", FALSE);
			self->count_backend_allocated = tuple_size;
		}
		if (haskeyset &&
		    self->num_cached_keys >= self->count_keyset_allocated)
		{
			SQLLEN	tuple_size = self->count_keyset_allocated;

			if (tuple_size < 1)
				tuple_size = TUPLE_MALLOC_INC;
			else
				tuple_size *= 2;
			QR_REALLOC_return_with_error(self->keyset, KeySet, sizeof(KeySet) * tuple_size, self, "Out of mwmory while allocating keyset", FALSE);
			memset(&self->keyset[self->count_keyset_allocated],
				   0,
				   (tuple_size - self->count_keyset_allocated) * sizeof(KeySet));
			self->count_keyset_allocated = tuple_size;
		}
	}
	return TRUE;
}

static SQLLEN enlargeKeyCache(QResultClass *self, SQLLEN add_size, const char *message)
{
	size_t	alloc, alloc_req;
	Int4	num_fields = self->num_fields;
	BOOL	curs = (NULL != QR_get_cursor(self));

	if (add_size <= 0)
		return self->count_keyset_allocated;
	alloc = self->count_backend_allocated;
	if (num_fields > 0 && ((alloc_req = (Int4)self->num_cached_rows + add_size) > alloc || !self->backend_tuples))
	{
		if (1 > alloc)
		{
			if (curs)
				alloc = alloc_req;
			else
				alloc = (alloc_req > TUPLE_MALLOC_INC ? alloc_req : TUPLE_MALLOC_INC);
		}
		else
		{
			do
			{
				alloc *= 2;
			} while (alloc < alloc_req);
		}
		self->count_backend_allocated = 0;
		QR_REALLOC_return_with_error(self->backend_tuples, TupleField, num_fields * sizeof(TupleField) * alloc, self, message, -1);
		self->count_backend_allocated = alloc;
	}
	alloc = self->count_keyset_allocated;
	if (QR_haskeyset(self) && ((alloc_req = (Int4)self->num_cached_keys + add_size) > alloc || !self->keyset))
	{
		if (1 > alloc)
		{
			if (curs)
				alloc = alloc_req;
			else
				alloc = (alloc_req > TUPLE_MALLOC_INC ? alloc_req : TUPLE_MALLOC_INC);
		}
		else
		{
			do
			{
				alloc *= 2;
			} while (alloc < alloc_req);
		}
		self->count_keyset_allocated = 0;
		QR_REALLOC_return_with_error(self->keyset, KeySet, sizeof(KeySet) * alloc, self, message, -1);
		self->count_keyset_allocated = alloc;
	}
	return alloc;
}

SQLLEN	QR_move_cursor_to_last(QResultClass *self, StatementClass *stmt)
{
	char		movecmd[64];
	QResultClass	*res;
	SQLLEN		moved;
	ConnectionClass	*conn = SC_get_conn(stmt);

	if (!QR_get_cursor(self))
		return 0;
	if (QR_once_reached_eof(self) &&
	    self->cursTuple >= self->num_total_read)
		return 0;
	SPRINTF_FIXED(movecmd,
		 "move all in \"%s\"", QR_get_cursor(self));
	res = CC_send_query(conn, movecmd, NULL, READ_ONLY_QUERY, stmt);
	if (!QR_command_maybe_successful(res))
	{
		QR_Destructor(res);
		SC_set_error(stmt, STMT_EXEC_ERROR, "move error occured", __FUNCTION__);
		return (-1);
	}
	moved = (-1);
	if (sscanf(res->command, "MOVE " FORMAT_ULEN, &moved) > 0)
	{
		moved++;
		self->cursTuple += moved;
		if (!QR_once_reached_eof(self))
		{
			self->num_total_read = self->cursTuple;
			QR_set_reached_eof(self);
		}
	}
	QR_Destructor(res);

	return	moved;
}

/*	This function is called by fetch_tuples() AND SQLFetch() */
int
QR_next_tuple(QResultClass *self, StatementClass *stmt)
{
	CSTR	func = "QR_next_tuple";
	int			ret = TRUE;

	/* Speed up access */
	SQLLEN		fetch_number = self->fetch_number, cur_fetch = 0;
	SQLLEN		num_total_rows;
	SQLLEN		num_backend_rows = self->num_cached_rows, num_rows_in;
	Int4		num_fields = self->num_fields, fetch_size, req_size;
	SQLLEN		offset = 0, end_tuple;
	char		boundary_adjusted = FALSE;
	TupleField *the_tuples = self->backend_tuples;
	QResultClass	*res;

	/* QR_set_command() dups this string so doesn't need static */
	char		fetch[128];
	QueryInfo	qi;
	ConnectionClass	*conn;
	ConnInfo   *ci;
	BOOL		reached_eof_now = FALSE, curr_eof; /* detecting EOF is pretty important */

MYLOG(DETAIL_LOG_LEVEL, "Oh %p->fetch_number=" FORMAT_LEN "\n", self, self->fetch_number);
MYLOG(DETAIL_LOG_LEVEL, "in total_read=" FORMAT_ULEN " cursT=" FORMAT_LEN " currT=" FORMAT_LEN " ad=%d total=" FORMAT_ULEN " rowsetSize=%d\n", self->num_total_read, self->cursTuple, stmt->currTuple, self->ad_count, QR_get_num_total_tuples(self), self->rowset_size_include_ommitted);

	num_total_rows = QR_get_num_total_tuples(self);
	conn = QR_get_conn(self);
	curr_eof = FALSE;
	req_size = QR_get_reqsize(self);
	/* Determine the optimum cache size.  */
	ci = &(conn->connInfo);
	fetch_size = ci->drivers.fetch_max;
	if ((Int4)req_size > fetch_size)
		fetch_size = req_size;
	if (QR_once_reached_eof(self) && self->cursTuple >= (Int4) QR_get_num_total_read(self))
		curr_eof = TRUE;
#define	return	DONT_CALL_RETURN_FROM_HERE???
#define	RETURN(code)	{ ret = code; goto cleanup;}
	ENTER_CONN_CS(conn);
	if (0 != self->move_offset)
	{
		char		movecmd[256];
		QResultClass	*mres = NULL;
		SQLULEN		movement, moved;

		movement = self->move_offset;
		if (QR_is_moving_backward(self))
		{
			if (fetch_size > req_size)
			{
				SQLLEN	incr_move = fetch_size - (req_size < 0 ? 1 : req_size);

				movement += incr_move;
				if (movement > (UInt4)(self->cursTuple + 1))
					movement = self->cursTuple + 1;
			}
			else
				self->cache_size = req_size;
MYLOG(DETAIL_LOG_LEVEL, "cache=" FORMAT_ULEN " rowset=%d movement=" FORMAT_ULEN "\n", self->cache_size, req_size, movement);
			SPRINTF_FIXED(movecmd,
					 "move backward " FORMAT_ULEN " in \"%s\"",
					 movement, QR_get_cursor(self));
		}
		else if (QR_is_moving_forward(self))
			SPRINTF_FIXED(movecmd,
					 "move " FORMAT_ULEN " in \"%s\"",
					 movement, QR_get_cursor(self));
		else
		{
			SPRINTF_FIXED(movecmd,
					 "move all in \"%s\"",
					 QR_get_cursor(self));
			movement = INT_MAX;
		}
		mres = CC_send_query(conn, movecmd, NULL, READ_ONLY_QUERY, stmt);
		if (!QR_command_maybe_successful(mres))
		{
			QR_Destructor(mres);
			SC_set_error(stmt, STMT_EXEC_ERROR, "move error occured", func);
			RETURN(-1)
		}
		moved = movement;
		if (sscanf(mres->command, "MOVE " FORMAT_ULEN, &moved) > 0)
		{
MYLOG(DETAIL_LOG_LEVEL, "moved=" FORMAT_ULEN " ? " FORMAT_ULEN "\n", moved, movement);
			if (moved < movement)
			{
				if (0 <  moved)
					moved++;
				else if (QR_is_moving_backward(self) && self->cursTuple < 0)
					;
				else if (QR_is_moving_not_backward(self) && curr_eof)
					;
				else
					moved++;
				if (QR_is_moving_not_backward(self))
				{
					curr_eof = TRUE;
					if (!QR_once_reached_eof(self))
					{
						self->num_total_read = self->cursTuple + moved;
						QR_set_reached_eof(self);
					}
				}
			}
		}
		/* ... by the following call */
		QR_set_rowstart_in_cache(self, -1);
		if (QR_is_moving_backward(self))
		{
			self->cursTuple -= moved;
			offset = moved - self->move_offset;
		}
		else
		{
			self->cursTuple += moved;
			offset = self->move_offset - moved;
		}
		QR_Destructor(mres);

		self->move_offset = 0;
		num_backend_rows = self->num_cached_rows;
	}
	else if (fetch_number < num_backend_rows)
	{
		if (!self->dataFilled) /* should never occur */
		{
			SC_set_error(stmt, STMT_EXEC_ERROR, "Hmm where are fetched data?", func);
			RETURN(-1)
		}
		/* return a row from cache */
		MYLOG(0, "fetch_number < fcount: returning tuple " FORMAT_LEN ", fcount = " FORMAT_LEN "\n", fetch_number, num_backend_rows);
		self->tupleField = the_tuples + (fetch_number * num_fields);
MYLOG(DETAIL_LOG_LEVEL, "tupleField=%p\n", self->tupleField);
		/* move to next row */
		QR_inc_next_in_cache(self);
		RETURN(TRUE)
	}
	else if (QR_once_reached_eof(self))
	{
		BOOL	reached_eod = FALSE;

		if (stmt->currTuple + 1 >= num_total_rows)
			reached_eod = TRUE;
		if (reached_eod)
		{
			MYLOG(0, "next_tuple: fetch end\n");
			self->tupleField = NULL;
			/* end of tuples */
			RETURN(-1)
		}
	}

	end_tuple = req_size + QR_get_rowstart_in_cache(self);
	/*
	 * See if we need to fetch another group of rows. We may be being
	 * called from send_query(), and if so, don't send another fetch,
	 * just fall through and read the tuples.
	 */
	self->tupleField = NULL;

	if (!QR_get_cursor(self))
	{
		MYLOG(0, "ALL_ROWS: done, fcount = " FORMAT_ULEN ", fetch_number = " FORMAT_LEN "\n", QR_get_num_total_tuples(self), fetch_number);
		self->tupleField = NULL;
		QR_set_reached_eof(self);
		RETURN(-1)		/* end of tuples */
	}

	if (QR_get_rowstart_in_cache(self) >= num_backend_rows ||
		QR_is_moving(self))
	{
		TupleField *tuple = self->backend_tuples;

		/* not a correction */
		self->cache_size = fetch_size;
		/* clear obsolete tuples */
MYLOG(DETAIL_LOG_LEVEL, "clear obsolete " FORMAT_LEN " tuples\n", num_backend_rows);
		ClearCachedRows(tuple, num_fields, num_backend_rows);
		self->dataFilled = FALSE;
		QR_stop_movement(self);
		self->move_offset = 0;
		QR_set_next_in_cache(self, offset + 1);
	}
	else
	{
		/*
		 *	The rowset boundary doesn't match that of
		 *	the inner resultset. Enlarge the resultset
		 *	and fetch the rest of the rowset.
		 */
		/* The next fetch size is */
		fetch_size = (Int4) (end_tuple - num_backend_rows);
		if (fetch_size <= 0)
		{
			MYLOG(0, "corrupted fetch_size end_tuple=" FORMAT_LEN " <= cached_rows=" FORMAT_LEN "\n", end_tuple, num_backend_rows);
			RETURN(-1)
		}
		/* and enlarge the cache size */
		self->cache_size += fetch_size;
		offset = self->fetch_number;
		QR_inc_next_in_cache(self);
		boundary_adjusted = TRUE;
	}

	if (enlargeKeyCache(self, self->cache_size - num_backend_rows, "Out of memory while reading tuples") < 0)
		RETURN(FALSE)

	/* Send a FETCH command to get more rows */
	SPRINTF_FIXED(fetch,
			 "fetch %d in \"%s\"",
			 fetch_size, QR_get_cursor(self));

	MYLOG(0, "sending actual fetch (%d) query '%s'\n", fetch_size, fetch);
	if (!boundary_adjusted)
	{
		QR_set_num_cached_rows(self, 0);
		QR_set_rowstart_in_cache(self, offset);
	}
	num_rows_in = self->num_cached_rows;

	/* don't read ahead for the next tuple (self) ! */
	qi.row_size = self->cache_size;
	qi.fetch_size = fetch_size;
	qi.result_in = self;
	qi.cursor = NULL;
	res = CC_send_query(conn, fetch, &qi, READ_ONLY_QUERY, stmt);
	if (!QR_command_maybe_successful(res))
	{
		if (!QR_get_message(self))
			QR_set_message(self, "Error fetching next group.");
		RETURN(FALSE)
	}
	cur_fetch = 0;

	self->tupleField = NULL;

	curr_eof = reached_eof_now = (QR_once_reached_eof(self) && self->cursTuple >= (Int4)self->num_total_read);
MYLOG(DETAIL_LOG_LEVEL, "reached_eof_now=%d\n", reached_eof_now);

	MYLOG(0, ": PGresult: fetch_total = " FORMAT_ULEN " & this_fetch = " FORMAT_ULEN "\n", self->num_total_read, self->num_cached_rows);
	MYLOG(0, ": PGresult: cursTuple = " FORMAT_LEN ", offset = " FORMAT_LEN "\n", self->cursTuple, offset);

	cur_fetch = self->num_cached_rows - num_rows_in;
	if (!ret)
		RETURN(ret)

	{
		SQLLEN	start_idx = 0;

		num_backend_rows = self->num_cached_rows;
		if (reached_eof_now)
		{
			MYLOG(0, "reached eof now\n");
			QR_set_reached_eof(self);
			if (self->ad_count > 0 && cur_fetch < fetch_size)
			{
				/* We have to append the tuples(keys) info from the added tuples(keys) here */
				SQLLEN	add_size;
				TupleField	*tuple, *added_tuple;

				start_idx = CacheIdx2GIdx(offset, stmt, self) - self->num_total_read;
				if (curr_eof && start_idx >= 0)
				{
					add_size = self->ad_count - start_idx;
					if (0 == num_backend_rows)
					{
						offset = 0;
						QR_set_rowstart_in_cache(self, offset);
						QR_set_next_in_cache(self, offset);
					}
				}
				else
				{
					start_idx = 0;
					add_size = self->ad_count;
				}
				if (add_size > fetch_size - cur_fetch)
					add_size = fetch_size - cur_fetch;
				else if (add_size < 0)
					add_size = 0;
MYLOG(DETAIL_LOG_LEVEL, "will add " FORMAT_LEN " added_tuples from " FORMAT_LEN " and select the " FORMAT_LEN "th added tuple " FORMAT_LEN "\n", add_size, start_idx, offset - num_backend_rows + start_idx, cur_fetch);
				if (enlargeKeyCache(self, add_size, "Out of memory while adding tuples") < 0)
					RETURN(FALSE)
				/* append the KeySet info first */
				memcpy(self->keyset + num_backend_rows, (void *)(self->added_keyset + start_idx), sizeof(KeySet) * add_size);
				/* and append the tuples info */
				tuple = self->backend_tuples + num_fields * num_backend_rows;
				memset(tuple, 0, sizeof(TupleField) * num_fields * add_size);
				added_tuple = self->added_tuples + num_fields * start_idx;
				ReplaceCachedRows(tuple, added_tuple, num_fields, add_size);
				self->num_cached_rows += add_size;
				self->num_cached_keys += add_size;
				num_backend_rows = self->num_cached_rows;
			}
		}
		if (offset < num_backend_rows)
		{
			/* set to first row */
			self->tupleField = self->backend_tuples + (offset * num_fields);
		}
		else
		{
			/* We are surely done here (we read 0 tuples) */
			MYLOG(0, " 'C': DONE (fcount == " FORMAT_LEN ")\n", num_backend_rows);
			ret = -1;	/* end of tuples */
		}
	}

	/*
	 If the cursor operation was invoked inside this function,
	 we have to set the status bits here.
	*/
	if (self->keyset && (self->dl_count > 0 || self->up_count > 0))
	{
		SQLLEN	i, lf;
		SQLLEN	lidx, hidx, lkidx, hkidx;
		SQLLEN	*deleted = self->deleted, *updated = self->updated;

		num_backend_rows = QR_get_num_cached_tuples(self);
		lidx = CacheIdx2GIdx(num_rows_in, stmt, self);
		hidx = CacheIdx2GIdx(num_backend_rows, stmt, self);
		lkidx = GIdx2KResIdx(lidx, stmt, self);
		hkidx = GIdx2KResIdx(hidx, stmt, self);
		/* For simplicty, use CURS_NEEDS_REREAD bit to mark the row */
		for (i = lkidx; i < hkidx; i++)
			self->keyset[i].status |= CURS_NEEDS_REREAD;
		/* deleted info */
		for (i = 0; i < self->dl_count && hidx > deleted[i]; i++)
		{
			if (lidx <= deleted[i])
			{
				lf = GIdx2KResIdx(deleted[i], stmt, self);
				if (lf >= 0 && lf < self->num_cached_keys)
				{
					self->keyset[lf].status = self->deleted_keyset[i].status;
					/* mark the row off */
					self->keyset[lf].status &= (~CURS_NEEDS_REREAD);
				}
			}
		}
		for (i = self->up_count - 1; i >= 0; i--)
		{
			if (hidx > updated[i] &&
			    lidx <= updated[i])
			{
				lf = GIdx2KResIdx(updated[i], stmt, self);
				/* in case the row is marked off */
				if (0 == (self->keyset[lf].status & CURS_NEEDS_REREAD))
					continue;
				self->keyset[lf] = self->updated_keyset[i];
				ReplaceCachedRows(self->backend_tuples + lf * num_fields, self->updated_tuples + i * num_fields, num_fields, 1);
				self->keyset[lf].status &= (~CURS_NEEDS_REREAD);
			}
		}
		/* reset CURS_NEEDS_REREAD bit */
		for (i = 0; i < num_backend_rows; i++)
		{
			self->keyset[i].status &= (~CURS_NEEDS_REREAD);
/*MYLOG(DETAIL_LOG_LEVEL, "keyset[%d].status=%x\n", i, self->keyset[i].status);*/
		}
	}

cleanup:
	LEAVE_CONN_CS(conn);
#undef	RETURN
#undef	return
MYLOG(DETAIL_LOG_LEVEL, "returning %d offset=" FORMAT_LEN "\n", ret, offset);
	return ret;
}

/*
 * Read tuples from a libpq PGresult object into QResultClass.
 *
 * The result status of the passed-in PGresult should be either
 * PGRES_TUPLES_OK, or PGRES_SINGLE_TUPLE. If it's PGRES_SINGLE_TUPLE,
 * this function will call PQgetResult() to read all the available tuples.
 */
static BOOL
QR_read_tuples_from_pgres(QResultClass *self, PGresult **pgres)
{
	Int2		field_lf;
	int			len;
	char	   *value;
	char	   *buffer;
	int		ci_num_fields = QR_NumResultCols(self);	/* speed up access */
	int		num_fields = self->num_fields;	/* speed up access */
	ColumnInfoClass *flds;
	int		effective_cols;
	char		tidoidbuf[32];
	int			rowno;
	int			nrows;
	int			resStatus;
	int		numTotalRows = 0;

	/* set the current row to read the fields into */
	effective_cols = QR_NumPublicResultCols(self);

	flds = QR_get_fields(self);

nextrow:
	resStatus = PQresultStatus(*pgres);
	switch (resStatus)
	{
		case PGRES_TUPLES_OK:
			QLOG(0, "\tok: - 'T' - %s\n", PQcmdStatus(*pgres));
			break;
		case PGRES_SINGLE_TUPLE:
			break;

		case PGRES_NONFATAL_ERROR:
		case PGRES_BAD_RESPONSE:
		case PGRES_FATAL_ERROR:
		default:
			handle_pgres_error(self->conn, *pgres, "read_tuples", self, TRUE);
			QR_set_rstatus(self, PORES_FATAL_ERROR);
			return FALSE;
	}

	nrows = PQntuples(*pgres);
	numTotalRows += nrows;

	for (rowno = 0; rowno < nrows; rowno++)
	{
		TupleField *this_tuplefield;
		KeySet	*this_keyset = NULL;

		if (!QR_prepare_for_tupledata(self))
			return FALSE;

		this_tuplefield = self->backend_tuples + (self->num_cached_rows * num_fields);
		if (QR_haskeyset(self))
		{
			/* this_keyset = self->keyset + self->cursTuple + 1; */
			this_keyset = self->keyset + self->num_cached_keys;
			this_keyset->status = 0;
		}

		QLOG(TUPLE_LOG_LEVEL, "\t");
		for (field_lf = 0; field_lf < ci_num_fields; field_lf++)
		{
			BOOL isnull = FALSE;

			isnull = PQgetisnull(*pgres, rowno, field_lf);

			if (isnull)
			{
				this_tuplefield[field_lf].len = 0;
				this_tuplefield[field_lf].value = 0;
				QPRINTF(TUPLE_LOG_LEVEL, " (null)");
				continue;
			}
			else
			{
				len = PQgetlength(*pgres, rowno, field_lf);
				value = PQgetvalue(*pgres, rowno, field_lf);
				if (field_lf >= effective_cols)
					buffer = tidoidbuf;
				else
				{
					QR_MALLOC_return_with_error(buffer, char, len + 1, self, "Out of memory in allocating item buffer.", FALSE);
				}
				memcpy(buffer, value, len);
				buffer[len] = '\0';

				QPRINTF(TUPLE_LOG_LEVEL, " '%s'(%d)", buffer, len);

				if (field_lf >= effective_cols)
				{
					if (NULL == this_keyset)
					{
						char	emsg[128];

						QR_set_rstatus(self, PORES_INTERNAL_ERROR);
						SPRINTF_FIXED(emsg, "Internal Error -- this_keyset == NULL ci_num_fields=%d effective_cols=%d", ci_num_fields, effective_cols);
						QR_set_message(self, emsg);
						return FALSE;
					}
					if (field_lf == effective_cols)
						sscanf(buffer, "(%u,%hu)",
							   &this_keyset->blocknum, &this_keyset->offset);
					else
						this_keyset->oid = strtoul(buffer, NULL, 10);
				}
				else
				{
					this_tuplefield[field_lf].len = len;
					this_tuplefield[field_lf].value = buffer;

					/*
					 * This can be used to set the longest length of the column
					 * for any row in the tuple cache.	It would not be accurate
					 * for varchar and text fields to use this since a tuple cache
					 * is only 100 rows. Bpchar can be handled since the strlen of
					 * all rows is fixed, assuming there are not 100 nulls in a
					 * row!
					 */

					if (flds && flds->coli_array && CI_get_display_size(flds, field_lf) < len)
						CI_get_display_size(flds, field_lf) = len;
				}
			}
		}
		QPRINTF(TUPLE_LOG_LEVEL, "\n");
		self->cursTuple++;
		if (self->num_fields > 0)
		{
			QR_inc_num_cache(self);
		}
		else if (QR_haskeyset(self))
			self->num_cached_keys++;

		if (self->cursTuple >= self->num_total_read)
			self->num_total_read = self->cursTuple + 1;
	}

	if (resStatus == PGRES_SINGLE_TUPLE)
	{
		/* Process next row */
		PQclear(*pgres);

		*pgres = PQgetResult(self->conn->pqconn);
		goto nextrow;
	}

	self->dataFilled = TRUE;
	self->tupleField = self->backend_tuples + (self->fetch_number * self->num_fields);
MYLOG(DETAIL_LOG_LEVEL, "tupleField=%p\n", self->tupleField);

	QR_set_rstatus(self, PORES_TUPLES_OK);

	return TRUE;
}
