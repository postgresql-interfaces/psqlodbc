/*--------
 * Module:			parse.c
 *
 * Description:		This module contains routines related to parsing SQL
 *					statements.  This can be useful for two reasons:
 *
 *					1. So the query does not actually have to be executed
 *					to return data about it
 *
 *					2. To be able to return information about precision,
 *					nullability, aliases, etc. in the functions
 *					SQLDescribeCol and SQLColAttributes.  Currently,
 *					Postgres doesn't return any information about
 *					these things in a query.
 *
 * Classes:			none
 *
 * API functions:	none
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *--------
 */
/* Multibyte support	Eiji Tokuya 2001-03-15 */

#include "psqlodbc.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "statement.h"
#include "connection.h"
#include "qresult.h"
#include "pgtypes.h"
#include "pgapifunc.h"
#include "catfunc.h"

#include "multibyte.h"

#define FLD_INCR	32
#define TAB_INCR	8
#define COLI_INCR	16

static	char	*getNextToken(int ccsc, char escape_in_literal, char *s, char *token, int smax, char *delim, char *quote, char *dquote, char *numeric);
static	void	getColInfo(COL_INFO *col_info, FIELD_INFO *fi, int k);
static	char	searchColInfo(COL_INFO *col_info, FIELD_INFO *fi);

Int4 FI_precision(const FIELD_INFO *fi)
{
	OID	ftype;

	if (!fi)	return -1;
	ftype = FI_type(fi);
	switch (ftype)
	{
		case PG_TYPE_NUMERIC:
			return fi->column_size;
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
			return fi->decimal_digits;
	}
	return 0;
}
Int4 FI_scale(const FIELD_INFO *fi)
{
	OID	ftype;

	if (!fi)	return -1;
	ftype = FI_type(fi);
	switch (ftype)
	{
		case PG_TYPE_NUMERIC:
			return fi->decimal_digits;
	}
	return 0;
}

static char *
getNextToken(
	int ccsc, /* client encoding */
	char escape_ch,
	char *s, char *token, int smax, char *delim, char *quote, char *dquote, char *numeric)
{
	int			i = 0;
	int		out = 0, taglen;
	char		qc, in_quote, in_dollar_quote, in_escape;
	const	char	*tag, *tagend;
	encoded_str	encstr;
	char	literal_quote = LITERAL_QUOTE, identifier_quote = IDENTIFIER_QUOTE, dollar_quote = DOLLAR_QUOTE, escape_in_literal;

	if (smax <= 1)
		return NULL;

	smax--;

	/* skip leading delimiters */
	while (isspace((UCHAR) s[i]) || s[i] == ',')
	{
		/* mylog("skipping '%c'\n", s[i]); */
		i++;
	}

	if (s[i] == '\0')
	{
		token[0] = '\0';
		return NULL;
	}

	if (quote)
		*quote = FALSE;
	if (dquote)
		*dquote = FALSE;
	if (numeric)
		*numeric = FALSE;

	encoded_str_constr(&encstr, ccsc, &s[i]);
	/* get the next token */
	while (s[i] != '\0' && out < smax)
	{
		encoded_nextchar(&encstr);
		if (ENCODE_STATUS(encstr) != 0)
		{
			token[out++] = s[i++];
			continue;
		}
		if (isspace((UCHAR) s[i]) || s[i] == ',')
			break;
		/* Handle quoted stuff */
		in_quote = in_dollar_quote = FALSE;
		taglen = 0;
		tag = NULL;
		escape_in_literal = '\0';
		if (out == 0)
		{
			qc = s[i];
			if (qc == dollar_quote)
			{
				in_quote = in_dollar_quote = TRUE;
				tag = s + i;
				taglen = 1;
				if (tagend = strchr(s + i + 1, dollar_quote), NULL != tagend)
					taglen = tagend - s - i + 1;
				i += (taglen - 1);
				encoded_position_shift(&encstr, taglen - 1);
				if (quote)
					*quote = TRUE;
			}
			else if (qc == literal_quote)
			{
				in_quote = TRUE;
				if (quote)
					*quote = TRUE;
				escape_in_literal = escape_ch;
				if (!escape_in_literal)
				{
					if (LITERAL_EXT == s[i - 1])
						escape_in_literal = ESCAPE_IN_LITERAL;
				}
			}
			else if (qc == identifier_quote)
			{
				in_quote = TRUE;
				if (dquote)
					*dquote = TRUE;
			}
		}
		if (in_quote)
		{
			i++;				/* dont return the quote */
			in_escape = FALSE;
			while (s[i] != '\0' && out != smax)
			{
				encoded_nextchar(&encstr);
				if (ENCODE_STATUS(encstr) != 0)
				{
					token[out++] = s[i++];
					continue;
				}
				if (in_escape)
					in_escape = FALSE;
				else if (s[i] == qc)
				{
					if (!in_dollar_quote)
						break;
					if (strncmp(s + i, tag, taglen) == 0)
					{
						i += (taglen - 1);
						encoded_position_shift(&encstr, taglen - 1);
						break;
					}
					token[out++] = s[i];
				}
				else if (literal_quote == qc && s[i] == escape_in_literal)
				{
					in_escape = TRUE;
				}
				else
				{
					token[out++] = s[i];
				}
				i++;
			}
			if (s[i] == qc)
				i++;
			break;
		}

		/* Check for numeric literals */
		if (out == 0 && isdigit((UCHAR) s[i]))
		{
			if (numeric)
				*numeric = TRUE;
			token[out++] = s[i++];
			while (isalnum((UCHAR) s[i]) || s[i] == '.')
				token[out++] = s[i++];

			break;
		}

		if (ispunct((UCHAR) s[i]) && s[i] != '_')
		{
			mylog("got ispunct: s[%d] = '%c'\n", i, s[i]);

			if (out == 0)
			{
				token[out++] = s[i++];
				break;
			}
			else
				break;
		}

		if (out != smax)
			token[out++] = s[i];

		i++;
	}

	/* mylog("done -- s[%d] = '%c'\n", i, s[i]); */

	token[out] = '\0';

	/* find the delimiter  */
	while (isspace((UCHAR) s[i]))
		i++;

	/* return the most priority delimiter */
	if (s[i] == ',')
	{
		if (delim)
			*delim = s[i];
	}
	else if (s[i] == '\0')
	{
		if (delim)
			*delim = '\0';
	}
	else
	{
		if (delim)
			*delim = ' ';
	}

	/* skip trailing blanks  */
	while (isspace((UCHAR) s[i]))
		i++;

	return &s[i];
}

static void
getColInfo(COL_INFO *col_info, FIELD_INFO *fi, int k)
{
	char	   *str;

inolog("getColInfo non-manual result\n");
	fi->dquote = TRUE;
	STR_TO_NAME(fi->column_name, QR_get_value_backend_text(col_info->result, k, COLUMNS_COLUMN_NAME));

	fi->columntype = (OID) QR_get_value_backend_int(col_info->result, k, COLUMNS_FIELD_TYPE, NULL);
	fi->column_size = QR_get_value_backend_int(col_info->result, k, COLUMNS_PRECISION, NULL);
	fi->length = QR_get_value_backend_int(col_info->result, k, COLUMNS_LENGTH, NULL);
	if (str = QR_get_value_backend_text(col_info->result, k, COLUMNS_SCALE), str)
		fi->decimal_digits = atoi(str);
	else
		fi->decimal_digits = -1;
	fi->nullable = QR_get_value_backend_int(col_info->result, k, COLUMNS_NULLABLE, NULL);
	fi->display_size = QR_get_value_backend_int(col_info->result, k, COLUMNS_DISPLAY_SIZE, NULL);
	fi->auto_increment = QR_get_value_backend_int(col_info->result, k, COLUMNS_AUTO_INCREMENT, NULL);
}


static char
searchColInfo(COL_INFO *col_info, FIELD_INFO *fi)
{
	int			k,
				cmp, attnum;
	const char	   *col;

inolog("searchColInfo num_cols=%d col=%s\n", QR_get_num_cached_tuples(col_info->result), PRINT_NAME(fi->column_name));
	if (fi->attnum < 0)
		return FALSE;
	for (k = 0; k < QR_get_num_cached_tuples(col_info->result); k++)
	{
		if (fi->attnum > 0)
		{
			attnum = QR_get_value_backend_int(col_info->result, k, COLUMNS_PHYSICAL_NUMBER, NULL);
inolog("searchColInfo %d attnum=%d\n", k, attnum);
			if (attnum == fi->attnum)
			{
				getColInfo(col_info, fi, k);
				mylog("PARSE: searchColInfo by attnum=%d\n", attnum);
				return TRUE;
			}
		}
		else if (NAME_IS_VALID(fi->column_name))
		{
			col = QR_get_value_backend_text(col_info->result, k, COLUMNS_COLUMN_NAME);
inolog("searchColInfo %d col=%s\n", k, col);
			if (fi->dquote)
				cmp = strcmp(col, GET_NAME(fi->column_name));
			else
				cmp = stricmp(col, GET_NAME(fi->column_name));
			if (!cmp)
			{
				if (!fi->dquote)
					STR_TO_NAME(fi->column_name, col);
				getColInfo(col_info, fi, k);

				mylog("PARSE: searchColInfo: \n");
				return TRUE;
			}
		}
	}

	return FALSE;
}

/*
 *	lower the unquoted name
 */
static
void lower_the_name(char *name, ConnectionClass *conn, BOOL dquote)
{
	if (!dquote)
	{
		char		*ptr;
		encoded_str	encstr;
		make_encoded_str(&encstr, conn, name);

		/* lower case table name */
		for (ptr = name; *ptr; ptr++)
		{
			encoded_nextchar(&encstr);
			if (ENCODE_STATUS(encstr) == 0)
				*ptr = tolower((UCHAR) *ptr);
		}
	}
}

static BOOL CheckHasOids(StatementClass * stmt)
{
	QResultClass	*res;
	BOOL		hasoids = TRUE, foundKey = FALSE;
	char		query[512];
	ConnectionClass	*conn = SC_get_conn(stmt);
	TABLE_INFO	*ti;

	if (0 != SC_checked_hasoids(stmt))
		return TRUE;
	if (!stmt->ti || !stmt->ti[0])
		return FALSE;
	ti = stmt->ti[0];
	sprintf(query, "select relhasoids, c.oid from pg_class c, pg_namespace n where relname = '%s' and nspname = '%s' and c.relnamespace = n.oid", SAFE_NAME(ti->table_name), SAFE_NAME(ti->schema_name));
	res = CC_send_query(conn, query, NULL, ROLLBACK_ON_ERROR | IGNORE_ABORT_ON_CONN, NULL);
	if (QR_command_maybe_successful(res))
	{
		stmt->num_key_fields = PG_NUM_NORMAL_KEYS;
		if (1 == QR_get_num_total_tuples(res))
		{
			const char *value = QR_get_value_backend_text(res, 0, 0);
			if (value && ('f' == *value || '0' == *value))
			{
				hasoids = FALSE;
				TI_set_has_no_oids(ti);
			}
			else
			{
				TI_set_hasoids(ti);
				foundKey = TRUE;
				STR_TO_NAME(ti->bestitem, OID_NAME);
				sprintf(query, "\"%s\" = %u", OID_NAME);
				STR_TO_NAME(ti->bestqual, query);
			}
			TI_set_hasoids_checked(ti);
			ti->table_oid = (OID) strtoul(QR_get_value_backend_text(res, 0, 1), NULL, 10);
		}
		QR_Destructor(res);
		res = NULL;
		if (!hasoids)
		{
			sprintf(query, "select a.attname, a.atttypid from pg_index i, pg_attribute a where indrelid=%u and indnatts=1 and indisunique and indexprs is null and indpred is null and i.indrelid = a.attrelid and a.attnum=i.indkey[0] and attnotnull and atttypid in (%d, %d)", ti->table_oid, PG_TYPE_INT4, PG_TYPE_OID);
			res = CC_send_query(conn, query, NULL, ROLLBACK_ON_ERROR | IGNORE_ABORT_ON_CONN, NULL);
			if (QR_command_maybe_successful(res) && QR_get_num_total_tuples(res) > 0)
			{
				foundKey = TRUE;
				STR_TO_NAME(ti->bestitem, QR_get_value_backend_text(res, 0, 0));
				sprintf(query, "\"%s\" = %%", SAFE_NAME(ti->bestitem));
				if (PG_TYPE_INT4 == (OID) QR_get_value_backend_int(res, 0, 1, NULL))
					strcat(query, "d");
				else
					strcat(query, "u");
				STR_TO_NAME(ti->bestqual, query);
			}
			else
			{
				/* stmt->updatable = FALSE; */
				foundKey = TRUE;
				stmt->num_key_fields--;
			}
		}
	}
	QR_Destructor(res);
	SC_set_checked_hasoids(stmt, foundKey); 
	return TRUE;
}
	
static BOOL increaseNtab(StatementClass *stmt, const char *func)
{
	TABLE_INFO	**ti = stmt->ti, *wti;

	if (!(stmt->ntab % TAB_INCR))
	{
		SC_REALLOC_return_with_error(ti, TABLE_INFO *, (stmt->ntab + TAB_INCR) * sizeof(TABLE_INFO *), stmt, "PGAPI_AllocStmt failed in parse_statement for TABLE_INFO", FALSE);
		stmt->ti = ti;
	}
	wti = ti[stmt->ntab] = (TABLE_INFO *) malloc(sizeof(TABLE_INFO));
	if (wti == NULL)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in parse_statement for TABLE_INFO(2).", func);
		return FALSE;
	}

	TI_Constructor(wti, SC_get_conn(stmt));
	stmt->ntab++;
	return TRUE;
}
	
static void setNumFields(IRDFields *irdflds, size_t numFields)
{
	FIELD_INFO	**fi = irdflds->fi;
	size_t		nfields = irdflds->nfields;

	if (numFields < nfields)
	{
		int	i;

		for (i = (int) numFields; i < (int) nfields; i++)
		{
			if (fi[i])
				fi[i]->flag = 0;
		}
	}
	irdflds->nfields = (UInt4) numFields;
}

void SC_initialize_cols_info(StatementClass *stmt, BOOL DCdestroy, BOOL parseReset)
{
	IRDFields	*irdflds = SC_get_IRDF(stmt);

	/* Free the parsed table information */
	if (stmt->ti)
	{
		TI_Destructor(stmt->ti, stmt->ntab);
		free(stmt->ti);
		stmt->ti = NULL;
	}
	stmt->ntab = 0;
	if (DCdestroy) /* Free the parsed field information */
		DC_Destructor((DescriptorClass *) SC_get_IRD(stmt));
	else
		setNumFields(irdflds, 0);
	if (parseReset)
	{
		stmt->parse_status = STMT_PARSE_NONE;
		stmt->updatable = FALSE;
	}
}

static BOOL allocateFields(IRDFields *irdflds, size_t sizeRequested)
{
	FIELD_INFO	**fi = irdflds->fi;
	size_t		alloc_size, incr_size;

	if (sizeRequested <= irdflds->allocated)
		return TRUE;
	alloc_size = (0 != irdflds->allocated ? irdflds->allocated : FLD_INCR);
	for (; alloc_size < sizeRequested; alloc_size *= 2)
			;
	incr_size = sizeof(FIELD_INFO *) * (alloc_size - irdflds->allocated);

	fi = (FIELD_INFO **) realloc(fi, alloc_size * sizeof(FIELD_INFO *));
	if (!fi)
	{
		irdflds->fi = NULL;
		irdflds->allocated = irdflds->nfields = 0;
		return FALSE;
	}
	memset(&fi[irdflds->allocated], 0, incr_size);
	irdflds->fi = fi;
	irdflds->allocated = (SQLSMALLINT) alloc_size;

	return TRUE;
}

static void xxxxx(FIELD_INFO *fi, QResultClass *res, int i)
{
	STR_TO_NAME(fi->column_alias, QR_get_fieldname(res, i));
	fi->basetype = QR_get_field_type(res, i);
	if (0 == fi->columntype)
		fi->columntype = fi->basetype;
	if (fi->attnum < 0)
	{
		fi->nullable = FALSE;
		fi->updatable = FALSE;
	}
	else if (fi->attnum > 0)
		fi->nullable = TRUE;	/* probably ? */

	if (NAME_IS_NULL(fi->column_name))
	{
		switch (fi->attnum)
		{
			case CTID_ATTNUM:
				STR_TO_NAME(fi->column_name, "ctid");
				break;
			case OID_ATTNUM:
				STR_TO_NAME(fi->column_name, OID_NAME);
				break;
			case XMIN_ATTNUM:
				STR_TO_NAME(fi->column_name, "xmin");
				break;
		}
	}
}

static has_multi_table(const StatementClass *stmt)
{
	BOOL multi_table = FALSE;
	QResultClass	*res;

inolog("has_multi_table ntab=%d", stmt->ntab);
	if (1 < stmt->ntab)
		multi_table = TRUE;
	else if (SC_has_join(stmt))
		multi_table = TRUE;
	else if (res = SC_get_Curres(stmt), NULL != res)
	{
		int	i, num_fields = QR_NumPublicResultCols(res);
		OID	reloid = 0, greloid;

		for (i = 0; i < num_fields; i++)
		{
			greloid = QR_get_relid(res, i);
			if (0 != greloid)
			{
				if (0 == reloid)
					reloid = greloid;
				else if (reloid != greloid)
				{
inolog(" dohhhhhh");
					multi_table = TRUE;
					break;
				}
			}
		}
	}
inolog(" multi=%d\n", multi_table);
	return multi_table;
}
/*
 *	SQLColAttribute tries to set the FIELD_INFO (using protocol 3).
 */
static BOOL ColAttSet(StatementClass *stmt, TABLE_INFO *rti)
{
	QResultClass	*res = SC_get_Curres(stmt);
	IRDFields	*irdflds = SC_get_IRDF(stmt);
	COL_INFO	*col_info = NULL;
	FIELD_INFO	**fi, *wfi;
	OID		reloid = 0;
	Int2		attid;
	int		i, num_fields;
	BOOL		fi_reuse, updatable;

mylog("ColAttSet in\n");
	if (rti)
	{
		if (reloid = rti->table_oid, 0 == reloid)
			return FALSE;
		if (0 != (rti->flags & TI_COLATTRIBUTE))
			return TRUE;
		col_info = rti->col_info;
	}
	if (!QR_command_maybe_successful(res))
		return FALSE;
	if (num_fields = QR_NumPublicResultCols(res), num_fields <= 0)
		return FALSE;
	fi = irdflds->fi;
	if (num_fields > (int) irdflds->allocated)
	{
		if (!allocateFields(irdflds, num_fields))
			return FALSE;
		fi = irdflds->fi;
	}
	setNumFields(irdflds, num_fields);
	updatable = rti ? TI_is_updatable(rti) : FALSE;
mylog("updatable=%d tab=%d fields=%d", updatable, stmt->ntab, num_fields);
	if (updatable)
	{
		if (1 > stmt->ntab)
			updatable = FALSE;
		else if (has_multi_table(stmt))
			updatable = FALSE;
	}
mylog("->%d\n", updatable);
	for (i = 0; i < num_fields; i++)
	{
		if (reloid == (OID) QR_get_relid(res, i))
		{
			if (wfi = fi[i], NULL == wfi)
			{
				wfi = (FIELD_INFO *) malloc(sizeof(FIELD_INFO));
				fi_reuse = FALSE;
				fi[i] = wfi;
			}
			else if (FI_is_applicable(wfi))
				continue;
			else
				fi_reuse = TRUE;
			FI_Constructor(wfi, fi_reuse);
			attid = (Int2) QR_get_attid(res, i);
			wfi->attnum = attid;
			if (searchColInfo(col_info, wfi))
			{
				STR_TO_NAME(wfi->column_alias, QR_get_fieldname(res, i));
				wfi->basetype = QR_get_field_type(res, i);
				wfi->updatable = updatable;
			}
			else
			{
				xxxxx(wfi, res, i);
			}
			wfi->ti = rti;
			wfi->flag |= FIELD_COL_ATTRIBUTE;
		}
	}
	if (rti)
		rti->flags |= TI_COLATTRIBUTE;
	return TRUE;
}

static BOOL
getCOLIfromTable(ConnectionClass *conn, pgNAME *schema_name, pgNAME table_name, 
COL_INFO **coli)
{
	int	colidx;
	BOOL	found = FALSE;

	*coli = NULL;
	if (NAME_IS_NULL(table_name))
		return TRUE;
	if (conn->schema_support)
	{
		if (NAME_IS_NULL(*schema_name))
		{
			const char *curschema = CC_get_current_schema(conn);
			/*
			 * Though current_schema() doesn't have
			 * much sense in PostgreSQL, we first
			 * check the current_schema() when no
			 * explicit schema name was specified.
			 */
			for (colidx = 0; colidx < conn->ntables; colidx++)
			{
				if (!NAMEICMP(conn->col_info[colidx]->table_name, table_name) &&
				    !stricmp(SAFE_NAME(conn->col_info[colidx]->schema_name), curschema))
				{
					mylog("FOUND col_info table='%s' current schema='%s'\n", PRINT_NAME(table_name), curschema);
					found = TRUE;
					STR_TO_NAME(*schema_name, curschema);
					break;
				}
			}
			if (!found)
			{
				QResultClass	*res;
				char		token[256];
				BOOL		tblFound = FALSE;

				/*
			  	 * We also have to check as follows.
			  	 */
				sprintf(token, "select nspname from pg_namespace n, pg_class c"
						" where c.relnamespace=n.oid and c.oid='\"%s\"'::regclass", SAFE_NAME(table_name));
				res = CC_send_query(conn, token, NULL, ROLLBACK_ON_ERROR | IGNORE_ABORT_ON_CONN, NULL);
				if (QR_command_maybe_successful(res))
				{
					if (QR_get_num_total_tuples(res) == 1)
					{
						tblFound = TRUE;
						STR_TO_NAME(*schema_name, QR_get_value_backend_text(res, 0, 0));
					}
				}
				QR_Destructor(res);
				if (!tblFound)
					return FALSE;
			}
		}
		if (!found && NAME_IS_VALID(*schema_name))
		{
			for (colidx = 0; colidx < conn->ntables; colidx++)
			{
				if (!NAMEICMP(conn->col_info[colidx]->table_name, table_name) &&
				    !NAMEICMP(conn->col_info[colidx]->schema_name, *schema_name))
				{
					mylog("FOUND col_info table='%s' schema='%s'\n", PRINT_NAME(table_name), PRINT_NAME(*schema_name));
					found = TRUE;
					break;
				}
			}
		}
	}
	else
	{
		for (colidx = 0; colidx < conn->ntables; colidx++)
		{
			if (!NAMEICMP(conn->col_info[colidx]->table_name, table_name))
			{
				mylog("FOUND col_info table='%s'\n", table_name);
				found = TRUE;
				break;
			}
		}
	}
	*coli = found ? conn->col_info[colidx] : NULL;
	return TRUE; /* success */
}

BOOL getCOLIfromTI(const char *func, ConnectionClass *conn, StatementClass *stmt, const OID reloid, TABLE_INFO **pti)
{
	BOOL	colatt = FALSE, found = FALSE;
	OID	greloid = reloid;
	TABLE_INFO	*wti = *pti;
	COL_INFO	*coli;
	HSTMT		hcol_stmt = NULL;
	QResultClass	*res;

inolog("getCOLIfromTI reloid=%u ti=%p\n", reloid, wti);
	if (!conn)
		conn = SC_get_conn(stmt);
	if (!wti)	/* SQLColAttribute case */
	{
		int	i;
		if (0 == greloid)
			return FALSE;
		colatt = TRUE;
		for (i = 0; i < stmt->ntab; i++)
		{
			if (stmt->ti[i]->table_oid == greloid)
			{
				wti = stmt->ti[i]; 
				break;
			}
		}
		if (!wti)
		{
inolog("before increaseNtab\n");
			if (!increaseNtab(stmt, func))
				return FALSE;
			wti = stmt->ti[stmt->ntab - 1];
			wti->table_oid = greloid; 
		}
		*pti = wti;
	}
inolog("fi=%p greloid=%d col_info=%p\n", wti, greloid, wti->col_info);
	if (0 == greloid)
		greloid = wti->table_oid;
	if (NULL != wti->col_info)
	{
		found = TRUE;
		goto cleanup;
	}
	if (greloid != 0)
	{
		int	colidx;

		for (colidx = 0; colidx < conn->ntables; colidx++)
		{
			if (conn->col_info[colidx]->table_oid == greloid)
			{
				mylog("FOUND col_info table=%ul\n", greloid);
				found = TRUE;
				wti->col_info = conn->col_info[colidx];
				break;
			}
		}
	}
	else
	{
		if (!getCOLIfromTable(conn, &wti->schema_name, wti->table_name, &coli))
		{
			if (stmt)
			{
				SC_set_parse_status(stmt, STMT_PARSE_FATAL);
				SC_set_error(stmt, STMT_EXEC_ERROR, "Table not found", func);
				stmt->updatable = FALSE;
			}
			return FALSE;
		}
		else if (NULL != coli)
		{
			found = TRUE;
			wti->col_info = coli;
		}
	}
	if (found)
		goto cleanup;
	else if (0 != greloid || NAME_IS_VALID(wti->table_name))
	{
		RETCODE		result;
		StatementClass	*col_stmt;

		mylog("PARSE: Getting PG_Columns for table='%s'\n", wti->table_name);

		result = PGAPI_AllocStmt(conn, &hcol_stmt);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			if (stmt)
				SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in parse_statement for columns.", func);
			goto cleanup;
		}

		col_stmt = (StatementClass *) hcol_stmt;
		col_stmt->internal = TRUE;

		if (greloid)
			result = PGAPI_Columns(hcol_stmt, NULL, 0,
					NULL, 0, NULL, 0, NULL, 0,
					PODBC_SEARCH_BY_IDS, greloid, 0);
		else
			result = PGAPI_Columns(hcol_stmt, NULL, 0, SAFE_NAME(wti->schema_name),
				 SQL_NTS, SAFE_NAME(wti->table_name), SQL_NTS, NULL, 0, PODBC_NOT_SEARCH_PATTERN, 0, 0);

		mylog("        Past PG_Columns\n");
		res = SC_get_Curres(col_stmt);
		if ((SQL_SUCCESS == result || SQL_SUCCESS_WITH_INFO == result)
			&& res && QR_get_num_cached_tuples(res) > 0)
		{
			COL_INFO	*coli;

			mylog("      Success\n");
			if (conn->ntables >= conn->coli_allocated)
			{
				Int2	new_alloc;

				new_alloc = conn->coli_allocated * 2;
				if (new_alloc <= conn->ntables)
					new_alloc = COLI_INCR;
				mylog("PARSE: Allocating col_info at ntables=%d\n", conn->ntables);

				conn->col_info = (COL_INFO **) realloc(conn->col_info, new_alloc * sizeof(COL_INFO *));
				if (!conn->col_info)
				{
					if (stmt)
						SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in parse_statement for col_info.", func);
					goto cleanup;
				}
				conn->coli_allocated = new_alloc;
			}

			mylog("PARSE: malloc at conn->col_info[%d]\n", conn->ntables);
			coli = conn->col_info[conn->ntables] = (COL_INFO *) malloc(sizeof(COL_INFO));
			if (!coli)
			{	
				if (stmt)
					SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in parse_statement for col_info(2).", func);
				goto cleanup;
			}
			col_info_initialize(coli);

			coli->result = res;
			if (res && QR_get_num_cached_tuples(res) > 0)
			{
				if (!greloid)
					greloid = (OID) strtoul(QR_get_value_backend_text(res, 0, COLUMNS_TABLE_OID), NULL, 10);
				if (!wti->table_oid)
					wti->table_oid = greloid;
				if (NAME_IS_NULL(wti->schema_name))
					STR_TO_NAME(wti->schema_name, 
						QR_get_value_backend_text(res, 0, COLUMNS_SCHEMA_NAME));
				if (NAME_IS_NULL(wti->table_name))
					STR_TO_NAME(wti->table_name, 
						QR_get_value_backend_text(res, 0, COLUMNS_TABLE_NAME));
			}
inolog("#2 %p->table_name=%s(%u)\n", wti, PRINT_NAME(wti->table_name), wti->table_oid);
			/*
			 * Store the table name and the SQLColumns result
			 * structure
			 */
			if (NAME_IS_VALID(wti->schema_name))
			{
				NAME_TO_NAME(coli->schema_name,  wti->schema_name);
			}
			else
				NULL_THE_NAME(coli->schema_name);
			NAME_TO_NAME(coli->table_name, wti->table_name);
			coli->table_oid = wti->table_oid;

			/*
			 * The connection will now free the result structures, so
			 * make sure that the statement doesn't free it
			 */
			SC_init_Result(col_stmt);

			conn->ntables++;

if (res && QR_get_num_cached_tuples(res) > 0)
inolog("oid item == %s\n", QR_get_value_backend_text(res, 0, 3));

			mylog("Created col_info table='%s', ntables=%d\n", PRINT_NAME(wti->table_name), conn->ntables);
			/* Associate a table from the statement with a SQLColumn info */
			found = TRUE;
			wti->col_info = coli;
		}
	}
cleanup:
	if (hcol_stmt)
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	if (found)
	{
		QResultClass	*res = wti->col_info->result;

		if (res && QR_get_num_cached_tuples(res) > 0)
		{
			if (!greloid)
				greloid = (OID) strtoul(QR_get_value_backend_text(res, 0, COLUMNS_TABLE_OID), NULL, 10);
			if (!wti->table_oid)
				wti->table_oid = greloid;
			if (NAME_IS_NULL(wti->schema_name))
				STR_TO_NAME(wti->schema_name, 
					QR_get_value_backend_text(res, 0, COLUMNS_SCHEMA_NAME));
			if (NAME_IS_NULL(wti->table_name))
				STR_TO_NAME(wti->table_name, 
					QR_get_value_backend_text(res, 0, COLUMNS_TABLE_NAME));
		}
inolog("#1 %p->table_name=%s(%u)\n", wti, PRINT_NAME(wti->table_name), wti->table_oid);
		if (colatt /* SQLColAttribute case */
		    && 0 == (wti->flags & TI_COLATTRIBUTE))
		{
			if (stmt)
				ColAttSet(stmt, wti);
		}
	}
	else if (!colatt && stmt)
		SC_set_parse_status(stmt, STMT_PARSE_FATAL);
inolog("getCOLIfromTI returns %d\n", found);
	return found;
}

SQLRETURN
SC_set_SS_columnkey(StatementClass *stmt)
{
	CSTR		func = "SC_set_SS_columnkey";
	IRDFields	*irdflds = SC_get_IRDF(stmt);
	FIELD_INFO	**fi = irdflds->fi, *tfi;
	size_t		nfields = irdflds->nfields;
	StatementClass	*pstmt = NULL;
	SQLRETURN	ret = SQL_SUCCESS;
	BOOL		contains_key = FALSE;
	int		i;

inolog("%s:fields=%d ntab=%d\n", func, nfields, stmt->ntab);
	if (!fi)		return ret;
	if (0 >= nfields)	return ret;
	if (!has_multi_table(stmt) && 1 == stmt->ntab)
	{
		TABLE_INFO	**ti = stmt->ti, *oneti;
		ConnectionClass *conn = SC_get_conn(stmt);
		OID	internal_asis_type = SQL_C_CHAR;
		char		keycolnam[MAX_INFO_STRING];
		SQLLEN		keycollen;

		ret = PGAPI_AllocStmt(conn, &pstmt);
		if (!SQL_SUCCEEDED(ret))
			return ret;
		oneti = ti[0];
		ret = PGAPI_PrimaryKeys(pstmt, NULL, 0, NULL, 0, NULL, 0, oneti->table_oid);
		if (!SQL_SUCCEEDED(ret))
			goto cleanup;
#ifdef	UNICODE_SUPPORT
		if (CC_is_in_unicode_driver(conn))
			internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */
		ret = PGAPI_BindCol(pstmt, 4, internal_asis_type, keycolnam, MAX_INFO_STRING, &keycollen);
		if (!SQL_SUCCEEDED(ret))
			goto cleanup;
		contains_key = TRUE;
		ret = PGAPI_Fetch(pstmt);
		while (SQL_SUCCEEDED(ret))
		{
			for (i = 0; i < nfields; i++)
			{
				if (tfi = fi[i], NULL == tfi)
					continue;
				if (!FI_is_applicable(tfi))
					continue;
				if (oneti == tfi->ti &&
				    strcmp(keycolnam, SAFE_NAME(tfi->column_name)) == 0)
				{
inolog("%s:key %s found at %p\n", func, keycolnam, fi + i);
					tfi->columnkey = TRUE;
					break;
				}
			}
			if (i >= nfields)
			{
				mylog("%s: %s not found\n", func, keycolnam);
				break;
			}
			ret = PGAPI_Fetch(pstmt);
		}
		if (SQL_SUCCEEDED(ret))
			contains_key = FALSE;
		else if (SQL_NO_DATA_FOUND != ret)
			goto cleanup;
		ret = SQL_SUCCESS;
	}
inolog("%s: contains_key=%d\n", func, contains_key);
	for (i = 0; i < nfields; i++)
	{
		if (tfi = fi[i], NULL == tfi)
			continue;
		if (!FI_is_applicable(tfi))
			continue;
		if (!contains_key || tfi->columnkey < 0)
			tfi->columnkey = FALSE;
	} 
cleanup:
	if (pstmt)
		PGAPI_FreeStmt(pstmt, SQL_DROP);
	return ret;
}

char
parse_statement(StatementClass *stmt, BOOL check_hasoids)
{
	CSTR		func = "parse_statement";
	char		token[256], stoken[256];
	char		delim,
				quote,
				dquote,
				numeric,
				unquoted;
	char	   *ptr,
			   *pptr = NULL;
	char		in_select = FALSE,
				in_distinct = FALSE,
				in_on = FALSE,
				in_from = FALSE,
				in_where = FALSE,
				in_table = FALSE,
				out_table = TRUE;
	char		in_field = FALSE,
				in_expr = FALSE,
				in_func = FALSE,
				in_dot = FALSE,
				in_as = FALSE;
	int			j,
				i,
				k = 0,
				n,
				blevel = 0, old_blevel, subqlevel = 0,
				allocated_size, new_size;
	FIELD_INFO **fi, *wfi;
	TABLE_INFO **ti, *wti;
	char		parse, maybe_join = 0;
	ConnectionClass *conn = SC_get_conn(stmt);
	IRDFields	*irdflds = SC_get_IRDF(stmt);
	BOOL		updatable = TRUE;

	mylog("%s: entering...\n", func);

	if (SC_parsed_status(stmt) != STMT_PARSE_NONE)
	{
		if (check_hasoids)
			CheckHasOids(stmt);
		return TRUE;
	}
	stmt->updatable = FALSE;
	ptr = stmt->statement;
	fi = irdflds->fi;
	ti = stmt->ti;

	allocated_size = irdflds->allocated;
	SC_initialize_cols_info(stmt, FALSE, TRUE);
	stmt->from_pos = -1;
	stmt->where_pos = -1;
#define	return	DONT_CALL_RETURN_FROM_HERE???

	while (pptr = ptr, (ptr = getNextToken(conn->ccsc, CC_get_escape(conn), pptr, token, sizeof(token), &delim, &quote, &dquote, &numeric)) != NULL)
	{
		unquoted = !(quote || dquote);

		mylog("unquoted=%d, quote=%d, dquote=%d, numeric=%d, delim='%c', token='%s', ptr='%s'\n", unquoted, quote, dquote, numeric, delim, token, ptr);

		old_blevel = blevel;
		if (unquoted && blevel == 0)
		{
			if (in_select)
			{
				if (!stricmp(token, "distinct"))
				{
					in_distinct = TRUE;
					updatable = FALSE;

					mylog("DISTINCT\n");
					continue;
				}
				else if (!stricmp(token, "into"))
				{
					in_select = FALSE;
					mylog("INTO\n");
					stmt->statement_type = STMT_TYPE_CREATE;
					SC_set_parse_status(stmt, STMT_PARSE_FATAL);
					goto cleanup;
				}
				else if (!stricmp(token, "from"))
				{
					in_select = FALSE;
					in_from = TRUE;
					if (stmt->from_pos < 0 &&
						(!strnicmp(pptr, "from", 4)))
					{
						mylog("First ");
						stmt->from_pos = pptr - stmt->statement;
					}

					mylog("FROM\n");
					continue;
				}
			} /* in_select && unquoted && blevel == 0 */
			else if ((!stricmp(token, "where") ||
				 !stricmp(token, "union") ||
				 !stricmp(token, "intersect") ||
				 !stricmp(token, "except") ||
				 !stricmp(token, "order") ||
				 !stricmp(token, "group") ||
				 !stricmp(token, "having")))
			{
				in_from = FALSE;
				in_where = TRUE;

				if (stmt->where_pos < 0)
					stmt->where_pos = pptr - stmt->statement;
				mylog("%s...\n", token);
				if (stricmp(token, "where") &&
				    stricmp(token, "order"))
				{
					updatable = FALSE;
					break;
				}
				continue;
			}
		} /* unquoted && blevel == 0 */
		/* check the change of blevel etc */
		if (unquoted)
		{
			if (!stricmp(token, "select"))
			{
				stoken[0] = '\0';
				if (0 == blevel)
				{
					in_select = TRUE; 
					mylog("SELECT\n");
					continue;
				}
				else
				{
					mylog("SUBSELECT\n");
					if (0 == subqlevel)
						subqlevel = blevel;
				}
			}
			else if (token[0] == '(')
			{
				blevel++;
				mylog("blevel++ = %d\n", blevel);
				/* aggregate function ? */
				if (stoken[0] && updatable && 0 == subqlevel)
				{
					if (stricmp(stoken, "count") == 0 ||
					    stricmp(stoken, "sum") == 0 ||
					    stricmp(stoken, "avg") == 0 ||
					    stricmp(stoken, "max") == 0 ||
					    stricmp(stoken, "min") == 0 ||
					    stricmp(stoken, "variance") == 0 ||
					    stricmp(stoken, "stddev") == 0)
						updatable = FALSE;
				}
			}
			else if (token[0] == ')')
			{
				blevel--;
				mylog("blevel-- = %d\n", blevel);
				if (blevel < subqlevel)
					subqlevel = 0;
			}
			if (blevel >= old_blevel && ',' != delim)
				strcpy(stoken, token);
			else
				stoken[0] = '\0';
		}
		if (in_select)
		{
			if (in_expr || in_func)
			{
				/* just eat the expression */
				mylog("in_expr=%d or func=%d\n", in_expr, in_func);

				if (blevel == 0)
				{
					if (delim == ',')
					{
						mylog("**** Got comma in_expr/func\n");
						in_func = FALSE;
						in_expr = FALSE;
						in_field = FALSE;
					}
					else if (unquoted && !stricmp(token, "as"))
					{
						mylog("got AS in_expr\n");
						in_func = FALSE;
						in_expr = FALSE;
						in_as = TRUE;
						in_field = TRUE;
					}
				}
				continue;
			} /* (in_expr || in_func) && in_select */

			if (in_distinct)
			{
				mylog("in distinct\n");

				if (unquoted && !stricmp(token, "on"))
				{
					in_on = TRUE;
					mylog("got on\n");
					continue;
				}
				if (in_on)
				{
					in_distinct = FALSE;
					in_on = FALSE;
					continue;	/* just skip the unique on field */
				}
				mylog("done distinct\n");
				in_distinct = FALSE;
			} /* in_distinct */

			if (!in_field)
			{
				BOOL	fi_reuse = FALSE;

				if (!token[0])
					continue;

				/* if (!(irdflds->nfields % FLD_INCR)) */
				if (irdflds->nfields >= allocated_size)
				{
					mylog("reallocing at nfld=%d\n", irdflds->nfields);
					new_size = irdflds->nfields + 1;
					if (!allocateFields(irdflds, new_size))
					{
						SC_set_parse_status(stmt, STMT_PARSE_FATAL);
						SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in parse_statement for FIELD_INFO.", func);
						goto cleanup;
					}
					fi = irdflds->fi;
					allocated_size = irdflds->allocated;
				}

				wfi = fi[irdflds->nfields];
				if (wfi)
					fi_reuse = TRUE;
				else
					wfi = fi[irdflds->nfields] = (FIELD_INFO *) malloc(sizeof(FIELD_INFO));
				if (wfi == NULL)
				{
					SC_set_parse_status(stmt, STMT_PARSE_FATAL);
					SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in parse_statement for FIELD_INFO(2).", func);
					goto cleanup;
				}

				/* Initialize the field info */
				FI_Constructor(wfi, fi_reuse);
				wfi->flag = FIELD_PARSING;

				/* double quotes are for qualifiers */
				if (dquote)
					wfi->dquote = TRUE;

				if (quote)
				{
					wfi->quote = TRUE;
					wfi->column_size = (int) strlen(token);
				}
				else if (numeric)
				{
					mylog("**** got numeric: nfld = %d\n", irdflds->nfields);
					wfi->numeric = TRUE;
				}
				else if (0 == old_blevel && blevel > 0)
				{				/* expression */
					mylog("got EXPRESSION\n");
					wfi->expr = TRUE;
					in_expr = TRUE;
					/* continue; */
				}
				else
				{
					STR_TO_NAME(wfi->column_name, token);
					NULL_THE_NAME(wfi->before_dot);
				}
				mylog("got field='%s', dot='%s'\n", PRINT_NAME(wfi->column_name), PRINT_NAME(wfi->before_dot));

				if (delim == ',')
					mylog("comma (1)\n");
				else
					in_field = TRUE;
				irdflds->nfields++;
				continue;
			} /* !in_field */

			/*
			 * We are in a field now
			 */
			wfi = fi[irdflds->nfields - 1];
			if (in_dot)
			{
				if (NAME_IS_VALID(wfi->before_dot))
				{
					MOVE_NAME(wfi->schema_name, wfi->before_dot);
				}
				MOVE_NAME(wfi->before_dot, wfi->column_name);
				STR_TO_NAME(wfi->column_name, token);

				if (delim == ',')
				{
					mylog("in_dot: got comma\n");
					in_field = FALSE;
				}
				in_dot = FALSE;
				continue;
			}

			if (in_as)
			{
				STR_TO_NAME(wfi->column_alias, token);
				mylog("alias for field '%s' is '%s'\n", PRINT_NAME(wfi->column_name), PRINT_NAME(wfi->column_alias));
				in_as = FALSE;
				in_field = FALSE;

				if (delim == ',')
					mylog("comma(2)\n");
				continue;
			}

			/* Function */
			if (0 == old_blevel && blevel > 0)
			{
				in_dot = FALSE;
				in_func = TRUE;
				wfi->func = TRUE;

				/*
				 * name will have the function name -- maybe useful some
				 * day
				 */
				mylog("**** got function = '%s'\n", PRINT_NAME(wfi->column_name));
				continue;
			}

			if (token[0] == '.')
			{
				in_dot = TRUE;
				mylog("got dot\n");
				continue;
			}

			in_dot = FALSE;
			if (!stricmp(token, "as"))
			{
				in_as = TRUE;
				mylog("got AS\n");
				continue;
			}

			/* otherwise, it's probably an expression */
			in_expr = TRUE;
			wfi->expr = TRUE;
			NULL_THE_NAME(wfi->column_name);
			wfi->column_size = 0;
			mylog("*** setting expression\n");
		} /* in_select end */

		if (in_from || in_where)
		{
			if (token[0] == ';') /* end of the first command */
			{
				in_select = in_from = in_where = in_table = FALSE;
				break;
			}
		}
		if (in_from)
		{
			switch (token[0])
			{
				case '\0':
					continue;
				case ',':
					out_table = TRUE; 
					continue;
			}
			if (out_table && !in_table) /* new table */
			{
				in_dot = FALSE;
				maybe_join = 0;
				if (!dquote)
				{
					if (token[0] == '(' ||
					    token[0] == ')')
						continue;
				}

				if (!increaseNtab(stmt, func))
				{
					SC_set_parse_status(stmt, STMT_PARSE_FATAL);
					goto cleanup;
				}

				ti = stmt->ti;
				wti = ti[stmt->ntab - 1];
				if (dquote || stricmp(token, "select"))
				{
					STR_TO_NAME(wti->table_name, token);
					lower_the_name(GET_NAME(wti->table_name), conn, dquote);
				}
				else
				{
					NULL_THE_NAME(wti->table_name);
					TI_no_updatable(wti);
				}
				mylog("got table = '%s'\n", PRINT_NAME(wti->table_name));

				if (0 == blevel && delim == ',')
				{
					out_table = TRUE;
					mylog("more than 1 tables\n");
				}
				else
				{
					out_table = FALSE;
					in_table = TRUE;
				}
				continue;
			}
			if (0 != blevel)
				continue;
			/* out_table is FALSE here */
			if (!dquote && !in_dot)
			{
				if (')' == token[0])
					continue;
				if (stricmp(token, "LEFT") == 0 ||
			    	    stricmp(token, "RIGHT") == 0 ||
			    	    stricmp(token, "OUTER") == 0 ||
			    	    stricmp(token, "FULL") == 0)
				{
					maybe_join = 1;
					in_table = FALSE;
					continue;
				}
				else if (stricmp(token, "INNER") == 0 ||
					 stricmp(token, "CROSS") == 0)
				{
					maybe_join = 2;
					in_table = FALSE;
					continue;
				}
				else if (stricmp(token, "JOIN") == 0)
				{
					in_table = FALSE;
					out_table = TRUE;
					switch (maybe_join)
					{
						case 1:
							SC_set_outer_join(stmt);
							break;
						case 2:
							SC_set_inner_join(stmt);
							break;
					}
					maybe_join = 0;
					continue;
				}
			}
			maybe_join = 0;
			if (in_table)
			{
				wti = ti[stmt->ntab - 1];
				if (in_dot)
				{
					MOVE_NAME(wti->schema_name, wti->table_name);
					STR_TO_NAME(wti->table_name, token);
					lower_the_name(GET_NAME(wti->table_name), conn, dquote);
					in_dot = FALSE;
					continue;
				}
				if (strcmp(token, ".") == 0)
				{
					in_dot = TRUE;
					continue;
				}
				if (dquote || stricmp(token, "as"))
				{
					if (!dquote)
					{
						if (stricmp(token, "ON") == 0)
						{
							in_table = FALSE;
							continue;
						}
					}
					STR_TO_NAME(wti->table_alias, token);
					mylog("alias for table '%s' is '%s'\n", PRINT_NAME(wti->table_name), PRINT_NAME(wti->table_alias));
					in_table = FALSE;
					if (delim == ',')
					{
						out_table = TRUE;
						mylog("more than 1 tables\n");
					}
				}
			}
		} /* in_from */
	}

	/*
	 * Resolve any possible field names with tables
	 */

	parse = TRUE;

	/* Resolve field names with tables */
	for (i = 0; i < (int) irdflds->nfields; i++)
	{
		wfi = fi[i];
		if (wfi->func || wfi->expr || wfi->numeric)
		{
			wfi->ti = NULL;
			wfi->columntype = wfi->basetype = (OID) 0;
			parse = FALSE;
			continue;
		}
		else if (wfi->quote)
		{						/* handle as text */
			wfi->ti = NULL;

			/*
			 * wfi->type = PG_TYPE_TEXT; wfi->column_size = 0; the
			 * following may be better
			 */
			wfi->basetype = PG_TYPE_UNKNOWN;
			if (wfi->column_size == 0)
			{
				wfi->basetype = PG_TYPE_VARCHAR;
				wfi->column_size = 254;
			}
			wfi->length = wfi->column_size;
			continue;
		}
		/* field name contains the schema name */
		else if (NAME_IS_VALID(wfi->schema_name))
		{
			int	matchidx = -1;

			for (k = 0; k < stmt->ntab; k++)
			{
				wti = ti[k];
				if (!NAMEICMP(wti->table_name, wfi->before_dot))
				{
					if (!NAMEICMP(wti->schema_name, wfi->schema_name))
					{
						wfi->ti = wti;
						break;
					}
					else if (NAME_IS_NULL(wti->schema_name))
					{
						if (matchidx < 0)
							matchidx = k;
						else
						{
							SC_set_parse_status(stmt, STMT_PARSE_FATAL);
							SC_set_error(stmt, STMT_EXEC_ERROR, "duplicated Table name", func);
							stmt->updatable = FALSE;
							goto cleanup;
						}
					}
				}
			}
			if (matchidx >= 0)
				wfi->ti = ti[matchidx];
		}
		/* it's a dot, resolve to table or alias */
		else if (NAME_IS_VALID(wfi->before_dot))
		{
			for (k = 0; k < stmt->ntab; k++)
			{
				wti = ti[k];
				if (!NAMEICMP(wti->table_alias, wfi->before_dot))
				{
					wfi->ti = wti;
					break;
				}
				else if (!NAMEICMP(wti->table_name, wfi->before_dot))
				{
					wfi->ti = wti;
					break;
				}
			}
		}
		else if (stmt->ntab == 1)
			wfi->ti = ti[0];
	}

	mylog("--------------------------------------------\n");
	mylog("nfld=%d, ntab=%d\n", irdflds->nfields, stmt->ntab);
	if (0 == stmt->ntab)
	{
		SC_set_parse_status(stmt, STMT_PARSE_FATAL);
		goto cleanup;
	}

	for (i = 0; i < (int) irdflds->nfields; i++)
	{
		wfi = fi[i];
		mylog("Field %d:  expr=%d, func=%d, quote=%d, dquote=%d, numeric=%d, name='%s', alias='%s', dot='%s'\n", i, wfi->expr, wfi->func, wfi->quote, wfi->dquote, wfi->numeric, PRINT_NAME(wfi->column_name), PRINT_NAME(wfi->column_alias), PRINT_NAME(wfi->before_dot));
		if (wfi->ti)
			mylog("     ----> table_name='%s', table_alias='%s'\n", PRINT_NAME(wfi->ti->table_name), PRINT_NAME(wfi->ti->table_alias));
	}

	for (i = 0; i < stmt->ntab; i++)
	{
		wti = ti[i];
		mylog("Table %d: name='%s', alias='%s'\n", i, PRINT_NAME(wti->table_name), PRINT_NAME(wti->table_alias));
	}

	/*
	 * Now save the SQLColumns Info for the parse tables
	 */

	/* Call SQLColumns for each table and store the result */
	if (stmt->ntab > 1)
		updatable = FALSE;
	else if (stmt->from_pos < 0)
		updatable = FALSE;
	for (i = 0; i < stmt->ntab; i++)
	{
		/* See if already got it */
		wti = ti[i];

		if (!getCOLIfromTI(func, NULL, stmt, 0, &wti))
			break;
	}
	if (STMT_PARSE_FATAL == SC_parsed_status(stmt))
	{
		goto cleanup;
	}

	mylog("Done PG_Columns\n");

	/*
	 * Now resolve the fields to point to column info
	 */
	if (updatable && 1 == stmt->ntab)
		updatable = TI_is_updatable(stmt->ti[0]);
	for (i = 0; i < (int) irdflds->nfields;)
	{
		wfi = fi[i];
		wfi->updatable = updatable;
		/* Dont worry about functions or quotes */
		if (wfi->func || wfi->quote || wfi->numeric)
		{
			wfi->updatable = FALSE;
			i++;
			continue;
		}

		/* Stars get expanded to all fields in the table */
		else if (SAFE_NAME(wfi->column_name)[0] == '*')
		{
			char		do_all_tables;
			Int2			total_cols,
						cols,
						increased_cols;

			mylog("expanding field %d\n", i);

			total_cols = 0;

			if (wfi->ti)		/* The star represents only the qualified
								 * table */
				total_cols = (Int2) QR_get_num_cached_tuples(wfi->ti->col_info->result);

			else
			{	/* The star represents all tables */
				/* Calculate the total number of columns after expansion */
				for (k = 0; k < stmt->ntab; k++)
					total_cols += (Int2) QR_get_num_cached_tuples(ti[k]->col_info->result);
			}
			increased_cols = total_cols - 1;

			/* Allocate some more field pointers if necessary */
			new_size = irdflds->nfields + increased_cols;

			mylog("k=%d, increased_cols=%d, allocated_size=%d, new_size=%d\n", k, increased_cols, allocated_size, new_size);

			if (new_size > allocated_size)
			{
				int	new_alloc = new_size;

				mylog("need more cols: new_alloc = %d\n", new_alloc);
				if (!allocateFields(irdflds, new_alloc))
				{
					SC_set_parse_status(stmt, STMT_PARSE_FATAL);
					goto cleanup;
				}
				fi = irdflds->fi;
				allocated_size = irdflds->allocated;
			}

			/*
			 * copy any other fields (if there are any) up past the
			 * expansion
			 */
			for (j = irdflds->nfields - 1; j > i; j--)
			{
				mylog("copying field %d to %d\n", j, increased_cols + j);
				fi[increased_cols + j] = fi[j];
			}
			mylog("done copying fields\n");

			/* Set the new number of fields */
			irdflds->nfields += increased_cols;
			mylog("irdflds->nfields now at %d\n", irdflds->nfields);


			/* copy the new field info */
			do_all_tables = (wfi->ti ? FALSE : TRUE);
			wfi = NULL;

			for (k = 0; k < (do_all_tables ? stmt->ntab : 1); k++)
			{
				TABLE_INFO *the_ti = do_all_tables ? ti[k] : fi[i]->ti;

				cols = (Int2) QR_get_num_cached_tuples(the_ti->col_info->result);

				for (n = 0; n < cols; n++)
				{
					FIELD_INFO	*afi;
					BOOL		reuse = TRUE;

					mylog("creating field info: n=%d\n", n);
					/* skip malloc (already did it for the Star) */
					if (k > 0 || n > 0)
					{
						mylog("allocating field info at %d\n", n + i);
						fi[n + i] = (FIELD_INFO *) malloc(sizeof(FIELD_INFO));
						if (fi[n + i] == NULL)
						{
							SC_set_parse_status(stmt, STMT_PARSE_FATAL);
							goto cleanup;
						}
						reuse = FALSE;
					}
					afi = fi[n + i];
					/* Initialize the new space (or the * field) */
					FI_Constructor(afi, reuse);
					afi->ti = the_ti;

					mylog("about to copy at %d\n", n + i);

					getColInfo(the_ti->col_info, afi, n);
					afi->updatable = updatable;

					mylog("done copying\n");
				}

				i += cols;
				mylog("i now at %d\n", i);
			}
		}

		/*
		 * We either know which table the field was in because it was
		 * qualified with a table name or alias -OR- there was only 1
		 * table.
		 */
		else if (wfi->ti)
		{
			if (!searchColInfo(fi[i]->ti->col_info, wfi))
			{
				parse = FALSE;
				wfi->updatable = FALSE;
			}
			i++;
		}

		/* Don't know the table -- search all tables in "from" list */
		else
		{
			for (k = 0; k < stmt->ntab; k++)
			{
				if (searchColInfo(ti[k]->col_info, wfi))
				{
					wfi->ti = ti[k];	/* now know the table */
					break;
				}
			}
			if (k >= stmt->ntab)
			{
				parse = FALSE;
				wfi->updatable = FALSE;
			}
			i++;
		}
	}

	if (check_hasoids && updatable)
		CheckHasOids(stmt);
	SC_set_parse_status(stmt, parse ? STMT_PARSE_COMPLETE : STMT_PARSE_INCOMPLETE);
	for (i = 0; i < (int) irdflds->nfields; i++)
	{
		wfi = fi[i];
		wfi->flag &= ~FIELD_PARSING;
		if (0 != wfi->columntype || 0 != wfi->basetype)
			wfi->flag |= FIELD_PARSED_OK;
	}

	stmt->updatable = updatable;
cleanup:
#undef	return
	if (STMT_PARSE_FATAL == SC_parsed_status(stmt))
	{
		SC_initialize_cols_info(stmt, FALSE, FALSE);
		parse = FALSE;
	}

	mylog("done parse_statement: parse=%d, parse_status=%d\n", parse, SC_parsed_status(stmt));
	return parse;
}
