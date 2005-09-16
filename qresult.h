/* File:			qresult.h
 *
 * Description:		See "qresult.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __QRESULT_H__
#define __QRESULT_H__

#include "psqlodbc.h"

#ifdef USE_LIBPQ
#include "libpqconnection.h"
#else
#include "connection.h"
#include "socket.h"
#endif /* USE_LIBPQ */

#include "columninfo.h"
#include "tuplelist.h"
#include "tuple.h"

#ifndef USE_LIBPQ
enum QueryResultCode_
{
	PGRES_EMPTY_QUERY = 0,
	PGRES_COMMAND_OK,			/* a query command that doesn't return */
	/* anything was executed properly by the backend */
	PGRES_TUPLES_OK,			/* a query command that returns tuples */
	/* was executed properly by the backend, PGresult */
	/* contains the resulttuples */
	PGRES_COPY_OUT,
	PGRES_COPY_IN,
	PGRES_BAD_RESPONSE,			/* an unexpected response was recv'd from
								 * the backend */
	PGRES_NONFATAL_ERROR,
	PGRES_FATAL_ERROR,
	PGRES_FIELDS_OK,			/* field information from a query was
								 * successful */
	PGRES_END_TUPLES,
	PGRES_INTERNAL_ERROR
};
typedef enum QueryResultCode_ QueryResultCode;
#endif /*USE _LIBPQ */

struct QResultClass_
{
	ColumnInfoClass *fields;	/* the Column information */
	TupleListClass *manual_tuples;		/* manual result tuple list */
	ConnectionClass *conn;		/* the connection this result is using
								 * (backend) */
	QResultClass	*next;		/* the following result class */

	/* Stuff for declare/fetch tuples */
	int			num_total_rows;	/* total count of rows read in */
	int			count_backend_allocated;/* m(re)alloced count */
	int			count_keyset_allocated; /* m(re)alloced count */
	int			num_backend_rows;	/* count of tuples kept in backend_tuples member */
	int			fetch_count;	/* logical rows read so far */
	int			currTuple;
	int			base;

	int			num_fields;		/* number of fields in the result */
	int			cache_size;
	int			rowset_size;
	Int4			recent_processed_row_count;

#ifdef USE_LIBPQ
	ExecStatusType status;
#else
	QueryResultCode status;
#endif /* USE_LIBPQ*/

	char	   *message;
	char	   *cursor;			/* The name of the cursor for select
								 * statements */
	char	   *command;
	char	   *notice;

	TupleField *backend_tuples; /* data from the backend (the tuple cache) */
	TupleField *tupleField;		/* current backend tuple being retrieved */

	char		inTuples;	/* is a fetch of rows from the backend
						in  progress ? */
	char		aborted;	/* was aborted ? */
	char		haskeyset;	/* this result contains keyset ? */
	KeySet		*keyset;
	UInt4		reload_count;
	UInt2		rb_alloc;	/* count of allocated rollback info */
	UInt2		rb_count;	/* count of rollback info */
	Rollback	*rollback;
	UInt2		dl_alloc;	/* count of allocated deleted info */
	UInt2		dl_count;	/* count of deleted info */
	UInt4		*deleted;
};

#define QR_get_fields(self)					(self->fields)


/*	These functions are for retrieving data from the qresult */
#ifdef USE_LIBPQ
/* Retrieve results from manual_tuples since it has the results */
#define QR_get_value_manual(self, tupleno, fieldno) (TL_get_fieldval(self->manual_tuples, tupleno, fieldno))
#define QR_get_value_backend(self,fieldno)			(TL_get_fieldval(self->manual_tuples,self->currTuple, fieldno))
#define QR_get_value_backend_row(self, tupleno, fieldno) (TL_get_fieldval(self->manual_tuples, tupleno, fieldno))
#else
#define QR_get_value_manual(self, tupleno, fieldno) (TL_get_fieldval(self->manual_tuples, tupleno, fieldno))
#define QR_get_value_backend(self, fieldno)			(self->tupleField[fieldno].value)
#define QR_get_value_backend_row(self, tupleno, fieldno) ((self->backend_tuples + (tupleno * self->num_fields))[fieldno].value)
#endif /* USE_LIBPQ */

/*	These functions are used by both manual and backend results */
#define QR_NumResultCols(self)		(CI_get_num_fields(self->fields))
#define QR_NumPublicResultCols(self)	(self->haskeyset ? CI_get_num_fields(self->fields) - 2 : CI_get_num_fields(self->fields))
#define QR_get_fieldname(self, fieldno_)	(CI_get_fieldname(self->fields, fieldno_))
#define QR_get_fieldsize(self, fieldno_)	(CI_get_fieldsize(self->fields, fieldno_))
#define QR_get_display_size(self, fieldno_) (CI_get_display_size(self->fields, fieldno_))
#define QR_get_atttypmod(self, fieldno_)	(CI_get_atttypmod(self->fields, fieldno_))
#define QR_get_field_type(self, fieldno_)	(CI_get_oid(self->fields, fieldno_))

/*	These functions are used only for manual result sets */
#define QR_get_num_total_tuples(self)		(self->manual_tuples ? TL_get_num_tuples(self->manual_tuples) : self->num_total_rows)
#define QR_get_num_backend_tuples(self)		(self->manual_tuples ? TL_get_num_tuples(self->manual_tuples) : self->num_backend_rows)
#define QR_add_tuple(self, new_tuple)		(TL_add_tuple(self->manual_tuples, new_tuple))
#define QR_set_field_info(self, field_num, name, adtid, adtsize)  (CI_set_field_info(self->fields, field_num, name, adtid, adtsize, -1))

/* status macros */
#define QR_command_successful(self)	( !(self->status == PGRES_BAD_RESPONSE || self->status == PGRES_NONFATAL_ERROR || self->status == PGRES_FATAL_ERROR))
#define QR_command_maybe_successful(self) ( !(self->status == PGRES_BAD_RESPONSE || self->status == PGRES_FATAL_ERROR))
#define QR_command_nonfatal(self)	( self->status == PGRES_NONFATAL_ERROR)
#define QR_command_fatal(self)	( self->status == PGRES_FATAL_ERROR)
#define QR_end_tuples(self)		( self->status == PGRES_END_TUPLES)
#define QR_set_status(self, condition)		( self->status = condition )
#define QR_set_aborted(self, aborted_)		( self->aborted = aborted_)
#define QR_set_haskeyset(self)		(self->haskeyset = TRUE)

#define QR_get_message(self)				(self->message)
#define QR_get_command(self)				(self->command)
#define QR_get_notice(self)					(self->notice)
#define QR_get_status(self)					(self->status)
#define QR_get_aborted(self)				(self->aborted)

#define QR_aborted(self)					(!self || self->aborted)

#define QR_MALLOC_return_with_error(t, tp, s, self, n, m, r) \
	{ \
		if (t = (tp *) malloc(s), NULL == t) \
		{ \
			QR_set_status(self, n); \
			QR_set_message(self, m); \
			return r; \
		} \
	}
#define QR_REALLOC_return_with_error(t, tp, s, self, m, r) \
	{ \
		if (t = (tp *) realloc(t, s), NULL == t) \
		{ \
			QR_set_status(self, PGRES_FATAL_ERROR); \
			QR_set_message(self, m); \
			return r; \
		} \
	}
#define QR_MALLOC_exit_if_error(t, tp, s, self, n, m) \
	{ \
		if (t = (tp *) malloc(s), NULL == t) \
		{ \
			self->status = n; \
			QR_set_message(self, m); \
			goto MALLOC_exit; \
		} \
	}

/*	Core Functions */
QResultClass *QR_Constructor(void);
void		QR_Destructor(QResultClass *self);
char		QR_read_tuple(QResultClass *self, char binary);
int			QR_next_tuple(QResultClass *self);
int			QR_close(QResultClass *self);
char		QR_fetch_tuples(QResultClass *self, ConnectionClass *conn, char *cursor);
void		QR_free_memory(QResultClass *self);
void		QR_set_command(QResultClass *self, const char *msg);
void		QR_set_message(QResultClass *self, const char *msg);
void		QR_set_notice(QResultClass *self, const char *msg);

void		QR_set_num_fields(QResultClass *self, int new_num_fields);	/* manual result only */

void		QR_inc_base(QResultClass *self, int base_inc);
void		QR_set_cache_size(QResultClass *self, int cache_size);
void		QR_set_rowset_size(QResultClass *self, int rowset_size);
void		QR_set_position(QResultClass *self, int pos);

#endif
