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
 * Comments:		See "notice.txt" for copyright and license information.
 *---------
 */

#include "qresult.h"
#include "statement.h"

#include "misc.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

static char QR_read_a_tuple_from_db(QResultClass *, char);

/*
 *	Used for building a Manual Result only
 *	All info functions call this function to create the manual result set.
 */
void
QR_set_num_fields(QResultClass *self, int new_num_fields)
{
	BOOL	allocrelatt = FALSE;

	if (!self)	return;
	mylog("in QR_set_num_fields\n");

	CI_set_num_fields(self->fields, new_num_fields, allocrelatt);

	mylog("exit QR_set_num_fields\n");
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
QR_set_rowset_size(QResultClass *self, Int4 rowset_size)
{
	self->rowset_size_include_ommitted = rowset_size;
}

void
QR_set_cursor(QResultClass *self, const char *name)
{
	ConnectionClass	*conn = QR_get_conn(self);

	if (self->cursor_name)
	{
		free(self->cursor_name);
		if (conn)
		{
			CONNLOCK_ACQUIRE(conn);
			conn->ncursors--;
			CONNLOCK_RELEASE(conn);
		}
		self->cursTuple = -1;
		self->pstatus = 0;
	}
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
		QR_set_no_cursor(self);
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
		mylog("QR_inc_rowstart_in_cache called while the cache is not ready\n");
	self->base += base_inc;
	if (QR_synchronize_keys(self))
		self->key_base = self->base;
}


/*
 * CLASS QResult
 */
QResultClass *
QR_Constructor()
{
	QResultClass *rv;

	mylog("in QR_Constructor\n");
	rv = (QResultClass *) malloc(sizeof(QResultClass));

	if (rv != NULL)
	{
		rv->rstatus = PORES_EMPTY_QUERY;
		rv->pstatus = 0;

		/* construct the column info */
		if (!(rv->fields = CI_Constructor()))
		{
			free(rv);
			return NULL;
		}
		rv->backend_tuples = NULL;
		rv->sqlstate[0] = '\0';
		rv->message = NULL;
		rv->messageref = NULL;
		rv->command = NULL;
		rv->notice = NULL;
		rv->conn = NULL;
		rv->next = NULL;
		rv->pstatus = 0;
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

	mylog("exit QR_Constructor\n");
	return rv;
}


void
QR_close_result(QResultClass *self, BOOL destroy)
{
	ConnectionClass	*conn;

	if (!self)	return;
	mylog("QResult: in QR_close_result\n");

	/*
	 * If conn is defined, then we may have used "backend_tuples", so in
	 * case we need to, free it up.  Also, close the cursor.
	 */
	if ((conn = QR_get_conn(self)) && conn->sock)
	{
		if (CC_is_in_trans(conn) ||
		    QR_is_withhold(self))
		{
			if (!QR_close(self))	/* close the cursor if there is one */
			{
			}
		}
	}

	QR_free_memory(self);		/* safe to call anyway */

	/* Should have been freed in the close() but just in case... */
	QR_set_cursor(self, NULL);

	/* Free up column info */
	if (destroy && self->fields)
	{
		CI_Destructor(self->fields);
		self->fields = NULL;
	}

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
	QR_Destructor(self->next);
	self->next = NULL;

	mylog("QResult: exit close_result\n");
	if (destroy)
	{
		free(self);
	}
}

void
QR_Destructor(QResultClass *self)
{
	mylog("QResult: enter DESTRUCTOR\n");
	if (!self)	return;
	QR_close_result(self, TRUE);

	mylog("QResult: exit DESTRUCTOR\n");
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
	size_t	alsize, pos;

	if (!msg || !msg[0])
		return;
	if (message)
	{
		pos = strlen(message) + 1;
		alsize = pos + strlen(msg) + 1;
	}
	else
	{
		pos = 0;
		alsize = strlen(msg) + 1;
	}
	if (message = realloc(message, alsize), NULL == message)
		return;
	if (pos > 0)
		message[pos - 1] = ';';
	strcpy(message + pos, msg);
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
	size_t	alsize, pos;

	if (!msg || !msg[0])
		return;
	if (message)
	{
		pos = strlen(message) + 1;
		alsize = pos + strlen(msg) + 1;
	}
	else
	{
		pos = 0;
		alsize = strlen(msg) + 1;
	}
	if (message = realloc(message, alsize), NULL == message)
		return;
	if (pos > 0)
		message[pos - 1] = ';';
	strcpy(message + pos, msg);
	self->notice = message;
}


TupleField	*QR_AddNew(QResultClass *self)
{
	size_t	alloc;
	UInt4	num_fields;

	if (!self)	return	NULL;
inolog("QR_AddNew %dth row(%d fields) alloc=%d\n", self->num_cached_rows, QR_NumResultCols(self), self->count_backend_allocated);
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

	mylog("QResult: free memory in, fcount=%d\n", num_backend_rows);

	if (self->backend_tuples)
	{
		ClearCachedRows(self->backend_tuples, num_fields, num_backend_rows);
		free(self->backend_tuples);
		self->count_backend_allocated = 0;
		self->backend_tuples = NULL;
		self->dataFilled = FALSE;
	}
	if (self->keyset)
	{
		ConnectionClass	*conn = QR_get_conn(self);

		free(self->keyset);
		self->keyset = NULL;
		self->count_keyset_allocated = 0;
		if (self->reload_count > 0 && conn && conn->sock)
		{
			char	plannm[32];

			sprintf(plannm, "_KEYSET_%p", self);
			if (CC_is_in_error_trans(conn))
			{
				CC_mark_a_object_to_discard(conn, 's',plannm);
			}
			else
			{
				QResultClass	*res;
				char		cmd[64];

				sprintf(cmd, "DEALLOCATE \"%s\"", plannm);
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

	mylog("QResult: free memory out\n");
}


/*	This function is called by send_query() */
char
QR_fetch_tuples(QResultClass *self, ConnectionClass *conn, const char *cursor, int *LastMessageType)
{
	CSTR		func = "QR_fetch_tuples";
	SQLLEN			tuple_size;

	/*
	 * If called from send_query the first time (conn != NULL), then set
	 * the inTuples state, and read the tuples.  If conn is NULL, it
	 * implies that we are being called from next_tuple(), like to get
	 * more rows so don't call next_tuple again!
	 */
	if (conn != NULL)
	{
		ConnInfo   *ci = &(conn->connInfo);
		BOOL		fetch_cursor = (cursor && cursor[0]);

		if (NULL != LastMessageType)
			*LastMessageType = 0;
		QR_set_conn(self, conn);

		mylog("%s: cursor = '%s', self->cursor=%p\n", func, (cursor == NULL) ? "" : cursor, QR_get_cursor(self));

		if (cursor && !cursor[0])
			cursor = NULL;
		if (fetch_cursor)
		{
			if (!cursor)
			{
				QR_set_rstatus(self, PORES_INTERNAL_ERROR);
				QR_set_message(self, "Internal Error -- no cursor for fetch");
				return FALSE;
			}
		}
		QR_set_cursor(self, cursor);

		/*
		 * Read the field attributes.
		 *
		 * $$$$ Should do some error control HERE! $$$$
		 */
		if (CI_read_fields(QR_get_fields(self), QR_get_conn(self)))
		{
			QR_set_rstatus(self, PORES_FIELDS_OK);
			self->num_fields = CI_get_num_fields(self->fields);
			if (QR_haskeyset(self))
				self->num_fields -= self->num_key_fields;
		}
		else
		{
			if (NULL == QR_get_fields(self)->coli_array)
			{
				QR_set_rstatus(self, PORES_NO_MEMORY_ERROR);
				QR_set_messageref(self, "Out of memory while reading field information");
			}
			else
			{
				QR_set_rstatus(self, PORES_BAD_RESPONSE);
				QR_set_message(self, "Error reading field information");
			}
			return FALSE;
		}

		mylog("%s: past CI_read_fields: num_fields = %d\n", func, self->num_fields);

		if (fetch_cursor)
		{
			if (self->cache_size <= 0)
				self->cache_size = ci->drivers.fetch_max;
			tuple_size = self->cache_size;
		}
		else
			tuple_size = TUPLE_MALLOC_INC;

		/* allocate memory for the tuple cache */
		mylog("MALLOC: tuple_size = %d, size = %d\n", tuple_size, self->num_fields * sizeof(TupleField) * tuple_size);
		self->count_backend_allocated = self->count_keyset_allocated = 0;
		if (self->num_fields > 0)
		{
			QR_MALLOC_return_with_error(self->backend_tuples, TupleField, self->num_fields * sizeof(TupleField) * tuple_size, self, "Could not get memory for tuple cache.", FALSE);
			self->count_backend_allocated = tuple_size;
		}
		if (QR_haskeyset(self))
		{
			QR_MALLOC_return_with_error(self->keyset, KeySet, sizeof(KeySet) * tuple_size, self, "Could not get memory for key cache.", FALSE);
			memset(self->keyset, 0, sizeof(KeySet) * tuple_size);
			self->count_keyset_allocated = tuple_size;
		}

		QR_set_fetching_tuples(self);

		/* Force a read to occur in next_tuple */
		QR_set_num_cached_rows(self, 0);
		QR_set_next_in_cache(self, 0);
		QR_set_rowstart_in_cache(self, 0);
		self->key_base = 0;

		return QR_next_tuple(self, NULL, LastMessageType);
	}
	else
	{
		/*
		 * Always have to read the field attributes. But we dont have to
		 * reallocate memory for them!
		 */

		if (!CI_read_fields(NULL, QR_get_conn(self)))
		{
			QR_set_rstatus(self, PORES_BAD_RESPONSE);
			QR_set_message(self, "Error reading field information");
			return FALSE;
		}
		return TRUE;
	}
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
			UDWORD		flag = 0;
			char		buf[64];

			if (QR_needs_survival_check(self))
				flag = ROLLBACK_ON_ERROR | IGNORE_ABORT_ON_CONN;

			snprintf(buf, sizeof(buf), "close \"%s\"", QR_get_cursor(self));
			/* End the transaction if there are no cursors left on this conn */
			if (CC_is_in_trans(conn) &&
			    CC_does_autocommit(conn) &&
			    CC_cursor_count(conn) <= 1)
			{
				mylog("QResult: END transaction on conn=%p\n", conn);
				if ((ROLLBACK_ON_ERROR & flag) == 0)
				{
					strlcat(buf, ";commit", sizeof(buf));
					flag |= END_WITH_COMMIT;
					QR_set_cursor(self, NULL);
				}
				else
					does_commit = TRUE;
			}

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

		QR_set_no_fetching_tuples(self);
		self->cursTuple = -1;

		QR_set_cursor(self, NULL);
		QR_set_has_valid_base(self);
		if (!ret)
			return ret;

#ifdef	NOT_USED
		/* End the transaction if there are no cursors left on this conn */
		if (CC_does_autocommit(conn) && CC_cursor_count(conn) == 0)
		{
			mylog("QResult: END transaction on conn=%p\n", conn);

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


BOOL
QR_get_tupledata(QResultClass *self, BOOL binary)
{
	BOOL	haskeyset = QR_haskeyset(self);
	SQLULEN num_total_rows = QR_get_num_total_tuples(self);

inolog("QR_get_tupledata %p->num_fields=%d\n", self, self->num_fields);
	if (!QR_get_cursor(self))
	{
 
		if (self->num_fields > 0 &&
		    num_total_rows >= self->count_backend_allocated)
		{
			SQLLEN	tuple_size = self->count_backend_allocated;

			mylog("REALLOC: old_count = %d, size = %d\n", tuple_size, self->num_fields * sizeof(TupleField) * tuple_size);
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
			self->count_keyset_allocated = tuple_size;
		}
	}

	if (!QR_read_a_tuple_from_db(self, (char) binary))
	{
		if (0 == QR_get_rstatus(self))
		{
			QR_set_rstatus(self, PORES_BAD_RESPONSE);
			QR_set_message(self, "Error reading the tuple");
		}
		return FALSE;
	}
inolog("!!%p->cursTup=%d total_read=%d\n", self, self->cursTuple, self->num_total_read);
	if (!QR_once_reached_eof(self) && self->cursTuple >= (Int4) self->num_total_read)
		self->num_total_read = self->cursTuple + 1;
inolog("!!cursTup=%d total_read=%d\n", self->cursTuple, self->num_total_read);
	if (self->num_fields > 0)
	{
		QR_inc_num_cache(self);
	}
	else if (haskeyset)
		self->num_cached_keys++;

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

/*	This function is called by fetch_tuples() AND SQLFetch() */
int
QR_next_tuple(QResultClass *self, StatementClass *stmt, int *LastMessageType)
{
	CSTR	func = "QR_next_tuple";
	int			id, ret = TRUE;
	SocketClass *sock;

	/* Speed up access */
	SQLLEN		fetch_number = self->fetch_number, cur_fetch = 0;
	SQLLEN		num_total_rows;
	SQLLEN		num_backend_rows = self->num_cached_rows, num_rows_in;
	Int4		num_fields = self->num_fields, fetch_size, req_size;
	SQLLEN		offset = 0, end_tuple;
	char		boundary_adjusted = FALSE;
	TupleField *the_tuples = self->backend_tuples;

	/* ERROR_MSG_LENGTH is sufficient */
	char msgbuffer[ERROR_MSG_LENGTH + 1];

	/* QR_set_command() dups this string so doesn't need static */
	char		cmdbuffer[ERROR_MSG_LENGTH + 1];
	char		fetch[128];
	QueryInfo	qi;
	ConnectionClass	*conn;
	ConnInfo   *ci = NULL;
	BOOL		msg_truncated, rcvend, loopend, kill_conn, internally_invoked = FALSE;
	BOOL		reached_eof_now = FALSE, curr_eof; /* detecting EOF is pretty important */
	BOOL		ExecuteRequest = FALSE;
	Int4		response_length;

inolog("Oh %p->fetch_number=%d\n", self, self->fetch_number);
inolog("in total_read=%d cursT=%d currT=%d ad=%d total=%d rowsetSize=%d\n", self->num_total_read, self->cursTuple, stmt ? stmt->currTuple : -1, self->ad_count, QR_get_num_total_tuples(self), self->rowset_size_include_ommitted);

	if (NULL != LastMessageType)
		*LastMessageType = 0;
	num_total_rows = QR_get_num_total_tuples(self);
	conn = QR_get_conn(self);
	curr_eof = FALSE;
	req_size = self->rowset_size_include_ommitted;
	if (QR_once_reached_eof(self) && self->cursTuple >= (Int4) QR_get_num_total_read(self))
		curr_eof = TRUE;
	if (0 != self->move_offset)
	{
		char		movecmd[256];
		QResultClass	*mres;
		SQLULEN		movement, moved;

		movement = self->move_offset;
		if (QR_is_moving_backward(self))
		{
			if (self->cache_size > req_size)
			{
				SQLLEN	incr_move = self->cache_size - (req_size < 0 ? 1 : req_size);
				
				movement += incr_move;
				if (movement > (UInt4)(self->cursTuple + 1))
					movement = self->cursTuple + 1;
			}
			else
				self->cache_size = req_size;
inolog("cache=%d rowset=%d movement=" FORMAT_ULEN "\n", self->cache_size, req_size, movement);
			sprintf(movecmd, "move backward " FORMAT_ULEN " in \"%s\"", movement, QR_get_cursor(self));
		}
		else if (QR_is_moving_forward(self))
			sprintf(movecmd, "move " FORMAT_ULEN " in \"%s\"", movement, QR_get_cursor(self));
		else
		{
			sprintf(movecmd, "move all in \"%s\"", QR_get_cursor(self));
			movement = INT_MAX;
		}
		mres = CC_send_query(conn, movecmd, NULL, 0, stmt);
		if (!QR_command_maybe_successful(mres))
		{
			QR_Destructor(mres);
			if (stmt)
				SC_set_error(stmt, STMT_EXEC_ERROR, "move error occured", func);
			return -1;
		}
		moved = movement;
		if (sscanf(mres->command, "MOVE " FORMAT_ULEN, &moved) > 0)
		{
inolog("moved=%d ? " FORMAT_ULEN "\n", moved, movement);
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
				if (QR_is_moving_from_the_last(self))  /* in case of FETCH LAST */
				{
					SQLULEN	bmovement, mback;
					SQLLEN	rowset_start = self->cursTuple + 1, back_offset = self->move_offset, backpt;
inolog("FETCH LAST case\n");
					if (getNthValid(self, QR_get_num_total_tuples(self) - 1, SQL_FETCH_PRIOR, self->move_offset, &backpt) < 0)
					{
						/* the rowset_start is on BOF */
						self->tupleField = NULL;
						SC_set_rowset_start(stmt, -1, TRUE);
						stmt->currTuple = -1;
						return -1;
					}
					back_offset = QR_get_num_total_tuples(self) - backpt;
inolog("back_offset=%d and move_offset=%d\n", back_offset, self->move_offset);
					if (back_offset + 1 > (Int4) self->ad_count)
					{
						bmovement = back_offset + 1 - self->ad_count;
						sprintf(movecmd, "move backward " FORMAT_ULEN " in \"%s\"", bmovement, QR_get_cursor(self));
						QR_Destructor(mres);
						mres = CC_send_query(conn, movecmd, NULL, 0, stmt);
						if (!QR_command_maybe_successful(mres))
						{
							QR_Destructor(mres);
							if (stmt)
								SC_set_error(stmt, STMT_EXEC_ERROR, "move error occured", func);
							return -1;
						}

						if (sscanf(mres->command, "MOVE " FORMAT_ULEN, &mback) > 0)
						{
							if (mback < bmovement)
								mback++;
							if (moved < mback)
							{
								QR_set_move_backward(self);
								mback -= moved;
								moved = mback;
								self->move_offset = moved;
								rowset_start = self->cursTuple - moved + 1;
							}
							else
							{
								QR_set_move_forward(self);
								moved-= mback;
								self->move_offset = moved;
								rowset_start = self->cursTuple + moved + 1;
							}
						}
					}
					else
					{
						QR_set_move_forward(self);
						self->move_offset = moved + self->ad_count - back_offset - 1;
						rowset_start = self->cursTuple +self->move_offset + 1;
						/* adjust move_offset */
						/*** self->move_offset++; ***/
					}
					if (stmt)
					{
						SC_set_rowset_start(stmt, rowset_start, TRUE); /* affects the result's rowset_start but it is reset immediately ... */
						stmt->currTuple = RowIdx2GIdx(-1, stmt);
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
			if (stmt)
				SC_set_error(stmt, STMT_EXEC_ERROR, "Hmm where are fetched data?", func);
			return -1;
		}
		/* return a row from cache */
		mylog("%s: fetch_number < fcount: returning tuple %d, fcount = %d\n", func, fetch_number, num_backend_rows);
		self->tupleField = the_tuples + (fetch_number * num_fields);
inolog("tupleField=%p\n", self->tupleField);
		/* move to next row */
		QR_inc_next_in_cache(self);
		return TRUE;
	}
	else if (QR_once_reached_eof(self))
	{
		BOOL	reached_eod = FALSE;
		SQLULEN	num_total_read = self->num_total_read;

		if (stmt)
		{
			if (stmt->currTuple + 1 >= num_total_rows)
				reached_eod = TRUE;
		}
		else if (self->cursTuple + 1 >= (Int4)num_total_read)
		{
			if (self->ad_count == 0)
				reached_eod = TRUE;
		}
		if (reached_eod)
		{
			mylog("next_tuple: fetch end\n");
			self->tupleField = NULL;
			/* end of tuples */
			return -1;
		}
	}

	end_tuple = req_size + QR_get_rowstart_in_cache(self);
	/*
	 * See if we need to fetch another group of rows. We may be being
	 * called from send_query(), and if so, don't send another fetch,
	 * just fall through and read the tuples.
	 */
	self->tupleField = NULL;

	fetch_size = 0;
	if (!QR_is_fetching_tuples(self))
	{
		ci = &(conn->connInfo);
		if (!QR_get_cursor(self))
		{
			mylog("%s: ALL_ROWS: done, fcount = %d, fetch_number = %d\n", func, QR_get_num_total_tuples(self), fetch_number);
			self->tupleField = NULL;
			QR_set_reached_eof(self);
			return -1;		/* end of tuples */
		}

		if (QR_get_rowstart_in_cache(self) >= num_backend_rows ||
		    QR_is_moving(self))
		{
			TupleField *tuple = self->backend_tuples;

			/* not a correction */
			/* Determine the optimum cache size.  */
			if (ci->drivers.fetch_max % req_size == 0)
				fetch_size = ci->drivers.fetch_max;
			else if ((Int4)req_size < ci->drivers.fetch_max)
				/*fetch_size = (ci->drivers.fetch_max / req_size + 1) * req_size;*/
				fetch_size = (ci->drivers.fetch_max / req_size) * req_size;
			else
				fetch_size = req_size;

			self->cache_size = fetch_size;
			/* clear obsolete tuples */
inolog("clear obsolete %d tuples\n", num_backend_rows);
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
				mylog("corrupted fetch_size end_tuple=%d <= cached_rows=%d\n", end_tuple, num_backend_rows);
				return -1;
			}
			/* and enlarge the cache size */
			self->cache_size += fetch_size;
			offset = self->fetch_number;
			QR_inc_next_in_cache(self);
			boundary_adjusted = TRUE;
		}

		if (enlargeKeyCache(self, self->cache_size - num_backend_rows, "Out of memory while reading tuples") < 0)
			return FALSE;
		if (PROTOCOL_74(ci)
		    && !QR_is_permanent(self) /* Execute seems an invalid operation after COMMIT */ 
			)
		{
			ExecuteRequest = TRUE;
			if (!SendExecuteRequest(stmt, QR_get_cursor(self),
				fetch_size))
				return FALSE;
			if (!SendSyncRequest(conn))
				return FALSE;
		}
		else
		{
			QResultClass	*res;
			sprintf(fetch, "fetch %d in \"%s\"", fetch_size, QR_get_cursor(self));

			mylog("%s: sending actual fetch (%d) query '%s'\n", func, fetch_size, fetch);

			/* don't read ahead for the next tuple (self) ! */
			qi.row_size = self->cache_size;
			qi.result_in = self;
			qi.cursor = NULL;
			res = CC_send_query(conn, fetch, &qi, 0, stmt);
			if (!QR_command_maybe_successful(res))
			{
				if (!QR_get_message(self))
					QR_set_message(self, "Error fetching next group.");
				return FALSE;
			}
		}
		internally_invoked = TRUE;
		cur_fetch = 0;
		QR_set_fetching_tuples(self);
	}
	else
	{
		mylog("%s: inTuples = true, falling through: fcount = %d, fetch_number = %d\n", func, self->num_cached_rows, self->fetch_number);

		/*
		 * This is a pre-fetch (fetching rows right after query but
		 * before any real SQLFetch() calls.  This is done so the
		 * field attributes are available.
		 */
		QR_set_next_in_cache(self, 0);
	}

	if (!boundary_adjusted)
	{
		QR_set_rowstart_in_cache(self, offset);
		QR_set_num_cached_rows(self, 0);
	}

	sock = CC_get_socket(conn);
	self->tupleField = NULL;
	ci = &(conn->connInfo);
	num_rows_in = self->num_cached_rows;

	curr_eof = reached_eof_now = (QR_once_reached_eof(self) && self->cursTuple >= (Int4)self->num_total_read);
inolog("reached_eof_now=%d\n", reached_eof_now);
	for (kill_conn = loopend = rcvend = FALSE; !loopend;)
	{
		id = SOCK_get_id(sock);
		if (0 != SOCK_get_errcode(sock))
			break;
		if (NULL != LastMessageType)
			*LastMessageType = id;
		response_length = SOCK_get_response_length(sock);
		if (0 != SOCK_get_errcode(sock))
			break;
inolog("id='%c' response_length=%d\n", id, response_length);
		switch (id)
		{

			case 'P':
				mylog("Portal name within tuples ?? just ignore\n");
				SOCK_get_string(sock, msgbuffer, ERROR_MSG_LENGTH);
				break;
			case 'T':
				mylog("Tuples within tuples ?? OK try to handle them\n");
				QR_set_no_fetching_tuples(self);
				if (self->num_total_read > 0)
				{
					mylog("fetched %d rows\n", self->num_total_read);
					/* set to first row */
					self->tupleField = self->backend_tuples + (offset * num_fields);
				}
				else
				{
					mylog("    [ fetched 0 rows ]\n");
				}
				/* add new Result class */
				self->next = QR_Constructor();
				if (!self->next)
				{
					CC_set_error(conn, CONNECTION_COULD_NOT_RECEIVE, "Could not create result info in QR_next_tuple.", func);
					CC_on_abort(conn, CONN_DEAD);
					return FALSE;
				}
				QR_set_cache_size(self->next, self->cache_size);
				self = self->next;
				if (!QR_fetch_tuples(self, conn, NULL, LastMessageType))
				{
					CC_set_error(conn, CONNECTION_COULD_NOT_RECEIVE, QR_get_message(self), func);
					return FALSE;
				}

				loopend = rcvend = TRUE;
				break;

			case 'B':			/* Tuples in binary format */
			case 'D':			/* Tuples in ASCII format  */

				if (!QR_get_tupledata(self, id == 'B'))
				{
					ret = FALSE;
					loopend = TRUE;
				}
				cur_fetch++;
				break;			/* continue reading */

			case 'C':			/* End of tuple list */
				SOCK_get_string(sock, cmdbuffer, ERROR_MSG_LENGTH);
				QR_set_command(self, cmdbuffer);

				mylog("end of tuple list -- setting inUse to false: this = %p %s\n", self, cmdbuffer);

				qlog("    [ fetched %d rows ]\n", self->num_total_read);
				mylog("_%s: 'C' fetch_total = %d & this_fetch = %d\n", func, self->num_total_read, self->num_cached_rows);
				if (QR_is_fetching_tuples(self))
				{
					self->dataFilled = TRUE;
					QR_set_no_fetching_tuples(self);
					if (internally_invoked)
					{
						if (ExecuteRequest) /* Execute completed without accepting Portal Suspend */
							reached_eof_now = TRUE;
						else if (cur_fetch < fetch_size)
							reached_eof_now = TRUE;
					}
					else if (self->num_cached_rows < self->cache_size)
						reached_eof_now = TRUE;
					else if (!QR_get_cursor(self))
						reached_eof_now = TRUE;
				}
				if (reached_eof_now)
				{
					/* last row from cache */
					/* We are done because we didn't even get CACHE_SIZE tuples */
					mylog("%s: backend_rows < CACHE_SIZE: brows = %d, cache_size = %d\n", func, num_backend_rows, self->cache_size);
				}
				if (!internally_invoked ||
				    PG_VERSION_LE(conn, 6.3))
					loopend = rcvend = TRUE;
				break;

			case 'E':			/* Error */
				msg_truncated = handle_error_message(conn, msgbuffer, sizeof(msgbuffer), self->sqlstate, "next_tuple", self);

				mylog("ERROR from backend in next_tuple: '%s'\n", msgbuffer);
				qlog("ERROR from backend in next_tuple: '%s'\n", msgbuffer);

				if (!internally_invoked ||
				    PG_VERSION_LE(conn, 6.3))
					loopend = rcvend = TRUE;
				ret = FALSE;
				break;

			case 'N':			/* Notice */
				msg_truncated = handle_notice_message(conn, cmdbuffer, sizeof(cmdbuffer), self->sqlstate, "next_tuple", self);
				qlog("NOTICE from backend in next_tuple: '%s'\n", msgbuffer);
				continue;

			case 'Z':	/* Ready for query */
				EatReadyForQuery(conn);
				if (QR_is_fetching_tuples(self))
				{
					reached_eof_now = TRUE;
					QR_set_no_fetching_tuples(self);
				}
				loopend = rcvend = TRUE;
				break;
			case 's':	/* portal suspend */
				mylog("portal suspend");
				QR_set_no_fetching_tuples(self);
				self->dataFilled = TRUE;
				break;
			default:
				/* skip the unexpected response if possible */
				if (response_length >= 0)
					break;
				/* this should only happen if the backend
					* dumped core ??? */
				mylog("%s: Unexpected result from backend: id = '%c' (%d)\n", func, id, id);
				qlog("%s: Unexpected result from backend: id = '%c' (%d)\n", func, id, id);
				QR_set_message(self, "Unexpected result from backend. It probably crashed");
				loopend = kill_conn = TRUE;
		}
		if (0 != SOCK_get_errcode(sock))
			break;
	}
	if (!kill_conn && !rcvend && 0 == SOCK_get_errcode(sock))
	{
		if (PROTOCOL_74(ci))
		{
			for (;;) /* discard the result until ReadyForQuery comes */
			{
				id = SOCK_get_id(sock);
				if (0 != SOCK_get_errcode(sock))
					break;
				if (NULL != LastMessageType)
					*LastMessageType = id;
				response_length = SOCK_get_response_length(sock);
				if (0 != SOCK_get_errcode(sock))
					break;
				if ('Z' == id) /* ready for query */
				{
					EatReadyForQuery(conn);
					qlog("%s discarded data until ReadyForQuery comes\n", __FUNCTION__);
					if (QR_is_fetching_tuples(self))
					{
						reached_eof_now = TRUE;
						QR_set_no_fetching_tuples(self);
					}
					break;
				}
			}
		}
		else
			kill_conn = TRUE;
	}
	if (0 != SOCK_get_errcode(sock))
	{
		if (QR_command_maybe_successful(self))
			QR_set_message(self, "Communication error while getting a tuple");
		kill_conn = TRUE;
	}
	if (kill_conn)
	{
		if (0 == QR_get_rstatus(self))
			QR_set_rstatus(self, PORES_BAD_RESPONSE);
		CC_on_abort(conn, CONN_DEAD);
		ret = FALSE;
	}
	if (!ret)
		return ret;

	if (!QR_is_fetching_tuples(self))
	{
		SQLLEN	start_idx = 0;

		num_backend_rows = self->num_cached_rows;
		if (reached_eof_now)
		{
			mylog("%s: reached eof now\n", func);
			QR_set_reached_eof(self);
			if (!curr_eof)
				self->cursTuple++;
			if (self->ad_count > 0 &&
			    cur_fetch < fetch_size)
			{
				/* We have to append the tuples(keys) info from the added tuples(keys) here */
				SQLLEN	add_size;
				TupleField	*tuple, *added_tuple;

				if (curr_eof)
				{
					start_idx = CacheIdx2GIdx(offset, stmt, self) - self->num_total_read;
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
inolog("will add %d added_tuples from %d and select the %dth added tuple\n", add_size, start_idx, offset - num_backend_rows + start_idx);
				if (add_size > fetch_size - cur_fetch)
					add_size = fetch_size - cur_fetch;
				else if (add_size < 0)
					add_size = 0;
				if (enlargeKeyCache(self, add_size, "Out of memory while adding tuples") < 0)
					return FALSE;
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
			mylog("_%s: 'C': DONE (fcount == %d)\n", func, num_backend_rows);
			ret = -1;	/* end of tuples */
		}
	}

	/*
	 If the cursor operation was invoked inside this function,
	 we have to set the status bits here.
	*/
	if (internally_invoked && self->keyset && (self->dl_count > 0 || self->up_count > 0))
	{
		SQLLEN	i, lf;
		SQLLEN	lidx, hidx;
		SQLULEN	*deleted = self->deleted, *updated = self->updated;
 
		num_backend_rows = QR_get_num_cached_tuples(self);
		/* For simplicty, use CURS_NEEDS_REREAD bit to mark the row */
		for (i = num_rows_in; i < num_backend_rows; i++)
			self->keyset[i].status |= CURS_NEEDS_REREAD;
		hidx = RowIdx2GIdx(num_backend_rows, stmt);
		lidx = hidx - num_backend_rows;
		/* deleted info */
		for (i = 0; i < self->dl_count && hidx > (Int4)deleted[i]; i++)
		{
			if (lidx <= (Int4)deleted[i])
			{
				lf = num_backend_rows - hidx + deleted[i];
				self->keyset[lf].status = self->deleted_keyset[i].status;
				/* mark the row off */
				self->keyset[lf].status &= (~CURS_NEEDS_REREAD);
			}
		} 
		for (i = self->up_count - 1; i >= 0; i--)
		{
			if (hidx > (Int4)updated[i] &&
			    lidx <= (Int4)updated[i])
			{
				lf = num_backend_rows - hidx + updated[i];
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
/*inolog("keyset[%d].status=%x\n", i, self->keyset[i].status);*/
		}
	}

inolog("%s returning %d offset=%d\n", func, ret, offset);
	return ret;
}


static char
QR_read_a_tuple_from_db(QResultClass *self, char binary)
{
	Int2		field_lf;
	TupleField *this_tuplefield;
	KeySet	*this_keyset = NULL;
	char		bmp,
				bitmap[MAX_FIELDS];		/* Max. len of the bitmap */
	Int2		bitmaplen;		/* len of the bitmap in bytes */
	Int2		bitmap_pos;
	Int2		bitcnt;
	Int4		len;
	char	   *buffer;
	int		ci_num_fields = QR_NumResultCols(self);	/* speed up access */
	int		num_fields = self->num_fields;	/* speed up access */
	SocketClass *sock = CC_get_socket(QR_get_conn(self));
	ColumnInfoClass *flds;
	int		effective_cols;
	char		tidoidbuf[32];
	ConnInfo	*ci = &(QR_get_conn(self)->connInfo);

	/* set the current row to read the fields into */
	effective_cols = QR_NumPublicResultCols(self);
	this_tuplefield = self->backend_tuples + (self->num_cached_rows * num_fields);
	if (QR_haskeyset(self))
	{
		/* this_keyset = self->keyset + self->cursTuple + 1; */
		this_keyset = self->keyset + self->num_cached_keys;
		this_keyset->status = 0;
	}

	bitmaplen = (Int2) ci_num_fields / BYTELEN;
	if ((ci_num_fields % BYTELEN) > 0)
		bitmaplen++;

	/*
	 * At first the server sends a bitmap that indicates which database
	 * fields are null
	 */
	if (PROTOCOL_74(ci))
	{
		int	numf = SOCK_get_int(sock, sizeof(Int2));
if (effective_cols > 0)
{inolog("%dth record in cache numf=%d\n", self->num_cached_rows, numf);}
else
{inolog("%dth record in key numf=%d\n", self->num_cached_keys, numf);}
	}
	else
		SOCK_get_n_char(sock, bitmap, bitmaplen);


	bitmap_pos = 0;
	bitcnt = 0;
	bmp = bitmap[bitmap_pos];
	flds = self->fields;

	for (field_lf = 0; field_lf < ci_num_fields; field_lf++)
	{
		/* Check if the current field is NULL */
		if (!PROTOCOL_74(ci) && (!(bmp & 0200)))
		{
			/* YES, it is NULL ! */
			this_tuplefield[field_lf].len = 0;
			this_tuplefield[field_lf].value = 0;
		}
		else
		{
			/*
			 * NO, the field is not null. so get at first the length of
			 * the field (four bytes)
			 */
			len = SOCK_get_int(sock, VARHDRSZ);
inolog("QR_read_a_tuple_from_db len=%d\n", len);
			if (PROTOCOL_74(ci))
			{
				if (len < 0)
				{
					/* YES, it is NULL ! */
					this_tuplefield[field_lf].len = 0;
					this_tuplefield[field_lf].value = 0;
					continue;
				}
			}
			else
			if (!binary)
				len -= VARHDRSZ;

			if (field_lf >= effective_cols)
				buffer = tidoidbuf;
			else
			{
				QR_MALLOC_return_with_error(buffer, char, len + 1, self, "Out of memory in allocating item buffer.", FALSE);
			}
			SOCK_get_n_char(sock, buffer, len);
			buffer[len] = '\0';

			mylog("qresult: len=%d, buffer='%s'\n", len, buffer);

			if (field_lf >= effective_cols)
			{
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

		/*
		 * Now adjust for the next bit to be scanned in the next loop.
		 */
		bitcnt++;
		if (BYTELEN == bitcnt)
		{
			bitmap_pos++;
			bmp = bitmap[bitmap_pos];
			bitcnt = 0;
		}
		else
			bmp <<= 1;
	}
	self->cursTuple++;
	return TRUE;
}
