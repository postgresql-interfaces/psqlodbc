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
#include "socket.h"
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
 *	Read in field descriptions.
 *	If self is not null, then also store the information.
 *	If self is null, then just read, don't store.
 */
char
CI_read_fields(ColumnInfoClass *self, ConnectionClass *conn)
{
	CSTR		func = "CI_read_fields";
	Int2		lf;
	int			new_num_fields;
	OID		new_adtid, new_relid = 0, new_attid = 0;
	Int2		new_adtsize;
	Int4		new_atttypmod = -1;

	/* COLUMN_NAME_STORAGE_LEN may be sufficient but for safety */
	char		new_field_name[2 * COLUMN_NAME_STORAGE_LEN + 1];
	SocketClass *sock;
	ConnInfo   *ci;

	sock = CC_get_socket(conn);
	ci = &conn->connInfo;

	/* at first read in the number of fields that are in the query */
	new_num_fields = (Int2) SOCK_get_int(sock, sizeof(Int2));

	mylog("num_fields = %d\n", new_num_fields);

	if (self)
	{
		/* according to that allocate memory */
		CI_set_num_fields(self, new_num_fields, PROTOCOL_74(ci));
		if (NULL == self->coli_array)
			return FALSE;
	}

	/* now read in the descriptions */
	for (lf = 0; lf < new_num_fields; lf++)
	{
		SOCK_get_string(sock, new_field_name, 2 * COLUMN_NAME_STORAGE_LEN);
		if (PROTOCOL_74(ci))	/* tableid & columnid */
		{
			new_relid = SOCK_get_int(sock, sizeof(Int4));
			new_attid = SOCK_get_int(sock, sizeof(Int2));
		}
		new_adtid = (OID) SOCK_get_int(sock, 4);
		new_adtsize = (Int2) SOCK_get_int(sock, 2);

		/* If 6.4 protocol, then read the atttypmod field */
		if (PG_VERSION_GE(conn, 6.4))
		{
			mylog("READING ATTTYPMOD\n");
			new_atttypmod = (Int4) SOCK_get_int(sock, 4);

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
			if (PROTOCOL_74(ci))	/* format */
				SOCK_get_int(sock, sizeof(Int2));

		}

		mylog("%s: fieldname='%s', adtid=%d, adtsize=%d, atttypmod=%d (rel,att)=(%d,%d)\n", func, new_field_name, new_adtid, new_adtsize, new_atttypmod, new_relid, new_attid);

		if (self)
			CI_set_field_info(self, lf, new_field_name, new_adtid, new_adtsize, new_atttypmod, new_relid, new_attid);
	}

	return (SOCK_get_errcode(sock) == 0);
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
CI_set_num_fields(ColumnInfoClass *self, int new_num_fields, BOOL allocrelatt)
{
	CI_free_memory(self);		/* always safe to call */

	self->num_fields = new_num_fields;

	self->coli_array = (struct srvr_info *) calloc(sizeof(struct srvr_info), self->num_fields);
}


void
CI_set_field_info(ColumnInfoClass *self, int field_num, char *new_name,
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

	self->coli_array[field_num].display_size = 0;
	self->coli_array[field_num].relid = new_relid;
	self->coli_array[field_num].attid = new_attid;
}
