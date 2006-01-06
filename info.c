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
#include "connection.h"
#include "statement.h"
#include "qresult.h"
#include "bind.h"
#include "misc.h"
#include "pgtypes.h"
#include "pgapifunc.h"
#include "multibyte.h"
#include "catfunc.h"


/*	Trigger related stuff for SQLForeign Keys */
#define TRIGGER_SHIFT 3
#define TRIGGER_MASK   0x03
#define TRIGGER_DELETE 0x01
#define TRIGGER_UPDATE 0x02

#define	NULL_IF_NULL(a) ((a) ? ((const char *) a) : "(NULL)")

/* extern GLOBAL_VALUES globals; */

CSTR	pubstr = "public";
CSTR	likeop = "like";
CSTR	eqop = "=";


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
	char		odbcver[16];

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
#if (ODBCVER >= 0x0300)
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
#endif /* ODBCVER */
			break;

		case SQL_BOOKMARK_PERSISTENCE:	/* ODBC 2.0 */
			/* very simple bookmark support */
			len = 4;
			value = ci->drivers.use_declarefetch && PG_VERSION_LT(conn, 7.4) ? 0 : (SQL_BP_SCROLL | SQL_BP_DELETE | SQL_BP_UPDATE | SQL_BP_TRANSACTION);
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
			if (!ci->drivers.use_declarefetch || PG_VERSION_GE(conn, 7.4))
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
			p = NULL_CATALOG_NAME;
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
			sprintf(odbcver, "%02x.%02x", ODBCVER / 256, ODBCVER % 256);
			/* p = DRIVER_ODBC_VER; */
			p = odbcver;
			break;

		case SQL_DRIVER_VER:	/* ODBC 1.0 */
			p = POSTGRESDRIVERVERSION;
			break;

		case SQL_EXPRESSIONS_IN_ORDERBY:		/* ODBC 1.0 */
			p = "N";
			break;

		case SQL_FETCH_DIRECTION:		/* ODBC 1.0 */
			len = 4;
			value = (SQL_FD_FETCH_NEXT |
			 	SQL_FD_FETCH_NEXT | 
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
			if (PG_VERSION_GE(conn, 7.4))
				value = CC_get_max_idlen(conn);
#ifdef	MAX_COLUMN_LEN
			else
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
			if (PG_VERSION_GE(conn, 7.4))
				value = CC_get_max_idlen(conn);
#ifdef	MAX_SCHEMA_LEN
			else if (conn->schema_support)
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
			if (PG_VERSION_GE(conn, 7.4))
				value = CC_get_max_idlen(conn);
#ifdef	MAX_TABLE_LEN
			else
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
			if (0 != ci->updatable_cursors)
				value |= (SQL_POS_UPDATE | SQL_POS_DELETE | SQL_POS_ADD);
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
			p = (0 != ci->updatable_cursors) ? "Y" : "N";
			break;

		case SQL_SCROLL_CONCURRENCY:	/* ODBC 1.0 */
			len = 4;
			value = SQL_SCCO_READ_ONLY;
			if (0 != ci->updatable_cursors)
				value |= SQL_SCCO_OPT_ROWVER;
			if (ci->drivers.lie)
				value |= (SQL_SCCO_LOCK | SQL_SCCO_OPT_VALUES);
			break;

		case SQL_SCROLL_OPTIONS:		/* ODBC 1.0 */
			len = 4;
			value = SQL_SO_FORWARD_ONLY | SQL_SO_STATIC;
			if (0 != (ci->updatable_cursors & ALLOW_KEYSET_DRIVEN_CURSORS))
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
			if (0 != ci->updatable_cursors)
				value |= (SQL_SS_ADDITIONS | SQL_SS_DELETIONS | SQL_SS_UPDATES);
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
			CC_set_error(conn, CONN_NOT_IMPLEMENTED_ERROR, "Unrecognized key passed to PGAPI_GetInfo.", NULL);
			return SQL_ERROR;
	}

	result = SQL_SUCCESS;

	mylog("%s: p='%s', len=%d, value=%d, cbMax=%d\n", func, p ? p : "<NULL>", len, value, cbInfoValueMax);

	/*
	 * NOTE, that if rgbInfoValue is NULL, then no warnings or errors
	 * should result and just pcbInfoValue is returned, which indicates
	 * what length would be required if a real buffer had been passed in.
	 */
	if (p)
	{
		/* char/binary data */
		len = strlen(p);

		if (rgbInfoValue)
		{
#ifdef	UNICODE_SUPPORT
			if (conn->unicode)
			{
				len = utf8_to_ucs2(p, len, (SQLWCHAR *) rgbInfoValue, cbInfoValueMax / WCLEN);
				len *= WCLEN;
			}
			else
#endif /* UNICODE_SUPPORT */
			strncpy_null((char *) rgbInfoValue, p, (size_t) cbInfoValueMax);

			if (len >= cbInfoValueMax)
			{
				result = SQL_SUCCESS_WITH_INFO;
				CC_set_error(conn, CONN_TRUNCATED, "The buffer was too small for tthe InfoValue.", func);
			}
		}
#ifdef	UNICODE_SUPPORT
		else if (conn->unicode)
			len *= WCLEN;
#endif /* UNICODE_SUPPORT */
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
cleanup:

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
	TupleField	*tuple;
	int			i, result_cols;

	/* Int4 type; */
	Int4		pgType;
	Int2		sqlType;
	RETCODE		result = SQL_SUCCESS;

	mylog("%s: entering...fSqlType = %d\n", func, fSqlType);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "Error creating result.", func);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */
#if (ODBCVER >= 0x0300)
	result_cols = 19;
#else
	result_cols = 15;
#endif /* ODBCVER */
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	QR_set_num_fields(res, result_cols);
	QR_set_field_info_v(res, 0, "TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 1, "DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 2, "PRECISION", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 3, "LITERAL_PREFIX", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 4, "LITERAL_SUFFIX", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 5, "CREATE_PARAMS", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 6, "NULLABLE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 7, "CASE_SENSITIVE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 8, "SEARCHABLE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 9, "UNSIGNED_ATTRIBUTE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 10, "MONEY", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 11, "AUTO_INCREMENT", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 12, "LOCAL_TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 13, "MINIMUM_SCALE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 14, "MAXIMUM_SCALE", PG_TYPE_INT2, 2);
#if (ODBCVER >=0x0300)
	QR_set_field_info_v(res, 15, "SQL_DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 16, "SQL_DATATIME_SUB", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 17, "NUM_PREC_RADIX", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 18, "INTERVAL_PRECISION", PG_TYPE_INT2, 2);
#endif /* ODBCVER */

	for (i = 0, sqlType = sqlTypes[0]; sqlType; sqlType = sqlTypes[++i])
	{
		pgType = sqltype_to_pgtype(stmt, sqlType);

if (sqlType == SQL_LONGVARBINARY)
{
ConnInfo	*ci = &(SC_get_conn(stmt)->connInfo);
inolog("%d sqltype=%d -> pgtype=%d\n", ci->bytea_as_longvarbinary, sqlType, pgType);
}

		if (fSqlType == SQL_ALL_TYPES || fSqlType == sqlType)
		{
			int	pgtcount = 1, cnt;

			if (SQL_INTEGER == sqlType && PG_VERSION_GE(SC_get_conn(stmt), 6.4))
				pgtcount = 2;
			for (cnt = 0; cnt < pgtcount; cnt ++)
			{
				tuple = QR_AddNew(res);

				/* These values can't be NULL */
				if (1 == cnt)
				{
					set_tuplefield_string(&tuple[0], "serial");
					set_tuplefield_int2(&tuple[6], SQL_NO_NULLS);
inolog("serial in\n");
				}
				else
				{
					set_tuplefield_string(&tuple[0], pgtype_to_name(stmt, pgType));
					set_tuplefield_int2(&tuple[6], pgtype_nullable(stmt, pgType));
				}
				set_tuplefield_int2(&tuple[1], (Int2) sqlType);
				set_tuplefield_int2(&tuple[7], pgtype_case_sensitive(stmt, pgType));
				set_tuplefield_int2(&tuple[8], pgtype_searchable(stmt, pgType));
				set_tuplefield_int2(&tuple[10], pgtype_money(stmt, pgType));

			/*
			 * Localized data-source dependent data type name (always
			 * NULL)
			 */
				set_tuplefield_null(&tuple[12]);

				/* These values can be NULL */
				set_nullfield_int4(&tuple[2], pgtype_column_size(stmt, pgType, PG_STATIC, PG_STATIC));
				set_nullfield_string(&tuple[3], pgtype_literal_prefix(stmt, pgType));
				set_nullfield_string(&tuple[4], pgtype_literal_suffix(stmt, pgType));
				set_nullfield_string(&tuple[5], pgtype_create_params(stmt, pgType));
				if (1 == cnt)
				{
					set_nullfield_int2(&tuple[9], TRUE);
					set_nullfield_int2(&tuple[11], TRUE);
				}
				else
				{
					set_nullfield_int2(&tuple[9], pgtype_unsigned(stmt, pgType));
					set_nullfield_int2(&tuple[11], pgtype_auto_increment(stmt, pgType));
				}
				set_nullfield_int2(&tuple[13], pgtype_min_decimal_digits(stmt, pgType));
				set_nullfield_int2(&tuple[14], pgtype_max_decimal_digits(stmt, pgType));
#if (ODBCVER >=0x0300)
				set_nullfield_int2(&tuple[15], pgtype_to_sqldesctype(stmt, pgType, PG_STATIC));
				set_nullfield_int2(&tuple[16], pgtype_to_datetime_sub(stmt, pgType));
				set_nullfield_int4(&tuple[17], pgtype_radix(stmt, pgType));
				set_nullfield_int4(&tuple[18], 0);
#endif /* ODBCVER */
			}
		}
	}

cleanup:
#undef	return
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	if (stmt->internal)
		result = DiscardStatementSvp(stmt, result, FALSE);
	return result;
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
#if (ODBCVER < 0x0300)
		if (ci->drivers.lie)
		{
			int			i;

			memset(pfExists, 0, sizeof(UWORD) * 100);

			pfExists[SQL_API_SQLALLOCENV] = TRUE;
			pfExists[SQL_API_SQLFREEENV] = TRUE;
			for (i = SQL_API_SQLALLOCCONNECT; i <= SQL_NUM_FUNCTIONS; i++)
				pfExists[i] = TRUE;
			for (i = SQL_EXT_API_START; i <= SQL_EXT_API_LAST; i++)
				pfExists[i] = TRUE;
		}
		else
#endif
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
			pfExists[SQL_API_SQLDESCRIBECOL] = TRUE;	/* partial */
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
			pfExists[SQL_API_SQLPREPARE] = TRUE;		/* complete? */
			pfExists[SQL_API_SQLROWCOUNT] = TRUE;
			pfExists[SQL_API_SQLSETCURSORNAME] = TRUE;
			pfExists[SQL_API_SQLSETPARAM] = FALSE;		/* odbc 1.0 */
			pfExists[SQL_API_SQLTRANSACT] = TRUE;

			/* ODBC level 1 functions */
			pfExists[SQL_API_SQLBINDPARAMETER] = TRUE;
			pfExists[SQL_API_SQLCOLUMNS] = TRUE;
			pfExists[SQL_API_SQLDRIVERCONNECT] = TRUE;
			pfExists[SQL_API_SQLGETCONNECTOPTION] = TRUE;		/* partial */
			pfExists[SQL_API_SQLGETDATA] = TRUE;
			pfExists[SQL_API_SQLGETFUNCTIONS] = TRUE;
			pfExists[SQL_API_SQLGETINFO] = TRUE;
			pfExists[SQL_API_SQLGETSTMTOPTION] = TRUE;	/* partial */
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
			if (PG_VERSION_GE(conn, 7.4))
				pfExists[SQL_API_SQLCOLUMNPRIVILEGES] = FALSE;
			else
				pfExists[SQL_API_SQLCOLUMNPRIVILEGES] = FALSE;
			pfExists[SQL_API_SQLDATASOURCES] = FALSE;	/* only implemented by
														 * DM */
			if (PROTOCOL_74(ci))
				pfExists[SQL_API_SQLDESCRIBEPARAM] = TRUE;
			else
				pfExists[SQL_API_SQLDESCRIBEPARAM] = FALSE; /* not properly
														 * implemented */
			pfExists[SQL_API_SQLDRIVERS] = FALSE;		/* only implemented by
														 * DM */
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
			pfExists[SQL_API_SQLSETSCROLLOPTIONS] = TRUE;		/* odbc 1.0 */
			pfExists[SQL_API_SQLTABLEPRIVILEGES] = TRUE;
#if (ODBCVER >= 0x0300)
			if (0 == ci->updatable_cursors)
				pfExists[SQL_API_SQLBULKOPERATIONS] = FALSE;
			else
				pfExists[SQL_API_SQLBULKOPERATIONS] = TRUE;
#endif /* ODBCVER */
		}
	}
	else
	{
		if (ci->drivers.lie)
			*pfExists = TRUE;
		else
		{
			switch (fFunction)
			{
#if (ODBCVER < 0x0300)
				case SQL_API_SQLALLOCCONNECT:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLALLOCENV:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLALLOCSTMT:
					*pfExists = TRUE;
					break;
#endif /* ODBCVER */
				case SQL_API_SQLBINDCOL:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLCANCEL:
					*pfExists = TRUE;
					break;
#if (ODBCVER >= 0x0300)
				case SQL_API_SQLCOLATTRIBUTE:
#else
				case SQL_API_SQLCOLATTRIBUTES:
#endif /* ODBCVER */
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
#if (ODBCVER < 0x0300)
				case SQL_API_SQLERROR:
					*pfExists = TRUE;
					break;
#endif /* ODBCVER */
				case SQL_API_SQLEXECDIRECT:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLEXECUTE:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLFETCH:
					*pfExists = TRUE;
					break;
#if (ODBCVER < 0x0300)
				case SQL_API_SQLFREECONNECT:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLFREEENV:
					*pfExists = TRUE;
					break;
#endif /* ODBCVER */
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
#if (ODBCVER < 0x0300)
				case SQL_API_SQLSETPARAM:
					*pfExists = FALSE;
					break;		/* odbc 1.0 */
				case SQL_API_SQLTRANSACT:
					*pfExists = TRUE;
					break;
#endif /* ODBCVER */

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
#if (ODBCVER < 0x0300)
				case SQL_API_SQLGETCONNECTOPTION:
					*pfExists = TRUE;
					break;		/* partial */
#endif /* ODBCVER */
				case SQL_API_SQLGETDATA:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLGETFUNCTIONS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLGETINFO:
					*pfExists = TRUE;
					break;
#if (ODBCVER < 0x0300)
				case SQL_API_SQLGETSTMTOPTION:
					*pfExists = TRUE;
					break;		/* partial */
#endif /* ODBCVER */
				case SQL_API_SQLGETTYPEINFO:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLPARAMDATA:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLPUTDATA:
					*pfExists = TRUE;
					break;
#if (ODBCVER < 0x0300)
				case SQL_API_SQLSETCONNECTOPTION:
					*pfExists = TRUE;
					break;		/* partial */
				case SQL_API_SQLSETSTMTOPTION:
					*pfExists = TRUE;
					break;
#endif /* ODBCVER */
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
					if (PROTOCOL_74(ci))
						*pfExists = TRUE;
					else
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
#if (ODBCVER < 0x0300)
				case SQL_API_SQLPARAMOPTIONS:
					*pfExists = TRUE;
					break;
#endif /* ODBCVER */
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
#if (ODBCVER < 0x0300)
				case SQL_API_SQLSETSCROLLOPTIONS:
					*pfExists = TRUE;
					break;		/* odbc 1.0 */
#endif /* ODBCVER */
				case SQL_API_SQLTABLEPRIVILEGES:
					*pfExists = TRUE;
					break;
#if (ODBCVER >= 0x0300)
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
#endif /* ODBCVER */
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
			 UCHAR FAR * szTableQualifier, /* PV */
			 SWORD cbTableQualifier,
			 UCHAR FAR * szTableOwner, /* PV */
			 SWORD cbTableOwner,
			 UCHAR FAR * szTableName, /* PV */
			 SWORD cbTableName,
			 UCHAR FAR * szTableType,
			 SWORD cbTableType)
{
	CSTR func = "PGAPI_Tables";
	StatementClass *stmt = (StatementClass *) hstmt;
	StatementClass *tbl_stmt;
	QResultClass	*res;
	TupleField	*tuple;
	HSTMT		htbl_stmt = NULL;
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
				show_views;
	char		regular_table,
				view,
				systable;
	int			i;
	SWORD			internal_asis_type = SQL_C_CHAR, cbSchemaName;
	CSTR	likeeq = "like";
	const char	*szSchemaName;

	mylog("%s: entering...stmt=%x scnm=%x len=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);

	result = PGAPI_AllocStmt(stmt->hdbc, &htbl_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_Tables result.", func);
		return SQL_ERROR;
	}
	tbl_stmt = (StatementClass *) htbl_stmt;
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */
retry_public_schema:
	/*
	 * Create the query to find out the tables
	 */
	if (conn->schema_support)
	{
		/* view is represented by its relkind since 7.1 */
		strcpy(tables_query, "select relname, nspname, relkind"
		" from pg_catalog.pg_class c, pg_catalog.pg_namespace n");
		strcat(tables_query, " where relkind in ('r', 'v')");
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
	{
		schema_strcat1(tables_query, " and nspname %s '%.*s'", likeeq, szSchemaName, cbSchemaName, szTableName, cbTableName, conn);
		/* strcat(tables_query, " and pg_catalog.pg_table_is_visible(c.oid)"); */
	}
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
	while (i < sizeof(prefix) && prefix[i])
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
		while (i < sizeof(table_type) && table_type[i])
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
		if (conn->schema_support)
			strcat(tables_query, " and nspname not in ('pg_catalog', 'information_schema')");
		else
		{
			strcat(tables_query, " and relname !~ '^" POSTGRES_SYS_PREFIX);

			/* Also filter out user-defined system table types */
			for (i = 0; prefix[i]; i++)
			{
				strcat(tables_query, "|^");
				strcat(tables_query, prefix[i]);
			}
			strcat(tables_query, "'");
		}
	}

	/* match users */
	if (PG_VERSION_LT(conn, 7.1))
		/* filter out large objects in older versions */
		strcat(tables_query, " and relname !~ '^xinv[0-9]+'");

	if (conn->schema_support)
		strcat(tables_query, " and n.oid = relnamespace order by nspname, relname");
	else
		strcat(tables_query, " and usesysid = relowner order by relname");

	result = PGAPI_ExecDirect(htbl_stmt, tables_query, SQL_NTS, 0);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_full_error_copy(stmt, htbl_stmt, FALSE);
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
#ifdef	UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */
	result = PGAPI_BindCol(htbl_stmt, 1, internal_asis_type,
						   table_name, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(htbl_stmt, 2, internal_asis_type,
						   table_owner, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		goto cleanup;
	}
	result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
						   relkind_or_hasrules, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		goto cleanup;
	}

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_Tables result.", func);
		goto cleanup;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until */

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	extend_column_bindings(SC_get_ARDF(stmt), 5);

	stmt->catalog_result = TRUE;
	/* set the field names */
	QR_set_num_fields(res, 5);
	QR_set_field_info_v(res, 0, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 1, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 2, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 3, "TABLE_TYPE", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 4, "REMARKS", PG_TYPE_VARCHAR, 254);

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
			tuple = QR_AddNew(res);

			/* set_tuplefield_null(&tuple[0]); */
			set_tuplefield_string(&tuple[0], "");

			/*
			 * I have to hide the table owner from Access, otherwise it
			 * insists on referring to the table as 'owner.table'. (this
			 * is valid according to the ODBC SQL grammar, but Postgres
			 * won't support it.)
			 *
			 * set_tuplefield_string(&tuple[1], table_owner);
			 */

			mylog("%s: table_name = '%s'\n", func, table_name);

			if (conn->schema_support)
				set_tuplefield_string(&tuple[1], GET_SCHEMA_NAME(table_owner));
			else
				set_tuplefield_null(&tuple[1]);
			set_tuplefield_string(&tuple[2], table_name);
			set_tuplefield_string(&tuple[3], systable ? "SYSTEM TABLE" : (view ? "VIEW" : "TABLE"));
			set_tuplefield_string(&tuple[4], "");
			/*** set_tuplefield_string(&tuple[4], "TABLE"); ***/
		}
		result = PGAPI_Fetch(htbl_stmt);
	}
	if (result != SQL_NO_DATA_FOUND)
	{
		SC_full_error_copy(stmt, htbl_stmt, FALSE);
		goto cleanup;
	}
	ret = SQL_SUCCESS;

cleanup:
#undef	return
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	if (htbl_stmt)
		PGAPI_FreeStmt(htbl_stmt, SQL_DROP);

	if (stmt->internal)
		ret = DiscardStatementSvp(stmt, ret, FALSE);
	mylog("%s: EXIT, stmt=%x, ret=%d\n", func, stmt, ret);
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
	if (srclen <= 0 || !src)
	{
		dest[0] = '\0';
		return STRCPY_FAIL;
	}
mylog("src=%s ", src);
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
					if (SEARCH_PATTERN_ESCAPE == ESCAPE_IN_LITERAL)
						dest[outlen++] = ESCAPE_IN_LITERAL; /* and insert 1 more LEXER escape */
					dest[outlen++] = SEARCH_PATTERN_ESCAPE; /* MOVE LIKE escape */
					break;
				default:
					dest[outlen++] = ESCAPE_IN_LITERAL;
					if (outlen < dst_len)
						dest[outlen++] = ESCAPE_IN_LITERAL;
					if (outlen < dst_len)
						dest[outlen++] = ESCAPE_IN_LITERAL;
					if (outlen < dst_len)
						dest[outlen++] = ESCAPE_IN_LITERAL;
					break;
			}
		}
		if (*in == SEARCH_PATTERN_ESCAPE)
			escape_in = TRUE;
		else
		{
			escape_in = FALSE;
			if (outlen < dst_len)
				dest[outlen++] = *in;
		}
	}
	if (outlen < dst_len)
		dest[outlen] = '\0';
mylog("dest=%s\n", dest);
	return outlen;
}

static char	*
simpleCatalogEscape(const char *src, int srclen, char escape_ch, int *result_len, int ccsc)
{
	int	i, outlen;
	const char *in;
	char	*dest = NULL;
	BOOL	escape_in = FALSE;
	encoded_str	encstr;

	if (result_len)
		*result_len = 0;
	if (srclen == SQL_NULL_DATA)
		return dest;
	else if (srclen == SQL_NTS)
		srclen = strlen(src);
	if (srclen <= 0 || !src)
		return dest;
mylog("simple in=%s(%d)\n", src, srclen);
	encoded_str_constr(&encstr, ccsc, src);
	dest = malloc(2 * srclen + 1);
/* Inochi 2005.12.14 */
	for (i = 0, in = src, outlen = 0; i < srclen; i++, in++)
	{
                encoded_nextchar(&encstr);
                if (ENCODE_STATUS(encstr) != 0)
                {
                        dest[outlen++] = *in;
                        continue;
                }
		if ('\'' == *in ||
		    escape_ch == *in)
			dest[outlen++] = *in;
		dest[outlen++] = *in;
	}
	dest[outlen] = '\0';
	if (result_len)
		*result_len = outlen;
mylog("output=%s(%d)\n", dest, outlen);
	return dest;
}

static char	*
adjustLikePattern(const char *src, int srclen, char escape_ch, int *result_len, int ccsc)
{
	int	i, outlen;
	const char *in;
	char	*dest = NULL;
	BOOL	escape_in = FALSE;
	encoded_str	encstr;

	if (result_len)
		*result_len = 0;
	if (srclen == SQL_NULL_DATA)
		return dest;
	else if (srclen == SQL_NTS)
		srclen = strlen(src);
	if (srclen <= 0 || !src)
		return dest;
	encoded_str_constr(&encstr, ccsc, src);
	dest = malloc(2 * srclen + 1);
	for (i = 0, in = src, outlen = 0; i < srclen; i++, in++)
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
					if (escape_ch == ESCAPE_IN_LITERAL)
						dest[outlen++] = ESCAPE_IN_LITERAL; /* and insert 1 more LEXER escape */
					dest[outlen++] = escape_ch;
					break;
				default:
					if (escape_ch == ESCAPE_IN_LITERAL)
						dest[outlen++] = ESCAPE_IN_LITERAL;
					dest[outlen++] = escape_ch;
					if (escape_ch == ESCAPE_IN_LITERAL)
						dest[outlen++] = ESCAPE_IN_LITERAL;
					dest[outlen++] = escape_ch;
					break;
			}
		}
		if (*in == escape_ch)
			escape_in = TRUE;
		else
		{
			escape_in = FALSE;
			dest[outlen++] = *in;
		}
	}
	dest[outlen] = '\0';
	if (result_len)
		*result_len = outlen;
	return dest;
}

RETCODE		SQL_API
PGAPI_Columns(
			  HSTMT hstmt,
			  UCHAR FAR * szTableQualifier, /* OA */
			  SWORD cbTableQualifier,
			  UCHAR FAR * szTableOwner, /* PV */
			  SWORD cbTableOwner,
			  UCHAR FAR * szTableName, /* PV */
			  SWORD cbTableName,
			  UCHAR FAR * szColumnName, /* PV */
			  SWORD cbColumnName,
			  UWORD	flag)
{
	CSTR func = "PGAPI_Columns";
	StatementClass *stmt = (StatementClass *) hstmt;
	QResultClass	*res;
	TupleField	*tuple;
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
	char	*escTableName = NULL, *escColumnName = NULL;
	BOOL	search_pattern,	relisaview;
	ConnInfo   *ci;
	ConnectionClass *conn;
	SWORD		internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char	*like_or_eq;
	const char	*szSchemaName;

	mylog("%s: entering...stmt=%x scnm=%x len=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
#ifdef	UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */
retry_public_schema:
	/*
	 * Create the query to find out the columns (Note: pre 6.3 did not
	 * have the atttypmod field)
	 */
	if (conn->schema_support)
		snprintf(columns_query, sizeof(columns_query), "select u.nspname, c.relname, a.attname, a.atttypid"
	   ", t.typname, a.attnum, a.attlen, %s, a.attnotnull, c.relhasrules, c.relkind"
			" from pg_catalog.pg_namespace u, pg_catalog.pg_class c,"
			" pg_catalog.pg_attribute a, pg_catalog.pg_type t"
			" where u.oid = c.relnamespace"
			" and (not a.attisdropped)"
			" and c.oid= a.attrelid and a.atttypid = t.oid and (a.attnum > 0)",
			"a.atttypmod");
	else
		snprintf(columns_query, sizeof(columns_query), "select u.usename, c.relname, a.attname, a.atttypid"
	   ", t.typname, a.attnum, a.attlen, %s, a.attnotnull, c.relhasrules, c.relkind"
			" from pg_user u, pg_class c, pg_attribute a, pg_type t"
			" where u.usesysid = c.relowner"
	  " and c.oid= a.attrelid and a.atttypid = t.oid and (a.attnum > 0)",
			PG_VERSION_LE(conn, 6.2) ? "a.attlen" : "a.atttypmod");

	/*
	 *	TableName or ColumnName is ordinarily an pattern value,
	 */
	search_pattern = ((flag & PODBC_NOT_SEARCH_PATTERN) == 0); 
	if (search_pattern) 
	{
		like_or_eq = likeop;
		escTableName = adjustLikePattern(szTableName, cbTableName, SEARCH_PATTERN_ESCAPE, NULL, conn->ccsc);
		escColumnName = adjustLikePattern(szColumnName, cbColumnName, SEARCH_PATTERN_ESCAPE, NULL, conn->ccsc);
	}
	else
	{
		like_or_eq = eqop;
		escTableName = simpleCatalogEscape(szTableName, cbTableName, ESCAPE_IN_LITERAL, NULL, conn->ccsc);
		escColumnName = simpleCatalogEscape(szColumnName, cbColumnName, ESCAPE_IN_LITERAL, NULL, conn->ccsc);
	}
	if (escTableName)
		snprintf(columns_query, sizeof(columns_query), "%s and c.relname %s '%s'", columns_query, like_or_eq, escTableName);
	if (conn->schema_support)
		schema_strcat1(columns_query, " and u.nspname %s '%.*s'", like_or_eq, szSchemaName, cbSchemaName, szTableName, cbTableName, conn);
	else
		my_strcat1(columns_query, " and u.usename %s '%.*s'", like_or_eq, szSchemaName, cbSchemaName);
	my_strcat1(columns_query, " and a.attname %s '%.*s'", like_or_eq, szColumnName, cbColumnName);

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
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_Columns result.", func);
		result = SQL_ERROR;
		goto cleanup;
	}
	col_stmt = (StatementClass *) hcol_stmt;

	mylog("%s: hcol_stmt = %x, col_stmt = %x\n", func, hcol_stmt, col_stmt);

	result = PGAPI_ExecDirect(hcol_stmt, columns_query, SQL_NTS, 0);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_full_error_copy(stmt, col_stmt, FALSE);
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
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 2, internal_asis_type,
						   table_name, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 3, internal_asis_type,
						   field_name, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 4, SQL_C_LONG,
						   &field_type, 4, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 5, internal_asis_type,
						   field_type_name, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 6, SQL_C_SHORT,
						   &field_number, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 7, SQL_C_LONG,
						   &field_length, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 8, SQL_C_LONG,
						   &mod_length, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 9, internal_asis_type,
						   not_null, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 10, internal_asis_type,
						   relhasrules, MAX_INFO_STRING, NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 11, internal_asis_type,
						   relkind, sizeof(relkind), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_Columns result.", func);
		goto cleanup;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until */

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
#if (ODBCVER >= 0x0300)
	reserved_cols = 18;
#else
	reserved_cols = 12;
#endif /* ODBCVER */
	result_cols = reserved_cols + 2;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info_v(res, COLUMNS_CATALOG_NAME, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, COLUMNS_SCHEMA_NAME, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, COLUMNS_TABLE_NAME, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, COLUMNS_COLUMN_NAME, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, COLUMNS_DATA_TYPE, "DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, COLUMNS_TYPE_NAME, "TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, COLUMNS_PRECISION, "PRECISION", PG_TYPE_INT4, 4); /* COLUMN_SIZE */
	QR_set_field_info_v(res, COLUMNS_LENGTH, "LENGTH", PG_TYPE_INT4, 4); /* BUFFER_LENGTH */
	QR_set_field_info_v(res, COLUMNS_SCALE, "SCALE", PG_TYPE_INT2, 2); /* DECIMAL_DIGITS ***/
	QR_set_field_info_v(res, COLUMNS_RADIX, "RADIX", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, COLUMNS_NULLABLE, "NULLABLE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, COLUMNS_REMARKS, "REMARKS", PG_TYPE_VARCHAR, 254);

#if (ODBCVER >= 0x0300)
	QR_set_field_info_v(res, COLUMNS_COLUMN_DEF, "COLUMN_DEF", PG_TYPE_INT4, 254);
	QR_set_field_info_v(res, COLUMNS_SQL_DATA_TYPE, "SQL_DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, COLUMNS_SQL_DATETIME_SUB, "SQL_DATETIME_SUB", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, COLUMNS_CHAR_OCTET_LENGTH, "CHAR_OCTET_LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, COLUMNS_ORDINAL_POSITION, "ORDINAL_POSITION", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, COLUMNS_IS_NULLABLE, "IS_NULLABLE", PG_TYPE_VARCHAR, 254);
#endif /* ODBCVER */
	/* User defined fields */
	QR_set_field_info_v(res, COLUMNS_DISPLAY_SIZE, "DISPLAY_SIZE", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, COLUMNS_FIELD_TYPE, "FIELD_TYPE", PG_TYPE_INT4, 4);
	if (reserved_cols != COLUMNS_DISPLAY_SIZE)
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "reserved_cols unmatch", func);
		goto cleanup;
	}

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
			tuple = QR_AddNew(res);
			set_tuplefield_string(&tuple[COLUMNS_CATALOG_NAME], "");
			/* see note in SQLTables() */
			if (conn->schema_support)
				set_tuplefield_string(&tuple[COLUMNS_SCHEMA_NAME], GET_SCHEMA_NAME(table_owner));
			else
				set_tuplefield_string(&tuple[COLUMNS_SCHEMA_NAME], "");
			set_tuplefield_string(&tuple[COLUMNS_TABLE_NAME], table_name);
			set_tuplefield_string(&tuple[COLUMNS_COLUMN_NAME], "oid");
			sqltype = pgtype_to_concise_type(stmt, the_type, PG_STATIC);
			set_tuplefield_int2(&tuple[COLUMNS_DATA_TYPE], sqltype);
			set_tuplefield_string(&tuple[COLUMNS_TYPE_NAME], "OID");

			set_tuplefield_int4(&tuple[COLUMNS_PRECISION], pgtype_column_size(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&tuple[COLUMNS_LENGTH], pgtype_buffer_length(stmt, the_type, PG_STATIC, PG_STATIC));
			set_nullfield_int2(&tuple[COLUMNS_SCALE], pgtype_decimal_digits(stmt, the_type, PG_STATIC));
			set_nullfield_int2(&tuple[COLUMNS_RADIX], pgtype_radix(stmt, the_type));
			set_tuplefield_int2(&tuple[COLUMNS_NULLABLE], SQL_NO_NULLS);
			set_tuplefield_string(&tuple[COLUMNS_REMARKS], "");

#if (ODBCVER >= 0x0300)
			set_tuplefield_null(&tuple[COLUMNS_COLUMN_DEF]);
			set_tuplefield_int2(&tuple[COLUMNS_SQL_DATA_TYPE], sqltype);
			set_tuplefield_null(&tuple[COLUMNS_SQL_DATETIME_SUB]);
			set_tuplefield_int4(&tuple[COLUMNS_CHAR_OCTET_LENGTH], pgtype_transfer_octet_length(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&tuple[COLUMNS_ORDINAL_POSITION], ordinal);
			set_tuplefield_string(&tuple[COLUMNS_IS_NULLABLE], "No");
#endif /* ODBCVER */
			set_tuplefield_int4(&tuple[COLUMNS_DISPLAY_SIZE], pgtype_display_size(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&tuple[COLUMNS_FIELD_TYPE], the_type);
			ordinal++;
		}
	}

	while ((result == SQL_SUCCESS) || (result == SQL_SUCCESS_WITH_INFO))
	{
		tuple = QR_AddNew(res);

		sqltype = SQL_TYPE_NULL;	/* unspecified */
		set_tuplefield_string(&tuple[COLUMNS_CATALOG_NAME], "");
		/* see note in SQLTables() */
		if (conn->schema_support)
			set_tuplefield_string(&tuple[COLUMNS_SCHEMA_NAME], GET_SCHEMA_NAME(table_owner));
		else
			set_tuplefield_string(&tuple[COLUMNS_SCHEMA_NAME], "");
		set_tuplefield_string(&tuple[COLUMNS_TABLE_NAME], table_name);
		set_tuplefield_string(&tuple[COLUMNS_COLUMN_NAME], field_name);
		set_tuplefield_string(&tuple[COLUMNS_TYPE_NAME], field_type_name);


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
		qlog("%s: table='%s',field_name='%s',type=%d,name='%s'\n",
			 func, table_name, field_name, field_type, field_type_name);

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

				set_tuplefield_int4(&tuple[COLUMNS_PRECISION], column_size);
				set_tuplefield_int4(&tuple[COLUMNS_LENGTH], column_size + 2);		/* sign+dec.point */
				set_nullfield_int2(&tuple[COLUMNS_SCALE], decimal_digits);
#if (ODBCVER >= 0x0300)
				set_tuplefield_null(&tuple[COLUMNS_CHAR_OCTET_LENGTH]);
#endif /* ODBCVER */
				set_tuplefield_int4(&tuple[COLUMNS_DISPLAY_SIZE], column_size + 2);	/* sign+dec.point */
			}
		}
		else if ((field_type == PG_TYPE_DATETIME) ||
			(field_type == PG_TYPE_TIMESTAMP_NO_TMZONE))
		{
			if (PG_VERSION_GE(conn, 7.2))
			{
				useStaticScale = FALSE;

				set_nullfield_int2(&tuple[COLUMNS_SCALE], (Int2) mod_length);
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
#ifdef	__MS_REPORTS_ANSI_CHAR__
			if (mod_length > ci->drivers.max_varchar_size)
				sqltype = SQL_LONGVARCHAR;
			else
				sqltype = (field_type == PG_TYPE_BPCHAR) ? SQL_CHAR : SQL_VARCHAR;
#else
			if (mod_length > ci->drivers.max_varchar_size)
				sqltype = (conn->unicode ? SQL_WLONGVARCHAR : SQL_LONGVARCHAR);
			else
				sqltype = (field_type == PG_TYPE_BPCHAR) ? (conn->unicode ? SQL_WCHAR : SQL_CHAR) : (conn->unicode ? SQL_WVARCHAR : SQL_VARCHAR);
#endif /* __MS_LOVES_REPORTS_CHAR__ */

			mylog("%s: field type is VARCHAR,BPCHAR: field_type = %d, mod_length = %d\n", func, field_type, mod_length);

			set_tuplefield_int4(&tuple[COLUMNS_PRECISION], mod_length);
			set_tuplefield_int4(&tuple[COLUMNS_LENGTH], mod_length);
#if (ODBCVER >= 0x0300)
			set_tuplefield_int4(&tuple[COLUMNS_CHAR_OCTET_LENGTH], pgtype_transfer_octet_length(stmt, field_type, PG_STATIC, PG_STATIC));
#endif /* ODBCVER */
			set_tuplefield_int4(&tuple[COLUMNS_DISPLAY_SIZE], mod_length);
		}

		if (useStaticPrecision)
		{
			mylog("%s: field type is OTHER: field_type = %d, pgtype_length = %d\n", func, field_type, pgtype_buffer_length(stmt, field_type, PG_STATIC, PG_STATIC));

			set_tuplefield_int4(&tuple[COLUMNS_PRECISION], pgtype_column_size(stmt, field_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&tuple[COLUMNS_LENGTH], pgtype_buffer_length(stmt, field_type, PG_STATIC, PG_STATIC));
#if (ODBCVER >= 0x0300)
			set_tuplefield_null(&tuple[COLUMNS_CHAR_OCTET_LENGTH]);
#endif /* ODBCVER */
			set_tuplefield_int4(&tuple[COLUMNS_DISPLAY_SIZE], pgtype_display_size(stmt, field_type, PG_STATIC, PG_STATIC));
		}
		if (useStaticScale)
		{
			set_nullfield_int2(&tuple[COLUMNS_SCALE], pgtype_decimal_digits(stmt, field_type, PG_STATIC));
		}

		if (SQL_TYPE_NULL == sqltype)
		{
			sqltype = pgtype_to_concise_type(stmt, field_type, PG_STATIC);
			concise_type = pgtype_to_sqldesctype(stmt, field_type, PG_STATIC);
		}
		else
			concise_type = sqltype;
		set_tuplefield_int2(&tuple[COLUMNS_DATA_TYPE], sqltype);

		set_nullfield_int2(&tuple[COLUMNS_RADIX], pgtype_radix(stmt, field_type));
		set_tuplefield_int2(&tuple[COLUMNS_NULLABLE], (Int2) (not_null[0] == '1' ? SQL_NO_NULLS : pgtype_nullable(stmt, field_type)));
		set_tuplefield_string(&tuple[COLUMNS_REMARKS], "");
#if (ODBCVER >= 0x0300)
		set_tuplefield_null(&tuple[COLUMNS_COLUMN_DEF]);
		set_tuplefield_int2(&tuple[COLUMNS_SQL_DATA_TYPE], concise_type);
		set_nullfield_int2(&tuple[COLUMNS_SQL_DATETIME_SUB], pgtype_to_datetime_sub(stmt, field_type));
		set_tuplefield_int4(&tuple[COLUMNS_ORDINAL_POSITION], ordinal);
		set_tuplefield_null(&tuple[COLUMNS_IS_NULLABLE]);
#endif /* ODBCVER */
		set_tuplefield_int4(&tuple[COLUMNS_FIELD_TYPE], field_type);
		ordinal++;

		result = PGAPI_Fetch(hcol_stmt);

	}
	if (result != SQL_NO_DATA_FOUND)
	{
		SC_full_error_copy(stmt, col_stmt, FALSE);
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

		tuple = QR_AddNew(res);

		set_tuplefield_string(&tuple[COLUMNS_CATALOG_NAME], "");
		if (conn->schema_support)
			set_tuplefield_string(&tuple[COLUMNS_SCHEMA_NAME], GET_SCHEMA_NAME(table_owner));
		else
			set_tuplefield_string(&tuple[COLUMNS_SCHEMA_NAME], "");
		set_tuplefield_string(&tuple[COLUMNS_TABLE_NAME], table_name);
		set_tuplefield_string(&tuple[COLUMNS_COLUMN_NAME], "xmin");
		sqltype = pgtype_to_concise_type(stmt, the_type, PG_STATIC);
		set_tuplefield_int2(&tuple[COLUMNS_DATA_TYPE], sqltype);
		set_tuplefield_string(&tuple[COLUMNS_TYPE_NAME], pgtype_to_name(stmt, the_type));
		set_tuplefield_int4(&tuple[COLUMNS_PRECISION], pgtype_column_size(stmt, the_type, PG_STATIC, PG_STATIC));
		set_tuplefield_int4(&tuple[COLUMNS_LENGTH], pgtype_buffer_length(stmt, the_type, PG_STATIC, PG_STATIC));
		set_nullfield_int2(&tuple[COLUMNS_SCALE], pgtype_decimal_digits(stmt, the_type, PG_STATIC));
		set_nullfield_int2(&tuple[COLUMNS_RADIX], pgtype_radix(stmt, the_type));
		set_tuplefield_int2(&tuple[COLUMNS_NULLABLE], SQL_NO_NULLS);
		set_tuplefield_string(&tuple[COLUMNS_REMARKS], "");
#if (ODBCVER >= 0x0300)
		set_tuplefield_null(&tuple[COLUMNS_COLUMN_DEF]);
		set_tuplefield_int2(&tuple[COLUMNS_SQL_DATA_TYPE], sqltype);
		set_tuplefield_null(&tuple[COLUMNS_SQL_DATETIME_SUB]);
		set_tuplefield_int4(&tuple[COLUMNS_CHAR_OCTET_LENGTH], pgtype_transfer_octet_length(stmt, the_type, PG_STATIC, PG_STATIC));
		set_tuplefield_int4(&tuple[COLUMNS_ORDINAL_POSITION], ordinal);
		set_tuplefield_string(&tuple[COLUMNS_IS_NULLABLE], "No");
#endif /* ODBCVER */
		set_tuplefield_int4(&tuple[COLUMNS_DISPLAY_SIZE], pgtype_display_size(stmt, the_type, PG_STATIC, PG_STATIC));
		set_tuplefield_int4(&tuple[COLUMNS_FIELD_TYPE], the_type);
		ordinal++;
	}
	result = SQL_SUCCESS;

cleanup:
#undef	return
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	if (escTableName)
		free(escTableName);
	if (escColumnName)
		free(escColumnName);
	if (hcol_stmt)
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	if (stmt->internal)
		result = DiscardStatementSvp(stmt, result, FALSE);
	mylog("%s: EXIT,  stmt=%x\n", func, stmt);
	return result;
}


RETCODE		SQL_API
PGAPI_SpecialColumns(
					 HSTMT hstmt,
					 UWORD fColType,
					 UCHAR FAR * szTableQualifier, /* OA */
					 SWORD cbTableQualifier,
					 UCHAR FAR * szTableOwner, /* OA */
					 SWORD cbTableOwner,
					 UCHAR FAR * szTableName, /* OA(R) */
					 SWORD cbTableName,
					 UWORD fScope,
					 UWORD fNullable)
{
	CSTR func = "PGAPI_SpecialColumns";
	TupleField	*tuple;
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	QResultClass	*res;
	ConnInfo   *ci;
	HSTMT		hcol_stmt = NULL;
	StatementClass *col_stmt;
	char		columns_query[INFO_INQUIRY_LEN];
	char		*table_name = NULL, *escTableName = NULL;
	RETCODE		result = SQL_SUCCESS;
	char		relhasrules[MAX_INFO_STRING], relkind[8], relhasoids[8];
	BOOL		relisaview;
	SWORD		internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char	*szSchemaName;

	mylog("%s: entering...stmt=%x scnm=%x len=%d colType=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner, fColType);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;
	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
#ifdef	UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */

	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

	if (!szTableName)
	{
		SC_set_error(stmt, STMT_INVALID_NULL_ARG, "The table name is required", func);
		return result;
	}
#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */
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
	table_name = make_string(szTableName, cbTableName, NULL, 0);
	escTableName = simpleCatalogEscape(table_name, SQL_NTS, ESCAPE_IN_LITERAL, NULL, conn->ccsc); 
	/* my_strcat(columns_query, " and c.relname = '%.*s'", szTableName, cbTableName); */
	if (escTableName)
		snprintf(columns_query, sizeof(columns_query), "%s and c.relname = '%s'", columns_query, escTableName);
	/* SchemaName cannot contain a string search pattern */
	if (conn->schema_support)
		schema_strcat(columns_query, " and u.nspname = '%.*s'", szSchemaName, cbSchemaName, szTableName, cbTableName, conn);
	else
		my_strcat(columns_query, " and u.usename = '%.*s'", szSchemaName, cbSchemaName);


	result = PGAPI_AllocStmt(stmt->hdbc, &hcol_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for SQLSpecialColumns result.", func);
		result = SQL_ERROR;
		goto cleanup;
	}
	col_stmt = (StatementClass *) hcol_stmt;

	mylog("%s: hcol_stmt = %x, col_stmt = %x\n", func, hcol_stmt, col_stmt);

	result = PGAPI_ExecDirect(hcol_stmt, columns_query, SQL_NTS, 0);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_full_error_copy(stmt, col_stmt, FALSE);
		result = SQL_ERROR;
		goto cleanup;
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
		SC_error_copy(stmt, col_stmt, TRUE);
		result = SQL_ERROR;
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 2, internal_asis_type,
					relkind, sizeof(relkind), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		result = SQL_ERROR;
		goto cleanup;
	}
	relhasoids[0] = '1';
	if (PG_VERSION_GE(conn, 7.2))
	{
		result = PGAPI_BindCol(hcol_stmt, 3, internal_asis_type,
					relhasoids, sizeof(relhasoids), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, col_stmt, TRUE);
			result = SQL_ERROR;
			goto cleanup;
		}
	}

	result = PGAPI_Fetch(hcol_stmt);
	if (PG_VERSION_GE(conn, 7.1))
		relisaview = (relkind[0] == 'v');
	else
		relisaview = (relhasrules[0] == '1');
	PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	hcol_stmt = NULL;

	res = QR_Constructor();
	SC_set_Result(stmt, res);
	extend_column_bindings(SC_get_ARDF(stmt), 8);

	stmt->catalog_result = TRUE;
	QR_set_num_fields(res, 8);
	QR_set_field_info_v(res, 0, "SCOPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 1, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 2, "DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 3, "TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 4, "PRECISION", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 5, "LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 6, "SCALE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 7, "PSEUDO_COLUMN", PG_TYPE_INT2, 2);

	if (relisaview)
	{
		/* there's no oid for views */
		if (fColType == SQL_BEST_ROWID)
		{
			result = SQL_NO_DATA_FOUND;
			goto cleanup;
		}
		else if (fColType == SQL_ROWVER)
		{
			Int2		the_type = PG_TYPE_TID;

			tuple = QR_AddNew(res);

			set_tuplefield_null(&tuple[0]);
			set_tuplefield_string(&tuple[1], "ctid");
			set_tuplefield_int2(&tuple[2], pgtype_to_concise_type(stmt, the_type, PG_STATIC));
			set_tuplefield_string(&tuple[3], pgtype_to_name(stmt, the_type));
			set_tuplefield_int4(&tuple[4], pgtype_column_size(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&tuple[5], pgtype_buffer_length(stmt, the_type, PG_STATIC, PG_STATIC));
			set_tuplefield_int2(&tuple[6], pgtype_decimal_digits(stmt, the_type, PG_STATIC));
			set_tuplefield_int2(&tuple[7], SQL_PC_NOT_PSEUDO);
inolog("Add ctid\n");
		}
	}
	else
	{
		/* use the oid value for the rowid */
		if (fColType == SQL_BEST_ROWID)
		{
			if (relhasoids[0] != '1')
			{
				result = SQL_NO_DATA_FOUND;
				goto cleanup;
			}
			tuple = QR_AddNew(res);

			set_tuplefield_int2(&tuple[0], SQL_SCOPE_SESSION);
			set_tuplefield_string(&tuple[1], "oid");
			set_tuplefield_int2(&tuple[2], pgtype_to_concise_type(stmt, PG_TYPE_OID, PG_STATIC));
			set_tuplefield_string(&tuple[3], "OID");
			set_tuplefield_int4(&tuple[4], pgtype_column_size(stmt, PG_TYPE_OID, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&tuple[5], pgtype_buffer_length(stmt, PG_TYPE_OID, PG_STATIC, PG_STATIC));
			set_tuplefield_int2(&tuple[6], pgtype_decimal_digits(stmt, PG_TYPE_OID, PG_STATIC));
			set_tuplefield_int2(&tuple[7], SQL_PC_PSEUDO);
		}
		else if (fColType == SQL_ROWVER)
		{
			Int2		the_type = PG_TYPE_INT4;

			if (atoi(ci->row_versioning))
			{
				tuple = QR_AddNew(res);

				set_tuplefield_null(&tuple[0]);
				set_tuplefield_string(&tuple[1], "xmin");
				set_tuplefield_int2(&tuple[2], pgtype_to_concise_type(stmt, the_type, PG_STATIC));
				set_tuplefield_string(&tuple[3], pgtype_to_name(stmt, the_type));
				set_tuplefield_int4(&tuple[4], pgtype_column_size(stmt, the_type, PG_STATIC, PG_STATIC));
				set_tuplefield_int4(&tuple[5], pgtype_buffer_length(stmt, the_type, PG_STATIC, PG_STATIC));
				set_tuplefield_int2(&tuple[6], pgtype_decimal_digits(stmt, the_type, PG_STATIC));
				set_tuplefield_int2(&tuple[7], SQL_PC_PSEUDO);
			}
		}
	}

cleanup:
#undef	return
	if (table_name)
		free(table_name);
	if (escTableName)
		free(escTableName);
	stmt->status = STMT_FINISHED;
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);
	if (hcol_stmt)
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	if (stmt->internal)
		result = DiscardStatementSvp(stmt, result, FALSE);
	mylog("%s: EXIT,  stmt=%x\n", func, stmt);
	return result;
}


RETCODE		SQL_API
PGAPI_Statistics(
				 HSTMT hstmt,
				 UCHAR FAR * szTableQualifier, /* OA */
				 SWORD cbTableQualifier,
				 UCHAR FAR * szTableOwner, /* OA */
				 SWORD cbTableOwner,
				 UCHAR FAR * szTableName, /* OA(R) */
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
	char		*table_name = NULL, *escTableName = NULL;
	char		index_name[MAX_INFO_STRING];
	short		fields_vector[INDEX_KEYS_STORAGE_COUNT];
	char		isunique[10],
				isclustered[10],
				ishash[MAX_INFO_STRING];
	SDWORD		index_name_len,
				fields_vector_len;
	TupleField	*tuple;
	int			i;
	StatementClass *col_stmt,
			   *indx_stmt;
	char		column_name[MAX_INFO_STRING],
			table_schemaname[MAX_INFO_STRING],
				relhasrules[10], relkind[8];
	char	  **column_names = NULL;
	SQLINTEGER	column_name_len;
	int			total_columns = 0;
	ConnInfo   *ci;
	char		buf[256];
	SWORD		internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char	*szSchemaName;

	mylog("%s: entering...stmt=%x scnm=%x len=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	if (!szTableName)
	{
		SC_set_error(stmt, STMT_INVALID_NULL_ARG, "The table name is required", func);
		return result;
	}
	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
#ifdef	UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_Statistics result.", func);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until */

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	extend_column_bindings(SC_get_ARDF(stmt), 13);

	stmt->catalog_result = TRUE;
	/* set the field names */
	QR_set_num_fields(res, 13);
	QR_set_field_info_v(res, 0, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 1, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 2, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 3, "NON_UNIQUE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 4, "INDEX_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 5, "INDEX_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 6, "TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 7, "SEQ_IN_INDEX", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 8, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 9, "COLLATION", PG_TYPE_CHAR, 1);
	QR_set_field_info_v(res, 10, "CARDINALITY", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 11, "PAGES", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 12, "FILTER_CONDITION", PG_TYPE_VARCHAR, MAX_INFO_STRING);

	/*
	 * only use the table name... the owner should be redundant, and we
	 * never use qualifiers.
	 */
	table_name = make_string(szTableName, cbTableName, NULL, 0);
	if (!table_name)
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "No table name passed to PGAPI_Statistics.", func);
		goto cleanup;
	}
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */
	table_schemaname[0] = '\0';
	if (conn->schema_support)
		schema_strcat(table_schemaname, "%.*s", szSchemaName, cbSchemaName, szTableName, cbTableName, conn);

	/*
	 * we need to get a list of the field names first, so we can return
	 * them later.
	 */
	result = PGAPI_AllocStmt(stmt->hdbc, &hcol_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in PGAPI_Statistics for columns.", func);
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
	result = PGAPI_Columns(hcol_stmt, "", 0, table_schemaname, SQL_NTS,
						   table_name, SQL_NTS, "", 0, PODBC_NOT_SEARCH_PATTERN | PODBC_SEARCH_PUBLIC_SCHEMA);
	col_stmt->internal = FALSE;

	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}
	result = PGAPI_BindCol(hcol_stmt, 4, internal_asis_type,
						 column_name, sizeof(column_name), &column_name_len);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;

	}

	result = PGAPI_Fetch(hcol_stmt);
	while ((result == SQL_SUCCESS) || (result == SQL_SUCCESS_WITH_INFO))
	{
		if (0 == total_columns)
			PGAPI_GetData(hcol_stmt, 2, internal_asis_type, table_schemaname, sizeof(table_schemaname), NULL);
		total_columns++;

		column_names =
			(char **) realloc(column_names,
							  total_columns * sizeof(char *));
		column_names[total_columns - 1] =
			(char *) malloc(strlen(column_name) + 1);
		strcpy(column_names[total_columns - 1], column_name);

		mylog("%s: column_name = '%s'\n", func, column_name);

		result = PGAPI_Fetch(hcol_stmt);
	}

	if (result != SQL_NO_DATA_FOUND)
	{
		SC_full_error_copy(stmt, col_stmt, FALSE);
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
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in SQLStatistics for indices.", func);
		goto cleanup;

	}
	indx_stmt = (StatementClass *) hindx_stmt;

	/* TableName cannot contain a string search pattern */
	escTableName = simpleCatalogEscape(table_name, SQL_NTS, ESCAPE_IN_LITERAL, NULL, conn->ccsc); 
	if (conn->schema_support)
		snprintf(index_query, sizeof(index_query), "select c.relname, i.indkey, i.indisunique"
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
			,escTableName, table_schemaname);
	else
		snprintf(index_query, sizeof(index_query), "select c.relname, i.indkey, i.indisunique"
			", i.indisclustered, a.amname, c.relhasrules"
			" from pg_index i, pg_class c, pg_class d, pg_am a"
			" where d.relname = '%s'"
			" and d.oid = i.indrelid"
			" and i.indexrelid = c.oid"
			" and c.relam = a.oid order by"
			,escTableName);
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
		SC_full_error_copy(stmt, indx_stmt, FALSE);
		goto cleanup;
	}

	/* bind the index name column */
	result = PGAPI_BindCol(hindx_stmt, 1, internal_asis_type,
						   index_name, MAX_INFO_STRING, &index_name_len);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);	/* "Couldn't bind column 
						* in SQLStatistics."; */
		goto cleanup;

	}
	/* bind the vector column */
	result = PGAPI_BindCol(hindx_stmt, 2, SQL_C_DEFAULT,
			fields_vector, INDEX_KEYS_STORAGE_COUNT * 2, &fields_vector_len);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt, TRUE); /* "Couldn't bind column 
						 * in SQLStatistics."; */
		goto cleanup;

	}
	/* bind the "is unique" column */
	result = PGAPI_BindCol(hindx_stmt, 3, internal_asis_type,
						   isunique, sizeof(isunique), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);	/* "Couldn't bind column 
						 * in SQLStatistics."; */
		goto cleanup;
	}

	/* bind the "is clustered" column */
	result = PGAPI_BindCol(hindx_stmt, 4, internal_asis_type,
						   isclustered, sizeof(isclustered), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);	/* "Couldn't bind column *
						 * in SQLStatistics."; */
		goto cleanup;

	}

	/* bind the "is hash" column */
	result = PGAPI_BindCol(hindx_stmt, 5, internal_asis_type,
						   ishash, sizeof(ishash), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);	/* "Couldn't bind column * 
						 * in SQLStatistics."; */
		goto cleanup;

	}

	result = PGAPI_BindCol(hindx_stmt, 6, internal_asis_type,
					relhasrules, sizeof(relhasrules), NULL);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);
		goto cleanup;
	}

	relhasrules[0] = '0';
	result = PGAPI_Fetch(hindx_stmt);
	/* fake index of OID */
	if (relhasrules[0] != '1' && atoi(ci->show_oid_column) && atoi(ci->fake_oid_index))
	{
		tuple = QR_AddNew(res);

		/* no table qualifier */
		set_tuplefield_string(&tuple[0], "");
		/* don't set the table owner, else Access tries to use it */
		set_tuplefield_string(&tuple[1], GET_SCHEMA_NAME(table_schemaname));
		set_tuplefield_string(&tuple[2], table_name);

		/* non-unique index? */
		set_tuplefield_int2(&tuple[3], (Int2) (ci->drivers.unique_index ? FALSE : TRUE));

		/* no index qualifier */
		set_tuplefield_string(&tuple[4], "");

		sprintf(buf, "%s_idx_fake_oid", table_name);
		set_tuplefield_string(&tuple[5], buf);

		/*
		 * Clustered/HASH index?
		 */
		set_tuplefield_int2(&tuple[6], (Int2) SQL_INDEX_OTHER);
		set_tuplefield_int2(&tuple[7], (Int2) 1);

		set_tuplefield_string(&tuple[8], "oid");
		set_tuplefield_string(&tuple[9], "A");
		set_tuplefield_null(&tuple[10]);
		set_tuplefield_null(&tuple[11]);
		set_tuplefield_null(&tuple[12]);
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
				tuple = QR_AddNew(res);

				/* no table qualifier */
				set_tuplefield_string(&tuple[0], "");
				/* don't set the table owner, else Access tries to use it */
				set_tuplefield_string(&tuple[1], GET_SCHEMA_NAME(table_schemaname));
				set_tuplefield_string(&tuple[2], table_name);

				/* non-unique index? */
				if (ci->drivers.unique_index)
					set_tuplefield_int2(&tuple[3], (Int2) (atoi(isunique) ? FALSE : TRUE));
				else
					set_tuplefield_int2(&tuple[3], TRUE);

				/* no index qualifier */
				set_tuplefield_string(&tuple[4], "");
				set_tuplefield_string(&tuple[5], index_name);

				/*
				 * Clustered/HASH index?
				 */
				set_tuplefield_int2(&tuple[6], (Int2)
							   (atoi(isclustered) ? SQL_INDEX_CLUSTERED :
								(!strncmp(ishash, "hash", 4)) ? SQL_INDEX_HASHED : SQL_INDEX_OTHER));
				set_tuplefield_int2(&tuple[7], (Int2) (i + 1));

				if (fields_vector[i] == OID_ATTNUM)
				{
					set_tuplefield_string(&tuple[8], "oid");
					mylog("%s: column name = oid\n", func);
				}
				else if (fields_vector[i] < 0 || fields_vector[i] > total_columns)
				{
					set_tuplefield_string(&tuple[8], "UNKNOWN");
					mylog("%s: column name = UNKNOWN\n", func);
				}
				else
				{
					set_tuplefield_string(&tuple[8], column_names[fields_vector[i] - 1]);
					mylog("%s: column name = '%s'\n", func, column_names[fields_vector[i] - 1]);
				}

				set_tuplefield_string(&tuple[9], "A");
				set_tuplefield_null(&tuple[10]);
				set_tuplefield_null(&tuple[11]);
				set_tuplefield_null(&tuple[12]);
				i++;
			}
		}

		result = PGAPI_Fetch(hindx_stmt);
	}
	if (result != SQL_NO_DATA_FOUND)
	{
		/* "SQLFetch failed in SQLStatistics."; */
		SC_full_error_copy(stmt, indx_stmt, FALSE);
		goto cleanup;
	}
	ret = SQL_SUCCESS;

cleanup:
#undef	return
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	if (hcol_stmt)
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	if (hindx_stmt)
		PGAPI_FreeStmt(hindx_stmt, SQL_DROP);
	/* These things should be freed on any error ALSO! */
	if (table_name)
		free(table_name);
	if (escTableName)
		free(escTableName);
	if (column_names)
	{
		for (i = 0; i < total_columns; i++)
			free(column_names[i]);
		free(column_names);
	}

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	if (stmt->internal)
		ret = DiscardStatementSvp(stmt, ret, FALSE);
	mylog("%s: EXIT, stmt=%x, ret=%d\n", func, stmt, ret);

	return ret;
}


RETCODE		SQL_API
PGAPI_ColumnPrivileges(
					   HSTMT hstmt,
					   UCHAR FAR * szTableQualifier, /* OA */
					   SWORD cbTableQualifier,
					   UCHAR FAR * szTableOwner, /* OA */
					   SWORD cbTableOwner,
					   UCHAR FAR * szTableName, /* OA(R) */
					   SWORD cbTableName,
					   UCHAR FAR * szColumnName, /* PV */
					   SWORD cbColumnName,
					   UWORD flag)
{
	CSTR func = "PGAPI_ColumnPrivileges";
	StatementClass	*stmt = (StatementClass *) hstmt;
	ConnectionClass	*conn = SC_get_conn(stmt);
	RETCODE	result = SQL_ERROR;
	char	*escSchemaName = NULL, *escTableName = NULL, *escColumnName = NULL;
	const char	*like_or_eq;
	char	column_query[INFO_INQUIRY_LEN];
	BOOL	search_pattern;
	QResultClass	*res;

	mylog("%s: entering...\n", func);

	/* Neither Access or Borland care about this. */

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;
	if (PG_VERSION_LT(conn, 7.4))
		SC_set_error(stmt, STMT_NOT_IMPLEMENTED_ERROR, "Function not implementedyet", func);
	escSchemaName = simpleCatalogEscape(szTableOwner, cbTableOwner, ESCAPE_IN_LITERAL, NULL, conn->ccsc);
	escTableName = simpleCatalogEscape(szTableName, cbTableName, ESCAPE_IN_LITERAL, NULL, conn->ccsc);
	search_pattern = (0 == (flag & PODBC_NOT_SEARCH_PATTERN));
	if (search_pattern) 
	{
		like_or_eq = likeop;
		escColumnName = adjustLikePattern(szColumnName, cbColumnName, SEARCH_PATTERN_ESCAPE, NULL, conn->ccsc);
	}
	else
	{
		like_or_eq = eqop;
		escColumnName = simpleCatalogEscape(szColumnName, cbColumnName, ESCAPE_IN_LITERAL, NULL, conn->ccsc);
	}
	strcpy(column_query, "select '' as TABLE_CAT, table_schema as TABLE_SCHEM,"
			" table_name, column_name, grantor, grantee,"
			" privilege_type as PRIVILEGE, is_grantable from"
			" information_schema.column_privileges where true");
	if (escSchemaName)
		snprintf(column_query, sizeof(column_query),
			"%s and table_schem = '%s'", column_query, escSchemaName);  
	if (escTableName)
		snprintf(column_query, sizeof(column_query),
			"%s and table_name = '%s'", column_query, escTableName);  
	if (escColumnName)
		snprintf(column_query, sizeof(column_query),
			"%s and column_name %s '%s'", column_query, like_or_eq, escColumnName);
	if (res = CC_send_query(conn, column_query, NULL, IGNORE_ABORT_ON_CONN, stmt), !QR_command_maybe_successful(res))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_ColumnPrivileges query error", func);
		QR_Destructor(res);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	extend_column_bindings(SC_get_ARDF(stmt), 8);
	/* set up the current tuple pointer for SQLFetch */
	result = SQL_SUCCESS;
cleanup:
	/* set up the current tuple pointer for SQLFetch */
	stmt->status = STMT_FINISHED;
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	if (escSchemaName)
		free(escSchemaName);
	if (escTableName)
		free(escTableName);
	if (escColumnName)
		free(escColumnName);
	return result;
}


/*
 *	SQLPrimaryKeys()
 *
 *	Retrieve the primary key columns for the specified table.
 */
RETCODE		SQL_API
PGAPI_PrimaryKeys(
				  HSTMT hstmt,
				  UCHAR FAR * szTableQualifier, /* OA */
				  SWORD cbTableQualifier,
				  UCHAR FAR * szTableOwner, /* OA */
				  SWORD cbTableOwner,
				  UCHAR FAR * szTableName, /* OA(R) */
				  SWORD cbTableName)
{
	CSTR func = "PGAPI_PrimaryKeys";
	StatementClass *stmt = (StatementClass *) hstmt;
	QResultClass	*res;
	ConnectionClass *conn;
	TupleField	*tuple;
	RETCODE		ret = SQL_SUCCESS, result;
	int			seq = 0;
	HSTMT		htbl_stmt = NULL;
	StatementClass *tbl_stmt;
	char		tables_query[INFO_INQUIRY_LEN];
	char		attname[MAX_INFO_STRING];
	SDWORD		attname_len;
	char		*pktab = NULL, pkscm[TABLE_NAME_STORAGE_LEN + 1];
	Int2		result_cols;
	int			qno,
				qstart,
				qend;
	SWORD		internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char	*szSchemaName;

	mylog("%s: entering...stmt=%x scnm=%x len=%d\n", func, stmt, NULL_IF_NULL(szTableOwner), cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_PrimaryKeys result.", func);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until 
	 *
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	result_cols = 6;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info_v(res, 0, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 1, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 2, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 3, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 4, "KEY_SEQ", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 5, "PK_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);


	result = PGAPI_AllocStmt(stmt->hdbc, &htbl_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for Primary Key result.", func);
		ret = SQL_ERROR;
		goto cleanup;
	}
	tbl_stmt = (StatementClass *) htbl_stmt;

	conn = SC_get_conn(stmt);
#ifdef	UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */

	pktab = make_string(szTableName, cbTableName, NULL, 0);
	if (!pktab || pktab[0] == '\0')
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "No Table specified to PGAPI_PrimaryKeys.", func);
		ret = SQL_ERROR;
		goto cleanup;
	}
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */
retry_public_schema:
	pkscm[0] = '\0';
	if (conn->schema_support)
		schema_strcat(pkscm, "%.*s", szSchemaName, cbSchemaName, szTableName, cbTableName, conn);

	result = PGAPI_BindCol(htbl_stmt, 1, internal_asis_type,
						   attname, MAX_INFO_STRING, &attname_len);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		ret = SQL_ERROR;
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
					snprintf(tables_query, sizeof(tables_query), "select ta.attname, ia.attnum"
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
					snprintf(tables_query, sizeof(tables_query), "select ta.attname, ia.attnum"
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
					snprintf(tables_query, sizeof(tables_query), "select ta.attname, ia.attnum"
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
					snprintf(tables_query, sizeof(tables_query), "select ta.attname, ia.attnum"
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
			SC_full_error_copy(stmt, tbl_stmt, FALSE);
			ret = SQL_ERROR;
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
		tuple = QR_AddNew(res);

		set_tuplefield_null(&tuple[0]);

		/*
		 * I have to hide the table owner from Access, otherwise it
		 * insists on referring to the table as 'owner.table'. (this is
		 * valid according to the ODBC SQL grammar, but Postgres won't
		 * support it.)
		 */
		set_tuplefield_string(&tuple[1], GET_SCHEMA_NAME(pkscm));
		set_tuplefield_string(&tuple[2], pktab);
		set_tuplefield_string(&tuple[3], attname);
		set_tuplefield_int2(&tuple[4], (Int2) (++seq));
		set_tuplefield_null(&tuple[5]);

		mylog(">> primaryKeys: pktab = '%s', attname = '%s', seq = %d\n", pktab, attname, seq);

		result = PGAPI_Fetch(htbl_stmt);
	}

	if (result != SQL_NO_DATA_FOUND)
	{
		SC_full_error_copy(stmt, htbl_stmt, FALSE);
		ret = SQL_ERROR;
		goto cleanup;
	}
	ret = SQL_SUCCESS;

cleanup:
#undef	return
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	if (htbl_stmt)
		PGAPI_FreeStmt(htbl_stmt, SQL_DROP);

	if (pktab)
		free(pktab);
	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	if (stmt->internal)
		ret = DiscardStatementSvp(stmt, ret, FALSE); 
	mylog("%s: EXIT, stmt=%x, ret=%d\n", func, stmt, ret);
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
	if (!conn->origianl_client_encoding || !isMultibyte(serverTableName))
		return ret;
	if (!conn->server_encoding)
	{
		if (res = CC_send_query(conn, "select getdatabaseencoding()", NULL, IGNORE_ABORT_ON_CONN | ROLLBACK_ON_ERROR, NULL), QR_command_maybe_successsful(res))
		{
			if (QR_get_num_tuples(res) > 0)
				conn->server_encoding = strdup(QR_get_value_backend_row(res, 0, 0));
		}
		QR_Destructor(res);
	}
	if (!conn->server_encoding)
		return ret;
	snprintf(query, sizeof(query), "SET CLIENT_ENCODING TO '%s'", conn->server_encoding);
	bError = (!QR_command_maybe_successful(res = CC_send_query(conn, query, NULL, IGNORE_ABORT_ON_CONN | ROLLBACK_ON_ERROR, NULL));
	QR_Destrctor(res);
	if (!bError && continueExec)
	{
		if (conn->schema_support)
			snprintf(query, sizeof(query), "select OID from pg_catalog.pg_class,"
			" pg_catalog.pg_namespace"
			" where relname = '%s' and pg_namespace.oid = relnamespace and"
			" pg_namespace.nspname = '%s'", serverTableName, serverSchemaName);
		else
			snprintf(query, sizeof(query), "select OID from pg_class where relname = '%s'", serverTableName);
		if (res = CC_send_query(conn, query, NULL, ROLLBACK_ON_ERROR | GNORE_ABORT_ON_CONN), QR_command_maybe_successful(res))
		{
			if (QR_get_num_tuples(res) > 0)
				strcpy(saveoid, QR_get_value_backend_row(res, 0, 0));
			else
				continueExec = FALSE;
		}
		else
			bError = TRUE;
		QR_Destructor(res);
	}
	continueExec = (continueExec && !bError);
	/* restore the client encoding */
	snprintf(query, sizeof(query), "SET CLIENT_ENCODING TO '%s'", conn->original_client_encoding);
	bError = (QR_command_maybe_successful(res = CC_send_query(conn, query, NULL, IGNORE_ABORT_ON_CONN, NULL));
	QR_Destrcutor(res);
	if (bError || !continueExec)
		return ret;
	snprintf(query, sizeof(query), "select relname from pg_class where OID = %s", saveoid);
	if (res = CC_send_query(conn, query, NULL, IGNORE_ABORT_ON_CONN, NULL), QR_command_maybe_successful(res))
	{
		if (QR_get_num_tuples(res) > 0)
		{
			ret = strdup(QR_get_value_backend_row(res, 0, 0));
			*nameAlloced = TRUE;
		}
	}
	QR_Destructor(res);
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
	if (!conn->original_client_encoding || !isMultibyte(serverColumnName))
		return ret;
	if (!conn->server_encoding)
	{
		if (res = CC_send_query(conn, "select getdatabaseencoding()", NULL, IGNORE_ABORT_ON_CONN, NULL), QR_command_maybe_successful(res))
		{
			if (QR_get_num_tuples(res) > 0)
				conn->server_encoding = strdup(QR_get_value_backend_row(res, 0, 0));
		}
		QR_Destructor(res);
	}
	if (!conn->server_encoding)
		return ret;
	snprintf(query, sizeof(query), "SET CLIENT_ENCODING TO '%s'", conn->server_encoding);
	bError = (!QR_command_maybe_successful(res = CC_send_query(conn, query, NULL, IGNORE_ABORT_ON_CONN, NULL));
	QR_Destructor(res);
	if (!bError && continueExec)
	{
		if (conn->schema_support)
			snprintf(query, sizeof(query), "select attrelid, attnum from pg_catalog.pg_class,"
			" pg_catalog.pg_attribute, pg_catalog.pg_namespace "
				"where relname = '%s' and attrelid = pg_class.oid "
				"and (not attisdropped) "
				"and attname = '%s' and pg_namespace.oid = relnamespace and"
				" pg_namespace.nspname = '%s'", serverTableName, serverColumnName, serverSchemaName);
		else
			snprintf(query, sizeof(query), "select attrelid, attnum from pg_class, pg_attribute "
				"where relname = '%s' and attrelid = pg_class.oid "
				"and attname = '%s'", serverTableName, serverColumnName);
		if (res = CC_send_query(conn, query, NULL, ROLLBACK_ON_ERROR | IGNORE_ABORT_ON_CONN, NULL), QR_command_maybe_successful(res))
		{
			if (QR_get_num_tuples(res) > 0)
			{
				strcpy(saveattrelid, QR_get_value_backend_row(res, 0, 0));
				strcpy(saveattnum, QR_get_value_backend_row(res, 0, 1));
			}
			else
				continueExec = FALSE;
		}
		else
			bError = TRUE;
		QR_Destructor(res);
	}
	continueExec = (continueExec && !bError);
	/* restore the cleint encoding */
	snprintf(query, sizeof(query), "SET CLIENT_ENCODING TO '%s'", conn->original_client_encoding);
	bError = (!QR_command_maybe_successful(res = CC_send_query(conn, query, NULL, IGNORE_ABORT_ON_CONN, NULL));
	QR_Destructor(res);
	if (bError || !continueExec)
		return ret;
	snprintf(query, sizeof(query), "select attname from pg_attribute where attrelid = %s and attnum = %s", saveattrelid, saveattnum);
	if (res = CC_send_query(conn, query, NULL, IGNORE_ABORT_ON_CONN, NULL), QR_command_maybe_successful(res))
	{
		if (QR_get_num_tuples(res) > 0)
		{
			ret = strdup(QR_get_value_backend_row(res, 0, 0));
			*nameAlloced = TRUE;
		}
	}
	QR_Destructor(res);
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
	UWORD	flag = IGNORE_ABORT_ON_CONN | ROLLBACK_ON_ERROR;

	*nameAlloced = FALSE;
	if (!conn->original_client_encoding || !isMultibyte(serverColumnName))
		return ret;
	if (!conn->server_encoding)
	{
		if (res = CC_send_query(conn, "select getdatabaseencoding()", NULL, flag, NULL), QR_command_maybe_successful(res))
		{
			if (QR_get_num_cached_tuples(res) > 0)
				conn->server_encoding = strdup(QR_get_value_backend_row(res, 0, 0));
		}
	}
	QR_Destructor(res);
	if (!conn->server_encoding)
		return ret;
	snprintf(query, sizeof(query), "SET CLIENT_ENCODING TO '%s'", conn->server_encoding);
	bError = (!QR_command_maybe_successful((res = CC_send_query(conn, query, NULL, flag, NULL))));
	QR_Destructor(res);
	if (!bError && continueExec)
	{
		snprintf(query, sizeof(query), "select attnum from pg_attribute "
			"where attrelid = %u and attname = '%s'",
			relid, serverColumnName);
		if (res = CC_send_query(conn, query, NULL, flag, NULL), QR_command_maybe_successful(res))
		{
			if (QR_get_num_cached_tuples(res) > 0)
			{
				strcpy(saveattnum, QR_get_value_backend_row(res, 0, 0));
			}
			else
				continueExec = FALSE;
		}
		else
			bError = TRUE;
		QR_Destructor(res);
	}
	continueExec = (continueExec && !bError);
	/* restore the cleint encoding */
	snprintf(query, sizeof(query), "SET CLIENT_ENCODING TO '%s'", conn->original_client_encoding);
	bError = (!QR_command_maybe_successful((res = CC_send_query(conn, query, NULL, flag, NULL))));
	QR_Destructor(res);
	if (bError || !continueExec)
		return ret;
	snprintf(query, sizeof(query), "select attname from pg_attribute where attrelid = %u and attnum = %s", relid, saveattnum);
	if (res = CC_send_query(conn, query, NULL, flag, NULL), QR_command_maybe_successful(res))
	{
		if (QR_get_num_cached_tuples(res) > 0)
		{
			ret = strdup(QR_get_value_backend_row(res, 0, 0));
			*nameAlloced = TRUE;
		}
	}
	QR_Destructor(res);
	return ret;
}

RETCODE		SQL_API
PGAPI_ForeignKeys(
				  HSTMT hstmt,
				  UCHAR FAR * szPkTableQualifier, /* OA */
				  SWORD cbPkTableQualifier,
				  UCHAR FAR * szPkTableOwner, /* OA */
				  SWORD cbPkTableOwner,
				  UCHAR FAR * szPkTableName, /* OA(R) */
				  SWORD cbPkTableName,
				  UCHAR FAR * szFkTableQualifier, /* OA */
				  SWORD cbFkTableQualifier,
				  UCHAR FAR * szFkTableOwner, /* OA */
				  SWORD cbFkTableOwner,
				  UCHAR FAR * szFkTableName, /* OA(R) */
				  SWORD cbFkTableName)
{
	CSTR func = "PGAPI_ForeignKeys";
	StatementClass *stmt = (StatementClass *) hstmt;
	QResultClass	*res;
	TupleField	*tuple;
	HSTMT		htbl_stmt = NULL, hpkey_stmt = NULL;
	StatementClass *tbl_stmt;
	RETCODE		ret = SQL_ERROR, result, keyresult;
	char		tables_query[INFO_INQUIRY_LEN];
	char		trig_deferrable[2];
	char		trig_initdeferred[2];
	char		trig_args[1024];
	char		upd_rule[TABLE_NAME_STORAGE_LEN],
				del_rule[TABLE_NAME_STORAGE_LEN];
	char		*pk_table_needed = NULL;
	char		fk_table_fetched[TABLE_NAME_STORAGE_LEN + 1];
	char		*fk_table_needed = NULL;
	char		pk_table_fetched[TABLE_NAME_STORAGE_LEN + 1];
	char		schema_needed[SCHEMA_NAME_STORAGE_LEN + 1];
	char		schema_fetched[SCHEMA_NAME_STORAGE_LEN + 1];
	char	   *pkey_ptr,
			   *pkey_text = NULL,
			   *fkey_ptr,
			   *fkey_text = NULL;

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

#if (ODBCVER >= 0x0300)
	SWORD		defer_type;
#endif
	char		pkey[MAX_INFO_STRING];
	Int2		result_cols;
	UInt4		relid1, relid2;

	mylog("%s: entering...stmt=%x\n", func, stmt);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_ForeignKeys result.", func);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/* the binding structure for a statement is not set up until */

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
#if (ODBCVER >= 0x0300)
	result_cols = 15;
#else
	result_cols = 14;
#endif /* ODBCVER */
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info_v(res, 0, "PKTABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 1, "PKTABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 2, "PKTABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 3, "PKCOLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 4, "FKTABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 5, "FKTABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 6, "FKTABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 7, "FKCOLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 8, "KEY_SEQ", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 9, "UPDATE_RULE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 10, "DELETE_RULE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 11, "FK_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 12, "PK_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 13, "TRIGGER_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
#if (ODBCVER >= 0x0300)
	QR_set_field_info_v(res, 14, "DEFERRABILITY", PG_TYPE_INT2, 2);
#endif   /* ODBCVER >= 0x0300 */

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);


	result = PGAPI_AllocStmt(stmt->hdbc, &htbl_stmt);
	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_ForeignKeys result.", func);
		return SQL_ERROR;
	}

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */

	tbl_stmt = (StatementClass *) htbl_stmt;
	schema_needed[0] = '\0';
	schema_fetched[0] = '\0';

	pk_table_needed = make_string(szPkTableName, cbPkTableName, NULL, 0);
	fk_table_needed = make_string(szFkTableName, cbFkTableName, NULL, 0);

	conn = SC_get_conn(stmt);
#ifdef	UNICODE_SUPPORT
	if (conn->unicode)
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */
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
			snprintf(tables_query, sizeof(tables_query), "SELECT	pt.tgargs, "
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
			snprintf(tables_query, sizeof(tables_query), "SELECT	pt.tgargs, "
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
			SC_full_error_copy(stmt, tbl_stmt, FALSE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 1, SQL_C_BINARY,
							   trig_args, sizeof(trig_args), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 2, SQL_C_SHORT,
							   &trig_nargs, 0, NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
						 trig_deferrable, sizeof(trig_deferrable), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 4, internal_asis_type,
					 trig_initdeferred, sizeof(trig_initdeferred), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 5, internal_asis_type,
							   upd_rule, sizeof(upd_rule), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 6, internal_asis_type,
							   del_rule, sizeof(del_rule), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 7, SQL_C_ULONG,
							   &relid1, sizeof(relid1), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 8, SQL_C_ULONG,
							   &relid2, sizeof(relid2), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 9, internal_asis_type,
					pk_table_fetched, TABLE_NAME_STORAGE_LEN, NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		if (conn->schema_support)
		{
			result = PGAPI_BindCol(htbl_stmt, 10, internal_asis_type,
					schema_fetched, SCHEMA_NAME_STORAGE_LEN, NULL);
			if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
			{
				SC_error_copy(stmt, tbl_stmt, TRUE);
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
			SC_full_error_copy(stmt, tbl_stmt, FALSE);
			goto cleanup;
		}

		keyresult = PGAPI_AllocStmt(stmt->hdbc, &hpkey_stmt);
		if ((keyresult != SQL_SUCCESS) && (keyresult != SQL_SUCCESS_WITH_INFO))
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_ForeignKeys (pkeys) result.", func);
			goto cleanup;
		}

		keyresult = PGAPI_BindCol(hpkey_stmt, 4, internal_asis_type,
								  pkey, sizeof(pkey), NULL);
		if (keyresult != SQL_SUCCESS)
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't bindcol for primary keys for PGAPI_ForeignKeys result.", func);
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
				SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't get primary keys for PGAPI_ForeignKeys result.", func);
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

#if (ODBCVER >= 0x0300)
			/* Set deferrability type */
			if (!strcmp(trig_initdeferred, "y"))
				defer_type = SQL_INITIALLY_DEFERRED;
			else if (!strcmp(trig_deferrable, "y"))
				defer_type = SQL_INITIALLY_IMMEDIATE;
			else
				defer_type = SQL_NOT_DEFERRABLE;
#endif   /* ODBCVER >= 0x0300 */

			/* Get to first primary key */
			pkey_ptr = trig_args;
			for (i = 0; i < 5; i++)
				pkey_ptr += strlen(pkey_ptr) + 1;

			for (k = 0; k < num_keys; k++)
			{
				tuple = QR_AddNew(res);

				pkey_text = getClientColumnName(conn, relid2, pkey_ptr, &pkey_alloced);
				fkey_text = getClientColumnName(conn, relid1, fkey_ptr, &fkey_alloced);

				mylog("%s: pk_table = '%s', pkey_ptr = '%s'\n", func, pk_table_fetched, pkey_text);
				set_tuplefield_null(&tuple[0]);
				set_tuplefield_string(&tuple[1], GET_SCHEMA_NAME(schema_fetched));
				set_tuplefield_string(&tuple[2], pk_table_fetched);
				set_tuplefield_string(&tuple[3], pkey_text);

				mylog("%s: fk_table_needed = '%s', fkey_ptr = '%s'\n", func, fk_table_needed, fkey_text);
				set_tuplefield_null(&tuple[4]);
				set_tuplefield_string(&tuple[5], GET_SCHEMA_NAME(schema_needed));
				set_tuplefield_string(&tuple[6], fk_table_needed);
				set_tuplefield_string(&tuple[7], fkey_text);

				mylog("%s: upd_rule_type = '%i', del_rule_type = '%i'\n, trig_name = '%s'", func, upd_rule_type, del_rule_type, trig_args);
				set_tuplefield_int2(&tuple[8], (Int2) (k + 1));
				set_tuplefield_int2(&tuple[9], (Int2) upd_rule_type);
				set_tuplefield_int2(&tuple[10], (Int2) del_rule_type);
				set_tuplefield_null(&tuple[11]);
				set_tuplefield_null(&tuple[12]);
				set_tuplefield_string(&tuple[13], trig_args);
#if (ODBCVER >= 0x0300)
				set_tuplefield_int2(&tuple[14], defer_type);
#endif   /* ODBCVER >= 0x0300 */

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
			snprintf(tables_query, sizeof(tables_query), "SELECT	pt.tgargs, "
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
			snprintf(tables_query, sizeof(tables_query), "SELECT	pt.tgargs, "
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
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 1, SQL_C_BINARY,
							   trig_args, sizeof(trig_args), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 2, SQL_C_SHORT,
							   &trig_nargs, 0, NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
						 trig_deferrable, sizeof(trig_deferrable), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 4, internal_asis_type,
					 trig_initdeferred, sizeof(trig_initdeferred), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 5, internal_asis_type,
							   upd_rule, sizeof(upd_rule), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 6, internal_asis_type,
							   del_rule, sizeof(del_rule), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 7, SQL_C_ULONG,
						&relid1, sizeof(relid1), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 8, SQL_C_ULONG,
						&relid2, sizeof(relid2), NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 9, internal_asis_type,
					fk_table_fetched, TABLE_NAME_STORAGE_LEN, NULL);
		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		if (conn->schema_support)
		{
			result = PGAPI_BindCol(htbl_stmt, 10, internal_asis_type,
					schema_fetched, SCHEMA_NAME_STORAGE_LEN, NULL);
			if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO))
			{
				SC_error_copy(stmt, tbl_stmt, TRUE);
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
			SC_full_error_copy(stmt, tbl_stmt, FALSE);
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

#if (ODBCVER >= 0x0300)
			/* Set deferrability type */
			if (!strcmp(trig_initdeferred, "y"))
				defer_type = SQL_INITIALLY_DEFERRED;
			else if (!strcmp(trig_deferrable, "y"))
				defer_type = SQL_INITIALLY_IMMEDIATE;
			else
				defer_type = SQL_NOT_DEFERRABLE;
#endif   /* ODBCVER >= 0x0300 */

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

				tuple = QR_AddNew(res);

				mylog("pk_table_needed = '%s', pkey_ptr = '%s'\n", pk_table_needed, pkey_text);
				set_tuplefield_null(&tuple[0]);
				set_tuplefield_string(&tuple[1], GET_SCHEMA_NAME(schema_needed));
				set_tuplefield_string(&tuple[2], pk_table_needed);
				set_tuplefield_string(&tuple[3], pkey_text);

				mylog("fk_table = '%s', fkey_ptr = '%s'\n", fk_table_fetched, fkey_text);
				set_tuplefield_null(&tuple[4]);
				set_tuplefield_string(&tuple[5], GET_SCHEMA_NAME(schema_fetched));
				set_tuplefield_string(&tuple[6], fk_table_fetched);
				set_tuplefield_string(&tuple[7], fkey_text);

				set_tuplefield_int2(&tuple[8], (Int2) (k + 1));

				mylog("upd_rule = %d, del_rule= %d", upd_rule_type, del_rule_type);
				set_nullfield_int2(&tuple[9], (Int2) upd_rule_type);
				set_nullfield_int2(&tuple[10], (Int2) del_rule_type);

				set_tuplefield_null(&tuple[11]);
				set_tuplefield_null(&tuple[12]);

				set_tuplefield_string(&tuple[13], trig_args);

#if (ODBCVER >= 0x0300)
				mylog(" defer_type = %d\n", defer_type);
				set_tuplefield_int2(&tuple[14], defer_type);
#endif   /* ODBCVER >= 0x0300 */

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
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "No tables specified to PGAPI_ForeignKeys.", func);
		goto cleanup;
	}
	ret = SQL_SUCCESS;

cleanup:
#undef	return
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	if (pkey_alloced)
		free(pkey_text);
	if (fkey_alloced)
		free(fkey_text);
	if (pk_table_needed)
		free(pk_table_needed);
	if (fk_table_needed)
		free(fk_table_needed);

	if (htbl_stmt)
		PGAPI_FreeStmt(htbl_stmt, SQL_DROP);
	if (hpkey_stmt)
		PGAPI_FreeStmt(hpkey_stmt, SQL_DROP);

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	if (stmt->internal)
		ret = DiscardStatementSvp(stmt, ret, FALSE);
	mylog("%s(): EXIT, stmt=%x, ret=%d\n", func, stmt, ret);
	return ret;
}


#define	PRORET_COUNT
RETCODE		SQL_API
PGAPI_ProcedureColumns(
					   HSTMT hstmt,
					   UCHAR FAR * szProcQualifier, /* OA */
					   SWORD cbProcQualifier,
					   UCHAR FAR * szProcOwner, /* PV */
					   SWORD cbProcOwner,
					   UCHAR FAR * szProcName, /* PV */
					   SWORD cbProcName,
					   UCHAR FAR * szColumnName, /* PV */
					   SWORD cbColumnName,
					   UWORD flag)
{
	CSTR func = "PGAPI_ProcedureColumns";
	StatementClass	*stmt = (StatementClass *) hstmt;
	ConnectionClass *conn = SC_get_conn(stmt);
	char		proc_query[INFO_INQUIRY_LEN];
	Int2		result_cols;
	TupleField	*tuple;
	char		*schema_name, *procname;
	char		*escProcName = NULL;
	char		*params, *proargnames, *proargmodes, *delim;
	char		*atttypid, *attname, *column_name;
	QResultClass *res, *tres;
	Int4		tcount, paramcount, i, j, pgtype;
	RETCODE		result;
	BOOL		search_pattern, retout = TRUE;
	const char	*like_or_eq;
	int		ret_col = -1, ext_pos = -1, poid_pos = -1, attid_pos = -1, attname_pos = -1;
	UInt4		poid = 0, newpoid;

	mylog("%s: entering...\n", func);

	if (PG_VERSION_LT(conn, 6.5))
	{
		SC_set_error(stmt, STMT_NOT_IMPLEMENTED_ERROR, "Version is too old", func);
		return SQL_ERROR;
	}
	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;
	search_pattern = (0 == (flag & PODBC_NOT_SEARCH_PATTERN));
	if (search_pattern) 
	{
		like_or_eq = likeop;
		escProcName = adjustLikePattern(szProcName, cbProcName, SEARCH_PATTERN_ESCAPE, NULL, conn->ccsc);
	}
	else
	{
		like_or_eq = eqop;
		escProcName = simpleCatalogEscape(szProcName, cbProcName, ESCAPE_IN_LITERAL, NULL, conn->ccsc);
	}
	if (conn->schema_support)
	{
		strcpy(proc_query, "select proname, proretset, prorettype, "
				"pronargs, proargtypes, nspname, p.oid");
		ret_col = ext_pos = 7;
		poid_pos = 6;
#ifdef	PRORET_COUNT
		strcat(proc_query, ", atttypid, attname");
		attid_pos = ext_pos;
		attname_pos = ext_pos + 1;
		ret_col += 2;
		ext_pos = ret_col;
#endif /* PRORET_COUNT */
		if (PG_VERSION_GE(conn, 8.0))
		{
			strcat(proc_query, ", proargnames");
			ret_col++;
		}
		if (PG_VERSION_GE(conn, 8.1))
		{
			strcat(proc_query, ", proargmodes, proallargtypes");
			ret_col++;
		}
#ifdef	PRORET_COUNT
		strcat(proc_query, " from ((pg_catalog.pg_namespace n inner join"
				   " pg_catalog.pg_proc p on p.pronamespace = n.oid)"
			" inner join pg_type t on t.oid = p.prorettype)"
			" left outer join pg_attribute a on a.attrelid = t.typrelid "
			" and attnum > 0 and not attisdropped where");
#else
		strcat(proc_query, " from pg_catalog.pg_namespace n,"
				   " pg_catalog.pg_proc p where");
				   " p.pronamespace = n.oid  and"
				   " (not proretset) and");
#endif /* PRORETSET */
		strcat(proc_query, " has_function_privilege(p.oid, 'EXECUTE')");
		my_strcat1(proc_query, " and nspname %s '%.*s'", like_or_eq, szProcOwner, cbProcOwner);
		snprintf(proc_query, sizeof(proc_query), "%s and proname %s '%s'", proc_query, like_or_eq, escProcName);
		strcat(proc_query, " order by nspname, proname, p.oid, attnum");
	}
	else
	{
		strcpy(proc_query, "select proname, proretset, prorettype, "
				"pronargs, proargtypes from pg_proc where "
				"(not proretset)");
		ret_col = 5;
		/* my_strcat1(proc_query, " and proname %s '%.*s'", like_or_eq, szProcName, cbProcName); */
		snprintf(proc_query, sizeof(proc_query), " and proname %s '%s'", proc_query, like_or_eq, escProcName);
		strcat(proc_query, " order by proname, proretset");
	}
	if (tres = CC_send_query(conn, proc_query, NULL, IGNORE_ABORT_ON_CONN, stmt), !QR_command_maybe_successful(tres))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_ProcedureColumns query error", func);
		QR_Destructor(tres);
		return SQL_ERROR;
	}

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_ProcedureColumns result.", func);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/*
	 * the binding structure for a statement is not set up until
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
#if (ODBCVER >= 0x0300)
	result_cols = 19;
#else
	result_cols = 13;
#endif /* ODBCVER */
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info_v(res, 0, "PROCEDURE_CAT", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 1, "PROCEDUR_SCHEM", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 2, "PROCEDURE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 3, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 4, "COLUMN_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 5, "DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 6, "TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 7, "COLUMN_SIZE", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 8, "BUFFER_LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 9, "DECIMAL_DIGITS", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 10, "NUM_PREC_RADIX", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 11, "NULLABLE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 12, "REMARKS", PG_TYPE_VARCHAR, MAX_INFO_STRING);
#if (ODBCVER >= 0x0300)
	QR_set_field_info_v(res, 13, "COLUMN_DEF", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 14, "SQL_DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 15, "SQL_DATATIME_SUB", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 16, "CHAR_OCTET_LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 17, "ORIDINAL_POSITION", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 18, "IS_NULLABLE", PG_TYPE_VARCHAR, MAX_INFO_STRING);
#endif   /* ODBCVER >= 0x0300 */

	column_name = make_string(szColumnName, cbColumnName, NULL, 0);
	if (column_name) /* column_name is unavailable now */
	{
		tcount = 0;
		free(column_name);
	}
	else
		tcount = QR_get_num_total_tuples(tres);
	for (i = 0, poid = 0; i < tcount; i++)
	{
		if (conn->schema_support)
			schema_name = GET_SCHEMA_NAME(QR_get_value_backend_row(tres, i, 5));
		else
			schema_name = NULL;
		procname = QR_get_value_backend_row(tres, i, 0);
		pgtype = atoi(QR_get_value_backend_row(tres, i, 2));
		newpoid = 0;
		if (poid_pos >= 0)
			newpoid = atoi(QR_get_value_backend_row(tres, i, poid_pos));
mylog("newpoid=%d\n", newpoid);
		atttypid = NULL;
		if (attid_pos >= 0)
		{
			atttypid = QR_get_value_backend_row(tres, i, attid_pos);
mylog("atttypid=%s\n", atttypid ? atttypid : "(null)");
		}
		if (poid == 0 || newpoid != poid)
		{
			poid = newpoid;
			proargmodes = NULL;
			proargnames = NULL;
			if (ext_pos >=0)
			{
				if (PG_VERSION_GE(conn, 8.0))
					proargnames = QR_get_value_backend_row(tres, i, ext_pos);
				if (PG_VERSION_GE(conn, 8.1))
					proargmodes = QR_get_value_backend_row(tres, i, ext_pos + 1);
			}
			/* RETURN_VALUE info */ 
			if (pgtype != 0 && pgtype != PG_TYPE_VOID && !atttypid && !proargmodes)
			{
				tuple = QR_AddNew(res);
				set_tuplefield_null(&tuple[0]);
				set_nullfield_string(&tuple[1], schema_name);
				set_tuplefield_string(&tuple[2], procname);
				set_tuplefield_string(&tuple[3], "");
				set_tuplefield_int2(&tuple[4], SQL_RETURN_VALUE);
				set_tuplefield_int2(&tuple[5], pgtype_to_concise_type(stmt, pgtype, PG_STATIC));
				set_tuplefield_string(&tuple[6], pgtype_to_name(stmt, pgtype));
				set_nullfield_int4(&tuple[7], pgtype_column_size(stmt, pgtype, PG_STATIC, PG_STATIC));
				set_tuplefield_int4(&tuple[8], pgtype_buffer_length(stmt, pgtype, PG_STATIC, PG_STATIC));
				set_nullfield_int2(&tuple[9], pgtype_decimal_digits(stmt, pgtype, PG_STATIC));
				set_nullfield_int2(&tuple[10], pgtype_radix(stmt, pgtype));
				set_tuplefield_int2(&tuple[11], SQL_NULLABLE_UNKNOWN);
				set_tuplefield_null(&tuple[12]);
#if (ODBCVER >= 0x0300)
				set_tuplefield_null(&tuple[13]);
				set_nullfield_int2(&tuple[14], pgtype_to_sqldesctype(stmt, pgtype, PG_STATIC));
				set_nullfield_int2(&tuple[15], pgtype_to_datetime_sub(stmt, pgtype));
				set_nullfield_int4(&tuple[16], pgtype_transfer_octet_length(stmt, pgtype, PG_STATIC, PG_STATIC));
				set_tuplefield_int4(&tuple[17], 0);
				set_tuplefield_string(&tuple[18], "");
#endif   /* ODBCVER >= 0x0300 */
			}
			if (proargmodes)
			{
				const char *p;

				paramcount = 0;
				for (p = proargmodes; *p; p++)
				{
					if (',' == (*p))
						paramcount++;
				}
				paramcount++;
				params = QR_get_value_backend_row(tres, i, 8);
				if ('{' == *proargmodes)
					proargmodes++;
				if ('{' == *params)
					params++;
			}
			else
			{
				paramcount = atoi(QR_get_value_backend_row(tres, i, 3));
				params = QR_get_value_backend_row(tres, i, 4);
			}
			if (proargnames)
			{
				if ('{' == *proargnames)
					proargnames++;
			}
			/* PARAMETERS info */
			for (j = 0; j < paramcount; j++)
			{
				/* PG type of parameters */
				pgtype = 0;
				if (params)
				{
					while (isspace(*params) || ',' == *params)
						params++;
					if ('\0' == *params || '}' == *params)
						params = NULL;
					else
					{
						sscanf(params, "%d", &pgtype);
						while (isdigit(*params))
							params++;
					}
				}
				/* input/output type of parameters */
				if (proargmodes)
				{
					while (isspace(*proargmodes) || ',' == *proargmodes)
						proargmodes++;
					if ('\0' == *proargmodes || '}' == *proargmodes)
						proargmodes = NULL;
				}
				/* name of parameters */
				if (proargnames)
				{
					while (isspace(*proargnames) || ',' == *proargnames)
						proargnames++;
					if ('\0' == *proargnames || '}' == *proargnames)
						proargnames = NULL;
					else if ('"' == *proargnames)
					{
						proargnames++;
						for (delim = proargnames; *delim && *delim != '"'; delim++)
							;
					}
					else
					{
						for (delim = proargnames; *delim && !isspace(*delim) && '}' != *delim; delim++)
							;
					}
					if (proargnames && '\0' == *delim) /* discard the incomplete name */
						proargnames = NULL;
				}

				tuple = QR_AddNew(res);
				set_tuplefield_null(&tuple[0]);
				set_nullfield_string(&tuple[1], schema_name);
				set_tuplefield_string(&tuple[2], procname);
				if (proargnames)
				{
					*delim = '\0';
					set_tuplefield_string(&tuple[3], proargnames);
					proargnames = delim + 1;
				}
				else
					set_tuplefield_string(&tuple[3], "");
				if (proargmodes)
				{
					int	ptype;

					switch (*proargmodes)
					{
						case 'o':
							ptype = SQL_PARAM_OUTPUT;
						case 'b':
							ptype = SQL_PARAM_INPUT_OUTPUT;
							break;
						default:
							ptype = SQL_PARAM_INPUT;
							break;
					}
					set_tuplefield_int2(&tuple[4], ptype);
					proargmodes++;
				}
				else
					set_tuplefield_int2(&tuple[4], SQL_PARAM_INPUT);
				set_tuplefield_int2(&tuple[5], pgtype_to_concise_type(stmt, pgtype, PG_STATIC));
				set_tuplefield_string(&tuple[6], pgtype_to_name(stmt, pgtype));
				set_nullfield_int4(&tuple[7], pgtype_column_size(stmt, pgtype, PG_STATIC, PG_STATIC));
				set_tuplefield_int4(&tuple[8], pgtype_buffer_length(stmt, pgtype, PG_STATIC, PG_STATIC));
				set_nullfield_int2(&tuple[9], pgtype_decimal_digits(stmt, pgtype, PG_STATIC));
				set_nullfield_int2(&tuple[10], pgtype_radix(stmt, pgtype));
				set_tuplefield_int2(&tuple[11], SQL_NULLABLE_UNKNOWN);
				set_tuplefield_null(&tuple[12]);
#if (ODBCVER >= 0x0300)
				set_tuplefield_null(&tuple[13]);
				set_nullfield_int2(&tuple[14], pgtype_to_sqldesctype(stmt, pgtype, PG_STATIC));
				set_nullfield_int2(&tuple[15], pgtype_to_datetime_sub(stmt, pgtype));
				set_nullfield_int4(&tuple[16], pgtype_transfer_octet_length(stmt, pgtype, PG_STATIC, PG_STATIC));
				set_tuplefield_int4(&tuple[17], j + 1);
				set_tuplefield_string(&tuple[18], "");
#endif   /* ODBCVER >= 0x0300 */
			}
		}
		/* RESULT Columns info */
		if (atttypid != NULL)
		{
			int	typid = atoi(atttypid);

			attname = QR_get_value_backend_row(tres, i, attname_pos);
			tuple = QR_AddNew(res);
			set_tuplefield_null(&tuple[0]);
			set_nullfield_string(&tuple[1], schema_name);
			set_tuplefield_string(&tuple[2], procname);
			set_tuplefield_string(&tuple[3], attname);
			set_tuplefield_int2(&tuple[4], SQL_RESULT_COL);
			set_tuplefield_int2(&tuple[5], pgtype_to_concise_type(stmt, typid, PG_STATIC));
			set_tuplefield_string(&tuple[6], pgtype_to_name(stmt, typid));
			set_nullfield_int4(&tuple[7], pgtype_column_size(stmt, typid, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&tuple[8], pgtype_buffer_length(stmt, typid, PG_STATIC, PG_STATIC));
			set_nullfield_int2(&tuple[9], pgtype_decimal_digits(stmt, typid, PG_STATIC));
			set_nullfield_int2(&tuple[10], pgtype_radix(stmt, typid));
			set_tuplefield_int2(&tuple[11], SQL_NULLABLE_UNKNOWN);
			set_tuplefield_null(&tuple[12]);
#if (ODBCVER >= 0x0300)
			set_tuplefield_null(&tuple[13]);
			set_nullfield_int2(&tuple[14], pgtype_to_sqldesctype(stmt, typid, PG_STATIC));
			set_nullfield_int2(&tuple[15], pgtype_to_datetime_sub(stmt, typid));
			set_nullfield_int4(&tuple[16], pgtype_transfer_octet_length(stmt, typid, PG_STATIC, PG_STATIC));
			set_tuplefield_int4(&tuple[17], 0);
			set_tuplefield_string(&tuple[18], "");
#endif   /* ODBCVER >= 0x0300 */
		}
	}
	QR_Destructor(tres);
	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	if (escProcName)
		free(escProcName);
	stmt->status = STMT_FINISHED;
	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_Procedures(
				 HSTMT hstmt,
				 UCHAR FAR * szProcQualifier, /* OA */
				 SWORD cbProcQualifier,
				 UCHAR FAR * szProcOwner, /* PV */
				 SWORD cbProcOwner,
				 UCHAR FAR * szProcName, /* PV */
				 SWORD cbProcName)
{
	CSTR func = "PGAPI_Procedures";
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn = SC_get_conn(stmt);
	char		proc_query[INFO_INQUIRY_LEN];
	QResultClass *res;
	RETCODE		result;
	CSTR	likeeq = "like";

	mylog("%s: entering... scnm=%x len=%d\n", func, szProcOwner, cbProcOwner);

	if (PG_VERSION_LT(conn, 6.5))
	{
		SC_set_error(stmt, STMT_NOT_IMPLEMENTED_ERROR, "Version is too old", func);
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

	if (res = CC_send_query(conn, proc_query, NULL, IGNORE_ABORT_ON_CONN, stmt), !QR_command_maybe_successful(res))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_Procedures query error", func);
		QR_Destructor(res);
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
	SC_set_rowset_start(stmt, -1, FALSE);
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
	int usercount = QR_get_num_cached_tuples(allures), i, addcnt = 0;

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
					  UCHAR FAR * szTableQualifier, /* OA */
					  SWORD cbTableQualifier,
					  UCHAR FAR * szTableOwner, /* PV */
					  SWORD cbTableOwner,
					  UCHAR FAR * szTableName, /* PV */
					  SWORD cbTableName,
					  UWORD flag)
{
	StatementClass *stmt = (StatementClass *) hstmt;
	CSTR func = "PGAPI_TablePrivileges";
	ConnectionClass *conn = SC_get_conn(stmt);
	Int2		result_cols;
	char		proc_query[INFO_INQUIRY_LEN];
	QResultClass	*res, *wres = NULL, *allures = NULL;
	TupleField	*tuple;
	int		tablecount, usercount, i, j, k;
	BOOL		grpauth, sys, su;
	char		(*useracl)[ACLMAX] = NULL, *acl, *user, *delim, *auth;
	char		*reln, *owner, *priv, *schnm = NULL;
	RETCODE		result, ret = SQL_SUCCESS;
	const char	*like_or_eq;
	const char	*szSchemaName;
	SWORD		cbSchemaName;
	char		*escSchemaName = NULL, *escTableName = NULL;
	BOOL		search_pattern;

	mylog("%s: entering... scnm=%x len-%d\n", func, NULL_IF_NULL(szTableOwner), cbTableOwner);
	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	result_cols = 7;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	/* set the field names */
	res = QR_Constructor();
	SC_set_Result(stmt, res);
	QR_set_num_fields(res, result_cols);
	QR_set_field_info_v(res, 0, "TABLE_CAT", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 1, "TABLE_SCHEM", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 2, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 3, "GRANTOR", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 4, "GRANTEE", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 5, "PRIVILEGE", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, 6, "IS_GRANTABLE", PG_TYPE_VARCHAR, MAX_INFO_STRING);

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;
	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

#define	return	DONT_CALL_RETURN_FROM_HERE???
	/* StartRollbackState(stmt); */
retry_public_schema:
	if (conn->schema_support)
		strncpy_null(proc_query, "select relname, usename, relacl, nspname"
		" from pg_catalog.pg_namespace, pg_catalog.pg_class ,"
		" pg_catalog.pg_user where", sizeof(proc_query));
	else
		strncpy_null(proc_query, "select relname, usename, relacl"
		" from pg_class , pg_user where", sizeof(proc_query));
	search_pattern = (0 == (flag & PODBC_NOT_SEARCH_PATTERN));
	if (search_pattern) 
	{
		like_or_eq = likeop;
		escSchemaName = adjustLikePattern(szSchemaName, cbSchemaName, SEARCH_PATTERN_ESCAPE, NULL, conn->ccsc);
		escTableName = adjustLikePattern(szTableName, cbTableName, SEARCH_PATTERN_ESCAPE, NULL, conn->ccsc);
	}
	else
	{
		like_or_eq = eqop;
		escSchemaName = simpleCatalogEscape(szSchemaName, cbSchemaName, ESCAPE_IN_LITERAL, NULL, conn->ccsc);
		escTableName = simpleCatalogEscape(szTableName, cbTableName, ESCAPE_IN_LITERAL, NULL, conn->ccsc);
	}
	if (conn->schema_support)
	{
		if (escSchemaName)
			schema_strcat1(proc_query, " nspname %s '%.*s' and", like_or_eq, escSchemaName, SQL_NTS, szTableName, cbTableName, conn);
	}
	if (escTableName)
		snprintf(proc_query, sizeof(proc_query), "%s relname %s '%s' and", proc_query, like_or_eq, escTableName);
#ifdef NOT_USED
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
			schema_strcat1(proc_query, " nspname %s '%.*s' and", like_or_eq, esc_table_name, escTbnamelen, szTableName, cbTableName, conn);
		}
		escTbnamelen = reallyEscapeCatalogEscapes(szTableName, cbTableName, esc_table_name, sizeof(esc_table_name), conn->ccsc);
		my_strcat1(proc_query, " relname %s '%.*s' and", like_or_eq, esc_table_name, escTbnamelen);
	}
#endif /* NOT_USED */
	if (conn->schema_support)
	{
		strcat(proc_query, " pg_namespace.oid = relnamespace and relkind in ('r', 'v') and");
		if ((!szTableName || !cbTableName) && (!szTableOwner || !cbTableOwner))
			strcat(proc_query, " nspname not in ('pg_catalog', 'information_schema') and");
	}
	strcat(proc_query, " pg_user.usesysid = relowner");
	if (wres = CC_send_query(conn, proc_query, NULL, IGNORE_ABORT_ON_CONN, stmt), !QR_command_maybe_successful(wres))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_TablePrivileges query error", func);
		ret = SQL_ERROR;
		goto cleanup;
	}
	tablecount = QR_get_num_cached_tuples(wres);
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
			QR_Destructor(wres);
			wres = NULL;
			szSchemaName = pubstr;
			cbSchemaName = SQL_NTS;
			goto retry_public_schema;
		}
	}

	strncpy_null(proc_query, "select usename, usesysid, usesuper from pg_user", sizeof(proc_query));
	if (allures = CC_send_query(conn, proc_query, NULL, IGNORE_ABORT_ON_CONN, stmt), !QR_command_maybe_successful(allures))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_TablePrivileges query error", func);
		ret = SQL_ERROR;
		goto cleanup;
	}
	usercount = QR_get_num_cached_tuples(allures);
	useracl = (char (*)[ACLMAX]) malloc(usercount * sizeof(char [ACLMAX]));
	for (i = 0; i < tablecount; i++)
	{ 
		memset(useracl, 0, usercount * sizeof(char[ACLMAX]));
		acl = (char *) QR_get_value_backend_row(wres, i, 2);
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
				if (gres = CC_send_query(conn, proc_query, NULL, IGNORE_ABORT_ON_CONN, stmt), !QR_command_maybe_successful(gres))
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
				}
				QR_Destructor(gres);
			}
			else
				useracl_upd(useracl, allures, user, auth);
			if (!delim)
				break;
			user = delim + 1;
		}
		reln = QR_get_value_backend_row(wres, i, 0);
		owner = QR_get_value_backend_row(wres, i, 1);
		if (conn->schema_support)
			schnm = QR_get_value_backend_row(wres, i, 3);
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
				tuple = QR_AddNew(res);
				set_tuplefield_string(&tuple[0], "");
				if (conn->schema_support)
					set_tuplefield_string(&tuple[1], GET_SCHEMA_NAME(schnm));
				else
					set_tuplefield_string(&tuple[1], "");
				set_tuplefield_string(&tuple[2], reln);
				if (su || sys)
					set_tuplefield_string(&tuple[3], "_SYSTEM");
				else
					set_tuplefield_string(&tuple[3], owner);
				mylog("user=%s\n", user);
				set_tuplefield_string(&tuple[4], user);
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
				set_tuplefield_string(&tuple[5], priv);
				/* The owner and the super user are grantable */
				if (sys || su)
					set_tuplefield_string(&tuple[6], "YES");
				else
					set_tuplefield_string(&tuple[6], "NO");
			}
		}
	}
cleanup:
#undef	return
	if (escSchemaName)
		free(escSchemaName);
	if (escTableName)
		free(escTableName);
	if (useracl)
		free(useracl);
	if (wres)
		QR_Destructor(wres);
	if (allures)
		QR_Destructor(allures);
	if (stmt->internal) 
		ret = DiscardStatementSvp(stmt, ret, FALSE);
	return ret;
}

#ifdef	NOT_USED
select n.nspname, c.relname, a.attname, a.atttypid, t.typname, a.attnotnull from ((pg_class c inner join pg_namespace n on c.oid = %u and c.relnamespace = n.oid) inner join pg_attribute a on a.attrelid = c.oid and a.attnum = %u) inner join pg_type t on t.oid = a.atttypid;
#endif /* NOT USED */

