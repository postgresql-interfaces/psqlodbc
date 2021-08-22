/*-------
 * Module:			columninfo.c
 *
 * Description:		This module contains routines related to
 *					reading and storing the field information from a query.
 *
 * Classes:			ColumnInfoClass (Functions prefix: "CI_")
 *
 * API functions:	none
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */

#include "pgtypes.h"
#include "columninfo.h"

#include "connection.h"
#include <stdlib.h>
#include <string.h>
#include "pgapifunc.h"

#include <libpq-fe.h>

ColumnInfoClass *
CI_Constructor(void)
{
	ColumnInfoClass *rv;

	rv = (ColumnInfoClass *) malloc(sizeof(ColumnInfoClass));

	if (rv)
	{
		rv->refcount = 0;
		rv->num_fields = 0;
		rv->coli_array = NULL;
	}

	return rv;
}


void
CI_Destructor(ColumnInfoClass *self)
{
	CI_free_memory(self);

	free(self);
}

/*
 *	Read in field descriptions from a libpq result set.
 *	If self is not null, then also store the information.
 *	If self is null, then just read, don't store.
 */
BOOL
CI_read_fields_from_pgres(ColumnInfoClass *self, PGresult *pgres)
{
	Int2		lf;
	int			new_num_fields;
	OID		new_adtid, new_relid = 0, new_attid = 0;
	Int2		new_adtsize;
	Int4		new_atttypmod = -1;
	char	   *new_field_name;

	/* at first read in the number of fields that are in the query */
	new_num_fields = PQnfields(pgres);

	QLOG(0, "\tnFields: %d\n", new_num_fields);

	if (self)
	{
		/* according to that allocate memory */
		CI_set_num_fields(self, new_num_fields);
		if (new_num_fields > 0 && NULL == self->coli_array)
			return FALSE;
	}

	/* now read in the descriptions */
	for (lf = 0; lf < new_num_fields; lf++)
	{
		new_field_name = PQfname(pgres, lf);
		new_relid = PQftable(pgres, lf);
		new_attid = PQftablecol(pgres, lf);
		new_adtid = PQftype(pgres, lf);
		new_adtsize = PQfsize(pgres, lf);

		MYLOG(0, "READING ATTTYPMOD\n");
		new_atttypmod = PQfmod(pgres, lf);

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

		if (self)
			CI_set_field_info(self, lf, new_field_name, new_adtid, new_adtsize, new_atttypmod, new_relid, new_attid);
	}

	return TRUE;
}


void
CI_free_memory(ColumnInfoClass *self)
{
	register Int2 lf;
	int			num_fields = self->num_fields;

	/* Safe to call even if null */
	self->num_fields = 0;
	if (self->coli_array)
	{
		for (lf = 0; lf < num_fields; lf++)
		{
			if (self->coli_array[lf].name)
			{
				free(self->coli_array[lf].name);
				self->coli_array[lf].name = NULL;
			}
		}
		free(self->coli_array);
		self->coli_array = NULL;
	}
}


void
CI_set_num_fields(ColumnInfoClass *self, int new_num_fields)
{
	CI_free_memory(self);		/* always safe to call */

	self->num_fields = new_num_fields;

	self->coli_array = (struct srvr_info *) calloc(sizeof(struct srvr_info), self->num_fields);
}


void
CI_set_field_info(ColumnInfoClass *self, int field_num, const char *new_name,
		OID new_adtid, Int2 new_adtsize, Int4 new_atttypmod,
		OID new_relid, OID new_attid)
{
	/* check bounds */
	if ((field_num < 0) || (field_num >= self->num_fields))
		return;

	/* store the info */
	self->coli_array[field_num].name = strdup(new_name);
	self->coli_array[field_num].adtid= new_adtid;
	self->coli_array[field_num].adtsize = new_adtsize;
	self->coli_array[field_num].atttypmod = new_atttypmod;

	self->coli_array[field_num].display_size = PG_ADT_UNSET;
	self->coli_array[field_num].relid = new_relid;
	self->coli_array[field_num].attid = new_attid;
}
