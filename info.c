/*--------
 * Module:			info.c
 *
 * Description:		This module contains routines related to
 *					ODBC informational functions.
 *
 * Classes:			n/a
 *
 * API functions:	SQLGetInfo, SQLGetTypeInfo, SQLGetFunctions,
 *					SQLTables, SQLColumns, SQLStatistics, SQLSpecialColumns,
 *					SQLPrimaryKeys, SQLForeignKeys,
 *					SQLProcedureColumns(NI), SQLProcedures,
 *					SQLTablePrivileges, SQLColumnPrivileges(NI)
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *--------
 */

#include "psqlodbc.h"

#include <string.h>
#include <stdio.h>

#ifndef WIN32
#include <ctype.h>
#endif

#include "tuple.h"
#include "pgtypes.h"
#include "dlg_specific.h"

#include "environ.h"

#ifdef USE_LIBPQ
#include "libpqconnection.h"
#else
#include "connection.h"
#endif /* USE_LIBPQ */

#include "statement.h"
#include "qresult.h"
#include "bind.h"
#include "misc.h"
#include "pgtypes.h"
#include "pgapifunc.h"
#include "multibyte.h"


/*	Trigger related stuff for SQLForeign Keys */
#define TRIGGER_SHIFT 3
#define TRIGGER_MASK   0x03
#define TRIGGER_DELETE 0x01
#define TRIGGER_UPDATE 0x02

#define	NULL_IF_NULL(a) ((a) ? (const char*)(a) : "(NULL)")

/* extern GLOBAL_VALUES globals; */

CSTR	pubstr = "public";


RETCODE		SQL_API
PGAPI_GetInfo(
			  HDBC hdbc,
			  UWORD fInfoType,
			  PTR rgbInfoValue,
			  SWORD cbInfoValueMax,
			  SWORD FAR * pcbInfoValue)
{
	CSTR func = "PGAPI_GetInfo";
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	ConnInfo   *ci;
	char	   *p = NULL,
				tmp[MAX_INFO_STRING];
	int			len = 0,
				value = 0;
	RETCODE		result;

	mylog("%s: entering...fInfoType=%d\n", func, fInfoType);

	if (!conn)
	{
		CC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	ci = &(conn->connInfo);

	switch (fInfoType)
	{
		case SQL_ACCESSIBLE_PROCEDURES: /* ODBC 1.0 */
			p = "N";
			break;

		case SQL_ACCESSIBLE_TABLES:		/* ODBC 1.0 */
			p = "N";
			break;

		case SQL_ACTIVE_CONNECTIONS:	/* ODBC 1.0 */
			len = 2;
			value = MAX_CONNECTIONS;
			break;

		case SQL_ACTIVE_STATEMENTS:		/* ODBC 1.0 */
			len = 2;
			value = 0;
			break;

		case SQL_ALTER_TABLE:	/* ODBC 2.0 */
			len = 4;
			value = SQL_AT_ADD_COLUMN;
			if (PG_VERSION_GE(conn, 7.3))
				value |= SQL_AT_DROP_COLUMN;
			value |= SQL_AT_ADD_COLUMN_SINGLE;
			if (PG_VERSION_GE(conn, 7.1))
				value |= SQL_AT_ADD_CONSTRAINT
					| SQL_AT_ADD_TABLE_CONSTRAINT
					| SQL_AT_CONSTRAINT_INITIALLY_DEFERRED
					| SQL_AT_CONSTRAINT_INITIALLY_IMMEDIATE
					| SQL_AT_CONSTRAINT_DEFERRABLE;
			if (PG_VERSION_GE(conn, 7.3))
				value |= SQL_AT_DROP_TABLE_CONSTRAINT_RESTRICT
					| SQL_AT_DROP_TABLE_CONSTRAINT_CASCADE
					| SQL_AT_DROP_COLUMN_RESTRICT
					| SQL_AT_DROP_COLUMN_CASCADE;
			break;

		case SQL_BOOKMARK_PERSISTENCE:	/* ODBC 2.0 */
			/* very simple bookmark support */
			len = 4;
			value = ci->drivers.use_declarefetch ? 0 : (SQL_BP_SCROLL | SQL_BP_DELETE | SQL_BP_UPDATE | SQL_BP_TRANSACTION);
			break;

		case SQL_COLUMN_ALIAS:	/* ODBC 2.0 */
			p = "N";
			break;

		case SQL_CONCAT_NULL_BEHAVIOR:	/* ODBC 1.0 */
			len = 2;
			value = SQL_CB_NON_NULL;
			break;

		case SQL_CONVERT_BIGINT:
		case SQL_CONVERT_BINARY:
		case SQL_CONVERT_BIT:
		case SQL_CONVERT_CHAR:
		case SQL_CONVERT_DATE:
		case SQL_CONVERT_DECIMAL:
		case SQL_CONVERT_DOUBLE:
		case SQL_CONVERT_FLOAT:
		case SQL_CONVERT_INTEGER:
		case SQL_CONVERT_LONGVARBINARY:
		case SQL_CONVERT_LONGVARCHAR:
		case SQL_CONVERT_NUMERIC:
		case SQL_CONVERT_REAL:
		case SQL_CONVERT_SMALLINT:
		case SQL_CONVERT_TIME:
		case SQL_CONVERT_TIMESTAMP:
		case SQL_CONVERT_TINYINT:
		case SQL_CONVERT_VARBINARY:
		case SQL_CONVERT_VARCHAR:		/* ODBC 1.0 */
			len = 4;
			value = fInfoType;
			break;

		case SQL_CONVERT_FUNCTIONS:		/* ODBC 1.0 */
			len = 4;
			value = 0;
			break;

		case SQL_CORRELATION_NAME:		/* ODBC 1.0 */

			/*
			 * Saying no correlation name makes Query not work right.
			 * value = SQL_CN_NONE;
			 */
			len = 2;
			value = SQL_CN_ANY;
			break;

		case SQL_CURSOR_COMMIT_BEHAVIOR:		/* ODBC 1.0 */
			len = 2;
			value = SQL_CB_CLOSE;
			if (!ci->drivers.use_declarefetch)
				value = SQL_CB_PRESERVE;
			break;

		case SQL_CURSOR_ROLLBACK_BEHAVIOR:		/* ODBC 1.0 */
			len = 2;
			value = SQL_CB_CLOSE;
			if (!ci->drivers.use_declarefetch)
				value = SQL_CB_PRESERVE;
			break;

		case SQL_DATA_SOURCE_NAME:		/* ODBC 1.0 */
			p = CC_get_DSN(conn);
			break;

		case SQL_DATA_SOURCE_READ_ONLY: /* ODBC 1.0 */
			p = CC_is_onlyread(conn) ? "Y" : "N";
			break;

		case SQL_DATABASE_NAME:	/* Support for old ODBC 1.0 Apps */

			/*
			 * Returning the database name causes problems in MS Query. It
			 * generates query like: "SELECT DISTINCT a FROM byronnbad3
			 * bad3"
			 *
			 * p = CC_get_database(conn);
			 */
			p = "";
			break;

		case SQL_DBMS_NAME:		/* ODBC 1.0 */
			p = DBMS_NAME;
			break;

		case SQL_DBMS_VER:		/* ODBC 1.0 */

			/*
			 * The ODBC spec wants ##.##.#### ...whatever... so prepend
			 * the driver
			 */
			/* version number to the dbms version string */
			snprintf(tmp, sizeof(tmp) - 1, "%s %s", POSTGRESDRIVERVERSION, conn->pg_version);
                        tmp[sizeof(tmp) - 1] = '\0';
			p = tmp;
			break;

		case SQL_DEFAULT_TXN_ISOLATION: /* ODBC 1.0 */
			len = 4;
			if (PG_VERSION_LT(conn, 6.5))
				value = SQL_TXN_SERIALIZABLE;
			else
				value = SQL_TXN_READ_COMMITTED;
			break;

		case SQL_DRIVER_NAME:	/* ODBC 1.0 */
			p = DRIVER_FILE_NAME;
			break;

		case SQL_DRIVER_ODBC_VER:
			p = DRIVER_ODBC_VER;
			break;

		case SQL_DRIVER_VER:	/* ODBC 1.0 */
			p = POSTGRESDRIVERVERSION;
			break;

		case SQL_EXPRESSIONS_IN_ORDERBY:		/* ODBC 1.0 */
			p = "N";
			break;

		case SQL_FETCH_DIRECTION:		/* ODBC 1.0 */
			len = 4;
			value = ci->drivers.use_declarefetch ? (SQL_FD_FETCH_NEXT) :
			 (SQL_FD_FETCH_NEXT | 
			 SQL_FD_FETCH_FIRST |
			 SQL_FD_FETCH_LAST |
			 SQL_FD_FETCH_PRIOR |
			 SQL_FD_FETCH_ABSOLUTE |
			 SQL_FD_FETCH_RELATIVE |
			 SQL_FD_FETCH_BOOKMARK);
			break;

		case SQL_FILE_USAGE:	/* ODBC 2.0 */
			len = 2;
			value = SQL_FILE_NOT_SUPPORTED;
			break;

		case SQL_GETDATA_EXTENSIONS:	/* ODBC 2.0 */
			len = 4;
			value = (SQL_GD_ANY_COLUMN | SQL_GD_ANY_ORDER | SQL_GD_BOUND | SQL_GD_BLOCK);
			break;

		case SQL_GROUP_BY:		/* ODBC 2.0 */
			len = 2;
			value = SQL_GB_GROUP_BY_EQUALS_SELECT;
			break;

		case SQL_IDENTIFIER_CASE:		/* ODBC 1.0 */

			/*
			 * are identifiers case-sensitive (yes, but only when quoted.
			 * If not quoted, they default to lowercase)
			 */
			len = 2;
			value = SQL_IC_LOWER;
			break;

		case SQL_IDENTIFIER_QUOTE_CHAR: /* ODBC 1.0 */
			/* the character used to quote "identifiers" */
			p = PG_VERSION_LE(conn, 6.2) ? " " : "\"";
			break;

		case SQL_KEYWORDS:		/* ODBC 2.0 */
			p = "";
			break;

		case SQL_LIKE_ESCAPE_CLAUSE:	/* ODBC 2.0 */

			/*
			 * is there a character that escapes '%' and '_' in a LIKE
			 * clause? not as far as I can tell
			 */
			p = "N";
			break;

		case SQL_LOCK_TYPES:	/* ODBC 2.0 */
			len = 4;
			value = ci->drivers.lie ? (SQL_LCK_NO_CHANGE | SQL_LCK_EXCLUSIVE | SQL_LCK_UNLOCK) : SQL_LCK_NO_CHANGE;
			break;

		case SQL_MAX_BINARY_LITERAL_LEN:		/* ODBC 2.0 */
			len = 4;
			value = 0;
			break;

		case SQL_MAX_CHAR_LITERAL_LEN:	/* ODBC 2.0 */
			len = 4;
			value = 0;
			break;

		case SQL_MAX_COLUMN_NAME_LEN:	/* ODBC 1.0 */
			len = 2;
#ifdef	MAX_COLUMN_LEN
			value = MAX_COLUMN_LEN;
#endif /* MAX_COLUMN_LEN */
			if (0 == value)
			{
				if (PG_VERSION_GE(conn, 7.3))
					value = NAMEDATALEN_V73;
				else
					value = NAMEDATALEN_V72;
			}
			break;

		case SQL_MAX_COLUMNS_IN_GROUP_BY:		/* ODBC 2.0 */
			len = 2;
			value = 0;
			break;

		case SQL_MAX_COLUMNS_IN_INDEX:	/* ODBC 2.0 */
			len = 2;
			value = 0;
			break;

		case SQL_MAX_COLUMNS_IN_ORDER_BY:		/* ODBC 2.0 */
			len = 2;
			value = 0;
			break;

		case SQL_MAX_COLUMNS_IN_SELECT: /* ODBC 2.0 */
			len = 2;
			value = 0;
			break;

		case SQL_MAX_COLUMNS_IN_TABLE:	/* ODBC 2.0 */
			len = 2;
			value = 0;
			break;

		case SQL_MAX_CURSOR_NAME_LEN:	/* ODBC 1.0 */
			len = 2;
			value = MAX_CURSOR_LEN;
			break;

		case SQL_MAX_INDEX_SIZE:		/* ODBC 2.0 */
			len = 4;
			value = 0;
			break;

		case SQL_MAX_OWNER_NAME_LEN:	/* ODBC 1.0 */
			len = 2;
			value = 0;
#ifdef	MAX_SCHEMA_LEN
			if (conn->schema_support)
				value = MAX_SCHEMA_LEN;
#endif /* MAX_SCHEMA_LEN */
			if (0 == value)
			{
				if (PG_VERSION_GE(conn, 7.3))
					value = NAMEDATALEN_V73;
			}
			break;

		case SQL_MAX_PROCEDURE_NAME_LEN:		/* ODBC 1.0 */
			len = 2;
			value = 0;
			break;

		case SQL_MAX_QUALIFIER_NAME_LEN:		/* ODBC 1.0 */
			len = 2;
			value = 0;
			break;

		case SQL_MAX_ROW_SIZE:	/* ODBC 2.0 */
			len = 4;
			if (PG_VERSION_GE(conn, 7.1))
			{
				/* Large Rowa in 7.1+ */
				value = MAX_ROW_SIZE;
			}
			else
			{
				/* Without the Toaster we're limited to the blocksize */
				value = BLCKSZ;
			}
			break;

		case SQL_MAX_ROW_SIZE_INCLUDES_LONG:	/* ODBC 2.0 */

			/*
			 * does the preceding value include LONGVARCHAR and
			 * LONGVARBINARY fields?   Well, it does include longvarchar,
			 * but not longvarbinary.
			 */
			p = "Y";
			break;

		case SQL_MAX_STATEMENT_LEN:		/* ODBC 2.0 */
			/* maybe this should be 0? */
			len = 4;
			value = CC_get_max_query_len(conn);
			break;

		case SQL_MAX_TABLE_NAME_LEN:	/* ODBC 1.0 */
			len = 2;
#ifdef	MAX_TABLE_LEN
			value = MAX_TABLE_LEN;
#endif /* MAX_TABLE_LEN */
			if (0 == value)
			{
				if (PG_VERSION_GE(conn, 7.3))
					value = NAMEDATALEN_V73;
				else
					value = NAMEDATALEN_V72;
			}
			break;

		case SQL_MAX_TABLES_IN_SELECT:	/* ODBC 2.0 */
			len = 2;
			value = 0;
			break;

		case SQL_MAX_USER_NAME_LEN:
			len = 2;
			value = 0;
			break;

		case SQL_MULT_RESULT_SETS:		/* ODBC 1.0 */
			/* Don't support multiple result sets but say yes anyway? */
			p = "Y";
			break;

		case SQL_MULTIPLE_ACTIVE_TXN:	/* ODBC 1.0 */
			p = "Y";
			break;

		case SQL_NEED_LONG_DATA_LEN:	/* ODBC 2.0 */

			/*
			 * Don't need the length, SQLPutData can handle any size and
			 * multiple calls
			 */
			p = "N";
			break;

		case SQL_NON_NULLABLE_COLUMNS:	/* ODBC 1.0 */
			len = 2;
			value = SQL_NNC_NON_NULL;
			break;

		case SQL_NULL_COLLATION:		/* ODBC 2.0 */
			/* where are nulls sorted? */
			len = 2;
			value = SQL_NC_END;
			break;

		case SQL_NUMERIC_FUNCTIONS:		/* ODBC 1.0 */
			len = 4;
			value = 0;
			break;

		case SQL_ODBC_API_CONFORMANCE:	/* ODBC 1.0 */
			len = 2;
			value = SQL_OAC_LEVEL1;
			break;

		case SQL_ODBC_SAG_CLI_CONFORMANCE:		/* ODBC 1.0 */
			len = 2;
			value = SQL_OSCC_NOT_COMPLIANT;
			break;

		case SQL_ODBC_SQL_CONFORMANCE:	/* ODBC 1.0 */
			len = 2;
			value = SQL_OSC_CORE;
			break;

		case SQL_ODBC_SQL_OPT_IEF:		/* ODBC 1.0 */
			p = "N";
			break;

		case SQL_OJ_CAPABILITIES:		/* ODBC 2.01 */
			len = 4;
			if (PG_VERSION_GE(conn, 7.1))
			{
				/* OJs in 7.1+ */
				value = (SQL_OJ_LEFT |
						 SQL_OJ_RIGHT |
						 SQL_OJ_FULL |
						 SQL_OJ_NESTED |
						 SQL_OJ_NOT_ORDERED |
						 SQL_OJ_INNER |
						 SQL_OJ_ALL_COMPARISON_OPS);
			}
			else
				/* OJs not in <7.1 */
				value = 0;
			break;

		case SQL_ORDER_BY_COLUMNS_IN_SELECT:	/* ODBC 2.0 */
			p = (PG_VERSION_LE(conn, 6.3)) ? "Y" : "N";
			break;

		case SQL_OUTER_JOINS:	/* ODBC 1.0 */
			if (PG_VERSION_GE(conn, 7.1))
				/* OJs in 7.1+ */
				p = "Y";
			else
				/* OJs not in <7.1 */
				p = "N";
			break;

		case SQL_OWNER_TERM:	/* ODBC 1.0 */
			if (conn->schema_support)
				p = "schema";
			else
				p = "owner";
			break;

		case SQL_OWNER_USAGE:	/* ODBC 2.0 */
			len = 4;
			value = 0;
			if (conn->schema_support)
				value = SQL_OU_DML_STATEMENTS
					| SQL_OU_TABLE_DEFINITION
					| SQL_OU_INDEX_DEFINITION
					| SQL_OU_PRIVILEGE_DEFINITION
					;
			break;

		case SQL_POS_OPERATIONS:		/* ODBC 2.0 */
			len = 4;
			value = (SQL_POS_POSITION | SQL_POS_REFRESH);
#ifdef	DRIVER_CURSOR_IMPLEMENT
			if (ci->updatable_cursors)
				value |= (SQL_POS_UPDATE | SQL_POS_DELETE | SQL_POS_ADD);
#endif /* DRIVER_CURSOR_IMPLEMENT */
			break;

		case SQL_POSITIONED_STATEMENTS: /* ODBC 2.0 */
			len = 4;
			value = ci->drivers.lie ? (SQL_PS_POSITIONED_DELETE |
									   SQL_PS_POSITIONED_UPDATE |
									   SQL_PS_SELECT_FOR_UPDATE) : 0;
			break;

		case SQL_PROCEDURE_TERM:		/* ODBC 1.0 */
			p = "procedure";
			break;

		case SQL_PROCEDURES:	/* ODBC 1.0 */
			p = "Y";
			break;

		case SQL_QUALIFIER_LOCATION:	/* ODBC 2.0 */
			len = 2;
			value = SQL_QL_START;
			break;

		case SQL_QUALIFIER_NAME_SEPARATOR:		/* ODBC 1.0 */
			p = "";
			break;

		case SQL_QUALIFIER_TERM:		/* ODBC 1.0 */
			p = "";
			break;

		case SQL_QUALIFIER_USAGE:		/* ODBC 2.0 */
			len = 4;
			value = 0;
			break;

		case SQL_QUOTED_IDENTIFIER_CASE:		/* ODBC 2.0 */
			/* are "quoted" identifiers case-sensitive?  YES! */
			len = 2;
			value = SQL_IC_SENSITIVE;
			break;

		case SQL_ROW_UPDATES:	/* ODBC 1.0 */

			/*
			 * Driver doesn't support keyset-driven or mixed cursors, so
			 * not much point in saying row updates are supported
			 */
			p = (ci->updatable_cursors) ? "Y" : "N";
			break;

		case SQL_SCROLL_CONCURRENCY:	/* ODBC 1.0 */
			len = 4;
			value = SQL_SCCO_READ_ONLY;
#ifdef	DRIVER_CURSOR_IMPLEMENT
			if (ci->updatable_cursors)
				value |= SQL_SCCO_OPT_ROWVER;
#endif /* DRIVER_CURSOR_IMPLEMENT */
			if (ci->drivers.lie)
				value |= (SQL_SCCO_LOCK | SQL_SCCO_OPT_VALUES);
			break;

		case SQL_SCROLL_OPTIONS:		/* ODBC 1.0 */
			len = 4;
			value = SQL_SO_FORWARD_ONLY;
#ifdef	DECLAREFETCH_FORWARDONLY
			if (!ci->drivers.use_declarefetch)
#endif /* DECLAREFETCH_FORWARDONLY */
				value |= SQL_SO_STATIC;
			if (ci->updatable_cursors)
				value |= SQL_SO_KEYSET_DRIVEN;
			if (ci->drivers.lie)
				value |= (SQL_SO_DYNAMIC | SQL_SO_MIXED);
			break;

		case SQL_SEARCH_PATTERN_ESCAPE: /* ODBC 1.0 */
			if (PG_VERSION_GE(conn, 6.5))
				p = "\\";
			else
				p = "";
			break;

		case SQL_SERVER_NAME:	/* ODBC 1.0 */
			p = CC_get_server(conn);
			break;

		case SQL_SPECIAL_CHARACTERS:	/* ODBC 2.0 */
			p = "_";
			break;

		case SQL_STATIC_SENSITIVITY:	/* ODBC 2.0 */
			len = 4;
			value = 0;
#ifdef	DRIVER_CURSOR_IMPLEMENT
			if (ci->updatable_cursors)
				value |= (SQL_SS_ADDITIONS | SQL_SS_DELETIONS | SQL_SS_UPDATES);
#endif /* DRIVER_CURSOR_IMPLEMENT */
			break;

		case SQL_STRING_FUNCTIONS:		/* ODBC 1.0 */
			len = 4;
			value = (SQL_FN_STR_CONCAT |
					 SQL_FN_STR_LCASE |
					 SQL_FN_STR_LENGTH |
					 SQL_FN_STR_LOCATE |
					 SQL_FN_STR_LTRIM |
					 SQL_FN_STR_RTRIM |
					 SQL_FN_STR_SUBSTRING |
					 SQL_FN_STR_UCASE);
			break;

		case SQL_SUBQUERIES:	/* ODBC 2.0 */
			/* postgres 6.3 supports subqueries */
			len = 4;
			value = (SQL_SQ_QUANTIFIED |
					 SQL_SQ_IN |
					 SQL_SQ_EXISTS |
					 SQL_SQ_COMPARISON);
			break;

		case SQL_SYSTEM_FUNCTIONS:		/* ODBC 1.0 */
			len = 4;
			value = 0;
			break;

		case SQL_TABLE_TERM:	/* ODBC 1.0 */
			p = "table";
			break;

		case SQL_TIMEDATE_ADD_INTERVALS:		/* ODBC 2.0 */
			len = 4;
			value = 0;
			break;

		case SQL_TIMEDATE_DIFF_INTERVALS:		/* ODBC 2.0 */
			len = 4;
			value = 0;
			break;

		case SQL_TIMEDATE_FUNCTIONS:	/* ODBC 1.0 */
			len = 4;
			value = (SQL_FN_TD_NOW);
			break;

		case SQL_TXN_CAPABLE:	/* ODBC 1.0 */

			/*
			 * Postgres can deal with create or drop table statements in a
			 * transaction
			 */
			len = 2;
			value = SQL_TC_ALL;
			break;

		case SQL_TXN_ISOLATION_OPTION:	/* ODBC 1.0 */
			len = 4;
			if (PG_VERSION_LT(conn, 6.5))
				value = SQL_TXN_SERIALIZABLE;
			else if (PG_VERSION_GE(conn, 7.1))
				value = SQL_TXN_READ_COMMITTED | SQL_TXN_SERIALIZABLE;
			else
				value = SQL_TXN_READ_COMMITTED;
			break;

		case SQL_UNION: /* ODBC 2.0 */
			/* unions with all supported in postgres 6.3 */
			len = 4;
			value = (SQL_U_UNION | SQL_U_UNION_ALL);
			break;

		case SQL_USER_NAME:		/* ODBC 1.0 */
			p = CC_get_username(conn);
			break;

		default:
			/* unrecognized key */
			CC_set_error(conn, CONN_NOT_IMPLEMENTED_ERROR, "Unrecognized key passed to PGAPI_GetInfo.");
			return SQL_ERROR;
	}

	result = SQL_SUCCESS;

	/*
	 * NOTE, that if rgbInfoValue is NULL, then no warnings or errors
	 * should result and just pcbInfoValue is returned, which indicates
	 * what length would be required if a real buffer had been passed in.
	 */
	if (p)
	{
		/* char/binary data */
		len = strlen(p);
#ifdef  UNICODE_SUPPORT                
                /* Note that at this point we don't know if we've been called just
                 * to get the length of the output. If it's unicode, then we better
                 * adjust to bytes now, so we don't return a buffer size that's too
                 * small.
                 */
                if (conn->unicode)
                    len = len * WCLEN;
#endif

		if (rgbInfoValue)
		{
#ifdef  UNICODE_SUPPORT
			if (conn->unicode)
				len = utf8_to_ucs2(p, len, (SQLWCHAR *) rgbInfoValue, cbInfoValueMax / 2);
			else
#endif
				strncpy_null((char *) rgbInfoValue, p, (size_t) cbInfoValueMax);

			if (len >= cbInfoValueMax)
			{
				result = SQL_SUCCESS_WITH_INFO;
				CC_set_error(conn, CONN_TRUNCATED, "The buffer was too small for the InfoValue.");
			}
		}
	}
	else
	{
		/* numeric data */
		if (rgbInfoValue)
		{
			if (len == 2)
				*((WORD *) rgbInfoValue) = (WORD) value;
			else if (len == 4)
				*((DWORD *) rgbInfoValue) = (DWORD) value;
		}
	}


	if (pcbInfoValue)
		*pcbInfoValue = len;		
	
	mylog("%s: p='%s', len=%d, value=%d, cbMax=%d\n", func, p ? p : "<NULL>", len, value, cbInfoValueMax);
	
	return result;
}


RETCODE		SQL_API
PGAPI_GetTypeInfo(
				  HSTMT hstmt,
				  SWORD fSqlType)
{
	CSTR func = "PGAPI_GetTypeInfo";
	StatementClass *stmt = (StatementClass *) hstmt;
	QResultClass	*res;
	TupleNode  *row;
	int			i, result_cols;

	/* Int4 type; */
	Int4		pgType;
	Int2		sqlType;
	RETCODE		result;

	mylog("%s: entering...fSqlType = %d\n", func, fSqlType);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	stmt->manual_result = TRUE;
	if (res = QR_Constructor(), !res)
	{
		SC_log_error(func, "Error creating result.", stmt);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	result_cols = 19;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	QR_set_num_fields(res, result_cols);
	QR_set_field_info(res, 0, "TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 1, "DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 2, "PRECISION", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 3, "LITERAL_PREFIX", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 4, "LITERAL_SUFFIX", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 5, "CREATE_PARAMS", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 6, "NULLABLE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 7, "CASE_SENSITIVE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 8, "SEARCHABLE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 9, "UNSIGNED_ATTRIBUTE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 10, "MONEY", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 11, "AUTO_INCREMENT", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 12, "LOCAL_TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 13, "MINIMUM_SCALE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 14, "MAXIMUM_SCALE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 15, "SQL_DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 16, "SQL_DATATIME_SUB", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 17, "NUM_PREC_RADIX", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 18, "INTERVAL_PRECISION", PG_TYPE_INT2, 2);

	for (i = 0, sqlType = sqlTypes[0]; sqlType; sqlType = sqlTypes[++i])
	{
		pgType = sqltype_to_pgtype(stmt, sqlType);

		if (fSqlType == SQL_ALL_TYPES || fSqlType == sqlType)
		{
			int	pgtcount = 1, cnt;

			if (SQL_INTEGER == sqlType && PG_VERSION_GE(SC_get_conn(stmt), 6.4))
				pgtcount = 2;
			for (cnt = 0; cnt < pgtcount; cnt ++)
			{

			SC_MALLOC_return_with_error(row, TupleNode, (sizeof(TupleNode) + (result_cols - 1) * sizeof(TupleField)),
				stmt, "Couldn't alloc row", SQL_ERROR)

			/* These values can't be NULL */
			if (1 == cnt)
			{
				set_tuplefield_string(&row->tuple[0], "serial");
				set_tuplefield_int2(&row->tuple[6], SQL_NO_NULLS);
inolog("serial in\n");
			}
			else
			{
				set_tuplefield_string(&row->tuple[0], pgtype_to_name(stmt, pgType));
				set_tuplefield_int2(&row->tuple[6], pgtype_nullable(stmt, pgType));
			}
			set_tuplefield_int2(&row->tuple[1], (Int2) sqlType);
			set_tuplefield_int2(&row->tuple[7], pgtype_case_sensitive(stmt, pgType));
			set_tuplefield_int2(&row->tuple[8], pgtype_searchable(stmt, pgType));
			set_tuplefield_int2(&row->tuple[10], pgtype_money(stmt, pgType));

			/*
			 * Localized data-source dependent data type name (always
			 * NULL)
			 */
			set_tuplefield_null(&row->tuple[12]);

			/* These values can be NULL */
			set_nullfield_int4(&row->tuple[2], pgtype_column_size(stmt, pgType, PG_STATIC, PG_STATIC));
			set_nullfield_string(&row->tuple[3], pgtype_literal_prefix(stmt, pgType));
			set_nullfield_string(&row->tuple[4], pgtype_literal_suffix(stmt, pgType));
			set_nullfield_string(&row->tuple[5], pgtype_create_params(stmt, pgType));
			if (1 == cnt)
			{
				set_nullfield_int2(&row->tuple[9], TRUE);
				set_nullfield_int2(&row->tuple[11], TRUE);
			}
			else
			{
				set_nullfield_int2(&row->tuple[9], pgtype_unsigned(stmt, pgType));
				set_nullfield_int2(&row->tuple[11], pgtype_auto_increment(stmt, pgType));
			}
			set_nullfield_int2(&row->tuple[13], pgtype_min_decimal_digits(stmt, pgType));
			set_nullfield_int2(&row->tuple[14], pgtype_max_decimal_digits(stmt, pgType));
			set_nullfield_int2(&row->tuple[15], pgtype_to_sqldesctype(stmt, pgType, PG_STATIC));
			set_nullfield_int2(&row->tuple[16], pgtype_to_datetime_sub(stmt, pgType));
			set_nullfield_int4(&row->tuple[17], pgtype_radix(stmt, pgType));
			set_nullfield_int4(&row->tuple[18], 0);

			QR_add_tuple(res, row);

			}
		}
	}

	stmt->status = STMT_FINISHED;
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);

	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_GetFunctions(
				   HDBC hdbc,
				   UWORD fFunction,
				   UWORD FAR * pfExists)
{
	CSTR func = "PGAPI_GetFunctions";
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	ConnInfo   *ci = &(conn->connInfo);

	mylog("%s: entering...%u\n", func, fFunction);

	if (fFunction == SQL_API_ALL_FUNCTIONS)
	{
		memset(pfExists, 0, sizeof(UWORD) * 100);

		/* ODBC core functions */
		pfExists[SQL_API_SQLALLOCCONNECT] = TRUE;
		pfExists[SQL_API_SQLALLOCENV] = TRUE;
		pfExists[SQL_API_SQLALLOCSTMT] = TRUE;
		pfExists[SQL_API_SQLBINDCOL] = TRUE;
		pfExists[SQL_API_SQLCANCEL] = TRUE;
		pfExists[SQL_API_SQLCOLATTRIBUTES] = TRUE;
		pfExists[SQL_API_SQLCONNECT] = TRUE;
		pfExists[SQL_API_SQLDESCRIBECOL] = TRUE;		/* partial */
		pfExists[SQL_API_SQLDISCONNECT] = TRUE;
		pfExists[SQL_API_SQLERROR] = TRUE;
		pfExists[SQL_API_SQLEXECDIRECT] = TRUE;
		pfExists[SQL_API_SQLEXECUTE] = TRUE;
		pfExists[SQL_API_SQLFETCH] = TRUE;
		pfExists[SQL_API_SQLFREECONNECT] = TRUE;
		pfExists[SQL_API_SQLFREEENV] = TRUE;
		pfExists[SQL_API_SQLFREESTMT] = TRUE;
		pfExists[SQL_API_SQLGETCURSORNAME] = TRUE;
		pfExists[SQL_API_SQLNUMRESULTCOLS] = TRUE;
		pfExists[SQL_API_SQLPREPARE] = TRUE;			/* complete? */
		pfExists[SQL_API_SQLROWCOUNT] = TRUE;
		pfExists[SQL_API_SQLSETCURSORNAME] = TRUE;
		pfExists[SQL_API_SQLSETPARAM] = FALSE;			/* odbc 1.0 */
		pfExists[SQL_API_SQLTRANSACT] = TRUE;

		/* ODBC level 1 functions */
		pfExists[SQL_API_SQLBINDPARAMETER] = TRUE;
		pfExists[SQL_API_SQLCOLUMNS] = TRUE;
		pfExists[SQL_API_SQLDRIVERCONNECT] = TRUE;
		pfExists[SQL_API_SQLGETCONNECTOPTION] = TRUE;		/* partial */
		pfExists[SQL_API_SQLGETDATA] = TRUE;
		pfExists[SQL_API_SQLGETFUNCTIONS] = TRUE;
		pfExists[SQL_API_SQLGETINFO] = TRUE;
		pfExists[SQL_API_SQLGETSTMTOPTION] = TRUE;		/* partial */
		pfExists[SQL_API_SQLGETTYPEINFO] = TRUE;
		pfExists[SQL_API_SQLPARAMDATA] = TRUE;
		pfExists[SQL_API_SQLPUTDATA] = TRUE;
		pfExists[SQL_API_SQLSETCONNECTOPTION] = TRUE;		/* partial */
		pfExists[SQL_API_SQLSETSTMTOPTION] = TRUE;
		pfExists[SQL_API_SQLSPECIALCOLUMNS] = TRUE;
		pfExists[SQL_API_SQLSTATISTICS] = TRUE;
		pfExists[SQL_API_SQLTABLES] = TRUE;

		/* ODBC level 2 functions */
		pfExists[SQL_API_SQLBROWSECONNECT] = FALSE;
		pfExists[SQL_API_SQLCOLUMNPRIVILEGES] = FALSE;
		pfExists[SQL_API_SQLDATASOURCES] = FALSE;		/* only implemented by DM */
		pfExists[SQL_API_SQLDESCRIBEPARAM] = FALSE;		/* not properly implemented */
		pfExists[SQL_API_SQLDRIVERS] = FALSE;			/* only implemented by DM */
		pfExists[SQL_API_SQLEXTENDEDFETCH] = TRUE;
		pfExists[SQL_API_SQLFOREIGNKEYS] = TRUE;
		pfExists[SQL_API_SQLMORERESULTS] = TRUE;
		pfExists[SQL_API_SQLNATIVESQL] = TRUE;
		pfExists[SQL_API_SQLNUMPARAMS] = TRUE;
		pfExists[SQL_API_SQLPARAMOPTIONS] = TRUE;
		pfExists[SQL_API_SQLPRIMARYKEYS] = TRUE;
		if (PG_VERSION_LT(conn, 6.5))
			pfExists[SQL_API_SQLPROCEDURECOLUMNS] = FALSE;
		else
			pfExists[SQL_API_SQLPROCEDURECOLUMNS] = TRUE;
		if (PG_VERSION_LT(conn, 6.5))
			pfExists[SQL_API_SQLPROCEDURES] = FALSE;
		else
			pfExists[SQL_API_SQLPROCEDURES] = TRUE;
		pfExists[SQL_API_SQLSETPOS] = TRUE;
		pfExists[SQL_API_SQLSETSCROLLOPTIONS] = FALSE;		/* odbc 1.0 */
		pfExists[SQL_API_SQLTABLEPRIVILEGES] = TRUE;
		pfExists[SQL_API_SQLBULKOPERATIONS] = TRUE;
	}
	else
	{
		if (ci->drivers.lie)
			*pfExists = TRUE;
		else
		{
			switch (fFunction)
			{
				case SQL_API_SQLBINDCOL:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLCANCEL:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLCOLATTRIBUTE:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLCONNECT:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLDESCRIBECOL:
					*pfExists = TRUE;
					break;		/* partial */
				case SQL_API_SQLDISCONNECT:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLEXECDIRECT:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLEXECUTE:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLFETCH:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLFREESTMT:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLGETCURSORNAME:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLNUMRESULTCOLS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLPREPARE:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLROWCOUNT:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLSETCURSORNAME:
					*pfExists = TRUE;
					break;
				/* ODBC level 1 functions */
				case SQL_API_SQLBINDPARAMETER:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLCOLUMNS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLDRIVERCONNECT:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLGETDATA:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLGETFUNCTIONS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLGETINFO:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLGETTYPEINFO:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLPARAMDATA:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLPUTDATA:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLSPECIALCOLUMNS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLSTATISTICS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLTABLES:
					*pfExists = TRUE;
					break;

				/* ODBC level 2 functions */
				case SQL_API_SQLBROWSECONNECT:
					*pfExists = FALSE;
					break;
				case SQL_API_SQLCOLUMNPRIVILEGES:
					*pfExists = FALSE;
					break;
				case SQL_API_SQLDATASOURCES:
					*pfExists = FALSE;
					break;		/* only implemented by DM */
				case SQL_API_SQLDESCRIBEPARAM:
					*pfExists = FALSE;
					break;		/* not properly implemented */
				case SQL_API_SQLDRIVERS:
					*pfExists = FALSE;
					break;		/* only implemented by DM */
				case SQL_API_SQLEXTENDEDFETCH:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLFOREIGNKEYS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLMORERESULTS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLNATIVESQL:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLNUMPARAMS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLPRIMARYKEYS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLPROCEDURECOLUMNS:
					if (PG_VERSION_LT(conn, 6.5))
						*pfExists = FALSE;
					else
						*pfExists = TRUE;
					break;
				case SQL_API_SQLPROCEDURES:
					if (PG_VERSION_LT(conn, 6.5))
						*pfExists = FALSE;
					else
						*pfExists = TRUE;
					break;
				case SQL_API_SQLSETPOS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLTABLEPRIVILEGES:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLBULKOPERATIONS:	/* 24 */
				case SQL_API_SQLALLOCHANDLE:	/* 1001 */
				case SQL_API_SQLBINDPARAM:	/* 1002 */
				case SQL_API_SQLCLOSECURSOR:	/* 1003 */
				case SQL_API_SQLENDTRAN:	/* 1005 */
				case SQL_API_SQLFETCHSCROLL:	/* 1021 */
				case SQL_API_SQLFREEHANDLE:	/* 1006 */
				case SQL_API_SQLGETCONNECTATTR:	/* 1007 */
				case SQL_API_SQLGETDESCFIELD:	/* 1008 */
				case SQL_API_SQLGETDIAGFIELD:	/* 1010 */
				case SQL_API_SQLGETDIAGREC:	/* 1011 */
				case SQL_API_SQLGETENVATTR:	/* 1012 */
				case SQL_API_SQLGETSTMTATTR:	/* 1014 */
				case SQL_API_SQLSETCONNECTATTR:	/* 1016 */
				case SQL_API_SQLSETDESCFIELD:	/* 1017 */
				case SQL_API_SQLSETENVATTR:	/* 1019 */
				case SQL_API_SQLSETSTMTATTR:	/* 1020 */
					*pfExists = TRUE;
					break;
				case SQL_API_SQLGETDESCREC:	/* 1009 */
				case SQL_API_SQLSETDESCREC:	/* 1018 */
				case SQL_API_SQLCOPYDESC:	/* 1004 */
					*pfExists = FALSE;
					break;
				default:
					*pfExists = FALSE;
					break;
			}
		}
	}
	return SQL_SUCCESS;
}


#define	CSTR_SYS_TABLE	"SYSTEM TABLE"
#define	CSTR_TABLE	"TABLE"
#define	CSTR_VIEW	"VIEW"

RETCODE		SQL_API
PGAPI_Tables(
			 HSTMT hstmt,
			 UCHAR FAR * szTableQualifier,
			 SWORD cbTableQualifier,
			 UCHAR FAR * szTableOwner,
			 SWORD cbTableOwner,
			 UCHAR FAR * szTableName,
			 SWORD cbTableName,
			 UCHAR FAR * szTableType,
			 SWORD cbTableType)
{
	CSTR func = "PGAPI_Tables";
	StatementClass *stmt = (StatementClass *) hstmt;
	StatementClass *tbl_stmt;
	QResultClass	*res;
	TupleNode  *row;
	HSTMT		htbl_stmt;
	RETCODE		ret = SQL_ERROR, result;
	char	   *tableType;
	char		tables_query[INFO_INQUIRY_LEN];
	char		table_name[MAX_INFO_STRING],
				table_owner[MAX_INFO_STRING],
				relkind_or_hasrules[MAX_INFO_STRING];
#ifdef	HAVE_STRTOK_R
	char		*last;
#endif /* HAVE_STRTOK_R */
	ConnectionClass *conn;
	ConnInfo   *ci;
	char	   *prefix[32],
				prefixes[MEDIUM_REGISTRY_LEN];
	char	   *table_type[32],
				table_types[MAX_INFO_STRING];
	char		show_system_tables,
				show_regular_tables,
				show_views,
				remarks[254]; /*Added for holding Table Description, if any.*/
	char		regular_table,
				view,
				systable;
	int			i;
	SWORD			internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char *likeeq = "like", *szSchemaName;

	mylog("%s: entering...stmt=%u scnm=%x len=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	stmt->manual_result = TRUE;
	stmt->errormsg_created = TRUE;

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);

	result = PGAPI_AllocStmt(stmt->hdbc, &htbl_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_Tables result.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	tbl_stmt = (StatementClass *) htbl_stmt;
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

retry_public_schema:
	/*
	 * Create the query to find out the tables
	 */
	if (conn->schema_support)
	{
		/* view is represented by its relkind since 7.1 */

		/* Previously it was:
		 * strcpy(tables_query, "select relname, nspname, relkind"
		 * from pg_catalog.pg_class c, pg_catalog.pg_namespace n");
		 * strcat(tables_query, " where relkind in ('r', 'v')");
		 * Modified query to retrieve the description of the table:
		 */	

		strcpy(tables_query,"SELECT DISTINCT tt.relname, tt.nspname, tt.relkind, COALESCE(d.description,'') from");
        strcat(tables_query," (SELECT c.oid as oid, c.tableoid as tableoid, n.nspname as nspname, c.relname, c.relkind");
		strcat(tables_query," FROM pg_catalog.pg_class c LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace");
		strcat(tables_query," WHERE c.relkind IN ('r', 'v') ");

	}
	else if (PG_VERSION_GE(conn, 7.1))
	{
		/* view is represented by its relkind since 7.1 */
		strcpy(tables_query, "select relname, usename, relkind"
		" from pg_class c, pg_user u");
		strcat(tables_query, " where relkind in ('r', 'v')");

	}
	else
	{
		strcpy(tables_query, "select relname, usename, relhasrules from pg_class c, pg_user u");
		strcat(tables_query, " where relkind = 'r'");
	}

	if (conn->schema_support)
		schema_strcat1(tables_query, " and nspname %s '%.*s'", likeeq, szSchemaName, cbSchemaName, szTableName, cbTableName, conn);
	else
		my_strcat1(tables_query, " and usename %s '%.*s'", likeeq, szSchemaName, cbSchemaName);
	my_strcat1(tables_query, " and relname %s '%.*s'", likeeq, szTableName, cbTableName);


	/* Parse the extra systable prefix	*/
	strcpy(prefixes, ci->drivers.extra_systable_prefixes);
	i = 0;
#ifdef	HAVE_STRTOK_R
	prefix[i] = strtok_r(prefixes, ";", &last);
#else
	prefix[i] = strtok(prefixes, ";");
#endif /* HAVE_STRTOK_R */
	while (i < 31 && prefix[i])
#ifdef	HAVE_STRTOK_R
		prefix[++i] = strtok_r(NULL, ";", &last);
#else
		prefix[++i] = strtok(NULL, ";");
#endif /* HAVE_STRTOK_R */

	/* Parse the desired table types to return */
	show_system_tables = FALSE;
	show_regular_tables = FALSE;
	show_views = FALSE;

	/* make_string mallocs memory */
	tableType = make_string(szTableType, cbTableType, NULL, 0);
	if (tableType)
	{
		strcpy(table_types, tableType);
		free(tableType);
		i = 0;
#ifdef	HAVE_STRTOK_R
		table_type[i] = strtok_r(table_types, ",", &last);
#else
		table_type[i] = strtok(table_types, ",");
#endif /* HAVE_STRTOK_R */
		while (i < 31 && table_type[i])
#ifdef	HAVE_STRTOK_R
			table_type[++i] = strtok_r(NULL, ",", &last);
#else
			table_type[++i] = strtok(NULL, ",");
#endif /* HAVE_STRTOK_R */

		/* Check for desired table types to return */
		i = 0;
		while (table_type[i])
		{
			char *typestr = table_type[i];

			while (isspace(*typestr))
				typestr++;
			if (*typestr == '\'')
				typestr++;
			if (strnicmp(typestr, CSTR_SYS_TABLE, strlen(CSTR_SYS_TABLE)) == 0)
				show_system_tables = TRUE;
			else if (strnicmp(typestr, CSTR_TABLE, strlen(CSTR_TABLE)) == 0)
				show_regular_tables = TRUE;
			else if (strnicmp(typestr, CSTR_VIEW, strlen(CSTR_VIEW)) == 0)
				show_views = TRUE;
			i++;
		}
	}
	else
	{
		show_regular_tables = TRUE;
		show_views = TRUE;
	}

	/*
	 * If not interested in SYSTEM TABLES then filter them out to save
	 * some time on the query.	If treating system tables as regular
	 * tables, then dont filter either.
	 */
	if (!atoi(ci->show_system_tables) && !show_system_tables)
	{
		strcat(tables_query, " and relname !~ '^" POSTGRES_SYS_PREFIX);

		/* Also filter out user-defined system table types */
		i = 0;
		while (prefix[i])
		{
			strcat(tables_query, "|^");
			strcat(tables_query, prefix[i]);
			i++;
		}
		strcat(tables_query, "'");
	}

	/* match users */
	if (PG_VERSION_LT(conn, 7.1))
		/* filter out large objects in older versions */
		strcat(tables_query, " and relname !~ '^xinv[0-9]+'");

	if (conn->schema_support)
	{
		/* Previously it was:
		 * strcat(tables_query, " and n.oid = relnamespace order by nspname, relname");
		 * Modified query to retrieve the description of the table:
		 */	

		strcat(tables_query," ) AS tt LEFT JOIN pg_catalog.pg_description d ");
		strcat(tables_query," ON (tt.oid = d.objoid AND tt.tableoid = d.classoid AND d.objsubid = 0)");
		strcat(tables_query," order by nspname, relname");
	}

	else
		strcat(tables_query, " and usesysid = relowner order by relname");

	result = PGAPI_ExecDirect(htbl_stmt, tables_query, SQL_NTS, 0);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_full_error_copy(stmt, htbl_stmt);
		goto cleanup;
	}

	/* If not found */
	if (conn->schema_support &&
	    (res = SC_get_Result(tbl_stmt)) &&
	    0 == QR_get_num_total_tuples(res))
	{
		const char *user = CC_get_username(conn);

		/*
		 * If specified schema name == user_name and
		 * the current schema is 'public',
		 * retry the 'public' schema.
		 */
		if (szSchemaName &&
		    (cbSchemaName == SQL_NTS ||
		     cbSchemaName == (SWORD) strlen(user)) &&
		    strnicmp(szSchemaName, user, strlen(user)) == 0 &&
		    stricmp(CC_get_current_schema(conn), pubstr) == 0)
		{
			szSchemaName = pubstr;
			cbSchemaName = SQL_NTS;
			goto retry_public_schema;
		}
	}

#ifdef  UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif

	result = PGAPI_BindCol(htbl_stmt, 1, internal_asis_type,
						   table_name, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, tbl_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(htbl_stmt, 2, internal_asis_type,
						   table_owner, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, tbl_stmt);
		goto cleanup;
	}
	result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
						   relkind_or_hasrules, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, tbl_stmt);
		goto cleanup;
	}

	/* Binds the description column to variable 'remarks' */

	result = PGAPI_BindCol(htbl_stmt, 4, internal_asis_type,
							   remarks, 254, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
	}


	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_Tables result.");
		goto cleanup;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until */

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	extend_column_bindings(SC_get_ARDF(stmt), 5);

	/* set the field names */
	QR_set_num_fields(res, 5);
	QR_set_field_info(res, 0, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 1, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 2, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 3, "TABLE_TYPE", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 4, "REMARKS", PG_TYPE_VARCHAR, 254);

	/* add the tuples */
	result = PGAPI_Fetch(htbl_stmt);
	while ((result == SQL_SUCCESS) || (result == SQL_SUCCESS_WITH_INFO))
	{
		/*
		 * Determine if this table name is a system table. If treating
		 * system tables as regular tables, then no need to do this test.
		 */
		systable = FALSE;
		if (!atoi(ci->show_system_tables))
		{
			if (strncmp(table_name, POSTGRES_SYS_PREFIX, strlen(POSTGRES_SYS_PREFIX)) == 0)
				systable = TRUE;

			else
			{
				/* Check extra system table prefixes */
				i = 0;
				while (prefix[i])
				{
					mylog("table_name='%s', prefix[%d]='%s'\n", table_name, i, prefix[i]);
					if (strncmp(table_name, prefix[i], strlen(prefix[i])) == 0)
					{
						systable = TRUE;
						break;
					}
					i++;
				}
			}
		}

		/* Determine if the table name is a view */
		if (PG_VERSION_GE(conn, 7.1))
			/* view is represented by its relkind since 7.1 */
			view = (relkind_or_hasrules[0] == 'v');
		else
			view = (relkind_or_hasrules[0] == '1');

		/* It must be a regular table */
		regular_table = (!systable && !view);


		/* Include the row in the result set if meets all criteria */

		/*
		 * NOTE: Unsupported table types (i.e., LOCAL TEMPORARY, ALIAS,
		 * etc) will return nothing
		 */
		if ((systable && show_system_tables) ||
			(view && show_views) ||
			(regular_table && show_regular_tables))
		{
			SC_MALLOC_return_with_error(row, TupleNode, (sizeof(TupleNode) + (5 - 1) * sizeof(TupleField)), stmt, "Couldn't alloc row", SQL_ERROR) 

			/*set_tuplefield_string(&row->tuple[0], "");*/
			set_tuplefield_null(&row->tuple[0]);

			/*
			 * I have to hide the table owner from Access, otherwise it
			 * insists on referring to the table as 'owner.table'. (this
			 * is valid according to the ODBC SQL grammar, but Postgres
			 * won't support it.)
			 *
			 * set_tuplefield_string(&row->tuple[1], table_owner);
			 */

			mylog("%s: table_name = '%s'\n", func, table_name);

			if (conn->schema_support)
				set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(table_owner));
			else
				set_tuplefield_null(&row->tuple[1]);
			set_tuplefield_string(&row->tuple[2], table_name);
			set_tuplefield_string(&row->tuple[3], systable ? "SYSTEM TABLE" : (view ? "VIEW" : "TABLE"));
			set_tuplefield_string(&row->tuple[4], remarks);
			/*** set_tuplefield_string(&row->tuple[4], "TABLE"); ***/

			QR_add_tuple(res, row);
		}
		result = PGAPI_Fetch(htbl_stmt);
	}
	if (result != SQL_NO_DATA_FOUND)
	{
		SC_full_error_copy(stmt, htbl_stmt);
		goto cleanup;
	}
	ret = SQL_SUCCESS;

cleanup:
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;
	if (SQL_ERROR == ret)
		SC_log_error(func, "", stmt);

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);

	if (htbl_stmt)
		PGAPI_FreeStmt(htbl_stmt, SQL_DROP);

	mylog("%s: EXIT, stmt=%u, ret=%d\n", func, stmt, ret);
	return ret;
}


/*
 *	PostgreSQL needs 2 '\\' to escape '_' and '%'. 
 */
static int
reallyEscapeCatalogEscapes(const char *src, int srclen, char *dest, int dst_len, int ccsc)
{
	int	i, outlen;
	const char *in;
	BOOL	escape_in = FALSE;
	encoded_str	encstr;

	if (srclen == SQL_NULL_DATA)
	{
		dest[0] = '\0';
		return STRCPY_NULL;
	}
	else if (srclen == SQL_NTS)
		srclen = strlen(src);
	if (srclen <= 0)
		return STRCPY_FAIL;
	encoded_str_constr(&encstr, ccsc, src);
	for (i = 0, in = src, outlen = 0; i < srclen && outlen < dst_len; i++, in++)
	{
                encoded_nextchar(&encstr);
                if (ENCODE_STATUS(encstr) != 0)
                {
                        dest[outlen++] = *in;
                        continue;
                }
		if (escape_in)
		{
			switch (*in)
			{
				case '%':
				case '_':
					dest[outlen++] = '\\'; /* needs 1 more */
					break;
				default:
					dest[outlen++] = '\\';
					if (outlen < dst_len)
						dest[outlen++] = '\\';
					if (outlen < dst_len)
						dest[outlen++] = '\\';
					break;
			}
		}
		if (*in == '\\')
			escape_in = TRUE;
		else
			escape_in = FALSE;
		if (outlen < dst_len)
			dest[outlen++] = *in;
	}
	if (outlen < dst_len)
		dest[outlen] = '\0';
	return outlen;
}

RETCODE		SQL_API
PGAPI_Columns(
			  HSTMT hstmt,
			  UCHAR FAR * szTableQualifier,
			  SWORD cbTableQualifier,
			  UCHAR FAR * szTableOwner,
			  SWORD cbTableOwner,
			  UCHAR FAR * szTableName,
			  SWORD cbTableName,
			  UCHAR FAR * szColumnName,
			  SWORD cbColumnName,
			  UWORD	flag)
{
	CSTR func = "PGAPI_Columns";
	StatementClass *stmt = (StatementClass *) hstmt;
	QResultClass	*res;
	TupleNode  *row;
	HSTMT		hcol_stmt = NULL;
	StatementClass *col_stmt;
	char		columns_query[INFO_INQUIRY_LEN];
	RETCODE		result;
	char		table_owner[MAX_INFO_STRING],
				table_name[MAX_INFO_STRING],
				field_name[MAX_INFO_STRING],
				field_type_name[MAX_INFO_STRING];
	Int2		field_number, sqltype, concise_type,
				reserved_cols,
				result_cols,
				decimal_digits;
	Int4		field_type,
				the_type,
				field_length,
				mod_length,
				column_size,
				ordinal;
	char		useStaticPrecision, useStaticScale;
	char		not_null[MAX_INFO_STRING],
				relhasrules[MAX_INFO_STRING], relkind[8];
	BOOL		relisaview;
	ConnInfo   *ci;
	ConnectionClass *conn;
	SWORD		internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char *likeeq = "like", *szSchemaName;

	mylog("%s: entering...stmt=%u scnm=%x len=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	stmt->manual_result = TRUE;
	stmt->errormsg_created = TRUE;

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
#ifdef  UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

retry_public_schema:
	/*
	 * Create the query to find out the columns (Note: pre 6.3 did not
	 * have the atttypmod field)
	 */
	if (conn->schema_support)
		sprintf(columns_query, "select u.nspname, c.relname, a.attname, a.atttypid"
	   ", t.typname, a.attnum, a.attlen, %s, a.attnotnull, c.relhasrules, c.relkind"
			" from pg_catalog.pg_namespace u, pg_catalog.pg_class c,"
			" pg_catalog.pg_attribute a, pg_catalog.pg_type t"
			" where u.oid = c.relnamespace"
			" and (not a.attisdropped)"
			" and c.oid= a.attrelid and a.atttypid = t.oid and (a.attnum > 0)",
			"a.atttypmod");
	else
		sprintf(columns_query, "select u.usename, c.relname, a.attname, a.atttypid"
	   ", t.typname, a.attnum, a.attlen, %s, a.attnotnull, c.relhasrules, c.relkind"
			" from pg_user u, pg_class c, pg_attribute a, pg_type t"
			" where u.usesysid = c.relowner"
	  " and c.oid= a.attrelid and a.atttypid = t.oid and (a.attnum > 0)",
			PG_VERSION_LE(conn, 6.2) ? "a.attlen" : "a.atttypmod");

	if ((flag & PODBC_NOT_SEARCH_PATTERN) != 0) 
	{
		my_strcat(columns_query, " and c.relname = '%.*s'", szTableName, cbTableName);
		if (conn->schema_support)
			schema_strcat(columns_query, " and u.nspname = '%.*s'", szSchemaName, cbSchemaName, szTableName, cbTableName, conn);
		else
			my_strcat(columns_query, " and u.usename = '%.*s'", szSchemaName, cbSchemaName);
		my_strcat(columns_query, " and a.attname = '%.*s'", szColumnName, cbColumnName);
	}
	else
	{
		char	esc_table_name[TABLE_NAME_STORAGE_LEN * 2];
		int	escTbnamelen;

		escTbnamelen = reallyEscapeCatalogEscapes(szTableName, cbTableName, esc_table_name, sizeof(esc_table_name), conn->ccsc);
		my_strcat1(columns_query, " and c.relname %s '%.*s'", likeeq, esc_table_name, escTbnamelen);
		if (conn->schema_support)
			schema_strcat1(columns_query, " and u.nspname %s '%.*s'", likeeq, szSchemaName, cbSchemaName, szTableName, cbTableName, conn);
		else
			my_strcat1(columns_query, " and u.usename %s '%.*s'", likeeq, szSchemaName, cbSchemaName);
		my_strcat1(columns_query, " and a.attname %s '%.*s'", likeeq, szColumnName, cbColumnName);
	}

	if (!atoi(ci->show_system_tables))
	{
		if (conn->schema_support)
			strcat(columns_query, "  and nspname !~ '^" POSTGRES_SYS_PREFIX "'");
		else
			strcat(columns_query, "  and relname !~ '^" POSTGRES_SYS_PREFIX "'");
	}
	/*
	 * give the output in the order the columns were defined when the
	 * table was created
	 */
	if (conn->schema_support)
		strcat(columns_query, " order by u.nspname, c.relname, attnum");
	else
		strcat(columns_query, " order by c.relname, attnum");

	result = PGAPI_AllocStmt(stmt->hdbc, &hcol_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_Columns result.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	col_stmt = (StatementClass *) hcol_stmt;

	mylog("%s: hcol_stmt = %u, col_stmt = %u\n", func, hcol_stmt, col_stmt);

	result = PGAPI_ExecDirect(hcol_stmt, columns_query, SQL_NTS, 0);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_full_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	/* If not found */
	if (conn->schema_support &&
	    (flag & PODBC_SEARCH_PUBLIC_SCHEMA) != 0 &&
	    (res = SC_get_Result(col_stmt)) &&
	    0 == QR_get_num_total_tuples(res))
	{
		const char *user = CC_get_username(conn);

		/*
		 * If specified schema name == user_name and
		 * the current schema is 'public',
		 * retry the 'public' schema.
		 */
		if (szSchemaName &&
		    (cbSchemaName == SQL_NTS ||
		     cbSchemaName == (SWORD) strlen(user)) &&
		    strnicmp(szSchemaName, user, strlen(user)) == 0 &&
		    stricmp(CC_get_current_schema(conn), pubstr) == 0)
		{
			PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
			hcol_stmt = NULL;
			szSchemaName = pubstr;
			cbSchemaName = SQL_NTS;
			goto retry_public_schema;
		}
	}

	result = PGAPI_BindCol(hcol_stmt, 1, internal_asis_type,
						   table_owner, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 2, internal_asis_type,
						   table_name, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 3, internal_asis_type,
						   field_name, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 4, SQL_C_LONG,
						   &field_type, 4, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 5, internal_asis_type,
						   field_type_name, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 6, SQL_C_SHORT,
						   &field_number, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 7, SQL_C_LONG,
						   &field_length, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 8, SQL_C_LONG,
						   &mod_length, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 9, internal_asis_type,
						   not_null, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 10, internal_asis_type,
						   relhasrules, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 11, internal_asis_type,
						   relkind, sizeof(relkind), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_Columns result.");
		goto cleanup;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	reserved_cols = 18;

	result_cols = reserved_cols + 2;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info(res, 0, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 1, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 2, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 3, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 4, "DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 5, "TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 6, "PRECISION", PG_TYPE_INT4, 4); /* COLUMN_SIZE */
	QR_set_field_info(res, 7, "LENGTH", PG_TYPE_INT4, 4); /* BUFFER_LENGTH */
	QR_set_field_info(res, 8, "SCALE", PG_TYPE_INT2, 2); /* DECIMAL_DIGITS ***/
	QR_set_field_info(res, 9, "RADIX", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 10, "NULLABLE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 11, "REMARKS", PG_TYPE_VARCHAR, 254);

	/* User defined fields */
	QR_set_field_info(res, 12, "COLUMN_DEF", PG_TYPE_INT4, 254);
	QR_set_field_info(res, 13, "SQL_DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 14, "SQL_DATETIME_SUB", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 15, "CHAR_OCTET_LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 16, "ORDINAL_POSITION", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 17, "IS_NULLABLE", PG_TYPE_VARCHAR, 254);
	QR_set_field_info(res, reserved_cols, "DISPLAY_SIZE", PG_TYPE_INT4, 4);
	QR_set_field_info(res, reserved_cols + 1, "FIELD_TYPE", PG_TYPE_INT4, 4);

	ordinal = 1;
	result = PGAPI_Fetch(hcol_stmt);

	/*
	 * Only show oid if option AND there are other columns AND it's not
	 * being called by SQLStatistics . Always show OID if it's a system
	 * table
	 */

	if (PG_VERSION_GE(conn, 7.1))
		relisaview = (relkind[0] == 'v');
	else
		relisaview = (relhasrules[0] == '1');
	if (result != SQL_ERROR && !stmt->internal)
	{
		if (!relisaview &&
			(atoi(ci->show_oid_column) ||
			 strncmp(table_name, POSTGRES_SYS_PREFIX, strlen(POSTGRES_SYS_PREFIX)) == 0))
		{
			/* For OID fields */
			the_type = PG_TYPE_OID;
			SC_MALLOC_return_with_error(row, TupleNode,
			 (sizeof(TupleNode) + (result_cols - 1) * sizeof(TupleField)),
				stmt, "Couldn't alloc row", SQL_ERROR)

			set_tuplefield_string(&row->tuple[0], "");
			/* see note in SQLTables() */
			if (conn->schema_support)
				set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(table_owner));
			else
				set_tuplefield_string(&row->tuple[1], "");
			set_tuplefield_string(&row->tuple[2], table_name);
			set_tuplefield_string(&row->tuple[3], "oid");
			sqltype = pgtype_to_concise_type(stmt, the_type, PG_STATIC);
			set_tuplefield_int2(&row->tuple[4], sqltype);
			set_tuplefield_string(&row->tuple[5], "OID");

			set_tuplefield_int4(&row->tuple[6], pgtype_column_size(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[7], pgtype_buffer_length(stmt, the_type, PG_STATIC, PG_STATIC));
			set_nullfield_int2(&row->tuple[8], pgtype_decimal_digits(stmt, the_type, PG_STATIC));
			set_nullfield_int2(&row->tuple[9], pgtype_radix(stmt, the_type));
			set_tuplefield_int2(&row->tuple[10], SQL_NO_NULLS);
			set_tuplefield_string(&row->tuple[11], "");
			set_tuplefield_null(&row->tuple[12]);
			set_tuplefield_int2(&row->tuple[13], sqltype);
			set_tuplefield_null(&row->tuple[14]);
			set_tuplefield_int4(&row->tuple[15], pgtype_transfer_octet_length(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[16], ordinal);
			set_tuplefield_string(&row->tuple[17], "No");
			set_tuplefield_int4(&row->tuple[reserved_cols], pgtype_display_size(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[reserved_cols + 1], the_type);

			QR_add_tuple(res, row);
			ordinal++;
		}
	}

	while ((result == SQL_SUCCESS) || (result == SQL_SUCCESS_WITH_INFO))
	{
		SC_MALLOC_return_with_error(row, TupleNode,
		(sizeof(TupleNode) + (result_cols - 1) * sizeof(TupleField)),
			stmt, "Couldn't alloc row", SQL_ERROR)

		sqltype = SQL_TYPE_NULL;	/* unspecified */
		set_tuplefield_string(&row->tuple[0], "");
		/* see note in SQLTables() */
		if (conn->schema_support)
			set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(table_owner));
		else
			set_tuplefield_string(&row->tuple[1], "");
		set_tuplefield_string(&row->tuple[2], table_name);
		set_tuplefield_string(&row->tuple[3], field_name);
		set_tuplefield_string(&row->tuple[5], field_type_name);


		/*----------
		 * Some Notes about Postgres Data Types:
		 *
		 * VARCHAR - the length is stored in the pg_attribute.atttypmod field
		 * BPCHAR  - the length is also stored as varchar is
		 *
		 * NUMERIC - the decimal_digits is stored in atttypmod as follows:
		 *
		 *	column_size =((atttypmod - VARHDRSZ) >> 16) & 0xffff
		 *	decimal_digits	 = (atttypmod - VARHDRSZ) & 0xffff
		 *
		 *----------
		 */
		qlog("PGAPI_Columns: table='%s',field_name='%s',type=%d,name='%s'\n",
			 table_name, field_name, field_type, field_type_name);

		useStaticPrecision = TRUE;
		useStaticScale = TRUE;

		if (field_type == PG_TYPE_NUMERIC)
		{
			if (mod_length >= 4)
				mod_length -= 4;	/* the length is in atttypmod - 4 */

			if (mod_length >= 0)
			{
				useStaticPrecision = FALSE;
				useStaticScale = FALSE;

				column_size = (mod_length >> 16) & 0xffff;
				decimal_digits = mod_length & 0xffff;

				mylog("%s: field type is NUMERIC: field_type = %d, mod_length=%d, precision=%d, scale=%d\n", func, field_type, mod_length, column_size, decimal_digits);

				set_tuplefield_int4(&row->tuple[6], column_size);
				set_tuplefield_int4(&row->tuple[7], column_size + 2);		/* sign+dec.point */
				set_nullfield_int2(&row->tuple[8], decimal_digits);
				set_tuplefield_null(&row->tuple[15]);
				set_tuplefield_int4(&row->tuple[reserved_cols], column_size + 2);	/* sign+dec.point */
			}
		}
		else if ((field_type == PG_TYPE_DATETIME) ||
			(field_type == PG_TYPE_TIMESTAMP_NO_TMZONE))
		{
			if (PG_VERSION_GE(conn, 7.2))
			{
				useStaticScale = FALSE;

				set_nullfield_int2(&row->tuple[8], (Int2) mod_length);
			}
		}

		if ((field_type == PG_TYPE_VARCHAR) ||
			(field_type == PG_TYPE_BPCHAR))
		{
			useStaticPrecision = FALSE;

			if (mod_length >= 4)
				mod_length -= 4;	/* the length is in atttypmod - 4 */

			/* if (mod_length > ci->drivers.max_varchar_size || mod_length <= 0) */
			if (mod_length <= 0)
				mod_length = ci->drivers.max_varchar_size;
			if (mod_length > ci->drivers.max_varchar_size)
				sqltype = SQL_LONGVARCHAR;
			else
				sqltype = (field_type == PG_TYPE_BPCHAR) ? SQL_CHAR : SQL_VARCHAR;

			mylog("%s: field type is VARCHAR,BPCHAR: field_type = %d, mod_length = %d\n", func, field_type, mod_length);

			set_tuplefield_int4(&row->tuple[6], mod_length);
			set_tuplefield_int4(&row->tuple[7], mod_length);
			set_tuplefield_int4(&row->tuple[15], pgtype_transfer_octet_length(stmt, field_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[reserved_cols], mod_length);
		}

		if (useStaticPrecision)
		{
			mylog("%s: field type is OTHER: field_type = %d, pgtype_length = %d\n", func, field_type, pgtype_buffer_length(stmt, field_type, PG_STATIC, PG_STATIC));

			set_tuplefield_int4(&row->tuple[6], pgtype_column_size(stmt, field_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[7], pgtype_buffer_length(stmt, field_type, PG_STATIC, PG_STATIC));
			set_tuplefield_null(&row->tuple[15]);
			set_tuplefield_int4(&row->tuple[reserved_cols], pgtype_display_size(stmt, field_type, PG_STATIC, PG_STATIC));
		}
		if (useStaticScale)
		{
			set_nullfield_int2(&row->tuple[8], pgtype_decimal_digits(stmt, field_type, PG_STATIC));
		}

		if (SQL_TYPE_NULL == sqltype)
		{
			sqltype = pgtype_to_concise_type(stmt, field_type, PG_STATIC);
			concise_type = pgtype_to_sqldesctype(stmt, field_type, PG_STATIC);
		}
		else
			concise_type = sqltype;
		set_tuplefield_int2(&row->tuple[4], sqltype);

		set_nullfield_int2(&row->tuple[9], pgtype_radix(stmt, field_type));
		set_tuplefield_int2(&row->tuple[10], (Int2) (not_null[0] == '1' ? SQL_NO_NULLS : pgtype_nullable(stmt, field_type)));
		set_tuplefield_string(&row->tuple[11], "");
		set_tuplefield_null(&row->tuple[12]);
		set_tuplefield_int2(&row->tuple[13], concise_type);
		set_nullfield_int2(&row->tuple[14], pgtype_to_datetime_sub(stmt, field_type));
		set_tuplefield_int4(&row->tuple[16], ordinal);
		set_tuplefield_null(&row->tuple[17]);
		set_tuplefield_int4(&row->tuple[reserved_cols + 1], field_type);

		QR_add_tuple(res, row);
		ordinal++;

		result = PGAPI_Fetch(hcol_stmt);

	}
	if (result != SQL_NO_DATA_FOUND)
	{
		SC_full_error_copy(stmt, col_stmt);
		goto cleanup;
	}

	/*
	 * Put the row version column at the end so it might not be mistaken
	 * for a key field.
	 */
	if (!relisaview && !stmt->internal && atoi(ci->row_versioning))
	{
		/* For Row Versioning fields */
		the_type = PG_TYPE_INT4;

		SC_MALLOC_return_with_error(row, TupleNode, (sizeof(TupleNode) + (result_cols - 1) * sizeof(TupleField)),
			stmt, "Couldn't alloc row", SQL_ERROR)

		set_tuplefield_string(&row->tuple[0], "");
		if (conn->schema_support)
			set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(table_owner));
		else
			set_tuplefield_string(&row->tuple[1], "");
		set_tuplefield_string(&row->tuple[2], table_name);
		set_tuplefield_string(&row->tuple[3], "xmin");
		sqltype = pgtype_to_concise_type(stmt, the_type, PG_STATIC);
		set_tuplefield_int2(&row->tuple[4], sqltype);
		set_tuplefield_string(&row->tuple[5], pgtype_to_name(stmt, the_type));
		set_tuplefield_int4(&row->tuple[6], pgtype_column_size(stmt, the_type, PG_STATIC, PG_STATIC));
		set_tuplefield_int4(&row->tuple[7], pgtype_buffer_length(stmt, the_type, PG_STATIC, PG_STATIC));
		set_nullfield_int2(&row->tuple[8], pgtype_decimal_digits(stmt, the_type, PG_STATIC));
		set_nullfield_int2(&row->tuple[9], pgtype_radix(stmt, the_type));
		set_tuplefield_int2(&row->tuple[10], SQL_NO_NULLS);
		set_tuplefield_string(&row->tuple[11], "");
		set_tuplefield_null(&row->tuple[12]);
		set_tuplefield_int2(&row->tuple[13], sqltype);
		set_tuplefield_null(&row->tuple[14]);
		set_tuplefield_int4(&row->tuple[15], pgtype_transfer_octet_length(stmt, the_type, PG_STATIC, PG_STATIC));
		set_tuplefield_int4(&row->tuple[16], ordinal);
		set_tuplefield_string(&row->tuple[17], "No");
		set_tuplefield_int4(&row->tuple[reserved_cols], pgtype_display_size(stmt, the_type, PG_STATIC, PG_STATIC));
		set_tuplefield_int4(&row->tuple[reserved_cols + 1], the_type);

		QR_add_tuple(res, row);
		ordinal++;
	}
	result = SQL_SUCCESS;

cleanup:
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);

	if (SQL_SUCCESS != result &&
	    SQL_SUCCESS_WITH_INFO != result)
		SC_log_error(func, "", stmt);
	if (hcol_stmt)
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	mylog("%s: EXIT,  stmt=%u\n", func, stmt);
	return result;
}


RETCODE		SQL_API
PGAPI_SpecialColumns(
					 HSTMT hstmt,
					 UWORD fColType,
					 UCHAR FAR * szTableQualifier,
					 SWORD cbTableQualifier,
					 UCHAR FAR * szTableOwner,
					 SWORD cbTableOwner,
					 UCHAR FAR * szTableName,
					 SWORD cbTableName,
					 UWORD fScope,
					 UWORD fNullable)
{
	CSTR func = "PGAPI_SpecialColumns";
	TupleNode  *row;
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	QResultClass	*res;
	ConnInfo   *ci;
	HSTMT		hcol_stmt;
	StatementClass *col_stmt;
	char		columns_query[INFO_INQUIRY_LEN];
	RETCODE		result;
	char		relhasrules[MAX_INFO_STRING], relkind[8], relhasoids[8];
	BOOL		relisaview;
	SWORD		internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char	*szSchemaName;

	mylog("%s: entering...stmt=%u scnm=%x len=%d colType=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner, fColType);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;
	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
#ifdef  UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif
	stmt->manual_result = TRUE;
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

retry_public_schema:
	/*
	 * Create the query to find out if this is a view or not...
	 */
	strcpy(columns_query, "select c.relhasrules, c.relkind");
	if (PG_VERSION_GE(conn, 7.2))
		strcat(columns_query, ", c.relhasoids");
	if (conn->schema_support)
		strcat(columns_query, " from pg_catalog.pg_namespace u,"
		" pg_catalog.pg_class c where "
			"u.oid = c.relnamespace");
	else
		strcat(columns_query, " from pg_user u, pg_class c where "
			"u.usesysid = c.relowner");

	/* TableName cannot contain a string search pattern */
	my_strcat(columns_query, " and c.relname = '%.*s'", szTableName, cbTableName);
	/* SchemaName cannot contain a string search pattern */
	if (conn->schema_support)
		schema_strcat(columns_query, " and u.nspname = '%.*s'", szSchemaName, cbSchemaName, szTableName, cbTableName, conn);
	else
		my_strcat(columns_query, " and u.usename = '%.*s'", szSchemaName, cbSchemaName);


	result = PGAPI_AllocStmt(stmt->hdbc, &hcol_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for SQLSpecialColumns result.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	col_stmt = (StatementClass *) hcol_stmt;

	mylog("%s: hcol_stmt = %u, col_stmt = %u\n", func, hcol_stmt, col_stmt);

	result = PGAPI_ExecDirect(hcol_stmt, columns_query, SQL_NTS, 0);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_full_error_copy(stmt, col_stmt);
		SC_log_error(func, "", stmt);
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
		return SQL_ERROR;
	}

	/* If not found */
	if (conn->schema_support &&
	    (res = SC_get_Result(col_stmt)) &&
	    0 == QR_get_num_total_tuples(res))
	{
		const char *user = CC_get_username(conn);

		/*
		 * If specified schema name == user_name and
		 * the current schema is 'public',
		 * retry the 'public' schema.
		 */
		if (szSchemaName &&
		    (cbSchemaName == SQL_NTS ||
		     cbSchemaName == (SWORD) strlen(user)) &&
		    strnicmp(szSchemaName, user, strlen(user)) == 0 &&
		    stricmp(CC_get_current_schema(conn), pubstr) == 0)
		{
			PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
			hcol_stmt = NULL;
			szSchemaName = pubstr;
			cbSchemaName = SQL_NTS;
			goto retry_public_schema;
		}
	}

	result = PGAPI_BindCol(hcol_stmt, 1, internal_asis_type,
					relhasrules, sizeof(relhasrules), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		SC_log_error(func, "", stmt);
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
		return SQL_ERROR;
	}

	result = PGAPI_BindCol(hcol_stmt, 2, internal_asis_type,
					relkind, sizeof(relkind), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		SC_log_error(func, "", stmt);
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
		return SQL_ERROR;
	}
	relhasoids[0] = '1';
	if (PG_VERSION_GE(conn, 7.2))
	{
		result = PGAPI_BindCol(hcol_stmt, 3, internal_asis_type,
					relhasoids, sizeof(relhasoids), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, col_stmt);
			SC_log_error(func, "", stmt);
			PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
			return SQL_ERROR;
		}
	}

	result = PGAPI_Fetch(hcol_stmt);
	if (PG_VERSION_GE(conn, 7.1))
		relisaview = (relkind[0] == 'v');
	else
		relisaview = (relhasrules[0] == '1');
	PGAPI_FreeStmt(hcol_stmt, SQL_DROP);

	res = QR_Constructor();
	SC_set_Result(stmt, res);
	extend_column_bindings(SC_get_ARDF(stmt), 8);

	QR_set_num_fields(res, 8);
	QR_set_field_info(res, 0, "SCOPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 1, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 2, "DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 3, "TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 4, "PRECISION", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 5, "LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 6, "SCALE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 7, "PSEUDO_COLUMN", PG_TYPE_INT2, 2);

	if (relisaview)
	{
		/* there's no oid for views */
		if (fColType == SQL_BEST_ROWID)
		{
			return SQL_NO_DATA_FOUND;
		}
		else if (fColType == SQL_ROWVER)
		{
			Int2		the_type = PG_TYPE_TID;

			SC_MALLOC_return_with_error(row, TupleNode, (sizeof(TupleNode) + (8 - 1) * sizeof(TupleField)),
				stmt, "Couldn't alloc row", SQL_ERROR)

			set_tuplefield_null(&row->tuple[0]);
			set_tuplefield_string(&row->tuple[1], "ctid");
			set_tuplefield_int2(&row->tuple[2], pgtype_to_concise_type(stmt, the_type, PG_STATIC));
			set_tuplefield_string(&row->tuple[3], pgtype_to_name(stmt, the_type));
			set_tuplefield_int4(&row->tuple[4], pgtype_column_size(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[5], pgtype_buffer_length(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int2(&row->tuple[6], pgtype_decimal_digits(stmt, the_type, PG_STATIC));
			set_tuplefield_int2(&row->tuple[7], SQL_PC_NOT_PSEUDO);

			QR_add_tuple(res, row);
inolog("Add ctid\n");
		}
	}
	else
	{
		/* use the oid value for the rowid */
		if (fColType == SQL_BEST_ROWID)
		{
			if (relhasoids[0] != '1')
				return SQL_NO_DATA_FOUND;
			SC_MALLOC_return_with_error(row,  TupleNode, (sizeof(TupleNode) + (8 - 1) *sizeof(TupleField)),
				stmt, "Couldn't alloc row", SQL_ERROR)

			set_tuplefield_int2(&row->tuple[0], SQL_SCOPE_SESSION);
			set_tuplefield_string(&row->tuple[1], "oid");
			set_tuplefield_int2(&row->tuple[2], pgtype_to_concise_type(stmt, PG_TYPE_OID, PG_STATIC));
			set_tuplefield_string(&row->tuple[3], "OID");
			set_tuplefield_int4(&row->tuple[4], pgtype_column_size(stmt, PG_TYPE_OID, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[5], pgtype_buffer_length(stmt, PG_TYPE_OID, PG_STATIC, PG_STATIC));
			set_tuplefield_int2(&row->tuple[6], pgtype_decimal_digits(stmt, PG_TYPE_OID, PG_STATIC));
			set_tuplefield_int2(&row->tuple[7], SQL_PC_PSEUDO);

			QR_add_tuple(res, row);

		}
		else if (fColType == SQL_ROWVER)
		{
			Int2		the_type = PG_TYPE_INT4;

			if (atoi(ci->row_versioning))
			{
				SC_MALLOC_return_with_error(row, TupleNode,
				(sizeof(TupleNode) + (8 - 1) * sizeof(TupleField)),
					stmt, "Couldn't alloc row", SQL_ERROR)

				set_tuplefield_null(&row->tuple[0]);
				set_tuplefield_string(&row->tuple[1], "xmin");
				set_tuplefield_int2(&row->tuple[2], pgtype_to_concise_type(stmt, the_type, PG_STATIC));
				set_tuplefield_string(&row->tuple[3], pgtype_to_name(stmt, the_type));
				set_tuplefield_int4(&row->tuple[4], pgtype_column_size(stmt, the_type, PG_STATIC, PG_STATIC));
				set_tuplefield_int4(&row->tuple[5], pgtype_buffer_length(stmt, the_type, PG_STATIC, PG_STATIC));
				set_tuplefield_int2(&row->tuple[6], pgtype_decimal_digits(stmt, the_type, PG_STATIC));
				set_tuplefield_int2(&row->tuple[7], SQL_PC_PSEUDO);

				QR_add_tuple(res, row);
			}
		}
	}

	stmt->status = STMT_FINISHED;
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);

	mylog("%s: EXIT,  stmt=%u\n", func, stmt);
	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_Statistics(
				 HSTMT hstmt,
				 UCHAR FAR * szTableQualifier,
				 SWORD cbTableQualifier,
				 UCHAR FAR * szTableOwner,
				 SWORD cbTableOwner,
				 UCHAR FAR * szTableName,
				 SWORD cbTableName,
				 UWORD fUnique,
				 UWORD fAccuracy)
{
	CSTR func = "PGAPI_Statistics";
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	QResultClass	*res;
	char		index_query[INFO_INQUIRY_LEN];
	HSTMT		hcol_stmt = NULL, hindx_stmt = NULL;
	RETCODE		ret = SQL_ERROR, result;
	char		*table_name = NULL;
	char		index_name[MAX_INFO_STRING];
	short		fields_vector[INDEX_KEYS_STORAGE_COUNT];
	char		isunique[10],
				isclustered[10],
				ishash[MAX_INFO_STRING];
	SDWORD		index_name_len,
				fields_vector_len;
	TupleNode  *row;
	int			i;
	StatementClass *col_stmt,
			   *indx_stmt;
	char		column_name[MAX_INFO_STRING],
			table_qualifier[MAX_INFO_STRING],
				relhasrules[10];
	char	  **column_names = NULL;
	SQLINTEGER	column_name_len;
	int			total_columns = 0;
	ConnInfo   *ci;
	char		buf[256];
	SWORD		internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char	*szSchemaName;

	mylog("%s: entering...stmt=%u scnm=%x len=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	stmt->manual_result = TRUE;
	stmt->errormsg_created = TRUE;

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
#ifdef  UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif
	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_Statistics result.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until */

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	extend_column_bindings(SC_get_ARDF(stmt), 13);

	/* set the field names */
	QR_set_num_fields(res, 13);
	QR_set_field_info(res, 0, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 1, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 2, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 3, "NON_UNIQUE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 4, "INDEX_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 5, "INDEX_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 6, "TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 7, "SEQ_IN_INDEX", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 8, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 9, "COLLATION", PG_TYPE_CHAR, 1);
	QR_set_field_info(res, 10, "CARDINALITY", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 11, "PAGES", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 12, "FILTER_CONDITION", PG_TYPE_VARCHAR, MAX_INFO_STRING);

	/*
	 * only use the table name... the owner should be redundant, and we
	 * never use qualifiers.
	 */
	table_name = make_string(szTableName, cbTableName, NULL, 0);
	if (!table_name)
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "No table name passed to PGAPI_Statistics.");
		goto cleanup;
	}
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

	table_qualifier[0] = '\0';
	if (conn->schema_support)
		schema_strcat(table_qualifier, "%.*s", szSchemaName, cbSchemaName, szTableName, cbTableName, conn);

	/*
	 * we need to get a list of the field names first, so we can return
	 * them later.
	 */
	result = PGAPI_AllocStmt(stmt->hdbc, &hcol_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in PGAPI_Statistics for columns.");
		goto cleanup;
	}

	col_stmt = (StatementClass *) hcol_stmt;

	/*
	 * "internal" prevents SQLColumns from returning the oid if it is
	 * being shown. This would throw everything off.
	 */
	col_stmt->internal = TRUE;
	/* 
	 * table_name parameter cannot contain a string search pattern. 
	 */
	result = PGAPI_Columns(hcol_stmt, "", 0, table_qualifier, SQL_NTS,
						   table_name, SQL_NTS, "", 0, PODBC_NOT_SEARCH_PATTERN | PODBC_SEARCH_PUBLIC_SCHEMA);
	col_stmt->internal = FALSE;

	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;
	}
	result = PGAPI_BindCol(hcol_stmt, 4, internal_asis_type,
						 column_name, sizeof(column_name), &column_name_len);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt);
		goto cleanup;

	}

	result = PGAPI_Fetch(hcol_stmt);
	while ((result == SQL_SUCCESS) || (result == SQL_SUCCESS_WITH_INFO))
	{
		if (0 == total_columns)
			PGAPI_GetData(hcol_stmt, 2, internal_asis_type, table_qualifier, sizeof(table_qualifier), NULL);
		total_columns++;

		column_names =
			(char **) realloc(column_names,
							  total_columns * sizeof(char *));
		SC_MALLOC_return_with_error(column_names[total_columns - 1],
			char, (strlen(column_name) + 1), stmt, 
			"Couldn't alloc column_name", SQL_ERROR)
		strcpy(column_names[total_columns - 1], column_name);

		mylog("%s: column_name = '%s'\n", func, column_name);

		result = PGAPI_Fetch(hcol_stmt);
	}

	if (result != SQL_NO_DATA_FOUND)
	{
		SC_full_error_copy(stmt, col_stmt);
		goto cleanup;
	}
	PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	hcol_stmt = NULL;
	if (total_columns == 0)
	{
		/* Couldn't get column names in SQLStatistics.; */
		ret = SQL_SUCCESS;
		goto cleanup;
	}

	/* get a list of indexes on this table */
	result = PGAPI_AllocStmt(stmt->hdbc, &hindx_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in SQLStatistics for indices.");
		goto cleanup;

	}
	indx_stmt = (StatementClass *) hindx_stmt;

	if (conn->schema_support)
		sprintf(index_query, "select c.relname, i.indkey, i.indisunique"
			", i.indisclustered, a.amname, c.relhasrules, n.nspname"
			" from pg_catalog.pg_index i, pg_catalog.pg_class c,"
			" pg_catalog.pg_class d, pg_catalog.pg_am a,"
			" pg_catalog.pg_namespace n"
			" where d.relname = '%s'"
			" and n.nspname = '%s'"
			" and n.oid = d.relnamespace"
			" and d.oid = i.indrelid"
			" and i.indexrelid = c.oid"
			" and c.relam = a.oid order by"
			,table_name, table_qualifier);
	else
		sprintf(index_query, "select c.relname, i.indkey, i.indisunique"
			", i.indisclustered, a.amname, c.relhasrules"
			" from pg_index i, pg_class c, pg_class d, pg_am a"
			" where d.relname = '%s'"
			" and d.oid = i.indrelid"
			" and i.indexrelid = c.oid"
			" and c.relam = a.oid order by"
			,table_name);
	if (PG_VERSION_GT(SC_get_conn(stmt), 6.4))
		strcat(index_query, " i.indisprimary desc,");
	if (conn->schema_support)
		strcat(index_query, " i.indisunique, n.nspname, c.relname");
	else
		strcat(index_query, " i.indisunique, c.relname");

	result = PGAPI_ExecDirect(hindx_stmt, index_query, SQL_NTS, 0);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		/*
		 * "Couldn't execute index query (w/SQLExecDirect) in
		 * SQLStatistics.";
		 */
		SC_full_error_copy(stmt, indx_stmt);
		goto cleanup;
	}

	/* bind the index name column */
	result = PGAPI_BindCol(hindx_stmt, 1, internal_asis_type,
						   index_name, MAX_INFO_STRING, &index_name_len);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt);	/* "Couldn't bind column 
						* in SQLStatistics."; */
		goto cleanup;

	}
	/* bind the vector column */
	result = PGAPI_BindCol(hindx_stmt, 2, SQL_C_DEFAULT,
			fields_vector, INDEX_KEYS_STORAGE_COUNT * 2, &fields_vector_len);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt); /* "Couldn't bind column 
						 * in SQLStatistics."; */
		goto cleanup;

	}
	/* bind the "is unique" column */
	result = PGAPI_BindCol(hindx_stmt, 3, internal_asis_type,
						   isunique, sizeof(isunique), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt);	/* "Couldn't bind column 
						 * in SQLStatistics."; */
		goto cleanup;
	}

	/* bind the "is clustered" column */
	result = PGAPI_BindCol(hindx_stmt, 4, internal_asis_type,
						   isclustered, sizeof(isclustered), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt);	/* "Couldn't bind column *
						 * in SQLStatistics."; */
		goto cleanup;

	}

	/* bind the "is hash" column */
	result = PGAPI_BindCol(hindx_stmt, 5, internal_asis_type,
						   ishash, sizeof(ishash), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt);	/* "Couldn't bind column * 
						 * in SQLStatistics."; */
		goto cleanup;

	}

	result = PGAPI_BindCol(hindx_stmt, 6, internal_asis_type,
					relhasrules, sizeof(relhasrules), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt);
		goto cleanup;
	}

	relhasrules[0] = '0';
	result = PGAPI_Fetch(hindx_stmt);
	/* fake index of OID */
	if (relhasrules[0] != '1' && atoi(ci->show_oid_column) && atoi(ci->fake_oid_index))
	{
		row = (TupleNode *) malloc(sizeof(TupleNode) +
								   (13 - 1) *sizeof(TupleField));

		/* no table qualifier */
		set_tuplefield_string(&row->tuple[0], "");
		/* don't set the table owner, else Access tries to use it */
		set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(table_qualifier));
		set_tuplefield_string(&row->tuple[2], table_name);

		/* non-unique index? */
		set_tuplefield_int2(&row->tuple[3], (Int2) (ci->drivers.unique_index ? FALSE : TRUE));

		/* no index qualifier */
		set_tuplefield_string(&row->tuple[4], "");

		sprintf(buf, "%s_idx_fake_oid", table_name);
		set_tuplefield_string(&row->tuple[5], buf);

		/*
		 * Clustered/HASH index?
		 */
		set_tuplefield_int2(&row->tuple[6], (Int2) SQL_INDEX_OTHER);
		set_tuplefield_int2(&row->tuple[7], (Int2) 1);

		set_tuplefield_string(&row->tuple[8], "oid");
		set_tuplefield_string(&row->tuple[9], "A");
		set_tuplefield_null(&row->tuple[10]);
		set_tuplefield_null(&row->tuple[11]);
		set_tuplefield_null(&row->tuple[12]);

		QR_add_tuple(res, row);
	}

	while ((result == SQL_SUCCESS) || (result == SQL_SUCCESS_WITH_INFO))
	{
		/* If only requesting unique indexs, then just return those. */
		if (fUnique == SQL_INDEX_ALL ||
			(fUnique == SQL_INDEX_UNIQUE && atoi(isunique)))
		{
			i = 0;
			/* add a row in this table for each field in the index */
			while (i < INDEX_KEYS_STORAGE_COUNT && fields_vector[i] != 0)
			{
				row = (TupleNode *) malloc(sizeof(TupleNode) +
										   (13 - 1) *sizeof(TupleField));

				/* no table qualifier */
				set_tuplefield_string(&row->tuple[0], "");
				/* don't set the table owner, else Access tries to use it */
				set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(table_qualifier));
				set_tuplefield_string(&row->tuple[2], table_name);

				/* non-unique index? */
				if (ci->drivers.unique_index)
					set_tuplefield_int2(&row->tuple[3], (Int2) (atoi(isunique) ? FALSE : TRUE));
				else
					set_tuplefield_int2(&row->tuple[3], TRUE);

				/* no index qualifier */
				set_tuplefield_string(&row->tuple[4], "");
				set_tuplefield_string(&row->tuple[5], index_name);

				/*
				 * Clustered/HASH index?
				 */
				set_tuplefield_int2(&row->tuple[6], (Int2)
							   (atoi(isclustered) ? SQL_INDEX_CLUSTERED :
								(!strncmp(ishash, "hash", 4)) ? SQL_INDEX_HASHED : SQL_INDEX_OTHER));
				set_tuplefield_int2(&row->tuple[7], (Int2) (i + 1));

				if (fields_vector[i] == OID_ATTNUM)
				{
					set_tuplefield_string(&row->tuple[8], "oid");
					mylog("%s: column name = oid\n", func);
				}
				else if (fields_vector[i] < 0 || fields_vector[i] > total_columns)
				{
					set_tuplefield_string(&row->tuple[8], "UNKNOWN");
					mylog("%s: column name = UNKNOWN\n", func);
				}
				else
				{
					set_tuplefield_string(&row->tuple[8], column_names[fields_vector[i] - 1]);
					mylog("%s: column name = '%s'\n", func, column_names[fields_vector[i] - 1]);
				}

				set_tuplefield_string(&row->tuple[9], "A");
				set_tuplefield_null(&row->tuple[10]);
				set_tuplefield_null(&row->tuple[11]);
				set_tuplefield_null(&row->tuple[12]);

				QR_add_tuple(res, row);
				i++;
			}
		}

		result = PGAPI_Fetch(hindx_stmt);
	}
	if (result != SQL_NO_DATA_FOUND)
	{
		/* "SQLFetch failed in SQLStatistics."; */
		SC_full_error_copy(stmt, indx_stmt);
		goto cleanup;
	}
	ret = SQL_SUCCESS;

cleanup:
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	if (SQL_ERROR == ret)
		SC_log_error(func, "", stmt);
	if (hcol_stmt)
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	if (hindx_stmt)
		PGAPI_FreeStmt(hindx_stmt, SQL_DROP);
	/* These things should be freed on any error ALSO! */
	if (table_name)
		free(table_name);
	if (column_names)
	{
		for (i = 0; i < total_columns; i++)
			free(column_names[i]);
		free(column_names);
	}

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);

	mylog("%s: EXIT, stmt=%u, ret=%d\n", func, stmt, ret);

	return ret;
}


RETCODE		SQL_API
PGAPI_ColumnPrivileges(
					   HSTMT hstmt,
					   UCHAR FAR * szTableQualifier,
					   SWORD cbTableQualifier,
					   UCHAR FAR * szTableOwner,
					   SWORD cbTableOwner,
					   UCHAR FAR * szTableName,
					   SWORD cbTableName,
					   UCHAR FAR * szColumnName,
					   SWORD cbColumnName)
{
	CSTR func = "PGAPI_ColumnPrivileges";
	StatementClass	*stmt = (StatementClass *) hstmt;
	RETCODE	result;

	mylog("%s: entering...\n", func);

	/* Neither Access or Borland care about this. */

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;
	SC_set_error(stmt, STMT_NOT_IMPLEMENTED_ERROR, "not implemented");
	SC_log_error(func, "Function not implemented", stmt);
	return SQL_ERROR;
}


/*
 *	SQLPrimaryKeys()
 *
 *	Retrieve the primary key columns for the specified table.
 */
RETCODE		SQL_API
PGAPI_PrimaryKeys(
				  HSTMT hstmt,
				  UCHAR FAR * szTableQualifier,
				  SWORD cbTableQualifier,
				  UCHAR FAR * szTableOwner,
				  SWORD cbTableOwner,
				  UCHAR FAR * szTableName,
				  SWORD cbTableName)
{
	CSTR func = "PGAPI_PrimaryKeys";
	StatementClass *stmt = (StatementClass *) hstmt;
	QResultClass	*res;
	ConnectionClass *conn;
	TupleNode  *row;
	RETCODE		ret = SQL_ERROR, result;
	int			seq = 0;
	HSTMT		htbl_stmt = NULL;
	StatementClass *tbl_stmt;
	char		tables_query[INFO_INQUIRY_LEN];
	char		attname[MAX_INFO_STRING];
	SDWORD		attname_len;
	char	   *pktab;
	char		pkscm[TABLE_NAME_STORAGE_LEN + 1];
	Int2		result_cols;
	int			qno,
				qstart,
				qend;
	SWORD		internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char	*szSchemaName;

	mylog("%s: entering...stmt=%u scnm=%x len=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;
	stmt->manual_result = TRUE;
	stmt->errormsg_created = TRUE;

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_PrimaryKeys result.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until */

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	result_cols = 6;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info(res, 0, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 1, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 2, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 3, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 4, "KEY_SEQ", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 5, "PK_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);


	result = PGAPI_AllocStmt(stmt->hdbc, &htbl_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for Primary Key result.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	tbl_stmt = (StatementClass *) htbl_stmt;

	conn = SC_get_conn(stmt);
#ifdef  UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif
	pktab = make_string(szTableName, cbTableName, NULL, 0);
	if (pktab == NULL || pktab[0] == '\0')
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "No Table specified to PGAPI_PrimaryKeys.");
		goto cleanup;
	}
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

retry_public_schema:
	pkscm[0] = '\0';
	if (conn->schema_support)
		schema_strcat(pkscm, "%.*s", szSchemaName, cbSchemaName, szTableName, cbTableName, conn);

	result = PGAPI_BindCol(htbl_stmt, 1, internal_asis_type,
						   attname, MAX_INFO_STRING, &attname_len);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, tbl_stmt);
		goto cleanup;
	}

	if (PG_VERSION_LE(conn, 6.4))
		qstart = 2;
	else
		qstart = 1;
	qend = 2;
	for (qno = qstart; qno <= qend; qno++)
	{
		switch (qno)
		{
			case 1:

				/*
				 * Simplified query to remove assumptions about number of
				 * possible index columns. Courtesy of Tom Lane - thomas
				 * 2000-03-21
				 */
				if (conn->schema_support)
					sprintf(tables_query, "select ta.attname, ia.attnum"
						" from pg_catalog.pg_attribute ta,"
						" pg_catalog.pg_attribute ia, pg_catalog.pg_class c,"
						" pg_catalog.pg_index i, pg_catalog.pg_namespace n"
						" where c.relname = '%s'"
						" AND n.nspname = '%s'"
						" AND c.oid = i.indrelid"
						" AND n.oid = c.relnamespace"
						" AND i.indisprimary = 't'"
						" AND ia.attrelid = i.indexrelid"
						" AND ta.attrelid = i.indrelid"
						" AND ta.attnum = i.indkey[ia.attnum-1]"
						" AND (NOT ta.attisdropped)"
						" AND (NOT ia.attisdropped)"
						" order by ia.attnum", pktab, pkscm);
				else
					sprintf(tables_query, "select ta.attname, ia.attnum"
						" from pg_attribute ta, pg_attribute ia, pg_class c, pg_index i"
						" where c.relname = '%s'"
						" AND c.oid = i.indrelid"
						" AND i.indisprimary = 't'"
						" AND ia.attrelid = i.indexrelid"
						" AND ta.attrelid = i.indrelid"
						" AND ta.attnum = i.indkey[ia.attnum-1]"
						" order by ia.attnum", pktab);
				break;
			case 2:

				/*
				 * Simplified query to search old fashoned primary key
				 */
				if (conn->schema_support)
					sprintf(tables_query, "select ta.attname, ia.attnum"
						" from pg_catalog.pg_attribute ta,"
						" pg_catalog.pg_attribute ia, pg_catalog.pg_class c,"
						" pg_catalog.pg_index i, pg_catalog.pg_namespace n"
						" where c.relname = '%s_pkey'"
						" AND n.nspname = '%s'"
						" AND c.oid = i.indexrelid"
						" AND n.oid = c.relnamespace"
						" AND ia.attrelid = i.indexrelid"
						" AND ta.attrelid = i.indrelid"
						" AND ta.attnum = i.indkey[ia.attnum-1]"
						" AND (NOT ta.attisdropped)"
						" AND (NOT ia.attisdropped)"
						" order by ia.attnum", pktab, pkscm);
				else
					sprintf(tables_query, "select ta.attname, ia.attnum"
						" from pg_attribute ta, pg_attribute ia, pg_class c, pg_index i"
						" where c.relname = '%s_pkey'"
						" AND c.oid = i.indexrelid"
						" AND ia.attrelid = i.indexrelid"
						" AND ta.attrelid = i.indrelid"
						" AND ta.attnum = i.indkey[ia.attnum-1]"
						" order by ia.attnum", pktab);
				break;
		}
		mylog("%s: tables_query='%s'\n", func, tables_query);

		result = PGAPI_ExecDirect(htbl_stmt, tables_query, SQL_NTS, 0);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_full_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_Fetch(htbl_stmt);
		if (result != SQL_NO_DATA_FOUND)
			break;
	}

	/* If not found */
	if (conn->schema_support &&
	    SQL_NO_DATA_FOUND == result)
	{
		const char *user = CC_get_username(conn);

		/*
		 * If specified schema name == user_name and
		 * the current schema is 'public',
		 * retry the 'public' schema.
		 */
		if (szSchemaName &&
		    (cbSchemaName == SQL_NTS ||
		     cbSchemaName == (SWORD) strlen(user)) &&
		    strnicmp(szSchemaName, user, strlen(user)) == 0 &&
		    stricmp(CC_get_current_schema(conn), pubstr) == 0)
		{
			szSchemaName = pubstr;
			cbSchemaName = SQL_NTS;
			goto retry_public_schema;
		}
	}

	while ((result == SQL_SUCCESS) || (result == SQL_SUCCESS_WITH_INFO))
	{
		row = (TupleNode *) malloc(sizeof(TupleNode) + (result_cols - 1) *sizeof(TupleField));

		set_tuplefield_null(&row->tuple[0]);

		/*
		 * I have to hide the table owner from Access, otherwise it
		 * insists on referring to the table as 'owner.table'. (this is
		 * valid according to the ODBC SQL grammar, but Postgres won't
		 * support it.)
		 */
		set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(pkscm));
		set_tuplefield_string(&row->tuple[2], pktab);
		set_tuplefield_string(&row->tuple[3], attname);
		set_tuplefield_int2(&row->tuple[4], (Int2) (++seq));
		set_tuplefield_null(&row->tuple[5]);

		QR_add_tuple(res, row);

		mylog(">> primaryKeys: pktab = '%s', attname = '%s', seq = %d\n", pktab, attname, seq);

		result = PGAPI_Fetch(htbl_stmt);
	}

	if (result != SQL_NO_DATA_FOUND)
	{
		SC_full_error_copy(stmt, htbl_stmt);
		goto cleanup;
	}
	ret = SQL_SUCCESS;

cleanup:
	if (pktab)
		free(pktab);

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	if (SQL_ERROR == ret)
		SC_log_error(func, "", stmt);
	if (htbl_stmt)
		PGAPI_FreeStmt(htbl_stmt, SQL_DROP);

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);

	mylog("%s: EXIT, stmt=%u, ret=%d\n", func, stmt, ret);
	return ret;
}


/*
 *	Multibyte support stuff for SQLForeignKeys().
 *	There may be much more effective way in the
 *	future version. The way is very forcible currently.
 */
static BOOL
isMultibyte(const UCHAR *str)
{
	for (; *str; str++)
	{
		if (*str >= 0x80)
			return TRUE;
	}
	return FALSE;
}
#ifdef NOT_USED
static char *
getClientTableName(ConnectionClass *conn, const char *serverSchemaName, char *serverTableName, BOOL *nameAlloced)
{
	char		query[1024],
				saveoid[24],
			   *ret = serverTableName;
	BOOL		continueExec = TRUE,
				bError = FALSE;
	QResultClass *res;

	*nameAlloced = FALSE;
	if (!conn->client_encoding || !isMultibyte(serverTableName))
		return ret;
	if (!conn->server_encoding)
	{
		if (res = CC_send_query(conn, "select getdatabaseencoding()", NULL, CLEAR_RESULT_ON_ABORT), res)
		{
			if (QR_get_num_tuples(res) > 0)
				conn->server_encoding = strdup(QR_get_value_backend_row(res, 0, 0));
			QR_Destructor(res);
		}
	}
	if (!conn->server_encoding)
		return ret;
	sprintf(query, "SET CLIENT_ENCODING TO '%s'", conn->server_encoding);
	bError = (CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT) == NULL);
	if (!bError && continueExec)
	{
		if (conn->schema_support)
			sprintf(query, "select OID from pg_catalog.pg_class,"
			" pg_catalog.pg_namespace"
			" where relname = '%s' and pg_namespace.oid = relnamespace and"
			" pg_namespace.nspname = '%s'", serverTableName, serverSchemaName);
		else
			sprintf(query, "select OID from pg_class where relname = '%s'", serverTableName);
		if (res = CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT), res)
		{
			if (QR_get_num_tuples(res) > 0)
				strcpy(saveoid, QR_get_value_backend_row(res, 0, 0));
			else
				continueExec = FALSE;
			QR_Destructor(res);
		}
		else
			bError = TRUE;
	}
	continueExec = (continueExec && !bError);
	if (bError && CC_is_in_trans(conn))
	{
		CC_abort(conn);
		bError = FALSE;
	}
	/* restore the client encoding */
	sprintf(query, "SET CLIENT_ENCODING TO '%s'", conn->client_encoding);
	bError = (CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT) == NULL);
	if (bError || !continueExec)
		return ret;
	sprintf(query, "select relname from pg_class where OID = %s", saveoid);
	if (res = CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT), res)
	{
		if (QR_get_num_tuples(res) > 0)
		{
			ret = strdup(QR_get_value_backend_row(res, 0, 0));
			*nameAlloced = TRUE;
		}
		QR_Destructor(res);
	}
	return ret;
}
static char *
getClientColumnName(ConnectionClass *conn, const char * serverSchemaName, const char *serverTableName, char *serverColumnName, BOOL *nameAlloced)
{
	char		query[1024],
				saveattrelid[24],
				saveattnum[16],
			   *ret = serverColumnName;
	BOOL		continueExec = TRUE,
				bError = FALSE;
	QResultClass *res;

	*nameAlloced = FALSE;
	if (!conn->client_encoding || !isMultibyte(serverColumnName))
		return ret;
	if (!conn->server_encoding)
	{
		if (res = CC_send_query(conn, "select getdatabaseencoding()", NULL, CLEAR_RESULT_ON_ABORT), res)
		{
			if (QR_get_num_tuples(res) > 0)
				conn->server_encoding = strdup(QR_get_value_backend_row(res, 0, 0));
			QR_Destructor(res);
		}
	}
	if (!conn->server_encoding)
		return ret;
	sprintf(query, "SET CLIENT_ENCODING TO '%s'", conn->server_encoding);
	bError = (CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT) == NULL);
	if (!bError && continueExec)
	{
		if (conn->schema_support)
			sprintf(query, "select attrelid, attnum from pg_catalog.pg_class,"
			" pg_catalog.pg_attribute, pg_catalog.pg_namespace "
				"where relname = '%s' and attrelid = pg_class.oid "
				"and (not attisdropped) "
				"and attname = '%s' and pg_namespace.oid = relnamespace and"
				" pg_namespace.nspname = '%s'", serverTableName, serverColumnName, serverSchemaName);
		else
			sprintf(query, "select attrelid, attnum from pg_class, pg_attribute "
				"where relname = '%s' and attrelid = pg_class.oid "
				"and attname = '%s'", serverTableName, serverColumnName);
		if (res = CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT), res)
		{
			if (QR_get_num_tuples(res) > 0)
			{
				strcpy(saveattrelid, QR_get_value_backend_row(res, 0, 0));
				strcpy(saveattnum, QR_get_value_backend_row(res, 0, 1));
			}
			else
				continueExec = FALSE;
			QR_Destructor(res);
		}
		else
			bError = TRUE;
	}
	continueExec = (continueExec && !bError);
	if (bError && CC_is_in_trans(conn))
	{
		CC_abort(conn);
		bError = FALSE;
	}
	/* restore the cleint encoding */
	sprintf(query, "SET CLIENT_ENCODING TO '%s'", conn->client_encoding);
	bError = (CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT) == NULL);
	if (bError || !continueExec)
		return ret;
	sprintf(query, "select attname from pg_attribute where attrelid = %s and attnum = %s", saveattrelid, saveattnum);
	if (res = CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT), res)
	{
		if (QR_get_num_tuples(res) > 0)
		{
			ret = strdup(QR_get_value_backend_row(res, 0, 0));
			*nameAlloced = TRUE;
		}
		QR_Destructor(res);
	}
	return ret;
}
#endif /* NOT_USED */
static char *
getClientColumnName(ConnectionClass *conn, UInt4 relid, char *serverColumnName, BOOL *nameAlloced)
{
	char		query[1024], saveattnum[16],
			   *ret = serverColumnName;
	BOOL		continueExec = TRUE,
				bError = FALSE;
	QResultClass *res;

	*nameAlloced = FALSE;
	if (!conn->client_encoding || !isMultibyte(serverColumnName))
		return ret;
	if (!conn->server_encoding)
	{
		if (res = CC_send_query(conn, "select getdatabaseencoding()", NULL, CLEAR_RESULT_ON_ABORT), res)
		{
			if (QR_get_num_backend_tuples(res) > 0)
				conn->server_encoding = strdup(QR_get_value_backend_row(res, 0, 0));
			QR_Destructor(res);
		}
	}
	if (!conn->server_encoding)
		return ret;
	sprintf(query, "SET CLIENT_ENCODING TO '%s'", conn->server_encoding);
	bError = (CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT) == NULL);
	if (!bError && continueExec)
	{
		sprintf(query, "select attnum from pg_attribute "
			"where attrelid = %u and attname = '%s'",
			relid, serverColumnName);
		if (res = CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT), res)
		{
			if (QR_get_num_backend_tuples(res) > 0)
			{
				strcpy(saveattnum, QR_get_value_backend_row(res, 0, 0));
			}
			else
				continueExec = FALSE;
			QR_Destructor(res);
		}
		else
			bError = TRUE;
	}
	continueExec = (continueExec && !bError);
	if (bError && CC_is_in_trans(conn))
	{
		CC_abort(conn);
		bError = FALSE;
	}
	/* restore the cleint encoding */
	sprintf(query, "SET CLIENT_ENCODING TO '%s'", conn->client_encoding);
	bError = (CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT) == NULL);
	if (bError || !continueExec)
		return ret;
	sprintf(query, "select attname from pg_attribute where attrelid = %u and attnum = %s", relid, saveattnum);
	if (res = CC_send_query(conn, query, NULL, CLEAR_RESULT_ON_ABORT), res)
	{
		if (QR_get_num_backend_tuples(res) > 0)
		{
			ret = strdup(QR_get_value_backend_row(res, 0, 0));
			*nameAlloced = TRUE;
		}
		QR_Destructor(res);
	}
	return ret;
}

RETCODE		SQL_API
PGAPI_ForeignKeys(
				  HSTMT hstmt,
				  UCHAR FAR * szPkTableQualifier,
				  SWORD cbPkTableQualifier,
				  UCHAR FAR * szPkTableOwner,
				  SWORD cbPkTableOwner,
				  UCHAR FAR * szPkTableName,
				  SWORD cbPkTableName,
				  UCHAR FAR * szFkTableQualifier,
				  SWORD cbFkTableQualifier,
				  UCHAR FAR * szFkTableOwner,
				  SWORD cbFkTableOwner,
				  UCHAR FAR * szFkTableName,
				  SWORD cbFkTableName)
{
	CSTR func = "PGAPI_ForeignKeys";
	StatementClass *stmt = (StatementClass *) hstmt;
	QResultClass	*res;
	TupleNode  *row;
	HSTMT		htbl_stmt = NULL, hpkey_stmt = NULL;
	StatementClass *tbl_stmt;
	RETCODE		ret = SQL_ERROR, result, keyresult;
	char		tables_query[INFO_INQUIRY_LEN];
	char		trig_deferrable[2];
	char		trig_initdeferred[2];
	char		trig_args[1024];
	char		upd_rule[TABLE_NAME_STORAGE_LEN],
				del_rule[TABLE_NAME_STORAGE_LEN];
	char	   *pk_table_needed;
	char		fk_table_fetched[TABLE_NAME_STORAGE_LEN + 1];
	char	   *fk_table_needed;
	char		pk_table_fetched[TABLE_NAME_STORAGE_LEN + 1];
	char		schema_needed[SCHEMA_NAME_STORAGE_LEN + 1];
	char		schema_fetched[SCHEMA_NAME_STORAGE_LEN + 1];
	char	   *pkey_ptr,
			   *pkey_text,
			   *fkey_ptr,
			   *fkey_text;

	ConnectionClass *conn;
	BOOL		pkey_alloced,
				fkey_alloced;
	int			i,
				j,
				k,
				num_keys;
	SWORD		trig_nargs,
				upd_rule_type = 0,
				del_rule_type = 0;
	SWORD		internal_asis_type = SQL_C_CHAR;
	SWORD		defer_type;
	char		pkey[MAX_INFO_STRING];
	Int2		result_cols;
	UInt4		relid1, relid2;

	mylog("%s: entering...stmt=%u\n", func, stmt);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	stmt->manual_result = TRUE;
	stmt->errormsg_created = TRUE;

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_ForeignKeys result.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until */

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	result_cols = 15;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info(res, 0, "PKTABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 1, "PKTABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 2, "PKTABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 3, "PKCOLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 4, "FKTABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 5, "FKTABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 6, "FKTABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 7, "FKCOLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 8, "KEY_SEQ", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 9, "UPDATE_RULE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 10, "DELETE_RULE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 11, "FK_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 12, "PK_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 13, "TRIGGER_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 14, "DEFERRABILITY", PG_TYPE_INT2, 2);

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);


	result = PGAPI_AllocStmt(stmt->hdbc, &htbl_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_ForeignKeys result.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}

	tbl_stmt = (StatementClass *) htbl_stmt;

	schema_needed[0] = '\0';
	schema_fetched[0] = '\0';

	pk_table_needed = make_string(szPkTableName, cbPkTableName, NULL, 0);
	fk_table_needed = make_string(szFkTableName, cbFkTableName, NULL, 0);

	conn = SC_get_conn(stmt);

	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;

	pkey_text = fkey_text = NULL;
	pkey_alloced = fkey_alloced = FALSE;

	/*
	 * Case #2 -- Get the foreign keys in the specified table (fktab) that
	 * refer to the primary keys of other table(s).
	 */
	if (fk_table_needed && fk_table_needed[0] != '\0')
	{
		mylog("%s: entering Foreign Key Case #2", func);
		if (conn->schema_support)
		{
			schema_strcat(schema_needed, "%.*s", szFkTableOwner, cbFkTableOwner, szFkTableName, cbFkTableName, conn);
			sprintf(tables_query, "SELECT	pt.tgargs, "
				"		pt.tgnargs, "
				"		pt.tgdeferrable, "
				"		pt.tginitdeferred, "
				"		pp1.proname, "
				"		pp2.proname, "
				"		pc.oid, "
				"		pc1.oid, "
				"		pc1.relname, "
				"		pn.nspname "
				"FROM	pg_catalog.pg_class pc, "
				"		pg_catalog.pg_proc pp1, "
				"		pg_catalog.pg_proc pp2, "
				"		pg_catalog.pg_trigger pt1, "
				"		pg_catalog.pg_trigger pt2, "
				"		pg_catalog.pg_proc pp, "
				"		pg_catalog.pg_trigger pt, "
				"		pg_catalog.pg_class pc1, "
				"		pg_catalog.pg_namespace pn, "
				"		pg_catalog.pg_namespace pn1 "
				"WHERE	pt.tgrelid = pc.oid "
				"AND pp.oid = pt.tgfoid "
				"AND pt1.tgconstrrelid = pc.oid "
				"AND pp1.oid = pt1.tgfoid "
				"AND pt2.tgfoid = pp2.oid "
				"AND pt2.tgconstrrelid = pc.oid "
				"AND ((pc.relname='%s') "
				"AND (pn1.oid = pc.relnamespace) "
				"AND (pn1.nspname = '%s') "
				"AND (pp.proname LIKE '%%ins') "
				"AND (pp1.proname LIKE '%%upd') "
				"AND (pp2.proname LIKE '%%del') "
				"AND (pt1.tgrelid=pt.tgconstrrelid) "
				"AND (pt1.tgconstrname=pt.tgconstrname) "
				"AND (pt2.tgrelid=pt.tgconstrrelid) "
				"AND (pt2.tgconstrname=pt.tgconstrname) "
				"AND (pt.tgconstrrelid=pc1.oid) "
				"AND (pc1.relnamespace=pn.oid))",
				fk_table_needed, schema_needed);
		}
		else
			sprintf(tables_query, "SELECT	pt.tgargs, "
				"		pt.tgnargs, "
				"		pt.tgdeferrable, "
				"		pt.tginitdeferred, "
				"		pp1.proname, "
				"		pp2.proname, "
				"		pc.oid, "
				"		pc1.oid, "
				"		pc1.relname "
				"FROM	pg_class pc, "
				"		pg_proc pp1, "
				"		pg_proc pp2, "
				"		pg_trigger pt1, "
				"		pg_trigger pt2, "
				"		pg_proc pp, "
				"		pg_trigger pt, "
				"		pg_class pc1 "
				"WHERE	pt.tgrelid = pc.oid "
				"AND pp.oid = pt.tgfoid "
				"AND pt1.tgconstrrelid = pc.oid "
				"AND pp1.oid = pt1.tgfoid "
				"AND pt2.tgfoid = pp2.oid "
				"AND pt2.tgconstrrelid = pc.oid "
				"AND ((pc.relname='%s') "
				"AND (pp.proname LIKE '%%ins') "
				"AND (pp1.proname LIKE '%%upd') "
				"AND (pp2.proname LIKE '%%del') "
				"AND (pt1.tgrelid=pt.tgconstrrelid) "
				"AND (pt1.tgconstrname=pt.tgconstrname) "
				"AND (pt2.tgrelid=pt.tgconstrrelid) "
				"AND (pt2.tgconstrname=pt.tgconstrname) "
				"AND (pt.tgconstrrelid=pc1.oid)) ",
				fk_table_needed);

		result = PGAPI_ExecDirect(htbl_stmt, tables_query, SQL_NTS, 0);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_full_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 1, SQL_C_BINARY,
							   trig_args, sizeof(trig_args), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 2, SQL_C_SHORT,
							   &trig_nargs, 0, NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
						 trig_deferrable, sizeof(trig_deferrable), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 4, internal_asis_type,
					 trig_initdeferred, sizeof(trig_initdeferred), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 5, internal_asis_type,
							   upd_rule, sizeof(upd_rule), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 6, internal_asis_type,
							   del_rule, sizeof(del_rule), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 7, SQL_C_ULONG,
							   &relid1, sizeof(relid1), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 8, SQL_C_ULONG,
							   &relid2, sizeof(relid2), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 9, internal_asis_type,
					pk_table_fetched, TABLE_NAME_STORAGE_LEN, NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		if (conn->schema_support)
		{
			result = PGAPI_BindCol(htbl_stmt, 10, internal_asis_type,
					schema_fetched, SCHEMA_NAME_STORAGE_LEN, NULL);
			if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
			{
				SC_error_copy(stmt, tbl_stmt);
				goto cleanup;
			}
		}

		result = PGAPI_Fetch(htbl_stmt);
		if (result == SQL_NO_DATA_FOUND)
		{
			ret = SQL_SUCCESS;
			goto cleanup;
		}

		if (result != SQL_SUCCESS)
		{
			SC_full_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		keyresult = PGAPI_AllocStmt(stmt->hdbc, &hpkey_stmt);
		if ((keyresult != SQL_SUCCESS) && (keyresult != SQL_SUCCESS_WITH_INFO))
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_ForeignKeys (pkeys) result.");
			goto cleanup;
		}

		keyresult = PGAPI_BindCol(hpkey_stmt, 4, internal_asis_type,
								  pkey, sizeof(pkey), NULL);
		if (keyresult != SQL_SUCCESS)
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't bindcol for primary keys for PGAPI_ForeignKeys result.");
			goto cleanup;
		}

		while (result == SQL_SUCCESS)
		{
			/* Compute the number of keyparts. */
			num_keys = (trig_nargs - 4) / 2;

			mylog("Foreign Key Case#2: trig_nargs = %d, num_keys = %d\n", trig_nargs, num_keys);

			/* If there is a pk table specified, then check it. */
			if (pk_table_needed && pk_table_needed[0] != '\0')
			{
				/* If it doesn't match, then continue */
				if (strcmp(pk_table_fetched, pk_table_needed))
				{
					result = PGAPI_Fetch(htbl_stmt);
					continue;
				}
			}

			keyresult = PGAPI_PrimaryKeys(hpkey_stmt, NULL, 0, schema_fetched, SQL_NTS, pk_table_fetched, SQL_NTS);
			if (keyresult != SQL_SUCCESS)
			{
				SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't get primary keys for PGAPI_ForeignKeys result.");
				goto cleanup;
			}


			/* Get to first primary key */
			pkey_ptr = trig_args;
			for (i = 0; i < 5; i++)
				pkey_ptr += strlen(pkey_ptr) + 1;

			for (k = 0; k < num_keys; k++)
			{
				/* Check that the key listed is the primary key */
				keyresult = PGAPI_Fetch(hpkey_stmt);
				if (keyresult != SQL_SUCCESS)
				{
					num_keys = 0;
					break;
				}
				pkey_text = getClientColumnName(conn, relid2, pkey_ptr, &pkey_alloced);
				mylog("%s: pkey_ptr='%s', pkey='%s'\n", func, pkey_text, pkey);
				if (strcmp(pkey_text, pkey))
				{
					num_keys = 0;
					break;
				}
				if (pkey_alloced)
					free(pkey_text);
				/* Get to next primary key */
				for (k = 0; k < 2; k++)
					pkey_ptr += strlen(pkey_ptr) + 1;

			}

			/* Set to first fk column */
			fkey_ptr = trig_args;
			for (k = 0; k < 4; k++)
				fkey_ptr += strlen(fkey_ptr) + 1;

			/* Set update and delete actions for foreign keys */
			if (!strcmp(upd_rule, "RI_FKey_cascade_upd"))
				upd_rule_type = SQL_CASCADE;
			else if (!strcmp(upd_rule, "RI_FKey_noaction_upd"))
				upd_rule_type = SQL_NO_ACTION;
			else if (!strcmp(upd_rule, "RI_FKey_restrict_upd"))
				upd_rule_type = SQL_NO_ACTION;
			else if (!strcmp(upd_rule, "RI_FKey_setdefault_upd"))
				upd_rule_type = SQL_SET_DEFAULT;
			else if (!strcmp(upd_rule, "RI_FKey_setnull_upd"))
				upd_rule_type = SQL_SET_NULL;

			if (!strcmp(upd_rule, "RI_FKey_cascade_del"))
				del_rule_type = SQL_CASCADE;
			else if (!strcmp(upd_rule, "RI_FKey_noaction_del"))
				del_rule_type = SQL_NO_ACTION;
			else if (!strcmp(upd_rule, "RI_FKey_restrict_del"))
				del_rule_type = SQL_NO_ACTION;
			else if (!strcmp(upd_rule, "RI_FKey_setdefault_del"))
				del_rule_type = SQL_SET_DEFAULT;
			else if (!strcmp(upd_rule, "RI_FKey_setnull_del"))
				del_rule_type = SQL_SET_NULL;

			/* Set deferrability type */
			if (!strcmp(trig_initdeferred, "y"))
				defer_type = SQL_INITIALLY_DEFERRED;
			else if (!strcmp(trig_deferrable, "y"))
				defer_type = SQL_INITIALLY_IMMEDIATE;
			else
				defer_type = SQL_NOT_DEFERRABLE;

			/* Get to first primary key */
			pkey_ptr = trig_args;
			for (i = 0; i < 5; i++)
				pkey_ptr += strlen(pkey_ptr) + 1;

			for (k = 0; k < num_keys; k++)
			{
				row = (TupleNode *) malloc(sizeof(TupleNode) + (result_cols - 1) *sizeof(TupleField));

				pkey_text = getClientColumnName(conn, relid2, pkey_ptr, &pkey_alloced);
				fkey_text = getClientColumnName(conn, relid1, fkey_ptr, &fkey_alloced);

				mylog("%s: pk_table = '%s', pkey_ptr = '%s'\n", func, pk_table_fetched, pkey_text);
				set_tuplefield_null(&row->tuple[0]);
				set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(schema_fetched));
				set_tuplefield_string(&row->tuple[2], pk_table_fetched);
				set_tuplefield_string(&row->tuple[3], pkey_text);

				mylog("%s: fk_table_needed = '%s', fkey_ptr = '%s'\n", func, fk_table_needed, fkey_text);
				set_tuplefield_null(&row->tuple[4]);
				set_tuplefield_string(&row->tuple[5], GET_SCHEMA_NAME(schema_needed));
				set_tuplefield_string(&row->tuple[6], fk_table_needed);
				set_tuplefield_string(&row->tuple[7], fkey_text);

				mylog("%s: upd_rule_type = '%i', del_rule_type = '%i'\n, trig_name = '%s'", func, upd_rule_type, del_rule_type, trig_args);
				set_tuplefield_int2(&row->tuple[8], (Int2) (k + 1));
				set_tuplefield_int2(&row->tuple[9], (Int2) upd_rule_type);
				set_tuplefield_int2(&row->tuple[10], (Int2) del_rule_type);
				set_tuplefield_null(&row->tuple[11]);
				set_tuplefield_null(&row->tuple[12]);
				set_tuplefield_string(&row->tuple[13], trig_args);
				set_tuplefield_int2(&row->tuple[14], defer_type);

				QR_add_tuple(res, row);
				if (fkey_alloced)
					free(fkey_text);
				fkey_alloced = FALSE;
				if (pkey_alloced)
					free(pkey_text);
				pkey_alloced = FALSE;
				/* next primary/foreign key */
				for (i = 0; i < 2; i++)
				{
					fkey_ptr += strlen(fkey_ptr) + 1;
					pkey_ptr += strlen(pkey_ptr) + 1;
				}
			}

			result = PGAPI_Fetch(htbl_stmt);
		}
		PGAPI_FreeStmt(hpkey_stmt, SQL_DROP);
		hpkey_stmt = NULL;
	}

	/*
	 * Case #1 -- Get the foreign keys in other tables that refer to the
	 * primary key in the specified table (pktab).	i.e., Who points to
	 * me?
	 */
	else if (pk_table_needed[0] != '\0')
	{
		if (conn->schema_support)
		{
			schema_strcat(schema_needed, "%.*s", szPkTableOwner, cbPkTableOwner, szPkTableName, cbPkTableName, conn);
			sprintf(tables_query, "SELECT	pt.tgargs, "
				"		pt.tgnargs, "
				"		pt.tgdeferrable, "
				"		pt.tginitdeferred, "
				"		pp.proname, "
				"		pp1.proname, "
				"		pc.oid, "
				"		pc1.oid, "
				"		pc1.relname, "
				"		pn.nspname "
				"FROM	pg_catalog.pg_class pc, "
				"		pg_catalog.pg_class pc1, "
				"		pg_catalog.pg_class pc2, "
				"		pg_catalog.pg_proc pp, "
				"		pg_catalog.pg_proc pp1, "
				"		pg_catalog.pg_trigger pt, "
				"		pg_catalog.pg_trigger pt1, "
				"		pg_catalog.pg_trigger pt2, "
				"		pg_catalog.pg_namespace pn, "
				"		pg_catalog.pg_namespace pn1 "
				"WHERE	pt.tgconstrrelid = pc.oid "
				"	AND pt.tgrelid = pc1.oid "
				"	AND pt1.tgfoid = pp1.oid "
				"	AND pt1.tgconstrrelid = pc1.oid "
				"	AND pt2.tgconstrrelid = pc2.oid "
				"	AND pt2.tgfoid = pp.oid "
				"	AND pc2.oid = pt.tgrelid "
				"	AND ("
				"		 (pc.relname='%s') "
				"	AND  (pn1.oid = pc.relnamespace) "
				"	AND  (pn1.nspname = '%s') "
				"	AND  (pp.proname Like '%%upd') "
				"	AND  (pp1.proname Like '%%del')"
				"	AND	 (pt1.tgrelid = pt.tgconstrrelid) "
				"	AND	 (pt2.tgrelid = pt.tgconstrrelid) "
				"	AND (pn.oid = pc1.relnamespace) "
				"		)",
				pk_table_needed, schema_needed);
		}
		else
			sprintf(tables_query, "SELECT	pt.tgargs, "
				"		pt.tgnargs, "
				"		pt.tgdeferrable, "
				"		pt.tginitdeferred, "
				"		pp.proname, "
				"		pp1.proname, "
				"		pc.oid, "
				"		pc1.oid, "
				"		pc1.relname "
				"FROM	pg_class pc, "
				"		pg_class pc1, "
				"		pg_class pc2, "
				"		pg_proc pp, "
				"		pg_proc pp1, "
				"		pg_trigger pt, "
				"		pg_trigger pt1, "
				"		pg_trigger pt2 "
				"WHERE	pt.tgconstrrelid = pc.oid "
				"	AND pt.tgrelid = pc1.oid "
				"	AND pt1.tgfoid = pp1.oid "
				"	AND pt1.tgconstrrelid = pc1.oid "
				"	AND pt2.tgconstrrelid = pc2.oid "
				"	AND pt2.tgfoid = pp.oid "
				"	AND pc2.oid = pt.tgrelid "
				"	AND ("
				"		 (pc.relname='%s') "
				"	AND  (pp.proname Like '%%upd') "
				"	AND  (pp1.proname Like '%%del')"
				"	AND	 (pt1.tgrelid = pt.tgconstrrelid) "
				"	AND	 (pt2.tgrelid = pt.tgconstrrelid) "
				"		)",
				pk_table_needed);

		result = PGAPI_ExecDirect(htbl_stmt, tables_query, SQL_NTS, 0);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 1, SQL_C_BINARY,
							   trig_args, sizeof(trig_args), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 2, SQL_C_SHORT,
							   &trig_nargs, 0, NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
						 trig_deferrable, sizeof(trig_deferrable), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 4, internal_asis_type,
					 trig_initdeferred, sizeof(trig_initdeferred), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 5, internal_asis_type,
							   upd_rule, sizeof(upd_rule), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 6, internal_asis_type,
							   del_rule, sizeof(del_rule), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 7, SQL_C_ULONG,
						&relid1, sizeof(relid1), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 8, SQL_C_ULONG,
						&relid2, sizeof(relid2), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 9, internal_asis_type,
					fk_table_fetched, TABLE_NAME_STORAGE_LEN, NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		if (conn->schema_support)
		{
			result = PGAPI_BindCol(htbl_stmt, 10, internal_asis_type,
					schema_fetched, SCHEMA_NAME_STORAGE_LEN, NULL);
			if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
			{
				SC_error_copy(stmt, tbl_stmt);
				goto cleanup;
			}
		}

		result = PGAPI_Fetch(htbl_stmt);
		if (result == SQL_NO_DATA_FOUND)
		{
			ret = SQL_SUCCESS;
			goto cleanup;
		}

		if (result != SQL_SUCCESS)
		{
			SC_full_error_copy(stmt, tbl_stmt);
			goto cleanup;
		}

		while (result == SQL_SUCCESS)
		{
			/* Calculate the number of key parts */
			num_keys = (trig_nargs - 4) / 2;;

			/* Handle action (i.e., 'cascade', 'restrict', 'setnull') */
			if (!strcmp(upd_rule, "RI_FKey_cascade_upd"))
				upd_rule_type = SQL_CASCADE;
			else if (!strcmp(upd_rule, "RI_FKey_noaction_upd"))
				upd_rule_type = SQL_NO_ACTION;
			else if (!strcmp(upd_rule, "RI_FKey_restrict_upd"))
				upd_rule_type = SQL_NO_ACTION;
			else if (!strcmp(upd_rule, "RI_FKey_setdefault_upd"))
				upd_rule_type = SQL_SET_DEFAULT;
			else if (!strcmp(upd_rule, "RI_FKey_setnull_upd"))
				upd_rule_type = SQL_SET_NULL;

			if (!strcmp(upd_rule, "RI_FKey_cascade_del"))
				del_rule_type = SQL_CASCADE;
			else if (!strcmp(upd_rule, "RI_FKey_noaction_del"))
				del_rule_type = SQL_NO_ACTION;
			else if (!strcmp(upd_rule, "RI_FKey_restrict_del"))
				del_rule_type = SQL_NO_ACTION;
			else if (!strcmp(upd_rule, "RI_FKey_setdefault_del"))
				del_rule_type = SQL_SET_DEFAULT;
			else if (!strcmp(upd_rule, "RI_FKey_setnull_del"))
				del_rule_type = SQL_SET_NULL;

			/* Set deferrability type */
			if (!strcmp(trig_initdeferred, "y"))
				defer_type = SQL_INITIALLY_DEFERRED;
			else if (!strcmp(trig_deferrable, "y"))
				defer_type = SQL_INITIALLY_IMMEDIATE;
			else
				defer_type = SQL_NOT_DEFERRABLE;

			mylog("Foreign Key Case#1: trig_nargs = %d, num_keys = %d\n", trig_nargs, num_keys);

			/* Get to first primary key */
			pkey_ptr = trig_args;
			for (i = 0; i < 5; i++)
				pkey_ptr += strlen(pkey_ptr) + 1;

			/* Get to first foreign key */
			fkey_ptr = trig_args;
			for (k = 0; k < 4; k++)
				fkey_ptr += strlen(fkey_ptr) + 1;

			for (k = 0; k < num_keys; k++)
			{
				pkey_text = getClientColumnName(conn, relid1, pkey_ptr, &pkey_alloced);
				fkey_text = getClientColumnName(conn, relid2, fkey_ptr, &fkey_alloced);

				mylog("pkey_ptr = '%s', fk_table = '%s', fkey_ptr = '%s'\n", pkey_text, fk_table_fetched, fkey_text);

				row = (TupleNode *) malloc(sizeof(TupleNode) + (result_cols - 1) *sizeof(TupleField));

				mylog("pk_table_needed = '%s', pkey_ptr = '%s'\n", pk_table_needed, pkey_text);
				set_tuplefield_null(&row->tuple[0]);
				set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(schema_needed));
				set_tuplefield_string(&row->tuple[2], pk_table_needed);
				set_tuplefield_string(&row->tuple[3], pkey_text);

				mylog("fk_table = '%s', fkey_ptr = '%s'\n", fk_table_fetched, fkey_text);
				set_tuplefield_null(&row->tuple[4]);
				set_tuplefield_string(&row->tuple[5], GET_SCHEMA_NAME(schema_fetched));
				set_tuplefield_string(&row->tuple[6], fk_table_fetched);
				set_tuplefield_string(&row->tuple[7], fkey_text);

				set_tuplefield_int2(&row->tuple[8], (Int2) (k + 1));

				mylog("upd_rule = %d, del_rule= %d", upd_rule_type, del_rule_type);
				set_nullfield_int2(&row->tuple[9], (Int2) upd_rule_type);
				set_nullfield_int2(&row->tuple[10], (Int2) del_rule_type);

				set_tuplefield_null(&row->tuple[11]);
				set_tuplefield_null(&row->tuple[12]);

				set_tuplefield_string(&row->tuple[13], trig_args);

				mylog(" defer_type = %d\n", defer_type);
				set_tuplefield_int2(&row->tuple[14], defer_type);

				QR_add_tuple(res, row);
				if (pkey_alloced)
					free(pkey_text);
				pkey_alloced = FALSE;
				if (fkey_alloced)
					free(fkey_text);
				fkey_alloced = FALSE;

				/* next primary/foreign key */
				for (j = 0; j < 2; j++)
				{
					pkey_ptr += strlen(pkey_ptr) + 1;
					fkey_ptr += strlen(fkey_ptr) + 1;
				}
			}
			result = PGAPI_Fetch(htbl_stmt);
		}
	}
	else
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "No tables specified to PGAPI_ForeignKeys.");
		goto cleanup;
	}
	ret = SQL_SUCCESS;

cleanup:
	if (fk_table_needed)
		free(fk_table_needed);
	if (pk_table_needed)
		free(pk_table_needed);


	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	if (SQL_ERROR == ret)
		SC_log_error(func, "", stmt);
	if (pkey_alloced)
		free(pkey_text);
	if (fkey_alloced)
		free(fkey_text);

	if (htbl_stmt)
		PGAPI_FreeStmt(htbl_stmt, SQL_DROP);
	if (hpkey_stmt)
		PGAPI_FreeStmt(hpkey_stmt, SQL_DROP);

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);

	mylog("%s(): EXIT, stmt=%u, ret=%d\n", func, stmt, ret);
	return ret;
}


RETCODE		SQL_API
PGAPI_ProcedureColumns(
					   HSTMT hstmt,
					   UCHAR FAR * szProcQualifier,
					   SWORD cbProcQualifier,
					   UCHAR FAR * szProcOwner,
					   SWORD cbProcOwner,
					   UCHAR FAR * szProcName,
					   SWORD cbProcName,
					   UCHAR FAR * szColumnName,
					   SWORD cbColumnName)
{
	CSTR func = "PGAPI_ProcedureColumns";
	StatementClass	*stmt = (StatementClass *) hstmt;
	ConnectionClass *conn = SC_get_conn(stmt);
	char		proc_query[INFO_INQUIRY_LEN];
	Int2		result_cols;
	TupleNode	*row;
	char		*schema_name, *procname, *params;
	QResultClass *res, *tres;
	Int4		tcount, paramcount, i, j, pgtype;
	RETCODE		result;
	const char *likeeq = "like";

	mylog("%s: entering...\n", func);

	if (PG_VERSION_LT(conn, 6.5))
	{
		SC_set_error(stmt, STMT_NOT_IMPLEMENTED_ERROR, "Version is too old");
		SC_log_error(func, "Function not implemented", stmt);
		return SQL_ERROR;
	}
	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;
	if (conn->schema_support)
	{
		strcpy(proc_query, "select proname, proretset, prorettype, "
				"pronargs, proargtypes, nspname from "
				"pg_catalog.pg_namespace, pg_catalog.pg_proc where "
				"pg_proc.pronamespace = pg_namespace.oid "
				"and (not proretset)");
		schema_strcat1(proc_query, " and nspname %s '%.*s'", likeeq, szProcOwner, cbProcOwner, szProcName, cbProcName, conn);
		my_strcat1(proc_query, " and proname %s '%.*s'", likeeq, szProcName, cbProcName);
		strcat(proc_query, " order by nspname, proname, proretset");
	}
	else
	{
		strcpy(proc_query, "select proname, proretset, prorettype, "
				"pronargs, proargtypes from pg_proc where "
				"(not proretset)");
		my_strcat1(proc_query, " and proname %s '%.*s'", likeeq, szProcName, cbProcName);
		strcat(proc_query, " order by proname, proretset");
	}
	if (tres = CC_send_query(conn, proc_query, NULL, CLEAR_RESULT_ON_ABORT), !tres)
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_ProcedureColumns query error");
		return SQL_ERROR;
	}

	stmt->manual_result = TRUE;
	stmt->errormsg_created = TRUE;
	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_ProcedureColumns result.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/*
	 * the binding structure for a statement is not set up until
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	result_cols = 19;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info(res, 0, "PROCEDURE_CAT", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 1, "PROCEDUR_SCHEM", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 2, "PROCEDURE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 3, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 4, "COLUMN_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 5, "DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 6, "TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 7, "COLUMN_SIZE", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 8, "BUFFER_LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 9, "DECIMAL_DIGITS", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 10, "NUM_PREC_RADIX", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 11, "NULLABLE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 12, "REMARKS", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 13, "COLUMN_DEF", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 14, "SQL_DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 15, "SQL_DATATIME_SUB", PG_TYPE_INT2, 2);
	QR_set_field_info(res, 16, "CHAR_OCTET_LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 17, "ORIDINAL_POSITION", PG_TYPE_INT4, 4);
	QR_set_field_info(res, 18, "IS_NULLABLE", PG_TYPE_VARCHAR, MAX_INFO_STRING);

	if (0 == cbColumnName || !szColumnName || !szColumnName[0])
		tcount = QR_get_num_total_tuples(tres);
	else
		tcount = 0;
	for (i = 0; i < tcount; i++)
	{
		if (conn->schema_support)
			schema_name = GET_SCHEMA_NAME(QR_get_value_backend_row(tres, i, 5));
		else
			schema_name = NULL;
		procname = QR_get_value_backend_row(tres, i, 0);
		pgtype = atoi(QR_get_value_backend_row(tres, i, 2));
		if (pgtype != 0)
		{
			row = (TupleNode *) malloc(sizeof(TupleNode) + (result_cols - 1) *sizeof(TupleField));
			set_tuplefield_null(&row->tuple[0]);
			set_nullfield_string(&row->tuple[1], schema_name);
			set_tuplefield_string(&row->tuple[2], procname);
			set_tuplefield_string(&row->tuple[3], "");
			set_tuplefield_int2(&row->tuple[4], SQL_RETURN_VALUE);
			set_tuplefield_int2(&row->tuple[5], pgtype_to_concise_type(stmt, pgtype, PG_STATIC));
			set_tuplefield_string(&row->tuple[6], pgtype_to_name(stmt, pgtype));
			set_nullfield_int4(&row->tuple[7], pgtype_column_size(stmt, pgtype, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[8], pgtype_buffer_length(stmt, pgtype, PG_STATIC, PG_STATIC));
			set_nullfield_int2(&row->tuple[9], pgtype_decimal_digits(stmt, pgtype, PG_STATIC));
			set_nullfield_int2(&row->tuple[10], pgtype_radix(stmt, pgtype));
			set_tuplefield_int2(&row->tuple[11], SQL_NULLABLE_UNKNOWN);
			set_tuplefield_null(&row->tuple[12]);
			set_tuplefield_null(&row->tuple[13]);
			set_nullfield_int2(&row->tuple[14], pgtype_to_sqldesctype(stmt, pgtype, PG_STATIC));
			set_nullfield_int2(&row->tuple[15], pgtype_to_datetime_sub(stmt, pgtype));
			set_nullfield_int4(&row->tuple[16], pgtype_transfer_octet_length(stmt, pgtype, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[17], 0);
			set_tuplefield_string(&row->tuple[18], "");
			QR_add_tuple(res, row);
		}
		paramcount = atoi(QR_get_value_backend_row(tres, i, 3));
		params = QR_get_value_backend_row(tres, i, 4);
		for (j = 0; j < paramcount; j++)
		{
			while (isspace(*params))
				params++;
			sscanf(params, "%d", &pgtype);
			row = (TupleNode *) malloc(sizeof(TupleNode) + (result_cols - 1) *sizeof(TupleField));
			set_tuplefield_null(&row->tuple[0]);
			set_nullfield_string(&row->tuple[1], schema_name);
			set_tuplefield_string(&row->tuple[2], procname);
			set_tuplefield_string(&row->tuple[3], "");
			set_tuplefield_int2(&row->tuple[4], SQL_PARAM_INPUT);
			set_tuplefield_int2(&row->tuple[5], pgtype_to_concise_type(stmt, pgtype, PG_STATIC));
			set_tuplefield_string(&row->tuple[6], pgtype_to_name(stmt, pgtype));
			set_nullfield_int4(&row->tuple[7], pgtype_column_size(stmt, pgtype, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[8], pgtype_buffer_length(stmt, pgtype, PG_STATIC, PG_STATIC));
			set_nullfield_int2(&row->tuple[9], pgtype_decimal_digits(stmt, pgtype, PG_STATIC));
			set_nullfield_int2(&row->tuple[10], pgtype_radix(stmt, pgtype));
			set_tuplefield_int2(&row->tuple[11], SQL_NULLABLE_UNKNOWN);
			set_tuplefield_null(&row->tuple[12]);
			set_tuplefield_null(&row->tuple[13]);
			set_nullfield_int2(&row->tuple[14], pgtype_to_sqldesctype(stmt, pgtype, PG_STATIC));
			set_nullfield_int2(&row->tuple[15], pgtype_to_datetime_sub(stmt, pgtype));
			set_nullfield_int4(&row->tuple[16], pgtype_transfer_octet_length(stmt, pgtype, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&row->tuple[17], j + 1);
			set_tuplefield_string(&row->tuple[18], "");
			QR_add_tuple(res, row);
			while (isdigit(*params))
				params++;
		}
	}
	QR_Destructor(tres);
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;
	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);

	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_Procedures(
				 HSTMT hstmt,
				 UCHAR FAR * szProcQualifier,
				 SWORD cbProcQualifier,
				 UCHAR FAR * szProcOwner,
				 SWORD cbProcOwner,
				 UCHAR FAR * szProcName,
				 SWORD cbProcName)
{
	CSTR func = "PGAPI_Procedures";
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn = SC_get_conn(stmt);
	char		proc_query[INFO_INQUIRY_LEN];
	QResultClass *res;
	RETCODE		result;
	const char *likeeq = "like";

	mylog("%s: entering... scnm=%x len=%d\n", func, szProcOwner, cbProcOwner);

	if (PG_VERSION_LT(conn, 6.5))
	{
		SC_set_error(stmt, STMT_NOT_IMPLEMENTED_ERROR, "Version is too old");
		SC_log_error(func, "Function not implemented", stmt);
		return SQL_ERROR;
	}
	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	/*
	 * The following seems the simplest implementation
	 */
	if (conn->schema_support)
	{
		strcpy(proc_query, "select '' as " "PROCEDURE_CAT" ", nspname as " "PROCEDURE_SCHEM" ","
		" proname as " "PROCEDURE_NAME" ", '' as " "NUM_INPUT_PARAMS" ","
		   " '' as " "NUM_OUTPUT_PARAMS" ", '' as " "NUM_RESULT_SETS" ","
		   " '' as " "REMARKS" ","
		   " case when prorettype = 0 then 1::int2 else 2::int2 end"
		   " as "		  "PROCEDURE_TYPE" " from pg_catalog.pg_namespace,"
		   " pg_catalog.pg_proc"
		  " where pg_proc.pronamespace = pg_namespace.oid");
		schema_strcat1(proc_query, " and nspname %s '%.*s'", likeeq, szProcOwner, cbProcOwner, szProcName, cbProcName, conn);
		my_strcat1(proc_query, " and proname %s '%.*s'", likeeq, szProcName, cbProcName);
	}
	else
	{
		strcpy(proc_query, "select '' as " "PROCEDURE_CAT" ", '' as " "PROCEDURE_SCHEM" ","
		" proname as " "PROCEDURE_NAME" ", '' as " "NUM_INPUT_PARAMS" ","
		   " '' as " "NUM_OUTPUT_PARAMS" ", '' as " "NUM_RESULT_SETS" ","
		   " '' as " "REMARKS" ","
		   " case when prorettype = 0 then 1::int2 else 2::int2 end as " "PROCEDURE_TYPE" " from pg_proc");
		my_strcat1(proc_query, " where proname %s '%.*s'", likeeq, szProcName, cbProcName);
	}

	if (res = CC_send_query(conn, proc_query, NULL, CLEAR_RESULT_ON_ABORT), !res)
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_Procedures query error");
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;
	extend_column_bindings(SC_get_ARDF(stmt), 8);
	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);

	return SQL_SUCCESS;
}


#define	ACLMAX	8
#define ALL_PRIVILIGES "arwdRxt"
static int
usracl_auth(char *usracl, const char *auth)
{
	int	i, j, addcnt = 0;

	for (i = 0; auth[i]; i++)
	{
		for (j = 0; j < ACLMAX; j++)
		{
			if (usracl[j] == auth[i])
				break;
			else if (!usracl[j])
			{
				usracl[j]= auth[i];
				addcnt++;
				break;
			}
		}
	}
	return addcnt;
}
static void
useracl_upd(char (*useracl)[ACLMAX], QResultClass *allures, const char *user, const char *auth)
{
	int usercount = QR_get_num_backend_tuples(allures), i, addcnt = 0;

mylog("user=%s auth=%s\n", user, auth);
	if (user[0])
		for (i = 0; i < usercount; i++)
		{
			if (strcmp(QR_get_value_backend_row(allures, i, 0), user) == 0)
			{
				addcnt += usracl_auth(useracl[i], auth);
				break;
			}
		}
	else
		for (i = 0; i < usercount; i++)
		{
			addcnt += usracl_auth(useracl[i], auth);
		}
	mylog("addcnt=%d\n", addcnt);
}

RETCODE		SQL_API
PGAPI_TablePrivileges(
					  HSTMT hstmt,
					  UCHAR FAR * szTableQualifier,
					  SWORD cbTableQualifier,
					  UCHAR FAR * szTableOwner,
					  SWORD cbTableOwner,
					  UCHAR FAR * szTableName,
					  SWORD cbTableName,
					  UWORD flag)
{
	StatementClass *stmt = (StatementClass *) hstmt;
	CSTR func = "PGAPI_TablePrivileges";
	ConnectionClass *conn = SC_get_conn(stmt);
	Int2		result_cols;
	char		proc_query[INFO_INQUIRY_LEN];
	QResultClass	*res, *allures = NULL;
	TupleNode	*row;
	int		tablecount, usercount, i, j, k;
	BOOL		grpauth, sys, su;
	char		(*useracl)[ACLMAX], *acl, *user, *delim, *auth;
	char		*reln, *owner, *priv, *schnm = NULL;
	RETCODE		result;
	const char *likeeq = "like", *szSchemaName;
	SWORD	cbSchemaName;

	mylog("%s: entering... scnm=%x len-%d\n", func, NULL_IF_NULL(szTableOwner), cbTableOwner);
	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	result_cols = 7;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	/* set the field names */
	stmt->manual_result = TRUE;
	res = QR_Constructor();
	SC_set_Result(stmt, res);
	QR_set_num_fields(res, result_cols);
	QR_set_field_info(res, 0, "TABLE_CAT", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 1, "TABLE_SCHEM", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 2, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 3, "GRANTOR", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 4, "GRANTEE", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 5, "PRIVILEGE", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info(res, 6, "IS_GRANTABLE", PG_TYPE_VARCHAR, MAX_INFO_STRING);

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;
	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	stmt->rowset_start = -1;
	SC_set_current_col(stmt, -1);
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

retry_public_schema:
	if (conn->schema_support)
		strncpy_null(proc_query, "select relname, usename, relacl, nspname"
		" from pg_catalog.pg_namespace, pg_catalog.pg_class ,"
		" pg_catalog.pg_user where", sizeof(proc_query));
	else
		strncpy_null(proc_query, "select relname, usename, relacl"
		" from pg_class , pg_user where", sizeof(proc_query));
	if ((flag & PODBC_NOT_SEARCH_PATTERN) != 0)
	{
		if (conn->schema_support)
		{
			schema_strcat(proc_query, " nspname = '%.*s' and", szSchemaName, cbSchemaName, szTableName, cbTableName, conn);
		}
		my_strcat(proc_query, " relname = '%.*s' and", szTableName, cbTableName);
	}
	else
	{
		char	esc_table_name[TABLE_NAME_STORAGE_LEN * 2];
		int	escTbnamelen;

		if (conn->schema_support)
		{
			escTbnamelen = reallyEscapeCatalogEscapes(szSchemaName, cbSchemaName, esc_table_name, sizeof(esc_table_name), conn->ccsc);
			schema_strcat1(proc_query, " nspname %s '%.*s' and", likeeq, esc_table_name, escTbnamelen, szTableName, cbTableName, conn);
		}
		escTbnamelen = reallyEscapeCatalogEscapes(szTableName, cbTableName, esc_table_name, sizeof(esc_table_name), conn->ccsc);
		my_strcat1(proc_query, " relname %s '%.*s' and", likeeq, esc_table_name, escTbnamelen);
	}
	if (!atoi(conn->connInfo.show_system_tables))
	{
		if (conn->schema_support)
			strcat(proc_query, " nspname !~ '^" POSTGRES_SYS_PREFIX "' and");
		else
			strcat(proc_query, " relname !~ '^" POSTGRES_SYS_PREFIX "' and");
	}
	if (conn->schema_support)
		strcat(proc_query, " pg_namespace.oid = relnamespace and");
	strcat(proc_query, " pg_user.usesysid = relowner");
	if (res = CC_send_query(conn, proc_query, NULL, CLEAR_RESULT_ON_ABORT), !res)
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_TablePrivileges query error");
		return SQL_ERROR;
	}
	tablecount = QR_get_num_backend_tuples(res);
	/* If not found */
	if (conn->schema_support &&
	    (flag & PODBC_SEARCH_PUBLIC_SCHEMA) != 0 &&
	    0 == tablecount)
	{
		const char *user = CC_get_username(conn);

		/*
		 * If specified schema name == user_name and
		 * the current schema is 'public',
		 * retry the 'public' schema.
		 */
		if (szSchemaName &&
		    (cbSchemaName == SQL_NTS ||
		     cbSchemaName == (SWORD) strlen(user)) &&
		    strnicmp(szSchemaName, user, strlen(user)) == 0 &&
		    stricmp(CC_get_current_schema(conn), pubstr) == 0)
		{
			QR_Destructor(res);
			szSchemaName = pubstr;
			cbSchemaName = SQL_NTS;
			goto retry_public_schema;
		}
	}

	strncpy_null(proc_query, "select usename, usesysid, usesuper from pg_user", sizeof(proc_query));
	if (allures = CC_send_query(conn, proc_query, NULL, CLEAR_RESULT_ON_ABORT), !allures)
	{
		QR_Destructor(res);
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_TablePrivileges query error");
		return SQL_ERROR;
	}
	usercount = QR_get_num_backend_tuples(allures);
	useracl = (char (*)[ACLMAX]) malloc(usercount * sizeof(char [ACLMAX]));
	for (i = 0; i < tablecount; i++)
	{ 
		memset(useracl, 0, usercount * sizeof(char[ACLMAX]));
		acl = (char *) QR_get_value_backend_row(res, i, 2);
		if (acl && acl[0] == '{')
			user = acl + 1;
		else
			user = NULL;
		for (; user && *user;)
		{
			grpauth = FALSE;
			if (user[0] == '"' && strncmp(user + 1, "group ", 6) == 0)
			{
				user += 7;
				grpauth = TRUE;
			}
			if (delim = strchr(user, '='), !delim)
				break;
			*delim = '\0';
			auth = delim + 1;
			if (grpauth)
			{
				if (delim = strchr(auth, '"'), delim)
				{
					*delim = '\0';
					delim++;
				}
			}
			else if (delim = strchr(auth, ','), delim)
				*delim = '\0';
			else if (delim = strchr(auth, '}'), delim)
				*delim = '\0';
			if (grpauth) /* handle group privilege */
			{
				QResultClass	*gres;
				int		i;
				char	*grolist, *uid, *delm;

				snprintf(proc_query, sizeof(proc_query) - 1, "select grolist from pg_group where groname = '%s'", user);
				if (gres = CC_send_query(conn, proc_query, NULL, CLEAR_RESULT_ON_ABORT), gres != NULL)
				{
					grolist = QR_get_value_backend_row(gres, 0, 0);
					if (grolist && grolist[0] == '{')
					{
						for (uid = grolist + 1; *uid;)
						{
							if (delm = strchr(uid, ','), delm)
								*delm = '\0';
							else if (delm = strchr(uid, '}'), delm)
								*delm = '\0';
mylog("guid=%s\n", uid);
							for (i = 0; i < usercount; i++)
							{
								if (strcmp(QR_get_value_backend_row(allures, i, 1), uid) == 0)
									useracl_upd(useracl, allures, QR_get_value_backend_row(allures, i, 0), auth);
							}
							uid = delm + 1;
						}
					}
					QR_Destructor(gres);
				}
			}
			else
				useracl_upd(useracl, allures, user, auth);
			if (!delim)
				break;
			user = delim + 1;
		}
		reln = QR_get_value_backend_row(res, i, 0);
		owner = QR_get_value_backend_row(res, i, 1);
		if (conn->schema_support)
			schnm = QR_get_value_backend_row(res, i, 3);
		/* The owner has all privileges */
		useracl_upd(useracl, allures, owner, ALL_PRIVILIGES);
		for (j = 0; j < usercount; j++)
		{
			user = QR_get_value_backend_row(allures, j, 0);
			su = (strcmp(QR_get_value_backend_row(allures, j, 2), "t") == 0);
			sys = (strcmp(user, owner) == 0);
			/* Super user has all privileges */
			if (su)
				useracl_upd(useracl, allures, user, ALL_PRIVILIGES);
			for (k = 0; k < ACLMAX; k++)
			{
				if (!useracl[j][k])
					break;
				switch (useracl[j][k])
				{
					case 'R': /* rule */
					case 't': /* trigger */
						continue;
				}
				row = (TupleNode *) malloc(sizeof(TupleNode) + (7 - 1) *sizeof(TupleField));
				set_tuplefield_string(&row->tuple[0], "");
				if (conn->schema_support)
					set_tuplefield_string(&row->tuple[1], GET_SCHEMA_NAME(schnm));
				else
					set_tuplefield_string(&row->tuple[1], "");
				set_tuplefield_string(&row->tuple[2], reln);
				if (su || sys)
					set_tuplefield_string(&row->tuple[3], "_SYSTEM");
				else
					set_tuplefield_string(&row->tuple[3], owner);
				mylog("user=%s\n", user);
				set_tuplefield_string(&row->tuple[4], user);
				switch (useracl[j][k])
				{
					case 'a':
						priv = "INSERT";
						break;
					case 'r':
						priv = "SELECT";
						break;
					case 'w':
						priv = "UPDATE";
						break;
					case 'd':
						priv = "DELETE";
						break;
					case 'x':
						priv = "REFERENCES";
						break;
					default:
						priv = "";
				}
				set_tuplefield_string(&row->tuple[5], priv);
				/* The owner and the super user are grantable */
				if (sys || su)
					set_tuplefield_string(&row->tuple[6], "YES");
				else
					set_tuplefield_string(&row->tuple[6], "NO");
				QR_add_tuple(SC_get_Result(stmt), row);
			}
		}
	}
	free(useracl);
	QR_Destructor(res);
	QR_Destructor(allures); 
	return SQL_SUCCESS;
}

