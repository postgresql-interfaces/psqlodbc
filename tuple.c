/*-------
 * Module:			tuple.c
 *
 * Description:		This module contains functions for setting the data
 *					for individual fields (TupleField structure) of a
 *					manual result set.
 *
 * Important Note:	These functions are ONLY used in building manual
 *					result sets for info functions (SQLTables,
 *					SQLColumns, etc.)
 *
 * Classes:			n/a
 *
 * API functions:	none
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */

#include "tuple.h"
#include "misc.h"

#include <string.h>
#include <stdlib.h>


void
set_tuplefield_null(TupleField *tuple_field)
{
	tuple_field->len = 0;
	tuple_field->value = NULL;	/* strdup(""); */
}


void
set_tuplefield_string(TupleField *tuple_field, const char *string)
{
	if (string)
	{
		tuple_field->len = (Int4) strlen(string); /* PG restriction */
		tuple_field->value = strdup(string);
	}
	if (!tuple_field->value)
		set_tuplefield_null(tuple_field);
}


void
set_tuplefield_int2(TupleField *tuple_field, Int2 value)
{
	char		buffer[10];

	ITOA_FIXED(buffer, value);

	tuple_field->len = (Int4) (strlen(buffer) + 1);
	/* +1 ... is this correct (better be on the save side-...) */
	tuple_field->value = strdup(buffer);
}


void
set_tuplefield_int4(TupleField *tuple_field, Int4 value)
{
	char		buffer[15];

	ITOA_FIXED(buffer, value);

	tuple_field->len = (Int4) (strlen(buffer) + 1);
	/* +1 ... is this correct (better be on the save side-...) */
	tuple_field->value = strdup(buffer);
}
