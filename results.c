/*
 * Module:			results.c
 *
 * Description:		This module contains functions related to
 *					retrieving result information through the ODBC API.
 *
 * Classes:			n/a
 *
 * API functions:	SQLRowCount, SQLNumResultCols, SQLDescribeCol,
 *					SQLColAttributes, SQLGetData, SQLFetch, SQLExtendedFetch,
 *					SQLMoreResults, SQLSetPos, SQLSetScrollOptions(NI),
 *					SQLSetCursorName, SQLGetCursorName
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */

#include "psqlodbc.h"

#include <string.h>
#include "misc.h"
#include "dlg_specific.h"
#include "environ.h"
#include "connection.h"
#include "statement.h"
#include "bind.h"
#include "qresult.h"
#include "convert.h"
#include "pgtypes.h"

#include <stdio.h>
#include <limits.h>

#include "pgapifunc.h"

/*	Helper macro */
#define getEffectiveOid(conn, fi) pg_true_type((conn), (fi)->columntype, FI_type(fi))
#define	NULL_IF_NULL(a) ((a) ? ((const char *)(a)) : "(null)")


RETCODE		SQL_API
PGAPI_RowCount(HSTMT hstmt,
			   SQLLEN * pcrow)
{
	CSTR func = "PGAPI_RowCount";
	StatementClass *stmt = (StatementClass *) hstmt;
	QResultClass *res;

	mylog("%s: entering...\n", func);
	if (!stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}
	if (stmt->proc_return > 0)
	{
		*pcrow = 0;
		inolog("returning RowCount=%d\n", *pcrow);
		return SQL_SUCCESS;
	}

	res = SC_get_Curres(stmt);
	if (res)
	{
		if (stmt->status != STMT_FINISHED)
		{
			SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Can't get row count while statement is still executing.", func);
			return	SQL_ERROR;
		}
		if (res->recent_processed_row_count >= 0)
		{
			*pcrow = res->recent_processed_row_count;
			mylog("**** %s: THE ROWS: *pcrow = %d\n", func, *pcrow);

			return SQL_SUCCESS;
		}
		else if (QR_NumResultCols(res) > 0)
		{
			*pcrow = QR_get_cursor(res) ? -1 : QR_get_num_total_tuples(res) - res->dl_count;
			mylog("RowCount=%d\n", *pcrow);
			return SQL_SUCCESS;
		}
	}

	return SQL_SUCCESS;
}

static BOOL
SC_describe_ok(StatementClass *stmt, BOOL build_fi, int col_idx, const char *func)
{
	Int2		num_fields;
	QResultClass *result;
	BOOL		exec_ok = TRUE;

	num_fields = SC_describe(stmt);
	result = SC_get_Curres(stmt);

	mylog("%s: result = %p, status = %d, numcols = %d\n", func, result, stmt->status, result != NULL ? QR_NumResultCols(result) : -1);
	/****if ((!result) || ((stmt->status != STMT_FINISHED) && (stmt->status != STMT_PREMATURE))) ****/
	if (!QR_command_maybe_successful(result) || num_fields < 0)
	{
		/* no query has been executed on this statement */
		SC_set_error(stmt, STMT_EXEC_ERROR, "No query has been executed with that handle", func);
		exec_ok = FALSE;
	}
	else if (col_idx >= 0 && col_idx < num_fields)
	{
		OID	reloid = QR_get_relid(result, col_idx);
		IRDFields	*irdflds = SC_get_IRDF(stmt);
		FIELD_INFO	*fi;
		TABLE_INFO	*ti = NULL;

inolog("build_fi=%d reloid=%u\n", build_fi, reloid);
		if (build_fi && 0 != QR_get_attid(result, col_idx))
			getCOLIfromTI(func, NULL, stmt, reloid, &ti);
inolog("nfields=%d\n", irdflds->nfields);
		if (irdflds->fi && col_idx < (int) irdflds->nfields)
		{
			fi = irdflds->fi[col_idx];
			if (fi)
			{
				if (ti)
				{
					if (NULL == fi->ti)
						fi->ti = ti;
					if (!FI_is_applicable(fi)
					    && 0 != (ti->flags & TI_COLATTRIBUTE))
						fi->flag |= FIELD_COL_ATTRIBUTE;
				}
				fi->basetype = QR_get_field_type(result, col_idx);
				if (0 == fi->columntype)
					fi->columntype = fi->basetype;
			}
		}
	}
	return exec_ok;
}

/*
 *	This returns the number of columns associated with the database
 *	attached to "hstmt".
 */
RETCODE		SQL_API
PGAPI_NumResultCols(HSTMT hstmt,
					SQLSMALLINT * pccol)
{
	CSTR func = "PGAPI_NumResultCols";
	StatementClass *stmt = (StatementClass *) hstmt;
	QResultClass *result;
	char		parse_ok;
	RETCODE		ret = SQL_SUCCESS;

	mylog("%s: entering...\n", func);
	if (!stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}

	SC_clear_error(stmt);
#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */

	if (stmt->proc_return > 0)
	{
		*pccol = 0;
		goto cleanup;
	}
	parse_ok = FALSE;
	if (!stmt->catalog_result && SC_is_parse_forced(stmt) && SC_can_parse_statement(stmt))
	{
		if (SC_parsed_status(stmt) == STMT_PARSE_NONE)
		{
			mylog("%s: calling parse_statement on stmt=%p\n", func, stmt);
			parse_statement(stmt, FALSE);
		}

		if (SC_parsed_status(stmt) != STMT_PARSE_FATAL)
		{
			parse_ok = TRUE;
			*pccol = SC_get_IRDF(stmt)->nfields;
			mylog("PARSE: %s: *pccol = %d\n", func, *pccol);
		}
	}

	if (!parse_ok)
	{
		if (!SC_describe_ok(stmt, FALSE, -1, func))
		{
			ret = SQL_ERROR;
			goto cleanup;
		}

		result = SC_get_Curres(stmt);
		*pccol = QR_NumPublicResultCols(result);
	}

cleanup:
#undef	return
	return ret;
}

#define	USE_FI(fi, unknown) (fi && UNKNOWNS_AS_LONGEST != unknown)

/*
 *	Return information about the database column the user wants
 *	information about.
 */
RETCODE		SQL_API
PGAPI_DescribeCol(HSTMT hstmt,
				  SQLUSMALLINT icol,
				  SQLCHAR * szColName,
				  SQLSMALLINT cbColNameMax,
				  SQLSMALLINT * pcbColName,
				  SQLSMALLINT * pfSqlType,
				  SQLULEN * pcbColDef,
				  SQLSMALLINT * pibScale,
				  SQLSMALLINT * pfNullable)
{
	CSTR func = "PGAPI_DescribeCol";

	/* gets all the information about a specific column */
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	IRDFields  *irdflds;
	QResultClass *res = NULL;
	char	   *col_name = NULL;
	OID			fieldtype = 0;
	SQLLEN		column_size = 0;
	int		unknown_sizes;
	SQLINTEGER	decimal_digits = 0;
	ConnInfo   *ci;
	FIELD_INFO *fi;
	char		buf[255];
	int			len = 0;
	RETCODE		result = SQL_SUCCESS;

	mylog("%s: entering.%d..\n", func, icol);

	if (!stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
	unknown_sizes = ci->drivers.unknown_sizes;

	SC_clear_error(stmt);

#define	return	DONT_CALL_RETURN_FROM_HERE???
	irdflds = SC_get_IRDF(stmt);
	if (0 == icol) /* bookmark column */
	{
		SQLSMALLINT	fType = stmt->options.use_bookmarks == SQL_UB_VARIABLE ? SQL_BINARY : SQL_INTEGER;

inolog("answering bookmark info\n");
		if (szColName && cbColNameMax > 0)
			*szColName = '\0';
		if (pcbColName)
			*pcbColName = 0;
		if (pfSqlType)
			*pfSqlType = fType;
		if (pcbColDef)
			*pcbColDef = 10;
		if (pibScale)
			*pibScale = 0;
		if (pfNullable)
			*pfNullable = SQL_NO_NULLS;
		result = SQL_SUCCESS;
		goto cleanup;
	}

	/*
	 * Dont check for bookmark column. This is the responsibility of the
	 * driver manager.
	 */

	icol--;						/* use zero based column numbers */

	fi = NULL;
	if (icol < irdflds->nfields && irdflds->fi)
		fi = irdflds->fi[icol];
	if (!FI_is_applicable(fi) && !stmt->catalog_result && SC_is_parse_forced(stmt) && SC_can_parse_statement(stmt))
	{
		if (SC_parsed_status(stmt) == STMT_PARSE_NONE)
		{
			mylog("%s: calling parse_statement on stmt=%p\n", func, stmt);
			parse_statement(stmt, FALSE);
		}

		mylog("PARSE: DescribeCol: icol=%d, stmt=%p, stmt->nfld=%d, stmt->fi=%p\n", icol, stmt, irdflds->nfields, irdflds->fi);

		if (SC_parsed_status(stmt) != STMT_PARSE_FATAL && irdflds->fi)
		{
			if (icol < irdflds->nfields)
				fi = irdflds->fi[icol];
			else
			{
				SC_set_error(stmt, STMT_INVALID_COLUMN_NUMBER_ERROR, "Invalid column number in DescribeCol.", func);
				result = SQL_ERROR;
				goto cleanup;
			}
			mylog("DescribeCol: getting info for icol=%d\n", icol);
		}
	}

	if (!FI_is_applicable(fi))
	{
		/*
		 * If couldn't parse it OR the field being described was not parsed
		 * (i.e., because it was a function or expression, etc, then do it the
		 * old fashioned way.
		 */
		BOOL	build_fi = (NULL != pfNullable || NULL != pfSqlType);
		fi = NULL;
		if (!SC_describe_ok(stmt, build_fi, icol, func))
		{
			result = SQL_ERROR;
			goto cleanup;
		}

		res = SC_get_Curres(stmt);
		if (icol >= QR_NumPublicResultCols(res))
		{
			SC_set_error(stmt, STMT_INVALID_COLUMN_NUMBER_ERROR, "Invalid column number in DescribeCol.", func);
			SPRINTF_FIXED(buf, "Col#=%d, #Cols=%d,%d keys=%d", icol, QR_NumResultCols(res), QR_NumPublicResultCols(res), res->num_key_fields);
			SC_log_error(func, buf, stmt);
			result = SQL_ERROR;
			goto cleanup;
		}
		if (icol < irdflds->nfields && irdflds->fi)
			fi = irdflds->fi[icol];
	}
	res = SC_get_Curres(stmt);
#ifdef	SUPPRESS_LONGEST_ON_CURSORS
	if (UNKNOWNS_AS_LONGEST == unknown_sizes)
	{
		if (QR_once_reached_eof(res))
			unknown_sizes = UNKNOWNS_AS_LONGEST;
		else
			unknown_sizes = UNKNOWNS_AS_MAX;
	}
#endif /* SUPPRESS_LONGEST_ON_CURSORS */
	/* handle constants */
	if (res &&
	    -2 == QR_get_fieldsize(res, icol))
		unknown_sizes = UNKNOWNS_AS_LONGEST;

	if (FI_is_applicable(fi))
	{
		fieldtype = getEffectiveOid(conn, fi);
		if (NAME_IS_VALID(fi->column_alias))
			col_name = GET_NAME(fi->column_alias);
		else
			col_name = GET_NAME(fi->column_name);
		if (USE_FI(fi, unknown_sizes))
		{
			column_size = fi->column_size;
			decimal_digits = fi->decimal_digits;
		}
		else
		{
			column_size = pgtype_column_size(stmt, fieldtype, icol, unknown_sizes);
			decimal_digits = pgtype_decimal_digits(stmt, fieldtype, icol);
		}

		mylog("PARSE: fieldtype=%d, col_name='%s', column_size=%d\n", fieldtype, NULL_IF_NULL(col_name), column_size);
	}
	else
	{
		col_name = QR_get_fieldname(res, icol);
		fieldtype = QR_get_field_type(res, icol);

		column_size = pgtype_column_size(stmt, fieldtype, icol, unknown_sizes);
		decimal_digits = pgtype_decimal_digits(stmt, fieldtype, icol);
	}

	mylog("describeCol: col %d fieldname = '%s'\n", icol, NULL_IF_NULL(col_name));
	mylog("describeCol: col %d fieldtype = %d\n", icol, fieldtype);
	mylog("describeCol: col %d column_size = %d\n", icol, column_size);

	result = SQL_SUCCESS;

	/*
	 * COLUMN NAME
	 */
	len = col_name ? (int) strlen(col_name) : 0;

	if (pcbColName)
		*pcbColName = len;

	if (szColName && cbColNameMax > 0)
	{
		if (NULL != col_name)
			strncpy_null((char *) szColName, col_name, cbColNameMax);
		else
			szColName[0] = '\0';

		if (len >= cbColNameMax)
		{
			result = SQL_SUCCESS_WITH_INFO;
			SC_set_error(stmt, STMT_TRUNCATED, "The buffer was too small for the colName.", func);
		}
	}

	/*
	 * CONCISE(SQL) TYPE
	 */
	if (pfSqlType)
	{
		*pfSqlType = pgtype_to_concise_type(stmt, fieldtype, icol, unknown_sizes);

		mylog("describeCol: col %d *pfSqlType = %d\n", icol, *pfSqlType);
	}

	/*
	 * COLUMN SIZE(PRECISION in 2.x)
	 */
	if (pcbColDef)
	{
		if (column_size < 0)
			column_size = 0;		/* "I dont know" */

		*pcbColDef = column_size;

		mylog("describeCol: col %d  *pcbColDef = %d\n", icol, *pcbColDef);
	}

	/*
	 * DECIMAL DIGITS(SCALE in 2.x)
	 */
	if (pibScale)
	{
		if (decimal_digits < 0)
			decimal_digits = 0;

		*pibScale = (SQLSMALLINT) decimal_digits;
		mylog("describeCol: col %d  *pibScale = %d\n", icol, *pibScale);
	}

	/*
	 * NULLABILITY
	 */
	if (pfNullable)
	{
		if (SC_has_outer_join(stmt))
			*pfNullable = TRUE;
		else
			*pfNullable = fi ? fi->nullable : pgtype_nullable(conn, fieldtype);

		mylog("describeCol: col %d  *pfNullable = %d\n", icol, *pfNullable);
	}

cleanup:
#undef	return
	return result;
}



/*		Returns result column descriptor information for a result set. */
RETCODE		SQL_API
PGAPI_ColAttributes(HSTMT hstmt,
					SQLUSMALLINT icol,
					SQLUSMALLINT fDescType,
					PTR rgbDesc,
					SQLSMALLINT cbDescMax,
					SQLSMALLINT * pcbDesc,
					SQLLEN * pfDesc)
{
	CSTR func = "PGAPI_ColAttributes";
	StatementClass *stmt = (StatementClass *) hstmt;
	IRDFields	*irdflds;
	OID		field_type = 0;
	Int2		col_idx;
	ConnectionClass	*conn;
	ConnInfo	*ci;
	int		column_size, unknown_sizes;
	int			cols = 0;
	RETCODE		result;
	const char   *p = NULL;
	SQLLEN		value = 0;
	const	FIELD_INFO	*fi = NULL;
	const	TABLE_INFO	*ti = NULL;
	QResultClass	*res;
	BOOL		stmt_updatable;

	mylog("%s: entering..col=%d %d len=%d.\n", func, icol, fDescType,
				cbDescMax);

	if (!stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}
	stmt_updatable = SC_is_updatable(stmt)
		/* The following doesn't seem appropriate for client side cursors
		  && stmt->options.scroll_concurrency != SQL_CONCUR_READ_ONLY
		 */
		;

	if (pcbDesc)
		*pcbDesc = 0;
	irdflds = SC_get_IRDF(stmt);
	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);

	/*
	 * Dont check for bookmark column.	This is the responsibility of the
	 * driver manager.	For certain types of arguments, the column number
	 * is ignored anyway, so it may be 0.
	 */

	res = SC_get_Curres(stmt);
	if (0 == icol && SQL_DESC_COUNT != fDescType) /* bookmark column */
	{
inolog("answering bookmark info\n");
		switch (fDescType)
		{
			case SQL_DESC_OCTET_LENGTH:
				if (pfDesc)
					*pfDesc = 4;
				break;
			case SQL_DESC_TYPE:
				if (pfDesc)
					*pfDesc = stmt->options.use_bookmarks == SQL_UB_VARIABLE ? SQL_BINARY : SQL_INTEGER;
				break;
		}
		return SQL_SUCCESS;
	}

	col_idx = icol - 1;

	unknown_sizes = ci->drivers.unknown_sizes;

	/* not appropriate for SQLColAttributes() */
	if (stmt->catalog_result)
		unknown_sizes = UNKNOWNS_AS_LONGEST;
	else if (unknown_sizes == UNKNOWNS_AS_DONTKNOW)
		unknown_sizes = UNKNOWNS_AS_MAX;

	if (!stmt->catalog_result && SC_is_parse_forced(stmt) && SC_can_parse_statement(stmt))
	{
		if (SC_parsed_status(stmt) == STMT_PARSE_NONE)
		{
			mylog("%s: calling parse_statement\n", func);
			parse_statement(stmt, FALSE);
		}

		cols = irdflds->nfields;

		/*
		 * Column Count is a special case.	The Column number is ignored
		 * in this case.
		 */
		if (fDescType == SQL_DESC_COUNT)
		{
			if (pfDesc)
				*pfDesc = cols;

			return SQL_SUCCESS;
		}

		if (SC_parsed_status(stmt) != STMT_PARSE_FATAL && irdflds->fi)
		{
			if (col_idx >= cols)
			{
				SC_set_error(stmt, STMT_INVALID_COLUMN_NUMBER_ERROR, "Invalid column number in ColAttributes.", func);
				return SQL_ERROR;
			}
		}
	}

	if (col_idx < irdflds->nfields && irdflds->fi)
		fi = irdflds->fi[col_idx];
	if (FI_is_applicable(fi))
		field_type = getEffectiveOid(conn, fi);
	else
	{
		BOOL	build_fi = FALSE;

		fi = NULL;
		switch (fDescType)
		{
			case SQL_COLUMN_OWNER_NAME:
			case SQL_COLUMN_TABLE_NAME:
			case SQL_COLUMN_TYPE:
			case SQL_COLUMN_TYPE_NAME:
			case SQL_COLUMN_AUTO_INCREMENT:
			case SQL_DESC_NULLABLE:
			case SQL_DESC_BASE_TABLE_NAME:
			case SQL_DESC_BASE_COLUMN_NAME:
			case SQL_COLUMN_UPDATABLE:
			case 1212: /* SQL_CA_SS_COLUMN_KEY ? */
				build_fi = TRUE;
				break;
		}
		if (!SC_describe_ok(stmt, build_fi, col_idx, func))
			return SQL_ERROR;

		res = SC_get_Curres(stmt);
		cols = QR_NumPublicResultCols(res);

		/*
		 * Column Count is a special case.	The Column number is ignored
		 * in this case.
		 */
		if (fDescType == SQL_DESC_COUNT)
		{
			if (pfDesc)
				*pfDesc = cols;

			return SQL_SUCCESS;
		}

		if (col_idx >= cols)
		{
			SC_set_error(stmt, STMT_INVALID_COLUMN_NUMBER_ERROR, "Invalid column number in ColAttributes.", func);
			return SQL_ERROR;
		}

		field_type = QR_get_field_type(res, col_idx);
		if (col_idx < irdflds->nfields && irdflds->fi)
			fi = irdflds->fi[col_idx];
	}
	if (FI_is_applicable(fi))
	{
		ti = fi->ti;
		field_type = getEffectiveOid(conn, fi);
	}

	mylog("colAttr: col %d field_type=%d fi,ti=%p,%p\n", col_idx, field_type, fi, ti);

#ifdef SUPPRESS_LONGEST_ON_CURSORS
	if (UNKNOWNS_AS_LONGEST == unknown_sizes)
	{
		if (QR_once_reached_eof(res))
			unknown_sizes = UNKNOWNS_AS_LONGEST;
		else
			unknown_sizes = UNKNOWNS_AS_MAX;
	}
#endif /* SUPPRESS_LONGEST_ON_CURSORS */
	/* handle constants */
	if (res &&
	    -2 == QR_get_fieldsize(res, col_idx))
		unknown_sizes = UNKNOWNS_AS_LONGEST;

	column_size = (USE_FI(fi, unknown_sizes) && fi->column_size > 0) ? fi->column_size : pgtype_column_size(stmt, field_type, col_idx, unknown_sizes);
	switch (fDescType)
	{
		case SQL_COLUMN_AUTO_INCREMENT: /* == SQL_DESC_AUTO_UNIQUE_VALUE */
			if (fi && fi->auto_increment)
				value = TRUE;
			else
				value = pgtype_auto_increment(conn, field_type);
			if (value == -1)	/* non-numeric becomes FALSE (ODBC Doc) */
				value = FALSE;
			mylog("AUTO_INCREMENT=%d\n", value);

			break;

		case SQL_COLUMN_CASE_SENSITIVE: /* == SQL_DESC_CASE_SENSITIVE */
			value = pgtype_case_sensitive(conn, field_type);
			break;

			/*
			 * This special case is handled above.
			 *
			 * case SQL_COLUMN_COUNT:
			 */
		case SQL_COLUMN_DISPLAY_SIZE: /* == SQL_DESC_DISPLAY_SIZE */
			value = (USE_FI(fi, unknown_sizes) && 0 != fi->display_size) ? fi->display_size : pgtype_display_size(stmt, field_type, col_idx, unknown_sizes);

			mylog("%s: col %d, display_size= %d\n", func, col_idx, value);

			break;

		case SQL_COLUMN_LABEL: /* == SQL_DESC_LABEL */
			if (fi && (NAME_IS_VALID(fi->column_alias)))
			{
				p = GET_NAME(fi->column_alias);

				mylog("%s: COLUMN_LABEL = '%s'\n", func, p);
				break;
			}
			/* otherwise same as column name -- FALL THROUGH!!! */

		case SQL_DESC_NAME:
inolog("fi=%p", fi);
if (fi)
inolog(" (%s,%s)", PRINT_NAME(fi->column_alias), PRINT_NAME(fi->column_name));
			p = fi ? (NAME_IS_NULL(fi->column_alias) ? SAFE_NAME(fi->column_name) : GET_NAME(fi->column_alias)) : QR_get_fieldname(res, col_idx);

			mylog("%s: COLUMN_NAME = '%s'\n", func, p);
			break;

		case SQL_COLUMN_LENGTH:
			value = (USE_FI(fi, unknown_sizes) && fi->length > 0) ? fi->length : pgtype_buffer_length(stmt, field_type, col_idx, unknown_sizes);
			if (0 > value)
			/* if (-1 == value)  I'm not sure which is right */
				value = 0;

			mylog("%s: col %d, column_length = %d\n", func, col_idx, value);
			break;

		case SQL_COLUMN_MONEY: /* == SQL_DESC_FIXED_PREC_SCALE */
			value = pgtype_money(conn, field_type);
inolog("COLUMN_MONEY=%d\n", value);
			break;

		case SQL_DESC_NULLABLE:
			if (SC_has_outer_join(stmt))
				value = TRUE;
			else
				value = fi ? fi->nullable : pgtype_nullable(conn, field_type);
inolog("COLUMN_NULLABLE=%d\n", value);
			break;

		case SQL_COLUMN_OWNER_NAME: /* == SQL_DESC_SCHEMA_NAME */
			p = ti ? SAFE_NAME(ti->schema_name) : NULL_STRING;
			mylog("%s: SCHEMA_NAME = '%s'\n", func, p);
			break;

		case SQL_COLUMN_PRECISION: /* in 2.x */
			value = column_size;
			if (value < 0)
				value = 0;

			mylog("%s: col %d, column_size = %d\n", func, col_idx, value);
			break;

		case SQL_COLUMN_QUALIFIER_NAME: /* == SQL_DESC_CATALOG_NAME */
			p = ti ? CurrCatString(conn) : NULL_STRING;	/* empty string means *not supported* */
			break;

		case SQL_COLUMN_SCALE: /* in 2.x */
			value = pgtype_decimal_digits(stmt, field_type, col_idx);
inolog("COLUMN_SCALE=%d\n", value);
			if (value < 0)
				value = 0;
			break;

		case SQL_COLUMN_SEARCHABLE: /* == SQL_DESC_SEARCHABLE */
			value = pgtype_searchable(conn, field_type);
			break;

		case SQL_COLUMN_TABLE_NAME: /* == SQL_DESC_TABLE_NAME */
			p = ti ? SAFE_NAME(ti->table_name) : NULL_STRING;

			mylog("%s: TABLE_NAME = '%s'\n", func, p);
			break;

		case SQL_COLUMN_TYPE: /* == SQL_DESC_CONCISE_TYPE */
			value = pgtype_to_concise_type(stmt, field_type, col_idx, unknown_sizes);
			mylog("COLUMN_TYPE=%d\n", value);
			break;

		case SQL_COLUMN_TYPE_NAME: /* == SQL_DESC_TYPE_NAME */
			p = pgtype_to_name(stmt, field_type, col_idx, fi && fi->auto_increment);
			break;

		case SQL_COLUMN_UNSIGNED: /* == SQL_DESC_UNSINGED */
			value = pgtype_unsigned(conn, field_type);
			if (value == -1)	/* non-numeric becomes TRUE (ODBC Doc) */
				value = SQL_TRUE;

			break;

		case SQL_COLUMN_UPDATABLE: /* == SQL_DESC_UPDATABLE */

			/*
			 * Neither Access or Borland care about this.
			 *
			 * if (field_type == PG_TYPE_OID) pfDesc = SQL_ATTR_READONLY;
			 * else
			 */
			if (!stmt_updatable)
				value = SQL_ATTR_READONLY;
			else
				value = fi ? (fi->updatable ? SQL_ATTR_WRITE : SQL_ATTR_READONLY) : (QR_get_attid(res, col_idx) > 0 ? SQL_ATTR_WRITE : SQL_ATTR_READONLY);
			if (SQL_ATTR_READONLY != value)
			{
				const char *name = fi ? SAFE_NAME(fi->column_name) : QR_get_fieldname(res, col_idx);
				if (stricmp(name, OID_NAME) == 0 ||
				    stricmp(name, "ctid") == 0 ||
				    stricmp(name, XMIN_NAME) == 0)
					value = SQL_ATTR_READONLY;
				else if (conn->ms_jet && fi && fi->auto_increment)
					value = SQL_ATTR_READONLY;
			}

			mylog("%s: UPDATEABLE = %d\n", func, value);
			break;
		case SQL_DESC_BASE_COLUMN_NAME:

			p = fi ? SAFE_NAME(fi->column_name) : QR_get_fieldname(res, col_idx);

			mylog("%s: BASE_COLUMN_NAME = '%s'\n", func, p);
			break;
		case SQL_DESC_BASE_TABLE_NAME: /* the same as TABLE_NAME ok ? */
			p = ti ? SAFE_NAME(ti->table_name) : NULL_STRING;

			mylog("%s: BASE_TABLE_NAME = '%s'\n", func, p);
			break;
		case SQL_DESC_LENGTH: /* different from SQL_COLUMN_LENGTH */
			value = (fi && column_size > 0) ? column_size : pgtype_desclength(stmt, field_type, col_idx, unknown_sizes);
			if (-1 == value)
				value = 0;

			mylog("%s: col %d, desc_length = %d\n", func, col_idx, value);
			break;
		case SQL_DESC_OCTET_LENGTH:
			value = (USE_FI(fi, unknown_sizes) && fi->length > 0) ? fi->length : pgtype_attr_transfer_octet_length(conn, field_type, column_size, unknown_sizes);
			if (-1 == value)
				value = 0;
			mylog("%s: col %d, octet_length = %d\n", func, col_idx, value);
			break;
		case SQL_DESC_PRECISION: /* different from SQL_COLUMN_PRECISION */
			if (value = FI_precision(fi), value <= 0)
				value = pgtype_precision(stmt, field_type, col_idx, unknown_sizes);
			if (value < 0)
				value = 0;

			mylog("%s: col %d, desc_precision = %d\n", func, col_idx, value);
			break;
		case SQL_DESC_SCALE: /* different from SQL_COLUMN_SCALE */
			value = pgtype_scale(stmt, field_type, col_idx);
			if (value < 0)
				value = 0;
			break;
		case SQL_DESC_LOCAL_TYPE_NAME:
			p = pgtype_to_name(stmt, field_type, col_idx, fi && fi->auto_increment);
			break;
		case SQL_DESC_TYPE:
			value = pgtype_to_sqldesctype(stmt, field_type, col_idx, unknown_sizes);
			break;
		case SQL_DESC_NUM_PREC_RADIX:
			value = pgtype_radix(conn, field_type);
			break;
		case SQL_DESC_LITERAL_PREFIX:
			p = pgtype_literal_prefix(conn, field_type);
			break;
		case SQL_DESC_LITERAL_SUFFIX:
			p = pgtype_literal_suffix(conn, field_type);
			break;
		case SQL_DESC_UNNAMED:
			value = (fi && NAME_IS_NULL(fi->column_name) && NAME_IS_NULL(fi->column_alias)) ? SQL_UNNAMED : SQL_NAMED;
			break;
		case 1211: /* SQL_CA_SS_COLUMN_HIDDEN ? */
			value = 0;
			break;
		case 1212: /* SQL_CA_SS_COLUMN_KEY ? */
			if (fi)
			{
				if (fi->columnkey < 0)
				{
					SC_set_SS_columnkey(stmt);
				}
				value = fi->columnkey;
				mylog("%s:SS_COLUMN_KEY=%d\n", func, value);
				break;
			}
			SC_set_error(stmt, STMT_OPTION_NOT_FOR_THE_DRIVER, "this request may be for MS SQL Server", func);
			return SQL_ERROR;
		default:
			SC_set_error(stmt, STMT_INVALID_OPTION_IDENTIFIER, "ColAttribute for this type not implemented yet", func);
			return SQL_ERROR;
	}

	result = SQL_SUCCESS;

	if (p)
	{							/* char/binary data */
		size_t len = strlen(p);

		if (rgbDesc)
		{
			strncpy_null((char *) rgbDesc, p, (size_t) cbDescMax);

			if (len >= cbDescMax)
			{
				result = SQL_SUCCESS_WITH_INFO;
				SC_set_error(stmt, STMT_TRUNCATED, "The buffer was too small for the rgbDesc.", func);
			}
		}

		if (pcbDesc)
			*pcbDesc = (SQLSMALLINT) len;
	}
	else
	{
		/* numeric data */
		if (pfDesc)
			*pfDesc = value;
	}

	return result;
}


/*	Returns result data for a single column in the current row. */
RETCODE		SQL_API
PGAPI_GetData(HSTMT hstmt,
			  SQLUSMALLINT icol,
			  SQLSMALLINT fCType,
			  PTR rgbValue,
			  SQLLEN cbValueMax,
			  SQLLEN * pcbValue)
{
	CSTR func = "PGAPI_GetData";
	QResultClass *res;
	StatementClass *stmt = (StatementClass *) hstmt;
	UInt2		num_cols;
	SQLLEN		num_rows;
	OID		field_type;
	int		atttypmod;
	void	   *value = NULL;
	RETCODE		result = SQL_SUCCESS;
	char		get_bookmark = FALSE;
	SQLSMALLINT	target_type;
	int		precision = -1;
#ifdef	WITH_UNIXODBC
	SQLCHAR		dum_rgb[2] = "\0\0";
#endif	/* WITH_UNIXODBC */

	mylog("%s: enter, stmt=%p icol=%d\n", func, stmt, icol);

	if (!stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}
	res = SC_get_Curres(stmt);

	if (STMT_EXECUTING == stmt->status)
	{
		SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Can't get data while statement is still executing.", func);
		return SQL_ERROR;
	}

	if (stmt->status != STMT_FINISHED)
	{
		SC_set_error(stmt, STMT_STATUS_ERROR, "GetData can only be called after the successful execution on a SQL statement", func);
		return SQL_ERROR;
	}

#ifdef	WITH_UNIXODBC
	if (NULL == rgbValue) /* unixODBC allows rgbValue is NULL? */
	{
		cbValueMax = 0;
		rgbValue = dum_rgb; /* to avoid a crash */
	}
#endif	/* WITH_UNIXODBC */
	if (SQL_ARD_TYPE == fCType)
	{
		ARDFields	*opts;
		BindInfoClass	*binfo = NULL;

		opts = SC_get_ARDF(stmt);
		if (0 == icol)
			binfo = opts->bookmark;
		else if (icol <= opts->allocated && opts->bindings)
			binfo = &opts->bindings[icol - 1];
		if (binfo)
		{
			target_type = binfo->returntype;
			mylog("SQL_ARD_TYPE=%d\n", target_type);
			precision = binfo->precision;
		}
		else
		{
			SC_set_error(stmt, STMT_STATUS_ERROR, "GetData can't determine the type via ARD", func);
			return SQL_ERROR;
		}
	}
	else
		target_type = fCType;
	if (icol == 0)
	{
		if (stmt->options.use_bookmarks == SQL_UB_OFF)
		{
			SC_set_error(stmt, STMT_COLNUM_ERROR, "Attempt to retrieve bookmark with bookmark usage disabled", func);
			return SQL_ERROR;
		}

		/* Make sure it is the bookmark data type */
		switch (target_type)
		{
			case SQL_C_BOOKMARK:
			case SQL_C_VARBOOKMARK:
				break;
			default:
inolog("GetData Column 0 is type %d not of type SQL_C_BOOKMARK", target_type);
				SC_set_error(stmt, STMT_PROGRAM_TYPE_OUT_OF_RANGE, "Column 0 is not of type SQL_C_BOOKMARK", func);
				return SQL_ERROR;
		}

		get_bookmark = TRUE;
	}
	else
	{
		/* use zero-based column numbers */
		icol--;

		/* make sure the column number is valid */
		num_cols = QR_NumPublicResultCols(res);
		if (icol >= num_cols)
		{
			SC_set_error(stmt, STMT_INVALID_COLUMN_NUMBER_ERROR, "Invalid column number.", func);
			return SQL_ERROR;
		}
	}

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */
	if (!SC_is_fetchcursor(stmt))
	{
		/* make sure we're positioned on a valid row */
		num_rows = QR_get_num_total_tuples(res);
		if ((stmt->currTuple < 0) ||
			(stmt->currTuple >= num_rows))
		{
			SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Not positioned on a valid row for GetData.", func);
			result = SQL_ERROR;
			goto cleanup;
		}
		mylog("     num_rows = %d\n", num_rows);

		if (!get_bookmark)
		{
			SQLLEN	curt = GIdx2CacheIdx(stmt->currTuple, stmt, res);
			value = QR_get_value_backend_row(res, curt, icol);
inolog("currT=%d base=%d rowset=%d\n", stmt->currTuple, QR_get_rowstart_in_cache(res), SC_get_rowset_start(stmt));
			mylog("     value = '%s'\n", NULL_IF_NULL(value));
		}
	}
	else
	{
		/* it's a SOCKET result (backend data) */
		if (stmt->currTuple == -1 || !res || !res->tupleField)
		{
			SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Not positioned on a valid row for GetData.", func);
			result = SQL_ERROR;
			goto cleanup;
		}

		if (!get_bookmark)
		{
			/** value = QR_get_value_backend(res, icol); maybe thiw doesn't work */
			SQLLEN	curt = GIdx2CacheIdx(stmt->currTuple, stmt, res);
			value = QR_get_value_backend_row(res, curt, icol);
		}
		mylog("  socket: value = '%s'\n", NULL_IF_NULL(value));
	}

	if (get_bookmark)
	{
		BOOL	contents_get = FALSE;

		if (rgbValue)
		{
			if (SQL_C_BOOKMARK == target_type || sizeof(UInt4) <= cbValueMax)
			{
				Int4 bookmark = SC_make_int4_bookmark(stmt->currTuple);
				contents_get = TRUE;
				memcpy(rgbValue, &bookmark, sizeof(bookmark));
			}
		}
		if (pcbValue)
			*pcbValue = sizeof(Int4);

		if (contents_get)
			result = SQL_SUCCESS;
		else
		{
			SC_set_error(stmt, STMT_TRUNCATED, "The buffer was too small for the GetData.", func);
			result = SQL_SUCCESS_WITH_INFO;
		}
		goto cleanup;
	}

	field_type = QR_get_field_type(res, icol);
	atttypmod = QR_get_atttypmod(res, icol);

	mylog("**** %s: icol = %d, target_type = %d, field_type = %d, value = '%s'\n", func, icol, target_type, field_type, NULL_IF_NULL(value));

	SC_set_current_col(stmt, icol);

	result = copy_and_convert_field(stmt, field_type, atttypmod, value,
			target_type, precision, rgbValue, cbValueMax, pcbValue, pcbValue);

	switch (result)
	{
		case COPY_OK:
			result = SQL_SUCCESS;
			break;

		case COPY_UNSUPPORTED_TYPE:
			SC_set_error(stmt, STMT_RESTRICTED_DATA_TYPE_ERROR, "Received an unsupported type from Postgres.", func);
			result = SQL_ERROR;
			break;

		case COPY_UNSUPPORTED_CONVERSION:
			SC_set_error(stmt, STMT_RESTRICTED_DATA_TYPE_ERROR, "Couldn't handle the necessary data type conversion.", func);
			result = SQL_ERROR;
			break;

		case COPY_RESULT_TRUNCATED:
			SC_set_error(stmt, STMT_TRUNCATED, "The buffer was too small for the GetData.", func);
			result = SQL_SUCCESS_WITH_INFO;
			break;

		case COPY_INVALID_STRING_CONVERSION:    /* invalid string */
			SC_set_error(stmt, STMT_STRING_CONVERSION_ERROR, "invalid string conversion occured.", func);
			result = SQL_ERROR;
			break;

		case COPY_GENERAL_ERROR:		/* error msg already filled in */
			result = SQL_ERROR;
			break;

		case COPY_NO_DATA_FOUND:
			/* SC_log_error(func, "no data found", stmt); */
			result = SQL_NO_DATA_FOUND;
			break;

		default:
			SC_set_error(stmt, STMT_INTERNAL_ERROR, "Unrecognized return value from copy_and_convert_field.", func);
			result = SQL_ERROR;
			break;
	}

cleanup:
#undef	return
inolog("%s returning %d\n", __FUNCTION__, result);
	return result;
}


/*
 *		Returns data for bound columns in the current row ("hstmt->iCursor"),
 *		advances the cursor.
 */
RETCODE		SQL_API
PGAPI_Fetch(HSTMT hstmt)
{
	CSTR func = "PGAPI_Fetch";
	StatementClass *stmt = (StatementClass *) hstmt;
	ARDFields	*opts;
	QResultClass *res;
	BindInfoClass	*bookmark;
	RETCODE		retval = SQL_SUCCESS;

	mylog("%s: stmt = %p, stmt->result= %p\n", func, stmt, stmt ? SC_get_Curres(stmt) : NULL);

	if (!stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}

	SC_clear_error(stmt);

	if (!(res = SC_get_Curres(stmt)))
	{
		SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in PGAPI_Fetch.", func);
		return SQL_ERROR;
	}

	/* Not allowed to bind a bookmark column when using SQLFetch. */
	opts = SC_get_ARDF(stmt);
	if ((bookmark = opts->bookmark) && bookmark->buffer)
	{
		SC_set_error(stmt, STMT_COLNUM_ERROR, "Not allowed to bind a bookmark column when using PGAPI_Fetch", func);
		return SQL_ERROR;
	}

	if (stmt->status == STMT_EXECUTING)
	{
		SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Can't fetch while statement is still executing.", func);
		return SQL_ERROR;
	}

	if (stmt->status != STMT_FINISHED)
	{
		SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Fetch can only be called after the successful execution on a SQL statement", func);
		return SQL_ERROR;
	}

	if (opts->bindings == NULL)
	{
		if (!SC_may_fetch_rows(stmt))
			return SQL_NO_DATA_FOUND;
		/* just to avoid a crash if the user insists on calling this */
		/* function even if SQL_ExecDirect has reported an Error */
		SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Bindings were not allocated properly.", func);
		return SQL_ERROR;
	}

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */
	if (stmt->rowset_start < 0)
		SC_set_rowset_start(stmt, 0, TRUE);
	QR_set_reqsize(res, 1);
	/* QR_inc_rowstart_in_cache(res, stmt->last_fetch_count_include_ommitted); */
	SC_inc_rowset_start(stmt, stmt->last_fetch_count_include_ommitted);

	retval = SC_fetch(stmt);
#undef	return
	return retval;
}

static RETCODE SQL_API
SC_pos_reload_needed(StatementClass *stmt, SQLULEN req_size, UDWORD flag);

SQLLEN
getNthValid(const QResultClass *res, SQLLEN sta, UWORD orientation,
			SQLULEN nth, SQLLEN *nearest)
{
	SQLLEN	i, num_tuples = QR_get_num_total_tuples(res), nearp;
	SQLULEN count;
	KeySet	*keyset;

	if (!QR_once_reached_eof(res))
		num_tuples = INT_MAX;
	/* Note that the parameter nth is 1-based */
inolog("get %dth Valid data from %d to %s [dlt=%d]", nth, sta, orientation == SQL_FETCH_PRIOR ? "backward" : "forward", res->dl_count);
	if (0 == res->dl_count)
	{
		if (SQL_FETCH_PRIOR == orientation)
		{
			if (sta + 1 >= (SQLLEN) nth)
			{
				*nearest = sta + 1 - nth;
				return nth;
			}
			*nearest = -1;
			return -(SQLLEN)(sta + 1);
		}
		else
		{
			nearp = sta - 1 + nth;
			if (nearp < num_tuples)
			{
				*nearest = nearp;
				return nth;
			}
			*nearest = num_tuples;
			return -(SQLLEN)(num_tuples - sta);
		}
	}
	count = 0;
	if (QR_get_cursor(res))
	{
		SQLLEN	*deleted = res->deleted;
		SQLLEN	delsta;

		if (SQL_FETCH_PRIOR == orientation)
		{
			*nearest = sta + 1 - nth;
			delsta = (-1);
			for (i = res->dl_count - 1; i >=0 && *nearest <= deleted[i]; i--)
			{
inolog("deleted[%d]=%d\n", i, deleted[i]);
				if (sta >= deleted[i])
				{
					(*nearest)--;
					if (i > delsta)
						delsta = i;
				}
			}
inolog("nearest=%d\n", *nearest);
			if (*nearest < 0)
			{
				*nearest = -1;
				count = sta - delsta;
			}
			else
				return nth;
		}
		else
		{
			*nearest = sta - 1 + nth;
			delsta = res->dl_count;
			if (!QR_once_reached_eof(res))
				num_tuples = INT_MAX;
			for (i = 0; i < res->dl_count && *nearest >= deleted[i]; i++)
			{
				if (sta <= deleted[i])
				{
					(*nearest)++;
					if (i < delsta)
						delsta = i;
				}
			}
			if (*nearest >= num_tuples)
			{
				*nearest = num_tuples;
				count = *nearest - sta + delsta - res->dl_count;
			}
			else
				return nth;
		}
	}
	else if (SQL_FETCH_PRIOR == orientation)
	{
		for (i = sta, keyset = res->keyset + sta;
			i >= 0; i--, keyset--)
		{
			if (0 == (keyset->status & (CURS_SELF_DELETING | CURS_SELF_DELETED | CURS_OTHER_DELETED)))
			{
				*nearest = i;
inolog(" nearest=%d\n", *nearest);
				if (++count == nth)
					return count;
			}
		}
		*nearest = -1;
	}
	else
	{
		for (i = sta, keyset = res->keyset + sta;
			i < num_tuples; i++, keyset++)
		{
			if (0 == (keyset->status & (CURS_SELF_DELETING | CURS_SELF_DELETED | CURS_OTHER_DELETED)))
			{
				*nearest = i;
inolog(" nearest=%d\n", *nearest);
				if (++count == nth)
					return count;
			}
		}
		*nearest = num_tuples;
	}
inolog(" nearest not found\n");
	return -(SQLLEN)count;
}

static void
move_cursor_position_if_needed(StatementClass *self, QResultClass *res)
{
	SQLLEN	move_offset;

	/*
	 * The move direction must be initialized to is_not_moving or
	 * is_moving_from_the_last in advance.
	 */
	if (!QR_get_cursor(res))
	{
		QR_stop_movement(res); /* for safety */
		res->move_offset = 0;
		return;
	}
inolog("BASE=%d numb=%d curr=%d cursT=%d\n", QR_get_rowstart_in_cache(res), res->num_cached_rows, self->currTuple, res->cursTuple);

	/* normal case */
	res->move_offset = 0;
	move_offset = self->currTuple - res->cursTuple;
	if (QR_get_rowstart_in_cache(res) >= 0 &&
	     QR_get_rowstart_in_cache(res) <= res->num_cached_rows)
	{
		QR_set_next_in_cache(res, (QR_get_rowstart_in_cache(res) < 0) ? 0 : QR_get_rowstart_in_cache(res));
		return;
	}
	if (0 == move_offset)
		return;
	if (move_offset > 0)
	{
		QR_set_move_forward(res);
		res->move_offset = move_offset;
	}
	else
	{
		QR_set_move_backward(res);
		res->move_offset = -move_offset;
	}
}
/*
 *	return NO_DATA_FOUND macros
 *	  save_rowset_start or num_tuples must be defined
 */
#define	EXTFETCH_RETURN_BOF(stmt, res) \
{ \
inolog("RETURN_BOF\n"); \
	SC_set_rowset_start(stmt, -1, TRUE); \
	stmt->currTuple = -1; \
	/* move_cursor_position_if_needed(stmt, res); */ \
	return SQL_NO_DATA_FOUND; \
}
#define	EXTFETCH_RETURN_EOF(stmt, res) \
{ \
inolog("RETURN_EOF\n"); \
	SC_set_rowset_start(stmt, num_tuples, TRUE); \
	stmt->currTuple = -1; \
	/* move_cursor_position_if_needed(stmt, res); */ \
	return SQL_NO_DATA_FOUND; \
}

/*	This fetchs a block of data (rowset). */
RETCODE		SQL_API
PGAPI_ExtendedFetch(HSTMT hstmt,
					SQLUSMALLINT fFetchType,
					SQLLEN irow,
					SQLULEN * pcrow,
					SQLUSMALLINT * rgfRowStatus,
					SQLLEN bookmark_offset,
					SQLLEN rowsetSize)
{
	CSTR func = "PGAPI_ExtendedFetch";
	StatementClass *stmt = (StatementClass *) hstmt;
	ARDFields	*opts;
	QResultClass *res;
	BindInfoClass	*bookmark;
	SQLLEN		num_tuples, i, fc_io;
	SQLLEN		save_rowset_size, progress_size;
	SQLLEN		rowset_start, rowset_end = (-1);
	RETCODE		result = SQL_SUCCESS;
	char		truncated, error, should_set_rowset_start = FALSE;
	SQLLEN		currp;
	UWORD		pstatus;
	BOOL		currp_is_valid, reached_eof, useCursor;
	SQLLEN		reqsize = rowsetSize;

	mylog("%s: stmt=%p rowsetSize=%d\n", func, stmt, rowsetSize);

	if (!stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}

	/* if (SC_is_fetchcursor(stmt) && !stmt->manual_result) */
	if (SQL_CURSOR_FORWARD_ONLY == stmt->options.cursor_type)
	{
		if (fFetchType != SQL_FETCH_NEXT)
		{
			SC_set_error(stmt, STMT_FETCH_OUT_OF_RANGE, "The fetch type for PGAPI_ExtendedFetch isn't allowed with ForwardOnly cursor.", func);
			return SQL_ERROR;
		}
	}

	SC_clear_error(stmt);

	if (!(res = SC_get_Curres(stmt)))
	{
		SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in PGAPI_ExtendedFetch.", func);
		return SQL_ERROR;
	}

	opts = SC_get_ARDF(stmt);
	/*
	 * If a bookmark colunmn is bound but bookmark usage is off, then
	 * error
	 */
	if ((bookmark = opts->bookmark) && bookmark->buffer && stmt->options.use_bookmarks == SQL_UB_OFF)
	{
		SC_set_error(stmt, STMT_COLNUM_ERROR, "Attempt to retrieve bookmark with bookmark usage disabled", func);
		return SQL_ERROR;
	}

	if (stmt->status == STMT_EXECUTING)
	{
		SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Can't fetch while statement is still executing.", func);
		return SQL_ERROR;
	}

	if (stmt->status != STMT_FINISHED)
	{
		SC_set_error(stmt, STMT_STATUS_ERROR, "ExtendedFetch can only be called after the successful execution on a SQL statement", func);
		return SQL_ERROR;
	}

	if (opts->bindings == NULL)
	{
		if (!SC_may_fetch_rows(stmt))
			return SQL_NO_DATA_FOUND;
		/* just to avoid a crash if the user insists on calling this */
		/* function even if SQL_ExecDirect has reported an Error */
		SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Bindings were not allocated properly.", func);
		return SQL_ERROR;
	}

	/* Initialize to no rows fetched */
	if (rgfRowStatus)
		for (i = 0; i < rowsetSize; i++)
			*(rgfRowStatus + i) = SQL_ROW_NOROW;

	if (pcrow)
		*pcrow = 0;

	useCursor = (SC_is_fetchcursor(stmt) && NULL != QR_get_cursor(res));
	num_tuples = QR_get_num_total_tuples(res);
	reached_eof = QR_once_reached_eof(res) && QR_get_cursor(res);
	if (useCursor && !reached_eof)
		num_tuples = INT_MAX;

inolog("num_tuples=%d\n", num_tuples);
	/* Save and discard the saved rowset size */
	save_rowset_size = stmt->save_rowset_size;
	stmt->save_rowset_size = -1;
	rowset_start = SC_get_rowset_start(stmt);

	QR_stop_movement(res);
	res->move_offset = 0;
	switch (fFetchType)
	{
		int	min_nth;
		SQLLEN	bidx;

		case SQL_FETCH_NEXT:

			/*
			 * From the odbc spec... If positioned before the start of the
			 * RESULT SET, then this should be equivalent to
			 * SQL_FETCH_FIRST.
			 */

			progress_size = (save_rowset_size > 0 ? save_rowset_size : rowsetSize);
			if (rowset_start < 0)
				SC_set_rowset_start(stmt, 0, TRUE);
			else if (res->keyset)
			{
				if (stmt->last_fetch_count <= progress_size)
				{
					SC_inc_rowset_start(stmt, stmt->last_fetch_count_include_ommitted);
					progress_size -= stmt->last_fetch_count;
				}
				if (progress_size > 0)
				{
					if (getNthValid(res, SC_get_rowset_start(stmt),
						SQL_FETCH_NEXT, progress_size + 1,
						&rowset_start) <= 0)
					{
						EXTFETCH_RETURN_EOF(stmt, res)
					}
					else
						should_set_rowset_start =TRUE;
				}
			}
			else
				SC_inc_rowset_start(stmt, progress_size);
			mylog("SQL_FETCH_NEXT: num_tuples=%d, currtuple=%d, rowst=%d\n", num_tuples, stmt->currTuple, rowset_start);
			break;

		case SQL_FETCH_PRIOR:
			mylog("SQL_FETCH_PRIOR: num_tuples=%d, currtuple=%d\n", num_tuples, stmt->currTuple);

			/*
			 * From the odbc spec... If positioned after the end of the
			 * RESULT SET, then this should be equivalent to
			 * SQL_FETCH_LAST.
			 */
			if (rowset_start <= 0)
			{
				EXTFETCH_RETURN_BOF(stmt, res)
			}
			if (rowset_start >= num_tuples &&
			    rowsetSize > num_tuples)
			{
				SC_set_error(stmt, STMT_POS_BEFORE_RECORDSET, "fetch prior from eof and before the beginning", func);
				SC_set_rowset_start(stmt, 0, TRUE);
			}
			else if (QR_haskeyset(res))
			{
				rowset_end = rowset_start - 1;
				if (min_nth = getNthValid(res, rowset_end, SQL_FETCH_PRIOR, rowsetSize, &rowset_start), min_nth <= 0)
				{
					if (min_nth == 0)
					{
						EXTFETCH_RETURN_BOF(stmt, res)
					}
					else
					{
						SC_set_error(stmt, STMT_POS_BEFORE_RECORDSET, "fetch prior and before the beginning", func);
						rowset_end = (-1);
						SC_set_rowset_start(stmt, 0, TRUE);
					}
				}
				else
				{
					should_set_rowset_start = TRUE;
					reqsize = rowset_end - rowset_start + 1;
				}
			}
			else if (rowset_start < rowsetSize)
			{
				SC_set_error(stmt, STMT_POS_BEFORE_RECORDSET, "fetch prior from eof and before the beginning", func);
				SC_set_rowset_start(stmt, 0, TRUE);
			}
			else
				SC_inc_rowset_start(stmt, -rowsetSize);
			break;

		case SQL_FETCH_FIRST:
			mylog("SQL_FETCH_FIRST: num_tuples=%d, currtuple=%d\n", num_tuples, stmt->currTuple);

			SC_set_rowset_start(stmt, 0, TRUE);
			break;

		case SQL_FETCH_LAST:
			mylog("SQL_FETCH_LAST: num_tuples=%d, currtuple=%d\n", num_tuples, stmt->currTuple);

			if (!reached_eof)
			{
				QR_move_cursor_to_last(res, stmt);
				num_tuples = QR_get_num_total_tuples(res);
			}
			rowset_end = num_tuples - 1;
			if (min_nth = getNthValid(res, rowset_end, SQL_FETCH_PRIOR, rowsetSize, &rowset_start), min_nth <= 0)
			{
				if (min_nth == 0)
				{
					EXTFETCH_RETURN_BOF(stmt, res)
				}
				else
				{
					SC_set_error(stmt, STMT_POS_BEFORE_RECORDSET, "fetch last and before the beginning", func);
					rowset_end = (-1);
					SC_set_rowset_start(stmt, 0, TRUE);
				}
			}
			else
			{
				should_set_rowset_start = TRUE;
				reqsize = rowset_end - rowset_start + 1;
			}
			break;

		case SQL_FETCH_ABSOLUTE:
			mylog("SQL_FETCH_ABSOLUTE: num_tuples=%d, currtuple=%d, irow=%d\n", num_tuples, stmt->currTuple, irow);

			/* Position before result set, but dont fetch anything */
			if (irow == 0)
			{
				EXTFETCH_RETURN_BOF(stmt, res)
			}
			/* Position before the desired row */
			else if (irow > 0)
			{
				if (getNthValid(res, 0, SQL_FETCH_NEXT, irow, &rowset_start) <= 0)
				{
					EXTFETCH_RETURN_EOF(stmt, res)
				}
				else
					should_set_rowset_start = TRUE;
			}
			/* Position with respect to the end of the result set */
			else
			{
				if (!reached_eof)
				{
					QR_move_cursor_to_last(res, stmt);
					num_tuples = QR_get_num_total_tuples(res);
				}
				if (min_nth = getNthValid(res, num_tuples - 1, SQL_FETCH_PRIOR, -irow, &rowset_start), min_nth <= 0)
				{
					if (min_nth == 0)
					{
						EXTFETCH_RETURN_BOF(stmt, res)
					}
					else
					{
						SC_set_error(stmt, STMT_POS_BEFORE_RECORDSET, "fetch absolute and before the beginning", func);
						rowset_end = (-1);
						SC_set_rowset_start(stmt, 0, TRUE);
					}
				}
				else
					should_set_rowset_start = TRUE;
			}
			break;

		case SQL_FETCH_RELATIVE:

			/*
			 * Refresh the current rowset -- not currently implemented,
			 * but lie anyway
			 */
			if (irow == 0)
				break;

			if (irow > 0)
			{
				if (getNthValid(res, SC_get_rowset_start(stmt) + 1, SQL_FETCH_NEXT, irow, &rowset_start) <= 0)
				{
					EXTFETCH_RETURN_EOF(stmt, res)
				}
				else
					should_set_rowset_start = TRUE;
			}
			else
			{
				if (min_nth = getNthValid(res, SC_get_rowset_start(stmt) - 1, SQL_FETCH_PRIOR, -irow, &rowset_start), min_nth <= 0)
				{
					if (min_nth == 0)
						EXTFETCH_RETURN_BOF(stmt, res)
					else
					{
						SC_set_error(stmt, STMT_POS_BEFORE_RECORDSET, "fetch relative and before the beginning", func);
						rowset_end = (-1);
						SC_set_rowset_start(stmt, 0, TRUE);
					}
				}
				else
					should_set_rowset_start = TRUE;
			}
			break;

		case SQL_FETCH_BOOKMARK:
			bidx = SC_resolve_int4_bookmark(irow);

			if (bidx < 0)
			{
				if (!reached_eof)
				{
					QR_move_cursor_to_last(res, stmt);
					num_tuples = QR_get_num_total_tuples(res);
				}
				bidx = num_tuples - 1 - res->ad_count - bidx;
			}

			rowset_start = bidx;
			if (bookmark_offset >= 0)
			{
				if (getNthValid(res, bidx, SQL_FETCH_NEXT, bookmark_offset + 1, &rowset_start) <= 0)
				{
					EXTFETCH_RETURN_EOF(stmt, res)
				}
				else
					should_set_rowset_start = TRUE;
			}
			else if (getNthValid(res, bidx, SQL_FETCH_PRIOR, 1 - bookmark_offset, &rowset_start) <= 0)
			{
				stmt->currTuple = -1;
				EXTFETCH_RETURN_BOF(stmt, res)
			}
			else
				should_set_rowset_start = TRUE;
			break;

		default:
			SC_set_error(stmt, STMT_FETCH_OUT_OF_RANGE, "Unsupported PGAPI_ExtendedFetch Direction", func);
			return SQL_ERROR;
	}

	/*
	 * CHECK FOR PROPER CURSOR STATE
	 */

	/*
	 * Handle Declare Fetch style specially because the end is not really
	 * the end...
	 */
	if (!should_set_rowset_start)
		rowset_start = SC_get_rowset_start(stmt);
	if (useCursor)
	{
		if (reached_eof &&
		    rowset_start >= num_tuples)
		{
			EXTFETCH_RETURN_EOF(stmt, res)
		}
	}
	else
	{
		/* If *new* rowset is after the result_set, return no data found */
		if (rowset_start >= num_tuples)
		{
			EXTFETCH_RETURN_EOF(stmt, res)
		}
	}
	/* If *new* rowset is prior to result_set, return no data found */
	if (rowset_start < 0)
	{
		if (rowset_start + rowsetSize <= 0)
		{
			EXTFETCH_RETURN_BOF(stmt, res)
		}
		else
		{	/* overlap with beginning of result set,
			 * so get first rowset */
			SC_set_rowset_start(stmt, 0, TRUE);
		}
		should_set_rowset_start = FALSE;
	}

#define	return DONT_CALL_RETURN_FROM_HERE???
	/* set the rowset_start if needed */
	if (should_set_rowset_start)
		SC_set_rowset_start(stmt, rowset_start, TRUE);
	if (rowset_end < 0 && QR_haskeyset(res))
	{
		getNthValid(res, rowset_start,
			SQL_FETCH_NEXT, rowsetSize, &rowset_end);
		reqsize = rowset_end - rowset_start + 1;
	}
	QR_set_reqsize(res, (Int4) reqsize);
	/* currTuple is always 1 row prior to the rowset start */
	stmt->currTuple = RowIdx2GIdx(-1, stmt);

	if (SC_is_fetchcursor(stmt) ||
	    SQL_CURSOR_KEYSET_DRIVEN == stmt->options.cursor_type)
	{
		move_cursor_position_if_needed(stmt, res);
	}
	else
		QR_set_rowstart_in_cache(res, SC_get_rowset_start(stmt));

	if (res->keyset && !QR_get_cursor(res))
	{
		UDWORD	flag = 0;

		if (SQL_CURSOR_KEYSET_DRIVEN == stmt->options.cursor_type)
		{
			if (fFetchType != SQL_FETCH_NEXT ||
				QR_get_rowstart_in_cache(res) + reqsize > QR_get_num_cached_tuples(res))
			{
				flag = 1;
			}
		}
		if (SQL_RD_ON == stmt->options.retrieve_data ||
		    flag != 0)
		{
			SC_pos_reload_needed(stmt, reqsize, flag);
		}
	}
	/* Physical Row advancement occurs for each row fetched below */

	mylog("PGAPI_ExtendedFetch: new currTuple = %d\n", stmt->currTuple);

	truncated = error = FALSE;

	currp = -1;
	stmt->bind_row = 0;		/* set the binding location */
	result = SC_fetch(stmt);
	if (SQL_ERROR == result)
		goto cleanup;
	if (SQL_NO_DATA_FOUND != result && res->keyset)
	{
		currp = GIdx2KResIdx(SC_get_rowset_start(stmt), stmt, res);
inolog("currp=%d\n", currp);
		if (currp < 0)
		{
			result = SQL_ERROR;
			mylog("rowset_start=%d but currp=%d\n", SC_get_rowset_start(stmt), currp);
			SC_set_error(stmt, STMT_INTERNAL_ERROR, "rowset_start not in the keyset", func);
			goto cleanup;
		}
	}
	for (i = 0, fc_io = 0; SQL_NO_DATA_FOUND != result && SQL_ERROR != result; currp++)
	{
		fc_io++;
		currp_is_valid = FALSE;
		if (res->keyset)
		{
			if (currp < res->num_cached_keys)
			{
				currp_is_valid = TRUE;
				res->keyset[currp].status &= ~CURS_IN_ROWSET; /* Off the flag first */
			}
			else
			{
				mylog("Umm current row is out of keyset\n");
				break;
			}
		}
inolog("ExtFetch result=%d\n", result);
		if (currp_is_valid && SQL_SUCCESS_WITH_INFO == result && 0 == stmt->last_fetch_count)
		{
inolog("just skipping deleted row %d\n", currp);
			if (rowsetSize - i + fc_io > reqsize)
				QR_set_reqsize(res, (Int4) (rowsetSize - i + fc_io));
			result = SC_fetch(stmt);
			if (SQL_ERROR == result)
				break;
			continue;
		}

		/* Determine Function status */
		if (result == SQL_SUCCESS_WITH_INFO)
			truncated = TRUE;
		else if (result == SQL_ERROR)
			error = TRUE;

		/* Determine Row Status */
		if (rgfRowStatus)
		{
			if (result == SQL_ERROR)
				*(rgfRowStatus + i) = SQL_ROW_ERROR;
			else if (currp_is_valid)
			{
				pstatus = (res->keyset[currp].status & KEYSET_INFO_PUBLIC);
				if (pstatus != 0 && pstatus != SQL_ROW_ADDED)
				{
					rgfRowStatus[i] = pstatus;
				}
				else
					rgfRowStatus[i] = SQL_ROW_SUCCESS;
				/* refresh the status */
				/* if (SQL_ROW_DELETED != pstatus) */
				res->keyset[currp].status &= (~KEYSET_INFO_PUBLIC);
			}
			else
				*(rgfRowStatus + i) = SQL_ROW_SUCCESS;
		}
		if (SQL_ERROR != result && currp_is_valid)
			res->keyset[currp].status |= CURS_IN_ROWSET; /* This is the unique place where the CURS_IN_ROWSET bit is turned on */
		i++;
		if (i >= rowsetSize)
			break;
		stmt->bind_row = (SQLSETPOSIROW) i; /* set the binding location */
		result = SC_fetch(stmt);
	}
	if (SQL_ERROR == result)
		goto cleanup;

	/* Save the fetch count for SQLSetPos */
	stmt->last_fetch_count = i;
	stmt->save_rowset_size = rowsetSize;
	/*
	currp = KResIdx2GIdx(currp, stmt, res);
	stmt->last_fetch_count_include_ommitted = GIdx2RowIdx(currp, stmt);
	*/
	stmt->last_fetch_count_include_ommitted = fc_io;

	/* Reset next binding row */
	stmt->bind_row = 0;

	/* Move the cursor position to the first row in the result set. */
	stmt->currTuple = RowIdx2GIdx(0, stmt);

	/* For declare/fetch, need to reset cursor to beginning of rowset */
	if (useCursor)
		QR_set_position(res, 0);

	/* Set the number of rows retrieved */
	if (pcrow)
		*pcrow = i;
inolog("pcrow=%d\n", i);

	if (i == 0)
		/* Only DeclareFetch should wind up here */
		result = SQL_NO_DATA_FOUND;
	else if (error)
		result = SQL_ERROR;
	else if (truncated)
		result = SQL_SUCCESS_WITH_INFO;
	else if (SC_get_errornumber(stmt) == STMT_POS_BEFORE_RECORDSET)
		result = SQL_SUCCESS_WITH_INFO;
	else
		result = SQL_SUCCESS;

cleanup:
#undef	return
	return result;
}


/*
 *		This determines whether there are more results sets available for
 *		the "hstmt".
 */
/* CC: return SQL_NO_DATA_FOUND since we do not support multiple result sets */
RETCODE		SQL_API
PGAPI_MoreResults(HSTMT hstmt)
{
	CSTR func = "PGAPI_MoreResults";
	StatementClass	*stmt = (StatementClass *) hstmt;
	QResultClass	*res;
	RETCODE		ret = SQL_SUCCESS;

	mylog("%s: entering...\n", func);
	res = SC_get_Curres(stmt);
	if (res)
	{
		res = res->next;
		SC_set_Curres(stmt, res);
	}
	if (res)
	{
		SQLSMALLINT	num_p;

		if (stmt->multi_statement < 0)
			PGAPI_NumParams(stmt, &num_p);
		if (stmt->multi_statement > 0)
		{
			const char *cmdstr;

			SC_initialize_cols_info(stmt, FALSE, TRUE);
			stmt->statement_type = STMT_TYPE_UNKNOWN;
			if (cmdstr = QR_get_command(res), NULL != cmdstr)
				stmt->statement_type = statement_type(cmdstr);
			stmt->join_info = 0;
			SC_clear_parse_method(stmt);
		}
		stmt->diag_row_count = res->recent_processed_row_count;
		SC_set_rowset_start(stmt, -1, FALSE);
		stmt->currTuple = -1;
	}
	else
	{
		PGAPI_FreeStmt(hstmt, SQL_CLOSE);
		ret = SQL_NO_DATA_FOUND;
	}
	mylog("%s: returning %d\n", func, ret);
	return ret;
}


/*
 *	Stuff for updatable cursors.
 */
static Int2	getNumResultCols(const QResultClass *res)
{
	Int2	res_cols = QR_NumPublicResultCols(res);
	return res_cols;
}
static OID	getOid(const QResultClass *res, SQLLEN index)
{
	return res->keyset[index].oid;
}
static void getTid(const QResultClass *res, SQLLEN index, UInt4 *blocknum, UInt2 *offset)
{
	*blocknum = res->keyset[index].blocknum;
	*offset = res->keyset[index].offset;
}
static void KeySetSet(const TupleField *tuple, int num_fields, int num_key_fields, KeySet *keyset, BOOL statusInit)
{
	if (statusInit)
		keyset->status = 0;
	sscanf(tuple[num_fields - num_key_fields].value, "(%u,%hu)",
			&keyset->blocknum, &keyset->offset);
	if (num_key_fields > 1)
	{
		const char *oval = tuple[num_fields - 1].value;

		if ('-' == oval[0])
			sscanf(oval, "%d", &keyset->oid);
		else
			sscanf(oval, "%u", &keyset->oid);
	}
	else
		keyset->oid = 0;
}

static void AddRollback(StatementClass *stmt, QResultClass *res, SQLLEN index, const KeySet *keyset, Int4 dmlcode)
{
	ConnectionClass	*conn = SC_get_conn(stmt);
	Rollback *rollback;

	if (!CC_is_in_trans(conn))
		return;
inolog("AddRollback %d(%u,%u) %s\n", index, keyset->blocknum, keyset->offset, dmlcode == SQL_ADD ? "ADD" : (dmlcode == SQL_UPDATE ? "UPDATE" : (dmlcode == SQL_DELETE ? "DELETE" : "REFRESH")));
	if (!res->rollback)
	{
		res->rb_count = 0;
		res->rb_alloc = 10;
		rollback = res->rollback = malloc(sizeof(Rollback) * res->rb_alloc);
		if (!rollback)
		{
			res->rb_alloc = res->rb_count = 0;
			return;
		}
	}
	else
	{
		if (res->rb_count >= res->rb_alloc)
		{
			res->rb_alloc *= 2;
			if (rollback = realloc(res->rollback, sizeof(Rollback) * res->rb_alloc), !rollback)
			{
				res->rb_alloc = res->rb_count = 0;
				return;
			}
			res->rollback = rollback;
		}
		rollback = res->rollback + res->rb_count;
	}
	rollback->index = index;
	rollback->option = dmlcode;
	rollback->offset = 0;
	rollback->blocknum = 0;
	rollback->oid = 0;
	if (keyset)
	{
		rollback->blocknum = keyset->blocknum;
		rollback->offset = keyset->offset;
		rollback->oid = keyset->oid;
	}

	conn->result_uncommitted = 1;
	res->rb_count++;
}

SQLLEN ClearCachedRows(TupleField *tuple, int num_fields, SQLLEN num_rows)
{
	SQLLEN	i;

	for (i = 0; i < num_fields * num_rows; i++, tuple++)
	{
		if (tuple->value)
		{
inolog("freeing tuple[%d][%d].value=%p\n", i / num_fields, i % num_fields, tuple->value);
			free(tuple->value);
			tuple->value = NULL;
		}
		tuple->len = -1;
	}
	return i;
}
SQLLEN ReplaceCachedRows(TupleField *otuple, const TupleField *ituple, int num_fields, SQLLEN num_rows)
{
	SQLLEN	i;

inolog("ReplaceCachedRows %p num_fields=%d num_rows=%d\n", otuple, num_fields, num_rows);
	for (i = 0; i < num_fields * num_rows; i++, ituple++, otuple++)
	{
		if (otuple->value)
		{
			free(otuple->value);
			otuple->value = NULL;
		}
		if (ituple->value)
{
			otuple->value = strdup(ituple->value);
inolog("[%d,%d] %s copied\n", i / num_fields, i % num_fields, otuple->value);
}
		if (otuple->value)
			otuple->len = ituple->len;
		else
			otuple->len = -1;
	}
	return i;
}

static
int MoveCachedRows(TupleField *otuple, TupleField *ituple, Int2 num_fields, SQLLEN num_rows)
{
	int	i;

inolog("MoveCachedRows %p num_fields=%d num_rows=%d\n", otuple, num_fields, num_rows);
	for (i = 0; i < num_fields * num_rows; i++, ituple++, otuple++)
	{
		if (otuple->value)
		{
			free(otuple->value);
			otuple->value = NULL;
		}
		if (ituple->value)
		{
			otuple->value = ituple->value;
			ituple->value = NULL;
inolog("[%d,%d] %s copied\n", i / num_fields, i % num_fields, otuple->value);
		}
		otuple->len = ituple->len;
		ituple->len = -1;
	}
	return i;
}

static const char *
ti_quote(StatementClass *stmt, OID tableoid, char *buf, int buf_size)
{
	TABLE_INFO	*ti = stmt->ti[0];
	pgNAME		rNAME;

	if (0 == tableoid || !TI_has_subclass(ti))
		return quote_table(ti->schema_name, ti->table_name, buf, buf_size);
	else if (NAME_IS_VALID(rNAME = TI_From_IH(ti, tableoid)))
		return SAFE_NAME(rNAME);
	else
	{
		char	query[200];
		QResultClass	*res;
		char	*ret = "";

		SPRINTF_FIXED(query, "select relname, nspname from pg_class c, pg_namespace n where c.oid=%u and c.relnamespace=n.oid", tableoid);
		res = CC_send_query(SC_get_conn(stmt), query, NULL, READ_ONLY_QUERY, stmt);
		if (QR_command_maybe_successful(res) &&
		    QR_get_num_cached_tuples(res) == 1)
		{
			pgNAME	schema_name, table_name;

			SET_NAME_DIRECTLY(schema_name, QR_get_value_backend_text(res, 0, 1));
			SET_NAME_DIRECTLY(table_name, QR_get_value_backend_text(res, 0, 0));
			ret = quote_table(schema_name, table_name, buf, buf_size);
			TI_Ins_IH(ti, tableoid, ret);
		}
		QR_Destructor(res);
		return ret;
	}
}

static BOOL	tupleExists(StatementClass *stmt, const KeySet *keyset)
{
	char	selstr[256];
	const TABLE_INFO	*ti = stmt->ti[0];
	QResultClass	*res;
	RETCODE		ret = FALSE;
	const char *bestqual = GET_NAME(ti->bestqual);
	char table_fqn[256];

	SPRINTF_FIXED(selstr,
			 "select 1 from %s where ctid = '(%u,%u)'",
			 ti_quote(stmt, keyset->oid, table_fqn, sizeof(table_fqn)),
			 keyset->blocknum, keyset->offset);
	if (NULL != bestqual && 0 != keyset->oid && !TI_has_subclass(ti))
	{
		SPRINTFCAT_FIXED(selstr, " and ");
		SPRINTFCAT_FIXED(selstr, bestqual, keyset->oid);
	}
	res = CC_send_query(SC_get_conn(stmt), selstr, NULL, READ_ONLY_QUERY, NULL);
	if (QR_command_maybe_successful(res) && 1 == res->num_cached_rows)
		ret = TRUE;
	QR_Destructor(res);
	return ret;
}

static BOOL enlargeAdded(QResultClass *res, UInt4 number, const StatementClass *stmt)
{
	UInt4	alloc;
	int	num_fields = res->num_fields;

	alloc = res->ad_alloc;
	if (0 == alloc)
		alloc = number > 10 ? number : 10;
	else
		while (alloc < number)
		{
			alloc *= 2;
		}

	if (alloc <= res->ad_alloc)
		return TRUE;
	QR_REALLOC_return_with_error(res->added_keyset, KeySet, sizeof(KeySet) * alloc, res, "enlargeAdded failed", FALSE);
	if (SQL_CURSOR_KEYSET_DRIVEN != stmt->options.cursor_type)
		QR_REALLOC_return_with_error(res->added_tuples, TupleField, sizeof(TupleField) * num_fields * alloc, res, "enlargeAdded failed 2", FALSE);
	res->ad_alloc = alloc;
	return TRUE;
}
static void AddAdded(StatementClass *stmt, QResultClass *res, SQLLEN index, const TupleField *tuple_added)
{
	KeySet	*added_keyset, *keyset, keys;
	TupleField	*added_tuples = NULL, *tuple;
	UInt4	ad_count;
	Int2	num_fields;

	if (!res)	return;
	num_fields = res->num_fields;
inolog("AddAdded index=%d, tuple=%p, num_fields=%d\n", index, tuple_added, num_fields);
	ad_count = res->ad_count;
	res->ad_count++;
	if (QR_get_cursor(res))
		index = -(SQLLEN)res->ad_count;
	if (!tuple_added)
		return;
	KeySetSet(tuple_added, num_fields + res->num_key_fields, res->num_key_fields, &keys, TRUE);
	keys.status = SQL_ROW_ADDED;
	if (CC_is_in_trans(SC_get_conn(stmt)))
		keys.status |= CURS_SELF_ADDING;
	else
		keys.status |= CURS_SELF_ADDED;
	AddRollback(stmt, res, index, &keys, SQL_ADD);

	if (!QR_get_cursor(res))
		return;
	if (ad_count > 0 && 0 == res->ad_alloc)
		return;
	if (!enlargeAdded(res, ad_count + 1, stmt))
		return;
	added_keyset = res->added_keyset;
	added_tuples = res->added_tuples;

	keyset = added_keyset + ad_count;
	*keyset = keys;
	if (added_tuples)
	{
		tuple = added_tuples + num_fields * ad_count;
		memset(tuple, 0, sizeof(TupleField) * num_fields);
		ReplaceCachedRows(tuple, tuple_added, num_fields, 1);
	}
}

static	void RemoveAdded(QResultClass *, SQLLEN);
static	void RemoveUpdated(QResultClass *, SQLLEN);
static	void RemoveUpdatedAfterTheKey(QResultClass *, SQLLEN, const KeySet*);
static	void RemoveDeleted(QResultClass *, SQLLEN);

static	void RemoveAdded(QResultClass *res, SQLLEN index)
{
	SQLLEN	rmidx, mv_count;
	Int2	num_fields = res->num_fields;
	KeySet	*added_keyset;
	TupleField	*added_tuples;

	mylog("RemoveAdded index=%d\n", index);
	if (index < 0)
		rmidx = -index - 1;
	else
		rmidx = index - res->num_total_read;
	if (rmidx >= res->ad_count)
		return;
	added_keyset = res->added_keyset + rmidx;
	added_tuples = res->added_tuples + num_fields * rmidx;
	ClearCachedRows(added_tuples, num_fields, 1);
	mv_count = res->ad_count - rmidx - 1;
	if (mv_count > 0)
	{
		memmove(added_keyset, added_keyset + 1, mv_count * sizeof(KeySet));
		memmove(added_tuples, added_tuples + num_fields, mv_count * num_fields * sizeof(TupleField));
	}
	RemoveDeleted(res, index);
	RemoveUpdated(res, index);
	res->ad_count--;
	mylog("RemoveAdded removed=1 count=%d\n", res->ad_count);
}

static void
CommitAdded(QResultClass *res)
{
	KeySet	*added_keyset;
	int	i;
	UWORD	status;

	mylog("CommitAdded res=%p\n", res);
	if (!res || !res->added_keyset)	return;
	added_keyset = res->added_keyset;
	for (i = res->ad_count - 1; i >= 0; i--)
	{
		status = added_keyset[i].status;
		if (0 != (status & CURS_SELF_ADDING))
		{
			status |= CURS_SELF_ADDED;
			status &= ~CURS_SELF_ADDING;
		}
		if (0 != (status & CURS_SELF_UPDATING))
		{
			status |= CURS_SELF_UPDATED;
			status &= ~CURS_SELF_UPDATING;
		}
		if (0 != (status & CURS_SELF_DELETING))
		{
			status |= CURS_SELF_DELETED;
			status &= ~CURS_SELF_DELETING;
		}
		if (status != added_keyset[i].status)
		{
inolog("!!Commit Added=%d(%d)\n", QR_get_num_total_read(res) + i, i);
			added_keyset[i].status = status;
		}
	}
}

static int
AddDeleted(QResultClass *res, SQLULEN index, const KeySet *keyset)
{
	int	i;
	Int2	dl_count, new_alloc;
	SQLLEN	*deleted;
	KeySet	*deleted_keyset;
	UWORD	status;
	Int2	num_fields = res->num_fields;

inolog("AddDeleted %d\n", index);
	dl_count = res->dl_count;
	res->dl_count++;
	if (!QR_get_cursor(res))
		return TRUE;
	if (!res->deleted)
	{
		dl_count = 0;
		new_alloc = 10;
		QR_MALLOC_return_with_error(res->deleted, SQLLEN, sizeof(SQLLEN) * new_alloc, res, "Deleted index malloc error", FALSE);
		QR_MALLOC_return_with_error(res->deleted_keyset, KeySet, sizeof(KeySet) * new_alloc, res, "Deleted keyset malloc error", FALSE);
		deleted = res->deleted;
		deleted_keyset = res->deleted_keyset;
		res->dl_alloc = new_alloc;
	}
	else
	{
		if (dl_count >= res->dl_alloc)
		{
			new_alloc = res->dl_alloc * 2;
			res->dl_alloc = 0;
			QR_REALLOC_return_with_error(res->deleted, SQLLEN, sizeof(SQLLEN) * new_alloc, res, "Deleted index realloc error", FALSE);
			deleted = res->deleted;
			QR_REALLOC_return_with_error(res->deleted_keyset, KeySet, sizeof(KeySet) * new_alloc, res, "Deleted KeySet realloc error", FALSE);
			deleted_keyset = res->deleted_keyset;
			res->dl_alloc = new_alloc;
		}
		/* sort deleted indexes in ascending order */
		for (i = 0, deleted = res->deleted, deleted_keyset = res->deleted_keyset; i < dl_count; i++, deleted++, deleted_keyset += num_fields)
		{
			if (index < *deleted)
				break;
		}
		memmove(deleted + 1, deleted, sizeof(SQLLEN) * (dl_count - i));
		memmove(deleted_keyset + 1, deleted_keyset, sizeof(KeySet) * (dl_count - i));
	}
	*deleted = index;
	*deleted_keyset = *keyset;
	status = keyset->status;
	status &= (~KEYSET_INFO_PUBLIC);
	status |= SQL_ROW_DELETED;
	if (CC_is_in_trans(QR_get_conn(res)))
	{
		status |= CURS_SELF_DELETING;
		QR_get_conn(res)->result_uncommitted = 1;
	}
	else
	{
		status &= ~(CURS_SELF_ADDING | CURS_SELF_UPDATING | CURS_SELF_DELETING);
		status |= CURS_SELF_DELETED;
	}
	deleted_keyset->status = status;
	res->dl_count = dl_count + 1;

	return TRUE;
}

static void
RemoveDeleted(QResultClass *res, SQLLEN index)
{
	int	i, mv_count, rm_count = 0;
	SQLLEN	pidx, midx;
	SQLLEN	*deleted, num_read = QR_get_num_total_read(res);
	KeySet	*deleted_keyset;

	mylog("RemoveDeleted index=%d\n", index);
	if (index < 0)
	{
		midx = index;
		pidx = num_read - index - 1;
	}
	else
	{
		pidx = index;
		if (index >= num_read)
			midx = num_read - index - 1;
		else
			midx = index;
	}
	for (i = 0; i < res->dl_count; i++)
	{
		if (pidx == res->deleted[i] ||
		    midx == res->deleted[i])
		{
			mv_count = res->dl_count - i - 1;
			if (mv_count > 0)
			{
				deleted = res->deleted + i;
				deleted_keyset = res->deleted_keyset + i;
				memmove(deleted, deleted + 1, mv_count * sizeof(SQLLEN));
				memmove(deleted_keyset, deleted_keyset + 1, mv_count * sizeof(KeySet));
			}
			res->dl_count--;
			rm_count++;
		}
	}
	mylog("RemoveDeleted removed count=%d,%d\n", rm_count, res->dl_count);
}

static void
CommitDeleted(QResultClass *res)
{
	int	i;
	SQLLEN	*deleted;
	KeySet	*deleted_keyset;
	UWORD	status;

	if (!res->deleted)
		return;

	for (i = 0, deleted = res->deleted, deleted_keyset = res->deleted_keyset; i < res->dl_count; i++, deleted++, deleted_keyset++)
	{
		status = deleted_keyset->status;
		if (0 != (status & CURS_SELF_ADDING))
		{
			status |= CURS_SELF_ADDED;
			status &= ~CURS_SELF_ADDING;
		}
		if (0 != (status & CURS_SELF_UPDATING))
		{
			status |= CURS_SELF_UPDATED;
			status &= ~CURS_SELF_UPDATING;
		}
		if (0 != (status & CURS_SELF_DELETING))
		{
			status |= CURS_SELF_DELETED;
			status &= ~CURS_SELF_DELETING;
		}
		if (status != deleted_keyset->status)
		{
inolog("!!Commit Deleted=%d(%d)\n", *deleted, i);
			deleted_keyset->status = status;
		}
	}
}

static BOOL
enlargeUpdated(QResultClass *res, Int4 number, const StatementClass *stmt)
{
	Int2	alloc;

	alloc = res->up_alloc;
	if (0 == alloc)
		alloc = number > 10 ? number : 10;
	else
		while (alloc < number)
		{
			alloc *= 2;
		}
	if (alloc <= res->up_alloc)
		return TRUE;

	QR_REALLOC_return_with_error(res->updated, SQLLEN, sizeof(SQLLEN) * alloc, res, "enlargeUpdated failed", FALSE);
	QR_REALLOC_return_with_error(res->updated_keyset, KeySet, sizeof(KeySet) * alloc, res, "enlargeUpdated failed 2", FALSE);
	if (SQL_CURSOR_KEYSET_DRIVEN != stmt->options.cursor_type)
		QR_REALLOC_return_with_error(res->updated_tuples, TupleField, sizeof(TupleField) * res->num_fields * alloc, res, "enlargeUpdated failed 3", FALSE);
	res->up_alloc = alloc;

	return TRUE;
}

static void
AddUpdated(StatementClass *stmt, SQLLEN index, const KeySet *keyset, const TupleField *tuple_updated)
{
	QResultClass	*res;
	SQLLEN	*updated;
	KeySet	*updated_keyset;
	TupleField	*updated_tuples = NULL,  *tuple;
	/* SQLLEN	res_ridx; */
	UInt2	up_count;
	BOOL	is_in_trans;
	SQLLEN	upd_idx, upd_add_idx;
	Int2	num_fields;
	int	i;
	UWORD	status;

inolog("AddUpdated index=%d\n", index);
	if (!stmt)	return;
	if (res = SC_get_Curres(stmt), !res)	return;
	if (!keyset)	return;
	if (!QR_get_cursor(res))	return;
	up_count = res->up_count;
	if (up_count > 0 && 0 == res->up_alloc)	return;
	num_fields = res->num_fields;
	/*
	res_ridx = GIdx2CacheIdx(index, stmt, res);
	if (res_ridx >= 0 && res_ridx < QR_get_num_cached_tuples(res))
		tuple_updated = res->backend_tuples + res_ridx * num_fields;
	*/
	if (!tuple_updated)
		return;
	upd_idx = -1;
	upd_add_idx = -1;
	updated = res->updated;
	is_in_trans = CC_is_in_trans(SC_get_conn(stmt));
	updated_keyset = res->updated_keyset;
	status = keyset->status;
	status &= (~KEYSET_INFO_PUBLIC);
	status |= SQL_ROW_UPDATED;
	if (is_in_trans)
		status |= CURS_SELF_UPDATING;
	else
	{
		for (i = up_count - 1; i >= 0; i--)
		{
			if (updated[i] == index)
				break;
		}
		if (i >= 0)
			upd_idx = i;
		else
		{
			SQLLEN	num_totals = QR_get_num_total_tuples(res);
			if (index >= num_totals)
				upd_add_idx = num_totals - index;
		}
		status |= CURS_SELF_UPDATED;
		status &= ~(CURS_SELF_ADDING | CURS_SELF_UPDATING | CURS_SELF_DELETING);
	}

	tuple = NULL;
	/* update the corresponding add(updat)ed info */
	if (upd_add_idx >= 0)
	{
		res->added_keyset[upd_add_idx].status = status;
		if (res->added_tuples)
		{
			tuple = res->added_tuples + num_fields * upd_add_idx;
			ClearCachedRows(tuple, num_fields, 1);
		}
	}
	else if (upd_idx >= 0)
	{
		res->updated_keyset[upd_idx].status = status;
		if (res->updated_tuples)
		{
			tuple = res->added_tuples + num_fields * upd_add_idx;
			ClearCachedRows(tuple, num_fields, 1);
		}
	}
	else
	{
		if (!enlargeUpdated(res, res->up_count + 1, stmt))
			return;
		updated = res->updated;
		updated_keyset = res->updated_keyset;
		updated_tuples = res->updated_tuples;
		upd_idx = up_count;
		updated[up_count] = index;
		updated_keyset[up_count] = *keyset;
		updated_keyset[up_count].status = status;
		if (updated_tuples)
		{
			tuple = updated_tuples + num_fields * up_count;
			memset(tuple, 0, sizeof(TupleField) * num_fields);
		}
		res->up_count++;
	}

	if (tuple)
		ReplaceCachedRows(tuple, tuple_updated, num_fields, 1);
	if (is_in_trans)
		SC_get_conn(stmt)->result_uncommitted = 1;
	mylog("up_count=%d\n", res->up_count);
}

static void
RemoveUpdated(QResultClass *res, SQLLEN index)
{
	mylog("RemoveUpdated index=%d\n", index);
	RemoveUpdatedAfterTheKey(res, index, NULL);
}

static void
RemoveUpdatedAfterTheKey(QResultClass *res, SQLLEN index, const KeySet *keyset)
{
	SQLLEN	*updated, num_read = QR_get_num_total_read(res);
	KeySet	*updated_keyset;
	TupleField	*updated_tuples = NULL;
	SQLLEN	pidx, midx, mv_count;
	int	i, num_fields = res->num_fields, rm_count = 0;

	mylog("RemoveUpdatedAfterTheKey %d,(%u,%u)\n", index, keyset ? keyset->blocknum : 0, keyset ? keyset->offset : 0);
	if (index < 0)
	{
		midx = index;
		pidx = num_read - index - 1;
	}
	else
	{
		pidx = index;
		if (index >= num_read)
			midx = num_read - index - 1;
		else
			midx = index;
	}
	for (i = 0; i < res->up_count; i++)
	{
		updated = res->updated + i;
		if (pidx == *updated ||
		    midx == *updated)
		{
			updated_keyset = res->updated_keyset + i;
			if (keyset &&
			    updated_keyset->blocknum == keyset->blocknum &&
			    updated_keyset->offset == keyset->offset)
				break;
			updated_tuples = NULL;
			if (res->updated_tuples)
			{
				updated_tuples = res->updated_tuples + i * num_fields;
				ClearCachedRows(updated_tuples, num_fields, 1);
			}
			mv_count = res->up_count - i -1;
			if (mv_count > 0)
			{
				memmove(updated, updated + 1, sizeof(SQLLEN) * mv_count);
				memmove(updated_keyset, updated_keyset + 1, sizeof(KeySet) * mv_count);
				if (updated_tuples)
					memmove(updated_tuples, updated_tuples + num_fields, sizeof(TupleField) * num_fields * mv_count);
			}
			res->up_count--;
			rm_count++;
		}
	}
	mylog("RemoveUpdatedAfter removed count=%d,%d\n", rm_count, res->up_count);
}

static void
CommitUpdated(QResultClass *res)
{
	KeySet	*updated_keyset;
	int	i;
	UWORD	status;

	mylog("CommitUpdated res=%p\n", res);
	if (!res)	return;
	if (!QR_get_cursor(res))
		return;
	if (res->up_count <= 0)
		return;
	if (updated_keyset = res->updated_keyset, !updated_keyset)
		return;
	for (i = res->up_count - 1; i >= 0; i--)
	{
		status = updated_keyset[i].status;
		if (0 != (status & CURS_SELF_UPDATING))
		{
			status &= ~CURS_SELF_UPDATING;
			status |= CURS_SELF_UPDATED;
		}
		if (0 != (status & CURS_SELF_ADDING))
		{
			status &= ~CURS_SELF_ADDING;
			status |= CURS_SELF_ADDED;
		}
		if (0 != (status & CURS_SELF_DELETING))
		{
			status &= ~CURS_SELF_DELETING;
			status |= CURS_SELF_DELETED;
		}
		if (status != updated_keyset[i].status)
		{
inolog("!!Commit Updated=%d(%d)\n", res->updated[i], i);
			updated_keyset[i].status = status;
		}
	}
}


static void
DiscardRollback(StatementClass *stmt, QResultClass *res)
{
	int	i;
	SQLLEN	index, kres_ridx;
	UWORD	status;
	Rollback *rollback;
	KeySet	*keyset;
	BOOL	kres_is_valid;

inolog("DiscardRollback");
	if (QR_get_cursor(res))
	{
		CommitAdded(res);
		CommitUpdated(res);
		CommitDeleted(res);
		return;
	}

	if (0 == res->rb_count || NULL == res->rollback)
		return;
	rollback = res->rollback;
	keyset = res->keyset;
	for (i = 0; i < res->rb_count; i++)
	{
		index = rollback[i].index;
		status = 0;
		kres_is_valid = FALSE;
		if (index >= 0)
		{
			kres_ridx = GIdx2KResIdx(index, stmt, res);
			if (kres_ridx >= 0 && kres_ridx < res->num_cached_keys)
			{
				kres_is_valid = TRUE;
				status = keyset[kres_ridx].status;
			}
		}
		if (kres_is_valid)
		{
			keyset[kres_ridx].status &= ~(CURS_SELF_DELETING | CURS_SELF_UPDATING | CURS_SELF_ADDING);
			keyset[kres_ridx].status |= ((status & (CURS_SELF_DELETING | CURS_SELF_UPDATING | CURS_SELF_ADDING)) << 3);
		}
	}
	free(rollback);
	res->rollback = NULL;
	res->rb_count = res->rb_alloc = 0;
}

static QResultClass *positioned_load(StatementClass *stmt, UInt4 flag, const UInt4 *oidint, const char *tid);

static void
UndoRollback(StatementClass *stmt, QResultClass *res, BOOL partial)
{
	Int4	i, rollbp;
	SQLLEN	index, ridx, kres_ridx;
	UWORD	status;
	Rollback *rollback;
	KeySet	*keyset, keys, *wkey = NULL;
	BOOL	curs = (NULL != QR_get_cursor(res)), texist, kres_is_valid;

	if (0 == res->rb_count || NULL == res->rollback)
		return;
	rollback = res->rollback;
	keyset = res->keyset;

	rollbp = 0;
	if (partial)
	{
		SQLLEN	pidx, midx;
		int		rollbps;
		int		doubtp;
		int	j;

		for (i = 0, doubtp = 0; i < res->rb_count; i++)
		{
			keys.status = 0;
			keys.blocknum = rollback[i].blocknum;
			keys.offset = rollback[i].offset;
			keys.oid = rollback[i].oid;
			texist = tupleExists(stmt, &keys);
inolog("texist[%d]=%d", i, texist);
			if (SQL_ADD == rollback[i].option)
			{
				if (texist)
					doubtp = i + 1;
			}
			else if (SQL_REFRESH == rollback[i].option)
			{
				if (texist || doubtp == i)
					doubtp = i + 1;
			}
			else
			{
				if (texist)
					break;
				if (doubtp == i)
					doubtp = i + 1;
			}
inolog(" doubtp=%d\n", doubtp);
		}
		rollbp = i;
inolog(" doubtp=%d,rollbp=%d\n", doubtp, rollbp);
		do
		{
			rollbps = rollbp;
			for (i = doubtp; i < rollbp; i++)
			{
				index = rollback[i].index;
				if (SQL_ADD == rollback[i].option)
				{
inolog("index[%d]=%d\n", i, index);
					if (index < 0)
					{
						midx = index;
						pidx = res->num_total_read - index - 1;
					}
					else
					{
						pidx = index;
						midx = res->num_total_read - index - 1;
					}
inolog("pidx=%d,midx=%d\n", pidx, midx);
					for (j = rollbp - 1; j > i; j--)
					{
						if (rollback[j].index == midx ||
						    rollback[j].index == pidx)
						{
							if (SQL_DELETE == rollback[j].option)
							{
inolog("delete[%d].index=%d\n", j, rollback[j].index);
								break;
							}
							/*else if (SQL_UPDATE == rollback[j].option)
							{
inolog("update[%d].index=%d\n", j, rollback[j].index);
								if (IndexExists(stmt, res, rollback + j))
									break;
							}*/
						}
					}
					if (j <= i)
					{
						rollbp = i;
						break;
					}
				}
			}
		} while (rollbp < rollbps);
	}
inolog("rollbp=%d\n", rollbp);

	for (i = res->rb_count - 1; i >= rollbp; i--)
	{
inolog("UndoRollback %d(%d)\n", i, rollback[i].option);
		index = rollback[i].index;
		if (curs)
		{
			if (SQL_ADD == rollback[i].option)
				RemoveAdded(res, index);
			RemoveDeleted(res, index);
			keys.status = 0;
			keys.blocknum = rollback[i].blocknum;
			keys.offset = rollback[i].offset;
			keys.oid = rollback[i].oid;
			/* RemoveUpdatedAfterTheKey(res, index, &keys); is no longer needed? */
			RemoveUpdated(res, index);
		}
		status = 0;
		kres_is_valid = FALSE;
		if (index >= 0)
		{
			kres_ridx = GIdx2KResIdx(index, stmt, res);
			if (kres_ridx >= 0 && kres_ridx < res->num_cached_keys)
			{
				kres_is_valid = TRUE;
				wkey = keyset + kres_ridx;
				status = wkey->status;
			}
		}
inolog(" index=%d status=%hx", index, status);
		if (kres_is_valid)
		{
			QResultClass	*qres;
			Int2		num_fields = res->num_fields;

			ridx = GIdx2CacheIdx(index, stmt, res);
			if (SQL_ADD == rollback[i].option)
			{
				if (ridx >=0 && ridx < res->num_cached_rows)
				{
					TupleField *tuple = res->backend_tuples + res->num_fields * ridx;
					ClearCachedRows(tuple, res->num_fields, 1);
					res->num_cached_rows--;
				}
				res->num_cached_keys--;
				if (!curs)
					res->ad_count--;
			}
			else if (SQL_REFRESH == rollback[i].option)
				continue;
			else
			{
inolog(" (%u, %u)", wkey->blocknum,  wkey->offset);
				wkey->blocknum = rollback[i].blocknum;
				wkey->offset = rollback[i].offset;
				wkey->oid = rollback[i].oid;
inolog("->(%u, %u)\n", wkey->blocknum, wkey->offset);
				wkey->status &= ~KEYSET_INFO_PUBLIC;
				if (SQL_DELETE == rollback[i].option)
					wkey->status &= ~CURS_SELF_DELETING;
				else if (SQL_UPDATE == rollback[i].option)
					wkey->status &= ~CURS_SELF_UPDATING;
				wkey->status |= CURS_NEEDS_REREAD;
				if (ridx >=0 && ridx < res->num_cached_rows)
				{
					char	tidval[32];
					Oid	oid = wkey->oid;

					SPRINTF_FIXED(tidval,
							 "(%u,%u)", wkey->blocknum, wkey->offset);
					qres = positioned_load(stmt, 0, &oid, tidval);
					if (QR_command_maybe_successful(qres) &&
					    QR_get_num_cached_tuples(qres) == 1)
					{
						MoveCachedRows(res->backend_tuples + num_fields * ridx, qres->backend_tuples, num_fields, 1);
						wkey->status &= ~CURS_NEEDS_REREAD;
					}
					QR_Destructor(qres);
				}
			}
		}
	}
	res->rb_count = rollbp;
	if (0 == rollbp)
	{
		free(rollback);
		res->rollback = NULL;
		res->rb_alloc = 0;
	}
}

void
ProcessRollback(ConnectionClass *conn, BOOL undo, BOOL partial)
{
	int	i;
	StatementClass	*stmt;
	QResultClass	*res;

	for (i = 0; i < conn->num_stmts; i++)
	{
		if (stmt = conn->stmts[i], !stmt)
			continue;
		for (res = SC_get_Result(stmt); res; res = res->next)
		{
			if (undo)
				UndoRollback(stmt, res, partial);
			else
				DiscardRollback(stmt, res);
		}
	}
}


#define	LATEST_TUPLE_LOAD	1L
#define	USE_INSERTED_TID	(1L << 1)
static QResultClass *
positioned_load(StatementClass *stmt, UInt4 flag, const UInt4 *oidint, const char *tidval)
{
	CSTR	func = "positioned_load";
	CSTR	andqual = " and ";
	QResultClass *qres = NULL;
	char	*selstr, oideqstr[256];
	BOOL	latest = ((flag & LATEST_TUPLE_LOAD) != 0);
	size_t	len;
	TABLE_INFO	*ti = stmt->ti[0];
	const char *bestqual = GET_NAME(ti->bestqual);
	const ssize_t	from_pos = stmt->load_from_pos;
	const char *load_stmt = stmt->load_statement;

inolog("%s bestitem=%s bestqual=%s\n", func, SAFE_NAME(ti->bestitem), SAFE_NAME(ti->bestqual));
	if (!bestqual || !oidint)
		*oideqstr = '\0';
	else
	{
		STRCPY_FIXED(oideqstr, andqual);
		SPRINTFCAT_FIXED(oideqstr, bestqual, *oidint);
	}
	len = strlen(load_stmt);
	len += strlen(oideqstr);
	if (tidval)
		len += 100;
	else if ((flag & USE_INSERTED_TID) != 0)
		len += 50;
	else
		len += 20;
	selstr = malloc(len);
	if (!selstr)
	{
		SC_set_error(stmt,STMT_NO_MEMORY_ERROR, "Could not allocate memory for query", func);
		goto cleanup;
	}
	if (TI_has_subclass(ti))
	{
		OID	tableoid = *oidint;
		char	table_fqn[256];
		const char	*quoted_table;

		quoted_table = ti_quote(stmt, tableoid, table_fqn, sizeof(table_fqn));
		if (tidval)
		{
			if (latest)
			{
				snprintf(selstr, len,
					 "%.*sfrom %s where ctid = currtid2('%s', '%s')",
					 (int) from_pos, load_stmt,
					 quoted_table,
					 quoted_table,
					 tidval);
			}
			else
				snprintf(selstr, len, "%.*sfrom %s where ctid = '%s'", (int) from_pos, load_stmt, quoted_table, tidval);
		}
		else if ((flag & USE_INSERTED_TID) != 0)
			snprintf(selstr, len, "%.*sfrom %s where ctid = currtid(0, '(0,0)')", (int) from_pos, load_stmt, quoted_table);
		/*
		else if (bestitem && oidint)
		{
		}
		*/
		else
		{
			SC_set_error(stmt,STMT_INTERNAL_ERROR, "can't find the add and updating row because of the lack of oid", func);
			goto cleanup;
		}
	}
	else
	{
		if (tidval)
		{
			if (latest)
			{
				char table_fqn[256];

				snprintf(selstr, len,
					 "%s where ctid = currtid2('%s', '%s') %s",
					 load_stmt,
					 ti_quote(stmt, 0, table_fqn, sizeof(table_fqn)),
					 tidval, oideqstr);
			}
			else
				snprintf(selstr, len, "%s where ctid = '%s' %s", load_stmt, tidval, oideqstr);
		}
		else if ((flag & USE_INSERTED_TID) != 0)
			snprintf(selstr, len, "%s where ctid = currtid(0, '(0,0)') %s", load_stmt, oideqstr);
		else if (bestqual && oidint)
		{
			snprintf(selstr, len, "%s where ", load_stmt);
			snprintfcat(selstr, len, bestqual, *oidint);
		}
		else
		{
			SC_set_error(stmt,STMT_INTERNAL_ERROR, "can't find the add and updating row because of the lack of oid", func);
			goto cleanup;
		}
	}

	mylog("selstr=%s\n", selstr);
	qres = CC_send_query(SC_get_conn(stmt), selstr, NULL, READ_ONLY_QUERY, stmt);
cleanup:
	free(selstr);
	return qres;
}

BOOL QR_get_last_bookmark(const QResultClass *res, Int4 index, KeySet *keyset)
{
	int	i;

	if (res->dl_count > 0 && res->deleted)
	{
		for (i = 0; i < res->dl_count; i++)
		{
			if (res->deleted[i] == index)
			{
				*keyset = res->deleted_keyset[i];
				return TRUE;
			}
			if (res->deleted[i] > index)
				break;
		}
	}
	if (res->up_count > 0 && res->updated)
	{
		for (i = res->up_count - 1; i >= 0; i--)
		{
			if (res->updated[i] == index)
			{
				*keyset = res->updated_keyset[i];
				return TRUE;
			}
		}
	}
	return FALSE;
}

static RETCODE
SC_pos_reload_with_key(StatementClass *stmt, SQLULEN global_ridx, UInt2 *count, Int4 logKind, const KeySet *keyset)
{
	CSTR		func = "SC_pos_reload_with_key";
	int		res_cols;
	UInt2		rcnt;
	SQLLEN		kres_ridx;
	OID		oidint;
	QResultClass	*res, *qres;
	IRDFields	*irdflds = SC_get_IRDF(stmt);
	RETCODE		ret = SQL_ERROR;
	char		tidval[32];
	BOOL		use_ctid = TRUE;
	BOOL		idx_exist = TRUE;

	mylog("positioned load fi=%p ti=%p\n", irdflds->fi, stmt->ti);
	rcnt = 0;
	if (count)
		*count = 0;
	if (!(res = SC_get_Curres(stmt)))
	{
		SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in SC_pos_reload.", func);
		return SQL_ERROR;
	}

	kres_ridx = GIdx2KResIdx(global_ridx, stmt, res);
	if (kres_ridx < 0 || kres_ridx >= res->num_cached_keys)
	{
		if (NULL == keyset || keyset->offset == 0)
		{
			SC_set_error(stmt, STMT_ROW_OUT_OF_RANGE, "the target keys are out of the rowset", func);
			return SQL_ERROR;
		}
		idx_exist = FALSE;
	}
	else if (0 != (res->keyset[kres_ridx].status & CURS_SELF_ADDING))
	{
		if (NULL == keyset || keyset->offset == 0)
		{
			use_ctid = FALSE;
			mylog("The tuple is currently being added and can't use ctid\n");
		}
	}

	if (SC_update_not_ready(stmt))
		parse_statement(stmt, TRUE);	/* not preferable */
	if (!SC_is_updatable(stmt))
	{
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
		SC_set_error(stmt, STMT_INVALID_OPTION_IDENTIFIER, "the statement is read-only", func);
		return SQL_ERROR;
	}
	/* old oid & tid */
	if (idx_exist)
	{
		if (!(oidint = getOid(res, kres_ridx)))
		{
			if (!strcmp(SAFE_NAME(stmt->ti[0]->bestitem), OID_NAME))
			{
				SC_set_error(stmt, STMT_ROW_VERSION_CHANGED, "the row was already deleted ?", func);
				return SQL_SUCCESS_WITH_INFO;
			}
		}
	}
	res_cols = getNumResultCols(res);
	if (keyset) /* after or update */
	{
		char tid[32];

		oidint = keyset->oid;
		SPRINTF_FIXED(tid, "(%u,%hu)", keyset->blocknum, keyset->offset);
		qres = positioned_load(stmt, 0, &oidint, tid);
	}
	else
	{
		if (use_ctid)
			qres = positioned_load(stmt, LATEST_TUPLE_LOAD, &oidint, tidval);
		else
			qres = positioned_load(stmt, 0, &oidint, NULL);
		keyset = res->keyset + kres_ridx;
	}
	if (!QR_command_maybe_successful(qres))
	{
		ret = SQL_ERROR;
		SC_replace_error_with_res(stmt, STMT_ERROR_TAKEN_FROM_BACKEND, "positioned_load failed", qres, TRUE);
	}
	else if (rcnt = (UInt2) QR_get_num_cached_tuples(qres), rcnt == 1)
	{
		SQLLEN		res_ridx;

		switch (logKind)
		{
			case 0:
			case SQL_FETCH_BY_BOOKMARK:
				break;
			case SQL_UPDATE:
				AddUpdated(stmt, global_ridx, keyset, qres->tupleField);
				break;
			default:
				AddRollback(stmt, res, global_ridx, keyset, logKind);
		}
		res_ridx = GIdx2CacheIdx(global_ridx, stmt, res);
		if (res_ridx >= 0 && res_ridx < QR_get_num_cached_tuples(res))
		{
			TupleField *tuple_old, *tuple_new;
			int	effective_fields = res_cols;

			tuple_old = res->backend_tuples + res->num_fields * res_ridx;

			QR_set_position(qres, 0);
			tuple_new = qres->tupleField;
			if (SQL_CURSOR_KEYSET_DRIVEN == stmt->options.cursor_type &&
				strcmp(tuple_new[qres->num_fields - res->num_key_fields].value, tidval))
				res->keyset[kres_ridx].status |= SQL_ROW_UPDATED;
			KeySetSet(tuple_new, qres->num_fields, res->num_key_fields, res->keyset + kres_ridx, FALSE);
			MoveCachedRows(tuple_old, tuple_new, effective_fields, 1);
		}
		ret = SQL_SUCCESS;
	}
	else
	{
		SC_set_error(stmt, STMT_ROW_VERSION_CHANGED, "the content was deleted after last fetch", func);
		ret = SQL_SUCCESS_WITH_INFO;
		AddRollback(stmt, res, global_ridx, keyset, logKind);
		if (idx_exist)
		{
			if (stmt->options.cursor_type == SQL_CURSOR_KEYSET_DRIVEN)
			{
				res->keyset[kres_ridx].status |= SQL_ROW_DELETED;
			}
		}
	}
	QR_Destructor(qres);
	if (count)
		*count = rcnt;
	return ret;
}


RETCODE
SC_pos_reload(StatementClass *stmt, SQLULEN global_ridx, UInt2 *count, Int4 logKind)
{
	return SC_pos_reload_with_key(stmt, global_ridx, count, logKind, NULL);
}

static	const int	pre_fetch_count = 32;
static SQLLEN LoadFromKeyset(StatementClass *stmt, QResultClass * res, int rows_per_fetch, SQLLEN limitrow)
{
	CSTR	func = "LoadFromKeyset";
	ConnectionClass	*conn = SC_get_conn(stmt);
	SQLLEN	i;
	int	j, rowc, rcnt = 0;
	OID	oid;
	UInt4	blocknum;
	SQLLEN	kres_ridx;
	UInt2	offset;
	char	*qval = NULL;
	int	keys_per_fetch = 10;
	size_t	allen = 0;

	for (i = SC_get_rowset_start(stmt), kres_ridx = GIdx2KResIdx(i, stmt, res), rowc = 0;; i++, kres_ridx++)
	{
		if (i >= limitrow)
		{
			if (!rowc)
				break;
			if (res->reload_count > 0)
			{
				for (j = rowc; j < keys_per_fetch; j++)
				{
					strlcat(qval, j ? ",NULL" : "NULL", allen);
				}
			}
			rowc = -1; /* end of loop */
		}
		if (rowc < 0 || rowc >= keys_per_fetch)
		{
			QResultClass	*qres;

			strlcat(qval, ")", allen);
			qres = CC_send_query(conn, qval, NULL, CREATE_KEYSET | READ_ONLY_QUERY, stmt);
			if (QR_command_maybe_successful(qres))
			{
				SQLLEN		j, k, l;
				Int2		m;
				TupleField	*tuple, *tuplew;
				UInt4		bln;
				UInt2		off;

				for (j = 0; j < QR_get_num_total_read(qres); j++)
				{
					oid = getOid(qres, j);
					getTid(qres, j, &blocknum, &offset);
					for (k = SC_get_rowset_start(stmt); k < limitrow; k++)
					{
						getTid(res, k, &bln, &off);
						if (oid == getOid(res, k) &&
						    bln == blocknum &&
						    off == offset)
						{
							l = GIdx2CacheIdx(k, stmt, res);
							tuple = res->backend_tuples + res->num_fields * l;
							tuplew = qres->backend_tuples + qres->num_fields * j;
							for (m = 0; m < res->num_fields; m++, tuple++, tuplew++)
							{
								if (tuple->len > 0 && tuple->value)
									free(tuple->value);
								tuple->value = tuplew->value;
								tuple->len = tuplew->len;
								tuplew->value = NULL;
								tuplew->len = -1;
							}
							res->keyset[k].status &= ~CURS_NEEDS_REREAD;
							break;
						}
					}
				}
			}
			else
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Data Load Error", func);
				rcnt = -1;
				QR_Destructor(qres);
				break;
			}
			QR_Destructor(qres);
			if (rowc < 0)
				break;
			rowc = 0;
		}
		if (!rowc)
		{
			size_t lodlen = 0;

			if (!qval)
			{

				if (res->reload_count > 0)
					keys_per_fetch = res->reload_count;
				else
				{
					char	planname[32];
					int	j;
					QResultClass	*qres;

					if (rows_per_fetch >= pre_fetch_count * 2)
						keys_per_fetch = pre_fetch_count;
					else
						keys_per_fetch = rows_per_fetch;
					if (!keys_per_fetch)
						keys_per_fetch = 2;
					lodlen = strlen(stmt->load_statement);
					SPRINTF_FIXED(planname, "_KEYSET_%p", res);
					allen = 8 + strlen(planname) +
						3 + 4 * keys_per_fetch + 1
						+ 1 + 2 + lodlen + 20 +
						4 * keys_per_fetch + 1;
					SC_MALLOC_return_with_error(qval, char, allen,
						stmt, "Couldn't alloc qval", -1);
					snprintf(qval, allen, "PREPARE \"%s\"", planname);
					for (j = 0; j < keys_per_fetch; j++)
					{
						strlcat(qval, j ? ",tid" : "(tid", allen);
					}
					snprintfcat(qval, allen, ") as %s where ctid in ", stmt->load_statement);
					for (j = 0; j < keys_per_fetch; j++)
					{
						if (j == 0)
							strlcat(qval, "($1", allen);
						else
							snprintfcat(qval, allen, ",$%d", j + 1);
					}
					strlcat(qval, ")", allen);
					qres = CC_send_query(conn, qval, NULL, READ_ONLY_QUERY, stmt);
					if (QR_command_maybe_successful(qres))
					{
						res->reload_count = keys_per_fetch;
					}
					else
					{
						SC_set_error(stmt, STMT_EXEC_ERROR, "Prepare for Data Load Error", func);
						rcnt = -1;
						QR_Destructor(qres);
						break;
					}
					QR_Destructor(qres);
				}
				allen = 25 + 23 * keys_per_fetch;

				SC_REALLOC_return_with_error(qval, char, allen,
					stmt, "Couldn't alloc qval", -1);
			}
			if (res->reload_count > 0)
			{
				snprintf(qval, allen, "EXECUTE \"_KEYSET_%p\"(", res);
			}
			else
			{
				snprintf(qval, allen, "%s where ctid in (", stmt->load_statement);
			}
		}
		if (0 != (res->keyset[kres_ridx].status & CURS_NEEDS_REREAD))
		{
			getTid(res, kres_ridx, &blocknum, &offset);
			if (rowc)
				snprintfcat(qval, allen, ",'(%u,%u)'", blocknum, offset);
			else
				snprintfcat(qval, allen, "'(%u,%u)'", blocknum, offset);
			rowc++;
			rcnt++;
		}
	}
	if (qval)
		free(qval);
	return rcnt;
}

static SQLLEN LoadFromKeyset_inh(StatementClass *stmt, QResultClass * res, int rows_per_fetch, SQLLEN limitrow)
{
	ConnectionClass	*conn = SC_get_conn(stmt);
	SQLLEN	i;
	int	j, rowc, rcnt = 0;
	OID	oid, new_oid;
	UInt4	blocknum;
	SQLLEN	kres_ridx;
	UInt2	offset;
	char	*qval = NULL;
	int	keys_per_fetch = 10;
	const char *load_stmt = stmt->load_statement;
	const ssize_t	from_pos = stmt->load_from_pos;
	const int	max_identifier = 100;
	size_t	allen = 0;

mylog(" %s in rows_per_fetch=%d limitrow=%d\n", __FUNCTION__, rows_per_fetch, limitrow);
	new_oid = 0;
	for (i = SC_get_rowset_start(stmt), kres_ridx = GIdx2KResIdx(i, stmt, res), rowc = 0, oid = 0;; i++, kres_ridx++)
	{
		if (i >= limitrow)
		{
			if (!rowc)
				break;
			rowc = -1; /* end of loop */
		}
		else if (0 == (res->keyset[kres_ridx].status & CURS_NEEDS_REREAD))
			continue;
		else
			new_oid = getOid(res, kres_ridx);
		if (rowc < 0 ||
		    rowc >= keys_per_fetch ||
		    (oid != 0 &&
		     new_oid != oid))
		{
			QResultClass	*qres;

			strlcat(qval, ")", allen);
			qres = CC_send_query(conn, qval, NULL, CREATE_KEYSET | READ_ONLY_QUERY, stmt);
			if (QR_command_maybe_successful(qres))
			{
				SQLLEN		k, l;
				Int2		m;
				TupleField	*tuple, *tuplew;
				UInt4		bln;
				UInt2		off;
				OID		tbloid;

				for (j = 0; j < QR_get_num_total_read(qres); j++)
				{
					tbloid = getOid(qres, j);
					getTid(qres, j, &blocknum, &offset);
					for (k = SC_get_rowset_start(stmt); k < limitrow; k++)
					{
						getTid(res, k, &bln, &off);
						if (tbloid == getOid(res, k) &&
						    bln == blocknum &&
						    off == offset)
						{
							l = GIdx2CacheIdx(k, stmt, res);
							tuple = res->backend_tuples + res->num_fields * l;
							tuplew = qres->backend_tuples + qres->num_fields * j;
							for (m = 0; m < res->num_fields; m++, tuple++, tuplew++)
							{
								if (tuple->len > 0 && tuple->value)
									free(tuple->value);
								tuple->value = tuplew->value;
								tuple->len = tuplew->len;
								tuplew->value = NULL;
								tuplew->len = -1;
							}
							res->keyset[k].status &= ~CURS_NEEDS_REREAD;
							break;
						}
					}
				}
			}
			else
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Data Load Error", __FUNCTION__);
				rcnt = -1;
				QR_Destructor(qres);
				break;
			}
			QR_Destructor(qres);
			if (rowc < 0)
				break;
			rowc = 0;
		}
		if (!rowc)
		{
			char table_fqn[256];

			if (!qval)
			{
				if (rows_per_fetch >= pre_fetch_count * 2)
					keys_per_fetch = pre_fetch_count;
				else
					keys_per_fetch = rows_per_fetch;
				if (!keys_per_fetch)
					keys_per_fetch = 2;
				allen = from_pos + sizeof("from ") +
					2 * (2 + max_identifier) + 1 *
					+ sizeof(" where ctid in ") +
					20 * keys_per_fetch + 2;
				SC_MALLOC_return_with_error(qval, char, allen,
					stmt, "Couldn't alloc qval", -1);
			}
			snprintf(qval, allen, "%.*sfrom %s where ctid in (", (int) from_pos, load_stmt, ti_quote(stmt, new_oid, table_fqn, sizeof(table_fqn)));
		}
		if (new_oid != oid)
			oid = new_oid;
		getTid(res, kres_ridx, &blocknum, &offset);
		if (rowc)
			snprintfcat(qval, allen, ",'(%u,%u)'", blocknum, offset);
		else
			snprintfcat(qval, allen, "'(%u,%u)'", blocknum, offset);
		rowc++;
		rcnt++;
	}
	if (qval)
		free(qval);
	return rcnt;
}

static RETCODE	SQL_API
SC_pos_reload_needed(StatementClass *stmt, SQLULEN req_size, UDWORD flag)
{
	CSTR	func = "SC_pos_reload_needed";
	Int4		req_rows_size;
	SQLLEN		i, limitrow;
	UInt2		qcount;
	QResultClass	*res;
	RETCODE		ret = SQL_ERROR;
	SQLLEN		kres_ridx, rowc;
	Int4		rows_per_fetch;
	BOOL		create_from_scratch = (0 != flag);

	mylog("%s\n", func);
	if (!(res = SC_get_Curres(stmt)))
	{
		SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in SC_pos_reload_needed.", func);
		return SQL_ERROR;
	}
	if (SC_update_not_ready(stmt))
		parse_statement(stmt, TRUE);	/* not preferable */
	if (!SC_is_updatable(stmt))
	{
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
		SC_set_error(stmt, STMT_INVALID_OPTION_IDENTIFIER, "the statement is read-only", func);
		return SQL_ERROR;
	}
	rows_per_fetch = 0;
	req_rows_size = QR_get_reqsize(res);
	if (req_size > req_rows_size)
		req_rows_size = (UInt4) req_size;
	if (create_from_scratch)
	{
		rows_per_fetch = ((pre_fetch_count - 1) / req_rows_size + 1) * req_rows_size;
		limitrow = RowIdx2GIdx(rows_per_fetch, stmt);
	}
	else
	{
		limitrow = RowIdx2GIdx(req_rows_size, stmt);
	}
	if (limitrow > res->num_cached_keys)
		limitrow = res->num_cached_keys;
	if (create_from_scratch ||
	    !res->dataFilled)
	{
		ClearCachedRows(res->backend_tuples, res->num_fields, res->num_cached_rows);
		res->dataFilled = FALSE;
	}
	if (!res->dataFilled)
	{
		SQLLEN	brows = GIdx2RowIdx(limitrow, stmt);
		if (brows > res->count_backend_allocated)
		{
			QR_REALLOC_return_with_error(res->backend_tuples, TupleField, sizeof(TupleField) * res->num_fields * brows, res, "pos_reload_needed failed", SQL_ERROR);
			res->count_backend_allocated = brows;
		}
		if (brows > 0)
			memset(res->backend_tuples, 0, sizeof(TupleField) * res->num_fields * brows);
		QR_set_num_cached_rows(res, brows);
		QR_set_rowstart_in_cache(res, 0);
		if (SQL_RD_ON != stmt->options.retrieve_data)
			return SQL_SUCCESS;
		for (i = SC_get_rowset_start(stmt), kres_ridx = GIdx2KResIdx(i, stmt,res); i < limitrow; i++, kres_ridx++)
		{
			if (0 == (res->keyset[kres_ridx].status & (CURS_SELF_DELETING | CURS_SELF_DELETED | CURS_OTHER_DELETED)))
				res->keyset[kres_ridx].status |= CURS_NEEDS_REREAD;
		}
	}
	if (TI_has_subclass(stmt->ti[0]))
	{
		if (rowc = LoadFromKeyset_inh(stmt, res, rows_per_fetch, limitrow), rowc < 0)
		{
			return SQL_ERROR;
		}
	}
	else if (rowc = LoadFromKeyset(stmt, res, rows_per_fetch, limitrow), rowc < 0)
	{
		return SQL_ERROR;
	}
	for (i = SC_get_rowset_start(stmt), kres_ridx = GIdx2KResIdx(i, stmt, res); i < limitrow; i++)
	{
		if (0 != (res->keyset[kres_ridx].status & CURS_NEEDS_REREAD))
		{
			ret = SC_pos_reload(stmt, i, &qcount, 0);
			if (SQL_ERROR == ret)
			{
				break;
			}
			if (SQL_ROW_DELETED == (res->keyset[kres_ridx].status & KEYSET_INFO_PUBLIC))
			{
				res->keyset[kres_ridx].status |= CURS_OTHER_DELETED;
			}
			res->keyset[kres_ridx].status &= ~CURS_NEEDS_REREAD;
		}
	}
	res->dataFilled = TRUE;
	return ret;
}

static RETCODE	SQL_API
SC_pos_newload(StatementClass *stmt, const UInt4 *oidint, BOOL tidRef,
			   const char *tidval)
{
	CSTR	func = "SC_pos_newload";
	int			i;
	QResultClass *res, *qres;
	RETCODE		ret = SQL_ERROR;

	mylog("positioned new ti=%p\n", stmt->ti);
	if (!(res = SC_get_Curres(stmt)))
	{
		SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in SC_pos_newload.", func);
		return SQL_ERROR;
	}
	if (SC_update_not_ready(stmt))
		parse_statement(stmt, TRUE);	/* not preferable */
	if (!SC_is_updatable(stmt))
	{
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
		SC_set_error(stmt, STMT_INVALID_OPTION_IDENTIFIER, "the statement is read-only", func);
		return SQL_ERROR;
	}
	qres = positioned_load(stmt, (tidRef && NULL == tidval) ? USE_INSERTED_TID : 0, oidint, tidRef ? tidval : NULL);
	if (!qres || !QR_command_maybe_successful(qres))
	{
		SC_set_error(stmt, STMT_ERROR_TAKEN_FROM_BACKEND, "positioned_load in pos_newload failed", func);
	}
	else
	{
		SQLLEN	count = QR_get_num_cached_tuples(qres);

		QR_set_position(qres, 0);
		if (count == 1)
		{
			int	effective_fields = res->num_fields;
			ssize_t	tuple_size;
			SQLLEN	num_total_rows, num_cached_rows, kres_ridx;
			BOOL	appendKey = FALSE, appendData = FALSE;
			TupleField *tuple_old, *tuple_new;

			tuple_new = qres->tupleField;
			num_total_rows = QR_get_num_total_tuples(res);

			AddAdded(stmt, res, num_total_rows, tuple_new);
			num_cached_rows = QR_get_num_cached_tuples(res);
			kres_ridx = GIdx2KResIdx(num_total_rows, stmt, res);
			if (QR_haskeyset(res))
			{	if (!QR_get_cursor(res))
				{
					appendKey = TRUE;
					if (num_total_rows == CacheIdx2GIdx(num_cached_rows, stmt, res))
						appendData = TRUE;
					else
					{
inolog("total %d <> backend %d - base %d + start %d cursor_type=%d\n",
num_total_rows, num_cached_rows,
QR_get_rowstart_in_cache(res), SC_get_rowset_start(stmt), stmt->options.cursor_type);
					}
				}
				else if (kres_ridx >= 0 && kres_ridx < res->cache_size)
				{
					appendKey = TRUE;
					appendData = TRUE;
				}
			}
			if (appendKey)
			{
				if (res->num_cached_keys >= res->count_keyset_allocated)
				{
					if (!res->count_keyset_allocated)
						tuple_size = TUPLE_MALLOC_INC;
					else
						tuple_size = res->count_keyset_allocated * 2;
					QR_REALLOC_return_with_error(res->keyset, KeySet, sizeof(KeySet) * tuple_size, res, "pos_newload failed", SQL_ERROR);
					res->count_keyset_allocated = tuple_size;
				}
				KeySetSet(tuple_new, qres->num_fields, res->num_key_fields, res->keyset + kres_ridx, TRUE);
				res->num_cached_keys++;
			}
			if (appendData)
			{
inolog("total %d == backend %d - base %d + start %d cursor_type=%d\n",
num_total_rows, num_cached_rows,
QR_get_rowstart_in_cache(res), SC_get_rowset_start(stmt), stmt->options.cursor_type);
				if (num_cached_rows >= res->count_backend_allocated)
				{
					if (!res->count_backend_allocated)
						tuple_size = TUPLE_MALLOC_INC;
					else
						tuple_size = res->count_backend_allocated * 2;
					QR_REALLOC_return_with_error(res->backend_tuples, TupleField, res->num_fields * sizeof(TupleField) * tuple_size, res, "SC_pos_newload failed", SQL_ERROR);
					res->count_backend_allocated = tuple_size;
				}
				tuple_old = res->backend_tuples + res->num_fields * num_cached_rows;
				for (i = 0; i < effective_fields; i++)
				{
					tuple_old[i].len = tuple_new[i].len;
					tuple_new[i].len = -1;
					tuple_old[i].value = tuple_new[i].value;
					tuple_new[i].value = NULL;
				}
				res->num_cached_rows++;
			}
			ret = SQL_SUCCESS;
		}
		else if (0 == count)
			ret = SQL_NO_DATA_FOUND;
		else
		{
			SC_set_error(stmt, STMT_ROW_VERSION_CHANGED, "the driver cound't identify inserted rows", func);
			ret = SQL_ERROR;
		}
		/* stmt->currTuple = SC_get_rowset_start(stmt) + ridx; */
	}
	QR_Destructor(qres);
	return ret;
}

static RETCODE SQL_API
irow_update(RETCODE ret, StatementClass *stmt, StatementClass *ustmt, SQLULEN global_ridx, const KeySet *old_keyset)
{
	CSTR	func = "irow_update";

	if (ret != SQL_ERROR)
	{
		int			updcnt;
		QResultClass		*tres = SC_get_Curres(ustmt);
		const char *cmdstr = QR_get_command(tres);

		if (cmdstr &&
			sscanf(cmdstr, "UPDATE %d", &updcnt) == 1)
		{
			if (updcnt == 1)
			{
				KeySet	keys;

				if (NULL != tres->backend_tuples &&
				    1 == QR_get_num_cached_tuples(tres))
				{
					KeySetSet(tres->backend_tuples, QR_NumResultCols(tres), QR_NumResultCols(tres), &keys, TRUE);
					if (SQL_SUCCEEDED(ret = SC_pos_reload_with_key(stmt, global_ridx, (UInt2 *) 0, SQL_UPDATE, &keys)))
						AddRollback(stmt, SC_get_Curres(stmt), global_ridx, old_keyset, SQL_UPDATE);
				}
				else
					ret = SQL_ERROR;
			}
			else if (updcnt == 0)
			{
				SC_set_error(stmt, STMT_ROW_VERSION_CHANGED, "the content was changed before updation", func);
				ret = SQL_ERROR;
				if (stmt->options.cursor_type == SQL_CURSOR_KEYSET_DRIVEN)
					SC_pos_reload(stmt, global_ridx, (UInt2 *) 0, 0);
			}
			else
				ret = SQL_ERROR;
		}
		else
			ret = SQL_ERROR;
		if (ret == SQL_ERROR && SC_get_errornumber(stmt) == 0)
		{
			SC_set_error(stmt, STMT_ERROR_TAKEN_FROM_BACKEND, "SetPos update return error", func);
		}
	}
	return ret;
}

/* SQL_NEED_DATA callback for SC_pos_update */
typedef struct
{
	BOOL		updyes;
	QResultClass	*res;
	StatementClass	*stmt, *qstmt;
	IRDFields	*irdflds;
	SQLSETPOSIROW		irow;
	SQLULEN		global_ridx;
	KeySet		old_keyset;
}	pup_cdata;
static RETCODE
pos_update_callback(RETCODE retcode, void *para)
{
	RETCODE	ret = retcode;
	pup_cdata *s = (pup_cdata *) para;
	SQLLEN	kres_ridx;
	BOOL	idx_exist = TRUE;

	if (s->updyes)
	{
		mylog("pos_update_callback in\n");
		ret = irow_update(ret, s->stmt, s->qstmt, s->global_ridx, &s->old_keyset);
inolog("irow_update ret=%d,%d\n", ret, SC_get_errornumber(s->qstmt));
		if (ret != SQL_SUCCESS)
			SC_error_copy(s->stmt, s->qstmt, TRUE);
		PGAPI_FreeStmt(s->qstmt, SQL_DROP);
		s->qstmt = NULL;
	}
	s->updyes = FALSE;
	kres_ridx = GIdx2KResIdx(s->global_ridx, s->stmt, s->res);
	if (kres_ridx < 0 || kres_ridx >= s->res->num_cached_keys)
	{
		idx_exist = FALSE;
	}
	if (SQL_SUCCESS == ret && s->res->keyset && idx_exist)
	{
		ConnectionClass	*conn = SC_get_conn(s->stmt);

		if (CC_is_in_trans(conn))
		{
			s->res->keyset[kres_ridx].status |= (SQL_ROW_UPDATED  | CURS_SELF_UPDATING);
		}
		else
			s->res->keyset[kres_ridx].status |= (SQL_ROW_UPDATED  | CURS_SELF_UPDATED);
	}
	if (s->irdflds->rowStatusArray)
	{
		switch (ret)
		{
			case SQL_SUCCESS:
				s->irdflds->rowStatusArray[s->irow] = SQL_ROW_UPDATED;
				break;
			default:
				s->irdflds->rowStatusArray[s->irow] = ret;
		}
	}

	return ret;
}
RETCODE
SC_pos_update(StatementClass *stmt,
		  SQLSETPOSIROW irow, SQLULEN global_ridx, const KeySet *keyset)
{
	CSTR	func = "SC_pos_update";
	int			i,
				num_cols,
				upd_cols;
	pup_cdata	s;
	ConnectionClass	*conn;
	ARDFields	*opts = SC_get_ARDF(stmt);
	BindInfoClass *bindings = opts->bindings;
	TABLE_INFO	*ti;
	FIELD_INFO	**fi;
	char		updstr[4096];
	RETCODE		ret;
	OID	oid;
	UInt4	blocknum;
	UInt2	pgoffset;
	SQLLEN	offset;
	SQLLEN	*used, kres_ridx;
	Int4	bind_size = opts->bind_size;
	BOOL	idx_exist = TRUE;
	char	table_fqn[256];

	s.stmt = stmt;
	s.irow = irow;
	s.global_ridx = global_ridx;
	s.irdflds = SC_get_IRDF(s.stmt);
	fi = s.irdflds->fi;
	if (!(s.res = SC_get_Curres(s.stmt)))
	{
		SC_set_error(s.stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in SC_pos_update.", func);
		return SQL_ERROR;
	}
	mylog("POS UPDATE %d+%d fi=%p ti=%p\n", s.irow, QR_get_rowstart_in_cache(s.res), fi, s.stmt->ti);
	if (SC_update_not_ready(stmt))
		parse_statement(s.stmt, TRUE);	/* not preferable */
	if (!SC_is_updatable(s.stmt))
	{
		s.stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
		SC_set_error(s.stmt, STMT_INVALID_OPTION_IDENTIFIER, "the statement is read-only", func);
		return SQL_ERROR;
	}
	kres_ridx = GIdx2KResIdx(s.global_ridx, s.stmt, s.res);
	if (kres_ridx < 0 || kres_ridx >= s.res->num_cached_keys)
	{
		if (NULL == keyset || keyset->offset == 0)
		{
			SC_set_error(s.stmt, STMT_ROW_OUT_OF_RANGE, "the target keys are out of the rowset", func);
			return SQL_ERROR;
		}
		idx_exist = FALSE;
	}
	if (idx_exist)
	{
		if (!(oid = getOid(s.res, kres_ridx)))
		{
			if (!strcmp(SAFE_NAME(stmt->ti[0]->bestitem), OID_NAME))
			{
				SC_set_error(stmt, STMT_ROW_VERSION_CHANGED, "the row was already deleted ?", func);
				return SQL_ERROR;
			}
		}
		getTid(s.res, kres_ridx, &blocknum, &pgoffset);
		s.old_keyset = s.res->keyset[kres_ridx];
	}
	else
	{
		oid = keyset->oid;
		blocknum = keyset->blocknum;
		pgoffset = keyset->offset;
		s.old_keyset = *keyset;
	}

	ti = s.stmt->ti[0];

	SPRINTF_FIXED(updstr,
			 "update %s set", ti_quote(stmt, oid, table_fqn, sizeof(table_fqn)));

	num_cols = s.irdflds->nfields;
	offset = opts->row_offset_ptr ? *opts->row_offset_ptr : 0;
	for (i = upd_cols = 0; i < num_cols; i++)
	{
		if (used = bindings[i].used, used != NULL)
		{
			used = LENADDR_SHIFT(used, offset);
			if (bind_size > 0)
				used = LENADDR_SHIFT(used, bind_size * s.irow);
			else
				used = LENADDR_SHIFT(used, s.irow * sizeof(SQLLEN));
			mylog("%d used=%d,%p\n", i, *used, used);
			if (*used != SQL_IGNORE && fi[i]->updatable)
			{
				if (upd_cols)
					SPRINTFCAT_FIXED(updstr,
								 ", \"%s\" = ?", GET_NAME(fi[i]->column_name));
				else
					SPRINTFCAT_FIXED(updstr,
								 " \"%s\" = ?", GET_NAME(fi[i]->column_name));
				upd_cols++;
			}
		}
		else
			mylog("%d null bind\n", i);
	}
	conn = SC_get_conn(s.stmt);
	s.updyes = FALSE;
	if (upd_cols > 0)
	{
		HSTMT		hstmt;
		int			j;
		ConnInfo	*ci = &(conn->connInfo);
		APDFields	*apdopts;
		IPDFields	*ipdopts;
		OID		fieldtype = 0;
		const char *bestitem = GET_NAME(ti->bestitem);
		const char *bestqual = GET_NAME(ti->bestqual);
		int	unknown_sizes = ci->drivers.unknown_sizes;

		SPRINTFCAT_FIXED(updstr,
					 " where ctid = '(%u, %u)'",
					 blocknum, pgoffset);
		if (bestqual)
		{
			SPRINTFCAT_FIXED(updstr, " and ");
			SPRINTFCAT_FIXED(updstr, bestqual, oid);
		}
		if (PG_VERSION_GE(conn, 8.2))
		{
			SPRINTFCAT_FIXED(updstr, " returning ctid");
			if (bestitem)
			{
				SPRINTFCAT_FIXED(updstr, ", ");
				SPRINTFCAT_FIXED(updstr, "\"%s\"", bestitem);
			}
		}
		mylog("updstr=%s\n", updstr);
		if (PGAPI_AllocStmt(conn, &hstmt, 0) != SQL_SUCCESS)
		{
			SC_set_error(s.stmt, STMT_NO_MEMORY_ERROR, "internal AllocStmt error", func);
			return SQL_ERROR;
		}
		s.qstmt = (StatementClass *) hstmt;
		apdopts = SC_get_APDF(s.qstmt);
		apdopts->param_bind_type = opts->bind_size;
		apdopts->param_offset_ptr = opts->row_offset_ptr;
		ipdopts = SC_get_IPDF(s.qstmt);
		SC_set_delegate(s.stmt, s.qstmt);
		extend_iparameter_bindings(ipdopts, num_cols);
		for (i = j = 0; i < num_cols; i++)
		{
			if (used = bindings[i].used, used != NULL)
			{
				used = LENADDR_SHIFT(used, offset);
				if (bind_size > 0)
					used = LENADDR_SHIFT(used, bind_size * s.irow);
				else
					used = LENADDR_SHIFT(used, s.irow * sizeof(SQLLEN));
				mylog("%d used=%d\n", i, *used);
				if (*used != SQL_IGNORE && fi[i]->updatable)
				{
					/* fieldtype = QR_get_field_type(s.res, i); */
					fieldtype = getEffectiveOid(conn, fi[i]);
					PIC_set_pgtype(ipdopts->parameters[j], fieldtype);
					PGAPI_BindParameter(hstmt,
						(SQLUSMALLINT) ++j,
						SQL_PARAM_INPUT,
						bindings[i].returntype,
						pgtype_to_concise_type(s.stmt, fieldtype, i, unknown_sizes),
																fi[i]->column_size > 0 ? fi[i]->column_size : pgtype_column_size(s.stmt, fieldtype, i, unknown_sizes),
						(SQLSMALLINT) fi[i]->decimal_digits,
						bindings[i].buffer,
						bindings[i].buflen,
						bindings[i].used);
				}
			}
		}
		s.qstmt->exec_start_row = s.qstmt->exec_end_row = s.irow;
		s.updyes = TRUE;
		ret = PGAPI_ExecDirect(hstmt, (SQLCHAR *) updstr, SQL_NTS, 0);
		if (ret == SQL_NEED_DATA)
		{
			pup_cdata *cbdata = (pup_cdata *) malloc(sizeof(pup_cdata));
			if (!cbdata)
			{
				SC_set_error(s.stmt, STMT_NO_MEMORY_ERROR, "Could not allocate memory for cbdata", func);
				return SQL_ERROR;
			}
			memcpy(cbdata, &s, sizeof(pup_cdata));
			if (0 == enqueueNeedDataCallback(s.stmt, pos_update_callback, cbdata))
				ret = SQL_ERROR;
			return ret;
		}
		/* else if (ret != SQL_SUCCESS) this is unneccesary
			SC_error_copy(s.stmt, s.qstmt, TRUE); */
	}
	else
	{
		ret = SQL_SUCCESS_WITH_INFO;
		SC_set_error(s.stmt, STMT_INVALID_CURSOR_STATE_ERROR, "update list null", func);
	}

	ret = pos_update_callback(ret, &s);
	return ret;
}
RETCODE
SC_pos_delete(StatementClass *stmt,
		  SQLSETPOSIROW irow, SQLULEN global_ridx, const KeySet *keyset)
{
	CSTR	func = "SC_pos_update";
	UWORD		offset;
	QResultClass *res, *qres;
	ConnectionClass	*conn = SC_get_conn(stmt);
	IRDFields	*irdflds = SC_get_IRDF(stmt);
	char		dltstr[4096];
	RETCODE		ret;
	SQLLEN		kres_ridx;
	OID		oid;
	UInt4		blocknum, qflag;
	TABLE_INFO	*ti;
	const char	*bestitem;
	const char	*bestqual;
	BOOL		idx_exist = TRUE;
	char		table_fqn[256];

	mylog("POS DELETE ti=%p\n", stmt->ti);
	if (!(res = SC_get_Curres(stmt)))
	{
		SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in SC_pos_delete.", func);
		return SQL_ERROR;
	}
	if (SC_update_not_ready(stmt))
		parse_statement(stmt, TRUE);	/* not preferable */
	if (!SC_is_updatable(stmt))
	{
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
		SC_set_error(stmt, STMT_INVALID_OPTION_IDENTIFIER, "the statement is read-only", func);
		return SQL_ERROR;
	}
	kres_ridx = GIdx2KResIdx(global_ridx, stmt, res);
	if (kres_ridx < 0 || kres_ridx >= res->num_cached_keys)
	{
		if (NULL == keyset || keyset->offset == 0)
		{
			SC_set_error(stmt, STMT_ROW_OUT_OF_RANGE, "the target keys are out of the rowset", func);
			return SQL_ERROR;
		}
		idx_exist = FALSE;
	}
	ti = stmt->ti[0];
	bestitem = GET_NAME(ti->bestitem);
	bestqual = GET_NAME(ti->bestqual);
	if (idx_exist)
	{
		if (!(oid = getOid(res, kres_ridx)))
		{
			if (bestitem && !strcmp(bestitem, OID_NAME))
			{
				SC_set_error(stmt, STMT_ROW_VERSION_CHANGED, "the row was already deleted ?", func);
				return SQL_ERROR;
			}
		}
		getTid(res, kres_ridx, &blocknum, &offset);
		keyset = res->keyset + kres_ridx;
	}
	else
	{
		oid = keyset->oid;
		blocknum = keyset->blocknum;
		offset = keyset->offset;
	}
	SPRINTF_FIXED(dltstr,
			 "delete from %s where ctid = '(%u, %u)'",
			 ti_quote(stmt, oid, table_fqn, sizeof(table_fqn)), blocknum, offset);
	if (bestqual && !TI_has_subclass(ti))
	{
		SPRINTFCAT_FIXED(dltstr, " and ");
		SPRINTFCAT_FIXED(dltstr, bestqual, oid);
	}

	mylog("dltstr=%s\n", dltstr);
	qflag = 0;
        if (stmt->external && !CC_is_in_trans(conn) &&
                 (!CC_does_autocommit(conn)))
		qflag |= GO_INTO_TRANSACTION;
	qres = CC_send_query(conn, dltstr, NULL, qflag, stmt);
	ret = SQL_SUCCESS;
	if (QR_command_maybe_successful(qres))
	{
		int			dltcnt;
		const char *cmdstr = QR_get_command(qres);

		if (cmdstr &&
			sscanf(cmdstr, "DELETE %d", &dltcnt) == 1)
		{
			if (dltcnt == 1)
			{
				RETCODE	tret = SC_pos_reload_with_key(stmt, global_ridx, (UInt2 *) 0, SQL_DELETE, keyset);
				if (!SQL_SUCCEEDED(tret))
					ret = tret;
			}
			else if (dltcnt == 0)
			{
				SC_set_error(stmt, STMT_ROW_VERSION_CHANGED, "the content was changed before deletion", func);
				ret = SQL_ERROR;
				if (idx_exist && stmt->options.cursor_type == SQL_CURSOR_KEYSET_DRIVEN)
					SC_pos_reload(stmt, global_ridx, (UInt2 *) 0, 0);
			}
			else
				ret = SQL_ERROR;
		}
		else
			ret = SQL_ERROR;
	}
	else
	{
		ret = SQL_ERROR;
		if (qres)
		{
			STRCPY_FIXED(res->sqlstate, qres->sqlstate);
			res->message = qres->message;
			qres->message = NULL;
		}
	}
	if (ret == SQL_ERROR && SC_get_errornumber(stmt) == 0)
	{
		SC_set_error(stmt, STMT_ERROR_TAKEN_FROM_BACKEND, "SetPos delete return error", func);
	}
	if (qres)
		QR_Destructor(qres);
	if (SQL_SUCCESS == ret && keyset)
		AddDeleted(res, global_ridx, keyset);
	if (SQL_SUCCESS == ret && keyset && idx_exist)
	{
		res->keyset[kres_ridx].status &= (~KEYSET_INFO_PUBLIC);
		if (CC_is_in_trans(conn))
		{
			res->keyset[kres_ridx].status |= (SQL_ROW_DELETED | CURS_SELF_DELETING);
		}
		else
			res->keyset[kres_ridx].status |= (SQL_ROW_DELETED | CURS_SELF_DELETED);
inolog(".status[%d]=%x\n", global_ridx, res->keyset[kres_ridx].status);
	}
	if (irdflds->rowStatusArray)
	{
		switch (ret)
		{
			case SQL_SUCCESS:
				irdflds->rowStatusArray[irow] = SQL_ROW_DELETED;
				break;
			default:
				irdflds->rowStatusArray[irow] = ret;
		}
	}
	return ret;
}

static RETCODE SQL_API
irow_insert(RETCODE ret, StatementClass *stmt, StatementClass *istmt,
			SQLLEN addpos)
{
	CSTR	func = "irow_insert";

	if (ret != SQL_ERROR)
	{
		int		addcnt;
		OID		oid, *poid = NULL;
		ARDFields	*opts = SC_get_ARDF(stmt);
		QResultClass	*ires = SC_get_Curres(istmt), *tres;
		const char *cmdstr;
		BindInfoClass	*bookmark;

		tres = (ires->next ? ires->next : ires);
		cmdstr = QR_get_command(tres);
		if (cmdstr &&
			sscanf(cmdstr, "INSERT %u %d", &oid, &addcnt) == 2 &&
			addcnt == 1)
		{
			RETCODE	qret;
			const char * tidval = NULL;
			char	tidv[32];
			KeySet	keys;

			if (NULL != tres->backend_tuples &&
			    1 == QR_get_num_cached_tuples(tres))
			{
				KeySetSet(tres->backend_tuples, QR_NumResultCols(tres), QR_NumResultCols(tres), &keys, TRUE);
				oid = keys.oid;
				SPRINTF_FIXED(tidv, "(%u,%hu)", keys.blocknum, keys.offset);
				tidval = tidv;
			}
			if (0 != oid)
				poid = &oid;
			qret = SC_pos_newload(stmt, poid, TRUE, tidval);
			if (SQL_ERROR == qret)
				return qret;

			if (SQL_NO_DATA_FOUND == qret)
			{
				qret = SC_pos_newload(stmt, poid, FALSE, NULL);
				if (SQL_ERROR == qret)
					return qret;
			}
			bookmark = opts->bookmark;
			if (bookmark && bookmark->buffer)
			{
				SC_set_current_col(stmt, -1);
				SC_Create_bookmark(stmt, bookmark, stmt->bind_row, addpos, &keys);
			}
		}
		else
		{
			SC_set_error(stmt, STMT_ERROR_TAKEN_FROM_BACKEND, "SetPos insert return error", func);
		}
	}
	return ret;
}

/* SQL_NEED_DATA callback for SC_pos_add */
typedef struct
{
	BOOL		updyes;
	QResultClass	*res;
	StatementClass	*stmt, *qstmt;
	IRDFields	*irdflds;
	SQLSETPOSIROW		irow;
}	padd_cdata;

static RETCODE
pos_add_callback(RETCODE retcode, void *para)
{
	RETCODE	ret = retcode;
	padd_cdata *s = (padd_cdata *) para;
	SQLLEN	addpos;

	if (s->updyes)
	{
		SQLSETPOSIROW	brow_save;

		mylog("pos_add_callback in ret=%d\n", ret);
		brow_save = s->stmt->bind_row;
		s->stmt->bind_row = s->irow;
		if (QR_get_cursor(s->res))
			addpos = -(SQLLEN)(s->res->ad_count + 1);
		else
			addpos = QR_get_num_total_tuples(s->res);
		ret = irow_insert(ret, s->stmt, s->qstmt, addpos);
		s->stmt->bind_row = brow_save;
	}
	s->updyes = FALSE;
	SC_setInsertedTable(s->qstmt, ret);
	if (ret != SQL_SUCCESS)
		SC_error_copy(s->stmt, s->qstmt, TRUE);
	PGAPI_FreeStmt((HSTMT) s->qstmt, SQL_DROP);
	s->qstmt = NULL;
	if (SQL_SUCCESS == ret && s->res->keyset)
	{
		SQLLEN	global_ridx = QR_get_num_total_tuples(s->res) - 1;
		ConnectionClass	*conn = SC_get_conn(s->stmt);
		SQLLEN	kres_ridx;
		UWORD	status = SQL_ROW_ADDED;

		if (CC_is_in_trans(conn))
			status |= CURS_SELF_ADDING;
		else
			status |= CURS_SELF_ADDED;
		kres_ridx = GIdx2KResIdx(global_ridx, s->stmt, s->res);
		if (kres_ridx >= 0 && kres_ridx < s->res->num_cached_keys)
		{
			s->res->keyset[kres_ridx].status = status;
		}
	}
	if (s->irdflds->rowStatusArray)
	{
		switch (ret)
		{
			case SQL_SUCCESS:
				s->irdflds->rowStatusArray[s->irow] = SQL_ROW_ADDED;
				break;
			default:
				s->irdflds->rowStatusArray[s->irow] = ret;
		}
	}

	return ret;
}

RETCODE
SC_pos_add(StatementClass *stmt,
		   SQLSETPOSIROW irow)
{
	CSTR	func = "SC_pos_add";
	int			num_cols,
				add_cols,
				i;
	HSTMT		hstmt;

	padd_cdata	s;
	ConnectionClass	*conn;
	ConnInfo	*ci;
	ARDFields	*opts = SC_get_ARDF(stmt);
	APDFields	*apdopts;
	IPDFields	*ipdopts;
	BindInfoClass *bindings = opts->bindings;
	FIELD_INFO	**fi = SC_get_IRDF(stmt)->fi;
	char		addstr[4096];
	RETCODE		ret;
	SQLULEN		offset;
	SQLLEN		*used;
	Int4		bind_size = opts->bind_size;
	OID		fieldtype;
	int		unknown_sizes;
	int		func_cs_count = 0;
	char		table_fqn[256];

	mylog("POS ADD fi=%p ti=%p\n", fi, stmt->ti);
	s.stmt = stmt;
	s.irow = irow;
	if (!(s.res = SC_get_Curres(s.stmt)))
	{
		SC_set_error(s.stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in SC_pos_add.", func);
		return SQL_ERROR;
	}
	if (SC_update_not_ready(stmt))
		parse_statement(s.stmt, TRUE);	/* not preferable */
	if (!SC_is_updatable(s.stmt))
	{
		s.stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
		SC_set_error(s.stmt, STMT_INVALID_OPTION_IDENTIFIER, "the statement is read-only", func);
		return SQL_ERROR;
	}
	s.irdflds = SC_get_IRDF(s.stmt);
	num_cols = s.irdflds->nfields;
	conn = SC_get_conn(s.stmt);

	SPRINTF_FIXED(addstr,
			 "insert into %s (",
			 ti_quote(s.stmt, 0, table_fqn, sizeof(table_fqn)));
	if (PGAPI_AllocStmt(conn, &hstmt, 0) != SQL_SUCCESS)
	{
		SC_set_error(s.stmt, STMT_NO_MEMORY_ERROR, "internal AllocStmt error", func);
		return SQL_ERROR;
	}
	if (opts->row_offset_ptr)
		offset = *opts->row_offset_ptr;
	else
		offset = 0;
	s.qstmt = (StatementClass *) hstmt;
	apdopts = SC_get_APDF(s.qstmt);
	apdopts->param_bind_type = opts->bind_size;
	apdopts->param_offset_ptr = opts->row_offset_ptr;
	ipdopts = SC_get_IPDF(s.qstmt);
	SC_set_delegate(s.stmt, s.qstmt);
	ci = &(conn->connInfo);
	unknown_sizes = ci->drivers.unknown_sizes;
	extend_iparameter_bindings(ipdopts, num_cols);
	for (i = add_cols = 0; i < num_cols; i++)
	{
		if (used = bindings[i].used, used != NULL)
		{
			used = LENADDR_SHIFT(used, offset);
			if (bind_size > 0)
				used = LENADDR_SHIFT(used, bind_size * s.irow);
			else
				used = LENADDR_SHIFT(used, s.irow * sizeof(SQLLEN));
			mylog("%d used=%d\n", i, *used);
			if (*used != SQL_IGNORE && fi[i]->updatable)
			{
				/* fieldtype = QR_get_field_type(s.res, i); */
				fieldtype = getEffectiveOid(conn, fi[i]);
				if (add_cols)
					SPRINTFCAT_FIXED(addstr,
								 ", \"%s\"", GET_NAME(fi[i]->column_name));
				else
					SPRINTFCAT_FIXED(addstr,
								 "\"%s\"", GET_NAME(fi[i]->column_name));
				PIC_set_pgtype(ipdopts->parameters[add_cols], fieldtype);
				PGAPI_BindParameter(hstmt,
					(SQLUSMALLINT) ++add_cols,
					SQL_PARAM_INPUT,
					bindings[i].returntype,
					pgtype_to_concise_type(s.stmt, fieldtype, i, unknown_sizes),
															fi[i]->column_size > 0 ? fi[i]->column_size : pgtype_column_size(s.stmt, fieldtype, i, unknown_sizes),
					(SQLSMALLINT) fi[i]->decimal_digits,
					bindings[i].buffer,
					bindings[i].buflen,
					bindings[i].used);
			}
		}
		else
			mylog("%d null bind\n", i);
	}
	s.updyes = FALSE;
#define	return	DONT_CALL_RETURN_FROM_HERE???
	ENTER_INNER_CONN_CS(conn, func_cs_count);
	if (add_cols > 0)
	{
		SPRINTFCAT_FIXED(addstr, ") values (");
		for (i = 0; i < add_cols; i++)
		{
			if (i)
				SPRINTFCAT_FIXED(addstr, ", ?");
			else
				SPRINTFCAT_FIXED(addstr, "?");
		}
		SPRINTFCAT_FIXED(addstr, ")");
		if (PG_VERSION_GE(conn, 8.2))
		{
			TABLE_INFO	*ti = stmt->ti[0];
			const char *bestitem = GET_NAME(ti->bestitem);

			SPRINTFCAT_FIXED(addstr, " returning ctid");
			if (bestitem)
			{
				SPRINTFCAT_FIXED(addstr, ", ");
				SPRINTFCAT_FIXED(addstr, "\"%s\"", bestitem);
			}
		}
		mylog("addstr=%s\n", addstr);
		s.qstmt->exec_start_row = s.qstmt->exec_end_row = s.irow;
		s.updyes = TRUE;
		ret = PGAPI_ExecDirect(hstmt, (SQLCHAR *) addstr, SQL_NTS, 0);
		if (ret == SQL_NEED_DATA)
		{
			padd_cdata *cbdata = (padd_cdata *) malloc(sizeof(padd_cdata));
			if (!cbdata)
			{
				SC_set_error(s.stmt, STMT_NO_MEMORY_ERROR, "Could not allocate memory for cbdata", func);
				ret = SQL_ERROR;
			goto cleanup;
			}
			memcpy(cbdata, &s, sizeof(padd_cdata));
			if (0 == enqueueNeedDataCallback(s.stmt, pos_add_callback, cbdata))
				ret = SQL_ERROR;
			goto cleanup;
		}
		/* else if (ret != SQL_SUCCESS) this is unneccesary
			SC_error_copy(s.stmt, s.qstmt, TRUE); */
	}
	else
	{
		ret = SQL_SUCCESS_WITH_INFO;
		SC_set_error(s.stmt, STMT_INVALID_CURSOR_STATE_ERROR, "insert list null", func);
	}

	ret = pos_add_callback(ret, &s);

cleanup:
#undef	return
	CLEANUP_FUNC_CONN_CS(func_cs_count, conn);
	return ret;
}

/*
 *	Stuff for updatable cursors end.
 */

RETCODE
SC_pos_refresh(StatementClass *stmt, SQLSETPOSIROW irow , SQLULEN global_ridx)
{
	RETCODE	ret;
	IRDFields	*irdflds = SC_get_IRDF(stmt);
	/* save the last_fetch_count */
	SQLLEN		last_fetch = stmt->last_fetch_count;
	SQLLEN		last_fetch2 = stmt->last_fetch_count_include_ommitted;
	SQLSETPOSIROW	bind_save = stmt->bind_row;
	BOOL		tuple_reload = FALSE;

	if (stmt->options.cursor_type == SQL_CURSOR_KEYSET_DRIVEN)
		tuple_reload = TRUE;
	else
	{
		QResultClass	*res = SC_get_Curres(stmt);
		if (res && res->keyset)
		{
			SQLLEN kres_ridx = GIdx2KResIdx(global_ridx, stmt, res);
			if (kres_ridx >= 0 && kres_ridx < QR_get_num_cached_tuples(res))
			{
				if (0 != (CURS_NEEDS_REREAD & res->keyset[kres_ridx].status))
					tuple_reload = TRUE;
			}
		}
	}
	if (tuple_reload)
		SC_pos_reload(stmt, global_ridx, (UInt2 *) 0, 0);
	stmt->bind_row = irow;
	ret = SC_fetch(stmt);
	/* restore the last_fetch_count */
	stmt->last_fetch_count = last_fetch;
	stmt->last_fetch_count_include_ommitted = last_fetch2;
	stmt->bind_row = bind_save;
	if (irdflds->rowStatusArray)
	{
		switch (ret)
		{
			case SQL_ERROR:
				irdflds->rowStatusArray[irow] = SQL_ROW_ERROR;
				break;
			case SQL_SUCCESS:
				irdflds->rowStatusArray[irow] = SQL_ROW_SUCCESS;
				break;
			case SQL_SUCCESS_WITH_INFO:
			default:
				irdflds->rowStatusArray[irow] = ret;
				break;
		}
	}

	return SQL_SUCCESS;
}

/*	SQL_NEED_DATA callback for PGAPI_SetPos */
typedef struct
{
	BOOL		need_data_callback, auto_commit_needed;
	QResultClass	*res;
	StatementClass	*stmt;
	ARDFields	*opts;
	GetDataInfo	*gdata;
	SQLLEN	idx, start_row, end_row, ridx;
	UWORD	fOption;
	SQLSETPOSIROW	irow, nrow, processed;
}	spos_cdata;
static
RETCODE spos_callback(RETCODE retcode, void *para)
{
	CSTR	func = "spos_callback";
	RETCODE	ret;
	spos_cdata *s = (spos_cdata *) para;
	QResultClass	*res;
	ARDFields	*opts;
	ConnectionClass	*conn;
	SQLULEN	global_ridx;
	SQLLEN	kres_ridx, pos_ridx = 0;

	ret = retcode;
	mylog("%s: %d in\n", func, s->need_data_callback);
	if (s->need_data_callback)
	{
		s->processed++;
		if (SQL_ERROR != retcode)
		{
			s->nrow++;
			s->idx++;
		}
	}
	else
	{
		s->ridx = -1;
		s->idx = s->nrow = s->processed = 0;
	}
	res = s->res;
	opts = s->opts;
	if (!res || !opts)
	{
		SC_set_error(s->stmt, STMT_SEQUENCE_ERROR, "Passed res or opts for spos_callback is NULL", func);
		return SQL_ERROR;
	}
	s->need_data_callback = FALSE;
	for (; SQL_ERROR != ret && s->nrow <= s->end_row; s->idx++)
	{
		global_ridx = RowIdx2GIdx(s->idx, s->stmt);
		if (SQL_ADD != s->fOption)
		{
			if ((int) global_ridx >= QR_get_num_total_tuples(res))
				break;
			if (res->keyset)
			{
				kres_ridx = GIdx2KResIdx(global_ridx, s->stmt, res);
				if (kres_ridx >= res->num_cached_keys)
					break;
				if (kres_ridx >= 0) /* the row may be deleted and not in the rowset */
				{
					if (0 == (res->keyset[kres_ridx].status & CURS_IN_ROWSET))
						continue;
				}
			}
		}
		if (s->nrow < s->start_row)
		{
			s->nrow++;
			continue;
		}
		s->ridx = s->nrow;
		pos_ridx = s->idx;
		if (0 != s->irow || !opts->row_operation_ptr || opts->row_operation_ptr[s->nrow] == SQL_ROW_PROCEED)
		{
			switch (s->fOption)
			{
				case SQL_UPDATE:
					ret = SC_pos_update(s->stmt, s->nrow, global_ridx, NULL);
					break;
				case SQL_DELETE:
					ret = SC_pos_delete(s->stmt, s->nrow, global_ridx, NULL);
					break;
				case SQL_ADD:
					ret = SC_pos_add(s->stmt, s->nrow);
					break;
				case SQL_REFRESH:
					ret = SC_pos_refresh(s->stmt, s->nrow, global_ridx);
					break;
			}
			if (SQL_NEED_DATA == ret)
			{
				spos_cdata *cbdata = (spos_cdata *) malloc(sizeof(spos_cdata));
				if (!cbdata)
				{
					SC_set_error(s->stmt, STMT_NO_MEMORY_ERROR, "Could not allocate memory for cbdata", func);
					return SQL_ERROR;
				}

				memcpy(cbdata, s, sizeof(spos_cdata));
				cbdata->need_data_callback = TRUE;
				if (0 == enqueueNeedDataCallback(s->stmt, spos_callback, cbdata))
					ret = SQL_ERROR;
				return ret;
			}
			s->processed++;
		}
		if (SQL_ERROR != ret)
			s->nrow++;
	}
	conn = SC_get_conn(s->stmt);
	if (s->auto_commit_needed)
		CC_set_autocommit(conn, TRUE);
	if (s->irow > 0)
	{
		if (SQL_ADD != s->fOption && s->ridx >= 0) /* for SQLGetData */
		{
			s->stmt->currTuple = RowIdx2GIdx(pos_ridx, s->stmt);
			QR_set_position(res, pos_ridx);
		}
	}
	else if (SC_get_IRDF(s->stmt)->rowsFetched)
		*(SC_get_IRDF(s->stmt)->rowsFetched) = s->processed;
	res->recent_processed_row_count = s->stmt->diag_row_count = s->processed;
if (opts)
{
inolog("processed=%d ret=%d rowset=%d", s->processed, ret, opts->size_of_rowset_odbc2);
inolog(",%d\n", opts->size_of_rowset);
}

	return ret;
}

/*
 *	This positions the cursor within a rowset, that was positioned using SQLExtendedFetch.
 *	This will be useful (so far) only when using SQLGetData after SQLExtendedFetch.
 */
RETCODE		SQL_API
PGAPI_SetPos(HSTMT hstmt,
			 SQLSETPOSIROW irow,
			 SQLUSMALLINT fOption,
			 SQLUSMALLINT fLock)
{
	CSTR func = "PGAPI_SetPos";
	RETCODE	ret;
	ConnectionClass	*conn;
	SQLLEN		rowsetSize;
	int		i;
	UInt2		gdata_allocated;
	GetDataInfo	*gdata_info;
	GetDataClass	*gdata = NULL;
	spos_cdata	s;

	s.stmt = (StatementClass *) hstmt;
	if (!s.stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}

	s.irow = irow;
	s.fOption = fOption;
	s.auto_commit_needed = FALSE;
	s.opts = SC_get_ARDF(s.stmt);
	gdata_info = SC_get_GDTI(s.stmt);
	gdata = gdata_info->gdata;
	mylog("%s fOption=%d irow=%d lock=%d currt=%d\n", func, s.fOption, s.irow, fLock, s.stmt->currTuple);
	if (s.stmt->options.scroll_concurrency != SQL_CONCUR_READ_ONLY)
		;
	else if (s.fOption != SQL_POSITION && s.fOption != SQL_REFRESH)
	{
		SC_set_error(s.stmt, STMT_NOT_IMPLEMENTED_ERROR, "Only SQL_POSITION/REFRESH is supported for PGAPI_SetPos", func);
		return SQL_ERROR;
	}

	if (!(s.res = SC_get_Curres(s.stmt)))
	{
		SC_set_error(s.stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in PGAPI_SetPos.", func);
		return SQL_ERROR;
	}

	rowsetSize = (s.stmt->transition_status == STMT_TRANSITION_EXTENDED_FETCH ? s.opts->size_of_rowset_odbc2 : s.opts->size_of_rowset);
	if (s.irow == 0) /* bulk operation */
	{
		if (SQL_POSITION == s.fOption)
		{
			SC_set_error(s.stmt, STMT_INVALID_CURSOR_POSITION, "Bulk Position operations not allowed.", func);
			return SQL_ERROR;
		}
		s.start_row = 0;
		s.end_row = rowsetSize - 1;
	}
	else
	{
		if (SQL_ADD != s.fOption && s.irow > s.stmt->last_fetch_count)
		{
			SC_set_error(s.stmt, STMT_ROW_OUT_OF_RANGE, "Row value out of range", func);
			return SQL_ERROR;
		}
		s.start_row = s.end_row = s.irow - 1;
	}

	gdata_allocated = gdata_info->allocated;
mylog("num_cols=%d gdatainfo=%d\n", QR_NumPublicResultCols(s.res), gdata_allocated);
	/* Reset for SQLGetData */
	if (gdata)
	{
		for (i = 0; i < gdata_allocated; i++)
			GETDATA_RESET(gdata[i]);
	}
	conn = SC_get_conn(s.stmt);
	switch (s.fOption)
	{
		case SQL_UPDATE:
		case SQL_DELETE:
		case SQL_ADD:
			if (s.auto_commit_needed = CC_does_autocommit(conn), s.auto_commit_needed)
				CC_set_autocommit(conn, FALSE);
			break;
		case SQL_POSITION:
			break;
	}

	s.need_data_callback = FALSE;
#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(s.stmt); */
	ret = spos_callback(SQL_SUCCESS, &s);
#undef	return
	if (SQL_SUCCEEDED(ret) && 0 == s.processed)
	{
		SC_set_error(s.stmt, STMT_ROW_OUT_OF_RANGE, "the row was deleted?", func);
		ret = SQL_ERROR;
	}
	mylog("%s returning %d\n", func, ret);
	return ret;
}


/*		Sets options that control the behavior of cursors. */
RETCODE		SQL_API
PGAPI_SetScrollOptions(HSTMT hstmt,
					   SQLUSMALLINT fConcurrency,
					   SQLLEN crowKeyset,
					   SQLUSMALLINT crowRowset)
{
	CSTR func = "PGAPI_SetScrollOptions";
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("%s: fConcurrency=%d crowKeyset=%d crowRowset=%d\n",
		  func, fConcurrency, crowKeyset, crowRowset);
	SC_set_error(stmt, STMT_NOT_IMPLEMENTED_ERROR, "SetScroll option not implemeted", func);

	return SQL_ERROR;
}


/*	Set the cursor name on a statement handle */
RETCODE		SQL_API
PGAPI_SetCursorName(HSTMT hstmt,
					const SQLCHAR * szCursor,
					SQLSMALLINT cbCursor)
{
	CSTR func = "PGAPI_SetCursorName";
	StatementClass *stmt = (StatementClass *) hstmt;

	mylog("%s: hstmt=%p, szCursor=%p, cbCursorMax=%d\n", func, hstmt, szCursor, cbCursor);

	if (!stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}

	SET_NAME_DIRECTLY(stmt->cursor_name, make_string(szCursor, cbCursor, NULL, 0));
	return SQL_SUCCESS;
}


/*	Return the cursor name for a statement handle */
RETCODE		SQL_API
PGAPI_GetCursorName(HSTMT hstmt,
					SQLCHAR * szCursor,
					SQLSMALLINT cbCursorMax,
					SQLSMALLINT * pcbCursor)
{
	CSTR func = "PGAPI_GetCursorName";
	StatementClass *stmt = (StatementClass *) hstmt;
	size_t		len = 0;
	RETCODE		result;

	mylog("%s: hstmt=%p, szCursor=%p, cbCursorMax=%d, pcbCursor=%p\n", func, hstmt, szCursor, cbCursorMax, pcbCursor);

	if (!stmt)
	{
		SC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}
	result = SQL_SUCCESS;
	len = strlen(SC_cursor_name(stmt));

	if (szCursor)
	{
		strncpy_null((char *) szCursor, SC_cursor_name(stmt), cbCursorMax);

		if (len >= cbCursorMax)
		{
			result = SQL_SUCCESS_WITH_INFO;
			SC_set_error(stmt, STMT_TRUNCATED, "The buffer was too small for the GetCursorName.", func);
		}
	}

	if (pcbCursor)
		*pcbCursor = (SQLSMALLINT) len;

	/*
	 * Because this function causes no db-access, there's
	 * no need to call DiscardStatementSvp()
	 */

	return result;
}

RETCODE
SC_fetch_by_bookmark(StatementClass *stmt)
{
	UInt2		offset;
	SQLLEN		kres_ridx;
	OID		oidint;
	UInt4		blocknum;
	QResultClass	*res, *qres;
	RETCODE		ret = SQL_ERROR;

	HSTMT		hstmt = NULL;
	StatementClass	*fstmt;
	SQLLEN		size_of_rowset,	cached_rows;
	SQLULEN		cRow;
	UInt2		num_fields;
	ARDFields	*opts = SC_get_ARDF(stmt);
	SQLHDESC	hdesc;
	APDFields	*apdf;
	const int	tidbuflen = 20;
	char 		*tidbuf = NULL, *query = NULL;
	int		i, lodlen;
	BindInfoClass	*bookmark_orig = opts->bookmark;
	TupleField	*otuple, *ituple;
	SQLUSMALLINT	*rowStatusArray;

	mylog("%s in\n", __FUNCTION__);

	if (!(res = SC_get_Curres(stmt)))
	{
		SC_set_error(stmt, STMT_INVALID_CURSOR_STATE_ERROR, "Null statement result in SC_fetch_by_bookmark.", __FUNCTION__);
		return SQL_ERROR;
	}
	if (SC_update_not_ready(stmt))
		parse_statement(stmt, TRUE);	/* not preferable */
	if (!SC_is_updatable(stmt))
	{
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
		SC_set_error(stmt, STMT_INVALID_OPTION_IDENTIFIER, "the statement is read-only", __FUNCTION__);
		return SQL_ERROR;
	}
	if (ret = PGAPI_AllocStmt(SC_get_conn(stmt), &hstmt, 0), !SQL_SUCCEEDED(ret))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "internal AllocStmt error", __FUNCTION__);
		return ret;
	}
	size_of_rowset = opts->size_of_rowset;
	SC_MALLOC_gexit_with_error(tidbuf, char, size_of_rowset * tidbuflen, stmt, "Couldn't allocate memory for tidbuf bind.", (ret = SQL_ERROR));
	for (i = 0; i < size_of_rowset; i++)
	{
		PG_BM	pg_bm;
		SQLLEN	bidx;

		pg_bm = SC_Resolve_bookmark(opts, i);
		bidx = pg_bm.index;

mylog("i=%d bidx=%d cached=%d\n", i, bidx, res->num_cached_keys);
		kres_ridx = GIdx2KResIdx(bidx, stmt, res);
		if (kres_ridx < 0 || kres_ridx >= res->num_cached_keys)
		{
			if (pg_bm.keys.offset > 0)
			{
				QR_get_last_bookmark(res, bidx, &pg_bm.keys);
				blocknum = pg_bm.keys.blocknum;
				offset = pg_bm.keys.offset;
				oidint = pg_bm.keys.oid;
			}
			else
			{
				SC_set_error(stmt, STMT_ROW_OUT_OF_RANGE, "the target rows is out of the rowset", __FUNCTION__);
				goto cleanup;
			}
		}
		else
		{
			if (!(oidint = getOid(res, kres_ridx)))
			{
				if (!strcmp(SAFE_NAME(stmt->ti[0]->bestitem), OID_NAME))
				{
					SC_set_error(stmt, STMT_ROW_VERSION_CHANGED, "the row was already deleted ?", __FUNCTION__);
				}
			}
			getTid(res, kres_ridx, &blocknum, &offset);
		}
		snprintf(tidbuf + i * tidbuflen, tidbuflen, "(%u,%u)", blocknum, offset);
		mylog("!!!! tidbuf=%s\n", tidbuf + i * tidbuflen);
	}
	if (!SQL_SUCCEEDED(PGAPI_BindParameter(hstmt, 1, SQL_PARAM_INPUT,
			SQL_C_CHAR, SQL_CHAR, tidbuflen, 0,
			tidbuf, tidbuflen, NULL)))
		goto cleanup;
	apdf = SC_get_APDF((StatementClass *) hstmt);
	apdf->paramset_size = size_of_rowset;
	if (!SQL_SUCCEEDED(PGAPI_GetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, (SQLPOINTER) &hdesc, SQL_IS_POINTER, NULL)))
		goto cleanup;
	if (!SQL_SUCCEEDED(PGAPI_SetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, (SQLPOINTER) hdesc, SQL_IS_POINTER)))
		goto cleanup;

	lodlen = strlen(stmt->load_statement) + 15;
	SC_MALLOC_gexit_with_error(query, char, lodlen, stmt, "Couldn't allocate memory for query buf.", (ret = SQL_ERROR));
	snprintf(query, lodlen, "%s where ctid=?", stmt->load_statement);
	if (!SQL_SUCCEEDED(ret = PGAPI_ExecDirect(hstmt, (SQLCHAR *) query, SQL_NTS, PODBC_RDONLY)))
		goto cleanup;
	/*
	 * Combine multiple results into one
	 */
	fstmt = (StatementClass *) hstmt;
	res = SC_get_Result(fstmt);
	num_fields = QR_NumResultCols(res);
	cached_rows = QR_get_num_cached_tuples(res);
	if (size_of_rowset > res->count_backend_allocated)
	{
		SC_REALLOC_gexit_with_error(res->backend_tuples, TupleField, size_of_rowset * sizeof(TupleField) * num_fields, hstmt, "Couldn't realloc memory for backend.", (ret = SQL_ERROR));
		res->count_backend_allocated = size_of_rowset;
	}
	memset(res->backend_tuples + num_fields * cached_rows, 0, (size_of_rowset - cached_rows) * num_fields * sizeof(TupleField));
	QR_set_num_cached_rows(res, size_of_rowset);
	res->num_total_read = size_of_rowset;
	rowStatusArray = (SC_get_IRDF(stmt))->rowStatusArray;
	for (i = 0, qres = res; i < size_of_rowset && NULL != qres; i++, qres = qres->next)
	{
		if (1 == QR_get_num_cached_tuples(qres))
		{
			otuple = res->backend_tuples + i * num_fields;
			ituple = qres->backend_tuples;
			if (otuple != ituple)
				MoveCachedRows(otuple, ituple, num_fields, 1);
			if (NULL != rowStatusArray)
				rowStatusArray[i] = SQL_ROW_SUCCESS;
		}
		else if (NULL != rowStatusArray)
			rowStatusArray[i] = SQL_ROW_DELETED;
	}

	/* Fetch and fill bind info */
	cRow = 0;
	opts->bookmark = NULL;
	ret = PGAPI_ExtendedFetch(fstmt, SQL_FETCH_NEXT, 0,
		&cRow, NULL, 0, size_of_rowset);
	mylog("%s cRow=" FORMAT_ULEN "\n", __FUNCTION__, cRow);

cleanup:
	if (NULL != hstmt)
	{
		PGAPI_SetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, (SQLPOINTER) NULL, SQL_IS_POINTER);
		PGAPI_FreeStmt(hstmt, SQL_DROP);
	}
	opts->bookmark = bookmark_orig;
	if (NULL != tidbuf)
		free(tidbuf);
	if (NULL != query)
		free(query);

	return ret;
}
