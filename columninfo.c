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
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#include "pgtypes.h"
#include "columninfo.h"
#include "connection.h"
#include <stdlib.h>
#include <string.h>
#include "pgapifunc.h"

ColumnInfoClass *
CI_Constructor()
{
	ColumnInfoClass *rv;

	rv = (ColumnInfoClass *) malloc(sizeof(ColumnInfoClass));

	if (rv)
	{
		rv->num_fields = 0;
		rv->name = NULL;
		rv->adtid = NULL;
		rv->adtsize = NULL;
		rv->display_size = NULL;
		rv->atttypmod = NULL;
	}

	return rv;
}


void
CI_Destructor(ColumnInfoClass *self)
{
	CI_free_memory(self);

	free(self);
}

void
CI_free_memory(ColumnInfoClass *self)
{
	register Int2 lf;
	int			num_fields = self->num_fields;

	for (lf = 0; lf < num_fields; lf++)
	{
		if (self->name[lf])
		{
			free(self->name[lf]);
			self->name[lf] = NULL;
		}
	}

	/* Safe to call even if null */
	self->num_fields = 0;
	if (self->name)
		free(self->name);
	self->name = NULL;
	if (self->adtid)
		free(self->adtid);
	self->adtid = NULL;
	if (self->adtsize)
		free(self->adtsize);
	self->adtsize = NULL;
	if (self->display_size)
		free(self->display_size);
	self->display_size = NULL;

	if (self->atttypmod)
		free(self->atttypmod);
	self->atttypmod = NULL;
}


void
CI_set_num_fields(ColumnInfoClass *self, int new_num_fields)
{
	CI_free_memory(self);		/* always safe to call */

	self->num_fields = new_num_fields;

	self->name = (char **) malloc(sizeof(char *) * self->num_fields);
	memset(self->name, 0, sizeof(char *) * self->num_fields);
	self->adtid = (Oid *) malloc(sizeof(Oid) * self->num_fields);
	self->adtsize = (Int2 *) malloc(sizeof(Int2) * self->num_fields);
	self->display_size = (Int2 *) malloc(sizeof(Int2) * self->num_fields);
	self->atttypmod = (Int4 *) malloc(sizeof(Int4) * self->num_fields);
}


void
CI_set_field_info(ColumnInfoClass *self, int field_num, char *new_name,
				  Oid new_adtid, Int2 new_adtsize, Int4 new_atttypmod)
{
	/* check bounds */
	if ((field_num < 0) || (field_num >= self->num_fields))
		return;

	/* store the info */
	self->name[field_num] = strdup(new_name);
	self->adtid[field_num] = new_adtid;
	self->adtsize[field_num] = new_adtsize;
	self->atttypmod[field_num] = new_atttypmod;

	self->display_size[field_num] = 0;
}
