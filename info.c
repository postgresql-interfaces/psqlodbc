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
 *					SQLProcedureColumns, SQLProcedures,
 *					SQLTablePrivileges, SQLColumnPrivileges(NI)
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *--------
 */

#include "psqlodbc.h"
#include "unicode_support.h"

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

static const SQLCHAR *pubstr = (SQLCHAR *) "public";
static const char	*likeop = "like";
static const char	*eqop = "=";

RETCODE		SQL_API
PGAPI_GetInfo(HDBC hdbc,
			  SQLUSMALLINT fInfoType,
			  PTR rgbInfoValue,
			  SQLSMALLINT cbInfoValueMax,
			  SQLSMALLINT * pcbInfoValue)
{
	CSTR func = "PGAPI_GetInfo";
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	ConnInfo   *ci;
	const char   *p = NULL;
	char		tmp[MAX_INFO_STRING];
	SQLULEN			len = 0,
				value = 0;
	RETCODE		result = SQL_ERROR;
	char		odbcver[16];

	mylog("%s: entering...fInfoType=%d\n", func, fInfoType);

	if (!conn)
	{
		CC_log_error(func, NULL_STRING, NULL);
		return SQL_INVALID_HANDLE;
	}

	ci = &(conn->connInfo);

	switch (fInfoType)
	{
		case SQL_ACCESSIBLE_PROCEDURES: /* ODBC 1.0 */
			p = "N";
			break;

		case SQL_ACCESSIBLE_TABLES:		/* ODBC 1.0 */
			p = CC_accessible_only(conn) ? "Y" : "N";
			break;

		case SQL_ACTIVE_CONNECTIONS:	/* ODBC 1.0 */
			len = 2;
			value = 0;
			break;

		case SQL_ACTIVE_STATEMENTS:		/* ODBC 1.0 */
			len = 2;
			value = 0;
			break;

		case SQL_ALTER_TABLE:	/* ODBC 2.0 */
			len = 4;
			value = SQL_AT_ADD_COLUMN
					| SQL_AT_DROP_COLUMN
					| SQL_AT_ADD_COLUMN_SINGLE
					| SQL_AT_ADD_CONSTRAINT
					| SQL_AT_ADD_TABLE_CONSTRAINT
					| SQL_AT_CONSTRAINT_INITIALLY_DEFERRED
					| SQL_AT_CONSTRAINT_INITIALLY_IMMEDIATE
					| SQL_AT_CONSTRAINT_DEFERRABLE
					| SQL_AT_DROP_TABLE_CONSTRAINT_RESTRICT
					| SQL_AT_DROP_TABLE_CONSTRAINT_CASCADE
					| SQL_AT_DROP_COLUMN_RESTRICT
					| SQL_AT_DROP_COLUMN_CASCADE;
			break;

		case SQL_BOOKMARK_PERSISTENCE:	/* ODBC 2.0 */
			/* very simple bookmark support */
			len = 4;
			value = SQL_BP_SCROLL | SQL_BP_DELETE | SQL_BP_UPDATE | SQL_BP_TRANSACTION;
			break;

		case SQL_COLUMN_ALIAS:	/* ODBC 2.0 */
			p = "Y";
			break;

		case SQL_CONCAT_NULL_BEHAVIOR:	/* ODBC 1.0 */
			len = 2;
			value = SQL_CB_NON_NULL;
			break;

		case SQL_CONVERT_INTEGER:
		case SQL_CONVERT_SMALLINT:
		case SQL_CONVERT_TINYINT:
		case SQL_CONVERT_BIT:
		case SQL_CONVERT_VARCHAR:		/* ODBC 1.0 */
			len = sizeof(SQLUINTEGER);
			value = SQL_CVT_BIT | SQL_CVT_INTEGER;
mylog("SQL_CONVERT_ mask=" FORMAT_ULEN "\n", value);
			break;
		case SQL_CONVERT_BIGINT:
		case SQL_CONVERT_DECIMAL:
		case SQL_CONVERT_DOUBLE:
		case SQL_CONVERT_FLOAT:
		case SQL_CONVERT_NUMERIC:
		case SQL_CONVERT_REAL:
		case SQL_CONVERT_DATE:
		case SQL_CONVERT_TIME:
		case SQL_CONVERT_TIMESTAMP:
		case SQL_CONVERT_BINARY:
		case SQL_CONVERT_LONGVARBINARY:
		case SQL_CONVERT_VARBINARY:		/* ODBC 1.0 */
		case SQL_CONVERT_CHAR:
		case SQL_CONVERT_LONGVARCHAR:
#ifdef UNICODE_SUPPORT
		case SQL_CONVERT_WCHAR:
		case SQL_CONVERT_WLONGVARCHAR:
		case SQL_CONVERT_WVARCHAR:
#endif /* UNICODE_SUPPORT */
			len = sizeof(SQLUINTEGER);
			value = 0;	/* CONVERT is unavailable */
			break;

		case SQL_CONVERT_FUNCTIONS:		/* ODBC 1.0 */
			len = sizeof(SQLUINTEGER);
			value = SQL_FN_CVT_CONVERT;
mylog("CONVERT_FUNCTIONS=" FORMAT_ULEN "\n", value);
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
			value = SQL_CB_PRESERVE;
			break;

		case SQL_CURSOR_ROLLBACK_BEHAVIOR:		/* ODBC 1.0 */
			len = 2;
			if (!ci->drivers.use_declarefetch)
				value = SQL_CB_PRESERVE;
			else
				value = SQL_CB_CLOSE;
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
			p = CurrCatString(conn);
			break;

		case SQL_DBMS_NAME:		/* ODBC 1.0 */
			if (CC_fake_mss(conn))
				p = "Microsoft SQL Server";
			else
				p = "PostgreSQL";
			break;

		case SQL_DBMS_VER:		/* ODBC 1.0 */

			/*
			 * The ODBC spec wants ##.##.#### ...whatever... so prepend
			 * the driver
			 */
			/* version number to the dbms version string */
			/*
			SPRINTF_FIXED(tmp, "%s %s", POSTGRESDRIVERVERSION, conn->pg_version);
                        tmp[sizeof(tmp) - 1] = '\0'; */
			if (CC_fake_mss(conn))
				p = "09.00.1399";
			else
			{
				STRCPY_FIXED(tmp, conn->pg_version);
				p = tmp;
			}
			break;

		case SQL_DEFAULT_TXN_ISOLATION: /* ODBC 1.0 */
			len = 4;
			if (0 == conn->default_isolation)
				conn->isolation = CC_get_isolation(conn);
			value = conn->default_isolation;
			break;

		case SQL_DRIVER_NAME:	/* ODBC 1.0 */
			p = DRIVER_FILE_NAME;
			break;

		case SQL_DRIVER_ODBC_VER:
			SPRINTF_FIXED(odbcver, "%02x.%02x", ODBCVER / 256, ODBCVER % 256);
			/* p = DRIVER_ODBC_VER; */
			p = odbcver;
			break;

		case SQL_DRIVER_VER:	/* ODBC 1.0 */
			p = POSTGRESDRIVERVERSION;
			break;

		case SQL_EXPRESSIONS_IN_ORDERBY:		/* ODBC 1.0 */
			p = "Y";
			break;

		case SQL_FETCH_DIRECTION:		/* ODBC 1.0 */
			len = 4;
			value = (SQL_FD_FETCH_NEXT |
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
			p = "\"";
			break;

		case SQL_KEYWORDS:		/* ODBC 2.0 */
			p = NULL_STRING;
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
			value = CC_get_max_idlen(conn);
			if (0 == value)
				value = NAMEDATALEN_V73 - 1;
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
			if (PG_VERSION_GT(conn, 7.4))
				value = CC_get_max_idlen(conn);
#ifdef	MAX_SCHEMA_LEN
			else
				value = MAX_SCHEMA_LEN;
#endif /* MAX_SCHEMA_LEN */
			if (0 == value)
				value = NAMEDATALEN_V73 - 1;
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
			/* No limit with tuptoaster in 7.1+ */
			value = 0;
			break;

		case SQL_MAX_STATEMENT_LEN:		/* ODBC 2.0 */
			len = 4;
			value = 0;
			break;

		case SQL_MAX_TABLE_NAME_LEN:	/* ODBC 1.0 */
			len = 2;
			if (PG_VERSION_GT(conn, 7.4))
				value = CC_get_max_idlen(conn);
#ifdef	MAX_TABLE_LEN
			else
				value = MAX_TABLE_LEN;
#endif /* MAX_TABLE_LEN */
			if (0 == value)
				value = NAMEDATALEN_V73 - 1;
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
			value = SQL_NC_HIGH;
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
			value = (SQL_OJ_LEFT |
					 SQL_OJ_RIGHT |
					 SQL_OJ_FULL |
					 SQL_OJ_NESTED |
					 SQL_OJ_NOT_ORDERED |
					 SQL_OJ_INNER |
					 SQL_OJ_ALL_COMPARISON_OPS);
			break;

		case SQL_ORDER_BY_COLUMNS_IN_SELECT:	/* ODBC 2.0 */
			p = "Y";
			break;

		case SQL_OUTER_JOINS:	/* ODBC 1.0 */
			p = "Y";
			break;

		case SQL_OWNER_TERM:	/* ODBC 1.0 */
			p = "schema";
			break;

		case SQL_OWNER_USAGE:	/* ODBC 2.0 */
			len = 4;
			value = SQL_OU_DML_STATEMENTS
					| SQL_OU_TABLE_DEFINITION
					| SQL_OU_INDEX_DEFINITION
					| SQL_OU_PRIVILEGE_DEFINITION;
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
			if (CurrCat(conn))
				value = SQL_QL_START;
			else
				value = 0;
			break;

		case SQL_QUALIFIER_NAME_SEPARATOR:		/* ODBC 1.0 */
			if (CurrCat(conn))
				p = ".";
			else
				p = NULL_STRING;
			break;

		case SQL_QUALIFIER_TERM:		/* ODBC 1.0 */
			if (CurrCat(conn))
				p = "catalog";
			else
				p = NULL_STRING;
			break;

		case SQL_QUALIFIER_USAGE:		/* ODBC 2.0 */
			len = 4;
			if (CurrCat(conn))
				value = SQL_CU_DML_STATEMENTS;
			else
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
			p = "\\";
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
			value = SQL_TXN_READ_UNCOMMITTED | SQL_TXN_READ_COMMITTED |
				SQL_TXN_REPEATABLE_READ | SQL_TXN_SERIALIZABLE;
			break;

		case SQL_UNION: /* ODBC 2.0 */
			/* unions with all supported in postgres 6.3 */
			len = 4;
			value = (SQL_U_UNION | SQL_U_UNION_ALL);
			break;

		case SQL_USER_NAME:		/* ODBC 1.0 */
			p = CC_get_username(conn);
			break;

		/* Keys for ODBC 3.0 */
		case SQL_DYNAMIC_CURSOR_ATTRIBUTES1:
			len = 4;
			value = 0;
			break;
		case SQL_DYNAMIC_CURSOR_ATTRIBUTES2:
			len = 4;
			value = 0;
			break;
		case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:
			len = 4;
			value = SQL_CA1_NEXT; /* others aren't allowed in ODBC spec */
			break;
		case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2:
			len = 4;
			value = SQL_CA2_READ_ONLY_CONCURRENCY;
			if (!ci->drivers.use_declarefetch || ci->drivers.lie)
				value |= SQL_CA2_CRC_EXACT;
			break;
		case SQL_KEYSET_CURSOR_ATTRIBUTES1:
			len = 4;
			value = SQL_CA1_NEXT | SQL_CA1_ABSOLUTE
				| SQL_CA1_RELATIVE | SQL_CA1_BOOKMARK
				| SQL_CA1_LOCK_NO_CHANGE | SQL_CA1_POS_POSITION
				| SQL_CA1_POS_REFRESH;
			if (0 != (ci->updatable_cursors & ALLOW_KEYSET_DRIVEN_CURSORS))
				value |= (SQL_CA1_POS_UPDATE | SQL_CA1_POS_DELETE
				| SQL_CA1_BULK_ADD
				| SQL_CA1_BULK_UPDATE_BY_BOOKMARK
				| SQL_CA1_BULK_DELETE_BY_BOOKMARK
				| SQL_CA1_BULK_FETCH_BY_BOOKMARK
				);
			if (ci->drivers.lie)
				value |= (SQL_CA1_LOCK_EXCLUSIVE
				| SQL_CA1_LOCK_UNLOCK
				| SQL_CA1_POSITIONED_UPDATE
				| SQL_CA1_POSITIONED_DELETE
				| SQL_CA1_SELECT_FOR_UPDATE
				);
			break;
		case SQL_KEYSET_CURSOR_ATTRIBUTES2:
			len = 4;
			value = SQL_CA2_READ_ONLY_CONCURRENCY;
			if (0 != (ci->updatable_cursors & ALLOW_KEYSET_DRIVEN_CURSORS))
				value |= (SQL_CA2_OPT_ROWVER_CONCURRENCY
				/*| SQL_CA2_CRC_APPROXIMATE*/
				);
			if (0 != (ci->updatable_cursors & SENSE_SELF_OPERATIONS))
				value |= (SQL_CA2_SENSITIVITY_DELETIONS
				| SQL_CA2_SENSITIVITY_UPDATES
				| SQL_CA2_SENSITIVITY_ADDITIONS
				);
			if (!ci->drivers.use_declarefetch || ci->drivers.lie)
				value |= SQL_CA2_CRC_EXACT;
			if (ci->drivers.lie)
				value |= (SQL_CA2_LOCK_CONCURRENCY
				| SQL_CA2_OPT_VALUES_CONCURRENCY
				| SQL_CA2_MAX_ROWS_SELECT
				| SQL_CA2_MAX_ROWS_INSERT
				| SQL_CA2_MAX_ROWS_DELETE
				| SQL_CA2_MAX_ROWS_UPDATE
				| SQL_CA2_MAX_ROWS_CATALOG
				| SQL_CA2_MAX_ROWS_AFFECTS_ALL
				| SQL_CA2_SIMULATE_NON_UNIQUE
				| SQL_CA2_SIMULATE_TRY_UNIQUE
				| SQL_CA2_SIMULATE_UNIQUE
				);
			break;

		case SQL_STATIC_CURSOR_ATTRIBUTES1:
			len = 4;
			value = SQL_CA1_NEXT | SQL_CA1_ABSOLUTE
				| SQL_CA1_RELATIVE | SQL_CA1_BOOKMARK
				| SQL_CA1_LOCK_NO_CHANGE | SQL_CA1_POS_POSITION
				| SQL_CA1_POS_REFRESH;
			if (0 != (ci->updatable_cursors & ALLOW_STATIC_CURSORS))
				value |= (SQL_CA1_POS_UPDATE | SQL_CA1_POS_DELETE
				);
			if (0 != (ci->updatable_cursors & ALLOW_BULK_OPERATIONS))
				value |= (SQL_CA1_BULK_ADD
				| SQL_CA1_BULK_UPDATE_BY_BOOKMARK
				| SQL_CA1_BULK_DELETE_BY_BOOKMARK
				| SQL_CA1_BULK_FETCH_BY_BOOKMARK
				);
			break;
		case SQL_STATIC_CURSOR_ATTRIBUTES2:
			len = 4;
			value = SQL_CA2_READ_ONLY_CONCURRENCY;
			if (0 != (ci->updatable_cursors & ALLOW_STATIC_CURSORS))
				value |= (SQL_CA2_OPT_ROWVER_CONCURRENCY
				);
			if (0 != (ci->updatable_cursors & SENSE_SELF_OPERATIONS))
				value |= (SQL_CA2_SENSITIVITY_DELETIONS
				| SQL_CA2_SENSITIVITY_UPDATES
				| SQL_CA2_SENSITIVITY_ADDITIONS
				);
			if (!ci->drivers.use_declarefetch || ci->drivers.lie)
				value |= (SQL_CA2_CRC_EXACT
				);
			break;

		case SQL_ODBC_INTERFACE_CONFORMANCE:
			len = 4;
			value = SQL_OIC_CORE;
			if (ci->drivers.lie)
				value = SQL_OIC_LEVEL2;
			break;
		case SQL_ACTIVE_ENVIRONMENTS:
			len = 2;
			value = 0;
			break;
		case SQL_AGGREGATE_FUNCTIONS:
			len = 4;
			value = SQL_AF_ALL;
			break;
		case SQL_ALTER_DOMAIN:
			len = 4;
			value = 0;
			break;
		case SQL_ASYNC_MODE:
			len = 4;
			value = SQL_AM_NONE;
			break;
		case SQL_BATCH_ROW_COUNT:
			len = 4;
			value = SQL_BRC_EXPLICIT;
			break;
		case SQL_BATCH_SUPPORT:
			len = 4;
			value = SQL_BS_SELECT_EXPLICIT | SQL_BS_ROW_COUNT_EXPLICIT;
			break;
		case SQL_CATALOG_NAME:
			if (CurrCat(conn))
				p = "Y";
			else
				p = "N";
			break;
		case SQL_COLLATION_SEQ:
			p = "";
			break;
		case SQL_CREATE_ASSERTION:
			len = 4;
			value = 0;
			break;
		case SQL_CREATE_CHARACTER_SET:
			len = 4;
			value = 0;
			break;
		case SQL_CREATE_COLLATION:
			len = 4;
			value = 0;
			break;
		case SQL_CREATE_DOMAIN:
			len = 4;
			value = 0;
			break;
		case SQL_CREATE_SCHEMA:
			len = 4;
			value = SQL_CS_CREATE_SCHEMA | SQL_CS_AUTHORIZATION;
			break;
		case SQL_CREATE_TABLE:
			len = 4;
			value = SQL_CT_CREATE_TABLE
				| SQL_CT_COLUMN_CONSTRAINT
				| SQL_CT_COLUMN_DEFAULT
				| SQL_CT_GLOBAL_TEMPORARY
				| SQL_CT_TABLE_CONSTRAINT
				| SQL_CT_CONSTRAINT_NAME_DEFINITION
				| SQL_CT_CONSTRAINT_INITIALLY_DEFERRED
				| SQL_CT_CONSTRAINT_INITIALLY_IMMEDIATE
				| SQL_CT_CONSTRAINT_DEFERRABLE;
			break;
		case SQL_CREATE_TRANSLATION:
			len = 4;
			value = 0;
			break;
		case SQL_CREATE_VIEW:
			len = 4;
			value = SQL_CV_CREATE_VIEW;
			break;
		case SQL_DDL_INDEX:
			len = 4;
			value = SQL_DI_CREATE_INDEX | SQL_DI_DROP_INDEX;
			break;
		case SQL_DESCRIBE_PARAMETER:
			p = "N";
			break;
		case SQL_DROP_ASSERTION:
			len = 4;
			value = 0;
			break;
		case SQL_DROP_CHARACTER_SET:
			len = 4;
			value = 0;
			break;
		case SQL_DROP_COLLATION:
			len = 4;
			value = 0;
			break;
		case SQL_DROP_DOMAIN:
			len = 4;
			value = 0;
			break;
		case SQL_DROP_SCHEMA:
			len = 4;
			value = SQL_DS_DROP_SCHEMA | SQL_DS_RESTRICT | SQL_DS_CASCADE;
			break;
		case SQL_DROP_TABLE:
			len = 4;
			value = SQL_DT_DROP_TABLE;
			value |= (SQL_DT_RESTRICT | SQL_DT_CASCADE);
			break;
		case SQL_DROP_TRANSLATION:
			len = 4;
			value = 0;
			break;
		case SQL_DROP_VIEW:
			len = 4;
			value = SQL_DV_DROP_VIEW;
			value |= (SQL_DV_RESTRICT | SQL_DV_CASCADE);
			break;
		case SQL_INDEX_KEYWORDS:
			len = 4;
			value = SQL_IK_NONE;
			break;
		case SQL_INFO_SCHEMA_VIEWS:
			len = 4;
			value = 0;
			break;
		case SQL_INSERT_STATEMENT:
			len = 4;
			value = SQL_IS_INSERT_LITERALS | SQL_IS_INSERT_SEARCHED | SQL_IS_SELECT_INTO;
			break;
		case SQL_MAX_IDENTIFIER_LEN:
			len = 2;
			value = CC_get_max_idlen(conn);
			if (0 == value)
				value = NAMEDATALEN_V73 - 1;
			break;
		case SQL_MAX_ROW_SIZE_INCLUDES_LONG:
			p = "Y";
			break;
		case SQL_PARAM_ARRAY_ROW_COUNTS:
			len = 4;
			value = SQL_PARC_BATCH;
			break;
		case SQL_PARAM_ARRAY_SELECTS:
			len = 4;
			value = SQL_PAS_BATCH;
			break;
		case SQL_SQL_CONFORMANCE:
			len = 4;
			value = SQL_SC_SQL92_ENTRY;
			break;
		case SQL_SQL92_DATETIME_FUNCTIONS:
			len = 4;
			value = SQL_SDF_CURRENT_DATE | SQL_SDF_CURRENT_TIME | SQL_SDF_CURRENT_TIMESTAMP;
			break;
		case SQL_SQL92_FOREIGN_KEY_DELETE_RULE:
			len = 4;
			value = SQL_SFKD_CASCADE | SQL_SFKD_NO_ACTION | SQL_SFKD_SET_DEFAULT | SQL_SFKD_SET_NULL;
			break;
		case SQL_SQL92_FOREIGN_KEY_UPDATE_RULE:
			len = 4;
			value = SQL_SFKU_CASCADE | SQL_SFKU_NO_ACTION | SQL_SFKU_SET_DEFAULT | SQL_SFKU_SET_NULL;
			break;
		case SQL_SQL92_GRANT:
			len = 4;
			value = SQL_SG_DELETE_TABLE | SQL_SG_INSERT_TABLE | SQL_SG_REFERENCES_TABLE | SQL_SG_SELECT_TABLE | SQL_SG_UPDATE_TABLE;
			break;
		case SQL_SQL92_NUMERIC_VALUE_FUNCTIONS:
			len = 4;
			value = SQL_SNVF_BIT_LENGTH | SQL_SNVF_CHAR_LENGTH
				| SQL_SNVF_CHARACTER_LENGTH | SQL_SNVF_EXTRACT
				| SQL_SNVF_OCTET_LENGTH | SQL_SNVF_POSITION;
			break;
		case SQL_SQL92_PREDICATES:
			len = 4;
			value = SQL_SP_BETWEEN | SQL_SP_COMPARISON
				| SQL_SP_EXISTS | SQL_SP_IN
				| SQL_SP_ISNOTNULL | SQL_SP_ISNULL
				| SQL_SP_LIKE | SQL_SP_OVERLAPS
				| SQL_SP_QUANTIFIED_COMPARISON;
			break;
		case SQL_SQL92_RELATIONAL_JOIN_OPERATORS:
			len = 4;
			value = SQL_SRJO_CROSS_JOIN | SQL_SRJO_EXCEPT_JOIN
				| SQL_SRJO_FULL_OUTER_JOIN | SQL_SRJO_INNER_JOIN
				| SQL_SRJO_INTERSECT_JOIN | SQL_SRJO_LEFT_OUTER_JOIN
				| SQL_SRJO_NATURAL_JOIN | SQL_SRJO_RIGHT_OUTER_JOIN
				| SQL_SRJO_UNION_JOIN;
			break;
		case SQL_SQL92_REVOKE:
			len = 4;
			value = SQL_SR_DELETE_TABLE | SQL_SR_INSERT_TABLE | SQL_SR_REFERENCES_TABLE | SQL_SR_SELECT_TABLE | SQL_SR_UPDATE_TABLE;
			break;
		case SQL_SQL92_ROW_VALUE_CONSTRUCTOR:
			len = 4;
			value = SQL_SRVC_VALUE_EXPRESSION | SQL_SRVC_NULL;
			break;
		case SQL_SQL92_STRING_FUNCTIONS:
			len = 4;
			value = SQL_SSF_CONVERT | SQL_SSF_LOWER
				| SQL_SSF_UPPER | SQL_SSF_SUBSTRING
				| SQL_SSF_TRANSLATE | SQL_SSF_TRIM_BOTH
				| SQL_SSF_TRIM_LEADING | SQL_SSF_TRIM_TRAILING;
			break;
		case SQL_SQL92_VALUE_EXPRESSIONS:
			len = 4;
			value = SQL_SVE_CASE | SQL_SVE_CAST | SQL_SVE_COALESCE | SQL_SVE_NULLIF;
			break;
#ifdef SQL_DTC_TRANSACTION_COST
		case SQL_DTC_TRANSACTION_COST:
#else
		case 1750:
#endif
			len = 4;
			break;
		/* The followings aren't implemented yet */
		case SQL_DATETIME_LITERALS:
			len = 4;
		case SQL_DM_VER:
			len = 0;
		case SQL_DRIVER_HDESC:
			len = 4;
		case SQL_MAX_ASYNC_CONCURRENT_STATEMENTS:
			len = 4;
		case SQL_STANDARD_CLI_CONFORMANCE:
			len = 4;
		case SQL_XOPEN_CLI_YEAR:
			len = 0;

		default:
			/* unrecognized key */
			CC_set_error(conn, CONN_NOT_IMPLEMENTED_ERROR, "Unrecognized key passed to PGAPI_GetInfo.", NULL);
			goto cleanup;
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
			if (CC_is_in_unicode_driver(conn))
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
				CC_set_error(conn, CONN_TRUNCATED, "The buffer was too small for the InfoValue.", func);
			}
		}
#ifdef	UNICODE_SUPPORT
		else if (CC_is_in_unicode_driver(conn))
			len *= WCLEN;
#endif /* UNICODE_SUPPORT */
	}
	else
	{
		/* numeric data */
		if (rgbInfoValue)
		{
			if (len == sizeof(SQLSMALLINT))
				*((SQLUSMALLINT *) rgbInfoValue) = (SQLUSMALLINT) value;
			else if (len == sizeof(SQLINTEGER))
				*((SQLUINTEGER *) rgbInfoValue) = (SQLUINTEGER) value;
		}
	}

	if (pcbInfoValue)
		*pcbInfoValue = (SQLSMALLINT) len;
cleanup:

	return result;
}

/*
 *	macros for pgtype_xxxx() calls which have PG_ATP_UNSET parameters
 */
#define PGTYPE_COLUMN_SIZE(conn, pgType) pgtype_attr_column_size(conn, pgType, PG_ATP_UNSET, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_TO_CONCISE_TYPE(conn, pgType) pgtype_attr_to_concise_type(conn, pgType, PG_ATP_UNSET, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_TO_SQLDESCTYPE(conn, pgType) pgtype_attr_to_sqldesctype(conn, pgType, PG_ATP_UNSET, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_BUFFER_LENGTH(conn, pgType) pgtype_attr_buffer_length(conn, pgType, PG_ATP_UNSET, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_DECIMAL_DIGITS(conn, pgType) pgtype_attr_decimal_digits(conn, pgType, PG_ATP_UNSET, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_TRANSFER_OCTET_LENGTH(conn, pgType) pgtype_attr_transfer_octet_length(conn, pgType, PG_ATP_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_TO_NAME(conn, pgType, auto_increment) pgtype_attr_to_name(conn, pgType, PG_ATP_UNSET, auto_increment)
#define PGTYPE_TO_DATETIME_SUB(conn, pgtype) pgtype_attr_to_datetime_sub(conn, pgtype, PG_ATP_UNSET)


RETCODE		SQL_API
PGAPI_GetTypeInfo(HSTMT hstmt,
				  SQLSMALLINT fSqlType)
{
	CSTR func = "PGAPI_GetTypeInfo";
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass	*conn;
	QResultClass	*res = NULL;
	TupleField	*tuple;
	int			i, result_cols;

	/* Int4 type; */
	Int4		pgType;
	Int2		sqlType;
	RETCODE		result = SQL_SUCCESS;

	mylog("%s: entering...fSqlType = %d\n", func, fSqlType);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	conn = SC_get_conn(stmt);
	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "Error creating result.", func);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

#define	return	DONT_CALL_RETURN_FROM_HERE???
	result_cols = NUM_OF_GETTYPE_FIELDS;
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
	QR_set_field_info_v(res, 15, "SQL_DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 16, "SQL_DATETIME_SUB", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, 17, "NUM_PREC_RADIX", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, 18, "INTERVAL_PRECISION", PG_TYPE_INT2, 2);

	for (i = 0, sqlType = sqlTypes[0]; sqlType; sqlType = sqlTypes[++i])
	{
		pgType = sqltype_to_pgtype(conn, sqlType);

if (sqlType == SQL_LONGVARBINARY)
{
ConnInfo	*ci = &(conn->connInfo);
inolog("%d sqltype=%d -> pgtype=%d\n", ci->bytea_as_longvarbinary, sqlType, pgType);
}

		if (fSqlType == SQL_ALL_TYPES || fSqlType == sqlType)
		{
			int	pgtcount = 1, aunq_match = -1, cnt;

			/*if (SQL_INTEGER == sqlType || SQL_TINYINT == sqlType)*/
			if (SQL_INTEGER == sqlType)
			{
mylog("sqlType=%d ms_jet=%d\n", sqlType, conn->ms_jet);
				if (conn->ms_jet)
				{
					aunq_match = 1;
					pgtcount = 2;
				}
mylog("aunq_match=%d pgtcount=%d\n", aunq_match, pgtcount);
			}
			for (cnt = 0; cnt < pgtcount; cnt ++)
			{
				if (tuple = QR_AddNew(res), NULL == tuple)
				{
					result = SQL_ERROR;
					SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't QR_AddNew.", func);
					goto cleanup;
				}

				/* These values can't be NULL */
				if (aunq_match == cnt)
				{
					set_tuplefield_string(&tuple[GETTYPE_TYPE_NAME], PGTYPE_TO_NAME(conn, pgType, TRUE));
					set_tuplefield_int2(&tuple[GETTYPE_NULLABLE], SQL_NO_NULLS);
inolog("serial in\n");
				}
				else
				{
					set_tuplefield_string(&tuple[GETTYPE_TYPE_NAME], PGTYPE_TO_NAME(conn, pgType, FALSE));
					set_tuplefield_int2(&tuple[GETTYPE_NULLABLE], pgtype_nullable(conn, pgType));
				}
				set_tuplefield_int2(&tuple[GETTYPE_DATA_TYPE], (Int2) sqlType);
				set_tuplefield_int2(&tuple[GETTYPE_CASE_SENSITIVE], pgtype_case_sensitive(conn, pgType));
				set_tuplefield_int2(&tuple[GETTYPE_SEARCHABLE], pgtype_searchable(conn, pgType));
				set_tuplefield_int2(&tuple[GETTYPE_FIXED_PREC_SCALE], pgtype_money(conn, pgType));

			/*
			 * Localized data-source dependent data type name (always
			 * NULL)
			 */
				set_tuplefield_null(&tuple[GETTYPE_LOCAL_TYPE_NAME]);

				/* These values can be NULL */
				set_nullfield_int4(&tuple[GETTYPE_COLUMN_SIZE], PGTYPE_COLUMN_SIZE(conn, pgType));
				set_nullfield_string(&tuple[GETTYPE_LITERAL_PREFIX], pgtype_literal_prefix(conn, pgType));
				set_nullfield_string(&tuple[GETTYPE_LITERAL_SUFFIX], pgtype_literal_suffix(conn, pgType));
				set_nullfield_string(&tuple[GETTYPE_CREATE_PARAMS], pgtype_create_params(conn, pgType));
				if (1 < pgtcount)
					set_tuplefield_int2(&tuple[GETTYPE_UNSIGNED_ATTRIBUTE], SQL_TRUE);
				else
					set_nullfield_int2(&tuple[GETTYPE_UNSIGNED_ATTRIBUTE], pgtype_unsigned(conn, pgType));
				if (aunq_match == cnt)
					set_tuplefield_int2(&tuple[GETTYPE_AUTO_UNIQUE_VALUE], SQL_TRUE);
				else
					set_nullfield_int2(&tuple[GETTYPE_AUTO_UNIQUE_VALUE], pgtype_auto_increment(conn, pgType));
				set_nullfield_int2(&tuple[GETTYPE_MINIMUM_SCALE], pgtype_min_decimal_digits(conn, pgType));
				set_nullfield_int2(&tuple[GETTYPE_MAXIMUM_SCALE], pgtype_max_decimal_digits(conn, pgType));
				set_tuplefield_int2(&tuple[GETTYPE_SQL_DATA_TYPE], PGTYPE_TO_SQLDESCTYPE(conn, pgType));
				set_nullfield_int2(&tuple[GETTYPE_SQL_DATETIME_SUB], PGTYPE_TO_DATETIME_SUB(conn, pgType));
				set_nullfield_int4(&tuple[GETTYPE_NUM_PREC_RADIX], pgtype_radix(conn, pgType));
				set_nullfield_int4(&tuple[GETTYPE_INTERVAL_PRECISION], 0);
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
	if (SQL_SUCCEEDED(result))
		SC_set_rowset_start(stmt, -1, FALSE);
	else
		SC_set_Result(stmt, NULL);
	SC_set_current_col(stmt, -1);

	return result;
}


RETCODE		SQL_API
PGAPI_GetFunctions(HDBC hdbc,
				   SQLUSMALLINT fFunction,
				   SQLUSMALLINT * pfExists)
{
	CSTR func = "PGAPI_GetFunctions";
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	ConnInfo   *ci = &(conn->connInfo);

	mylog("%s: entering...%u\n", func, fFunction);

	if (fFunction == SQL_API_ALL_FUNCTIONS)
	{
		memset(pfExists, 0, sizeof(pfExists[0]) * 100);

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
		pfExists[SQL_API_SQLCOLUMNPRIVILEGES] = FALSE;
		pfExists[SQL_API_SQLDATASOURCES] = FALSE;	/* only implemented by
													 * DM */
		if (SUPPORT_DESCRIBE_PARAM(ci))
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
		pfExists[SQL_API_SQLPROCEDURECOLUMNS] = TRUE;
		pfExists[SQL_API_SQLPROCEDURES] = TRUE;
		pfExists[SQL_API_SQLSETPOS] = TRUE;
		pfExists[SQL_API_SQLSETSCROLLOPTIONS] = TRUE;		/* odbc 1.0 */
		pfExists[SQL_API_SQLTABLEPRIVILEGES] = TRUE;
		if (0 == ci->updatable_cursors)
			pfExists[SQL_API_SQLBULKOPERATIONS] = FALSE;
		else
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
					if (SUPPORT_DESCRIBE_PARAM(ci))
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
				case SQL_API_SQLPRIMARYKEYS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLPROCEDURECOLUMNS:
					*pfExists = TRUE;
					break;
				case SQL_API_SQLPROCEDURES:
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


static char	*
simpleCatalogEscape(const SQLCHAR *src, SQLLEN srclen, const ConnectionClass *conn)
{
	int	i, outlen;
	const char *in;
	char	*dest = NULL, escape_ch = CC_get_escape(conn);
	encoded_str	encstr;

	if (!src || srclen == SQL_NULL_DATA)
		return dest;
	else if (srclen == SQL_NTS)
		srclen = (SQLLEN) strlen((char *) src);
	if (srclen <= 0)
		return dest;
mylog("simple in=%s(%d)\n", src, srclen);
	encoded_str_constr(&encstr, conn->ccsc, (char *) src);
	dest = malloc(2 * srclen + 1);
	if (!dest) return NULL;
	for (i = 0, in = (char *) src, outlen = 0; i < srclen; i++, in++)
	{
                encoded_nextchar(&encstr);
                if (ENCODE_STATUS(encstr) != 0)
                {
                        dest[outlen++] = *in;
                        continue;
                }
		if (LITERAL_QUOTE == *in ||
		    escape_ch == *in)
			dest[outlen++] = *in;
		dest[outlen++] = *in;
	}
	dest[outlen] = '\0';
mylog("simple output=%s(%d)\n", dest, outlen);
	return dest;
}

/*
 *	PostgreSQL needs 2 '\\' to escape '_' and '%'.
 */
static char	*
adjustLikePattern(const SQLCHAR *src, int srclen, const ConnectionClass *conn)
{
	int	i, outlen;
	const char *in;
	char	*dest = NULL, escape_in_literal = CC_get_escape(conn);
	BOOL	escape_in = FALSE;
	encoded_str	encstr;

	if (!src || srclen == SQL_NULL_DATA)
		return dest;
	else if (srclen == SQL_NTS)
		srclen = (int) strlen((char *) src);
	/* if (srclen <= 0) */
	if (srclen < 0)
		return dest;
mylog("adjust in=%.*s(%d)\n", srclen, src, srclen);
	encoded_str_constr(&encstr, conn->ccsc, (char *) src);
	dest = malloc(4 * srclen + 1);
	if (!dest) return NULL;
	for (i = 0, in = (char *) src, outlen = 0; i < srclen; i++, in++)
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
					break;
				default:
					if (SEARCH_PATTERN_ESCAPE == escape_in_literal)
						dest[outlen++] = escape_in_literal;
					dest[outlen++] = SEARCH_PATTERN_ESCAPE;
					break;
			}
		}
		if (*in == SEARCH_PATTERN_ESCAPE)
		{
			escape_in = TRUE;
			if (SEARCH_PATTERN_ESCAPE == escape_in_literal)
				dest[outlen++] = escape_in_literal; /* insert 1 more LEXER escape */
		}
		else
		{
			escape_in = FALSE;
			if (LITERAL_QUOTE == *in)
				dest[outlen++] = *in;
		}
		dest[outlen++] = *in;
	}
	if (escape_in)
	{
		if (SEARCH_PATTERN_ESCAPE == escape_in_literal)
			dest[outlen++] = escape_in_literal;
		dest[outlen++] = SEARCH_PATTERN_ESCAPE;
	}
	dest[outlen] = '\0';
mylog("adjust output=%s(%d)\n", dest, outlen);
	return dest;
}

#define	CSTR_SYS_TABLE	"SYSTEM TABLE"
#define	CSTR_TABLE	"TABLE"
#define	CSTR_VIEW	"VIEW"
#define CSTR_FOREIGN_TABLE "FOREIGN TABLE"
#define CSTR_MATVIEW "MATVIEW"

CSTR	like_op_sp = "like ";
CSTR	like_op_ext = "like E";
CSTR	eq_op_sp =	"= ";
CSTR	eq_op_ext =	"= E";

#define	 IS_VALID_NAME(str) ((str) && (str)[0])

static const char *gen_opestr(const char *orig_opestr, const ConnectionClass * conn)
{
	BOOL	addE = (0 != CC_get_escape(conn) && PG_VERSION_GE(conn, 8.1));

	if (0 == strcmp(orig_opestr, eqop))
		return (addE ? eq_op_ext : eq_op_sp);
	return (addE ? like_op_ext : like_op_sp);
}

/*
 *	If specified schema name == user_name and the current schema is
 *	'public', allowed to use the 'public' schema.
 */
static BOOL
allow_public_schema(ConnectionClass *conn, const SQLCHAR *szSchemaName, SQLSMALLINT cbSchemaName)
{
	const char *user = CC_get_username(conn);
	const char *curschema;
	size_t	userlen = strlen(user);
	size_t	schemalen;

	if (NULL == szSchemaName)
		return FALSE;

	if (SQL_NTS == cbSchemaName)
		schemalen = strlen((char *) szSchemaName);
	else
		schemalen = cbSchemaName;

	if (schemalen != userlen)
		return FALSE;
	if (strnicmp((char *) szSchemaName, user, userlen) != 0)
		return FALSE;

	curschema = CC_get_current_schema(conn);
	if (curschema == NULL)
		return FALSE;

	return stricmp(curschema, (const char *) pubstr) == 0;
}

RETCODE		SQL_API
PGAPI_Tables(HSTMT hstmt,
			 const SQLCHAR * szTableQualifier, /* PV X*/
			 SQLSMALLINT cbTableQualifier,
			 const SQLCHAR * szTableOwner, /* PV E*/
			 SQLSMALLINT cbTableOwner,
			 const SQLCHAR * szTableName, /* PV E*/
			 SQLSMALLINT cbTableName,
			 const SQLCHAR * szTableType,
			 SQLSMALLINT cbTableType,
			 UWORD	flag)
{
	CSTR func = "PGAPI_Tables";
	StatementClass *stmt = (StatementClass *) hstmt;
	StatementClass *tbl_stmt;
	QResultClass	*res;
	TupleField	*tuple;
	HSTMT		htbl_stmt = NULL;
	RETCODE		ret = SQL_ERROR, result;
	int		result_cols;
	char		*tableType = NULL;
	char		tables_query[INFO_INQUIRY_LEN];
	char		table_name[MAX_INFO_STRING],
				table_owner[MAX_INFO_STRING],
				relkind_or_hasrules[MAX_INFO_STRING];
#ifdef	HAVE_STRTOK_R
	char		*last;
#endif /* HAVE_STRTOK_R */
	ConnectionClass *conn;
	ConnInfo   *ci;
	char	*escCatName = NULL, *escSchemaName = NULL, *escTableName = NULL;
	/* Support up to 32 system table prefixes. Should be more than enough. */
#define MAX_PREFIXES 32
	char	   *prefix[MAX_PREFIXES],
				prefixes[MEDIUM_REGISTRY_LEN];
	int			nprefixes;
	char		show_system_tables,
				show_regular_tables,
				show_views,
				show_matviews,
				show_foreign_tables;
	char		regular_table,
				view,
				matview,
				foreign_table,
				systable;
	int			i;
	SQLSMALLINT		internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char	*like_or_eq, *op_string;
	const SQLCHAR *szSchemaName;
	BOOL		search_pattern;
	BOOL		list_cat = FALSE, list_schemas = FALSE, list_table_types = FALSE, list_some = FALSE;
	SQLLEN		cbRelname, cbRelkind, cbSchName;
	EnvironmentClass *env;

	mylog("%s: entering...stmt=%p scnm=%p len=%d\n", func, stmt, szTableOwner, cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
	env = CC_get_env(conn);

	result = PGAPI_AllocStmt(conn, &htbl_stmt, 0);
	if (!SQL_SUCCEEDED(result))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_Tables result.", func);
		return SQL_ERROR;
	}
	tbl_stmt = (StatementClass *) htbl_stmt;
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

#define	return	DONT_CALL_RETURN_FROM_HERE???
	search_pattern = (0 == (flag & PODBC_NOT_SEARCH_PATTERN));
	if (search_pattern)
	{
		like_or_eq = likeop;
		escCatName = adjustLikePattern(szTableQualifier, cbTableQualifier, conn);
		escTableName = adjustLikePattern(szTableName, cbTableName, conn);
	}
	else
	{
		like_or_eq = eqop;
		escCatName = simpleCatalogEscape(szTableQualifier, cbTableQualifier, conn);
		escTableName = simpleCatalogEscape(szTableName, cbTableName, conn);
	}
retry_public_schema:
	if (escSchemaName)
		free(escSchemaName);
	if (search_pattern)
		escSchemaName = adjustLikePattern(szSchemaName, cbSchemaName, conn);
	else
		escSchemaName = simpleCatalogEscape(szSchemaName, cbSchemaName, conn);
	/*
	 * Create the query to find out the tables
	 */
	/* make_string mallocs memory */
	tableType = make_string(szTableType, cbTableType, NULL, 0);
	if (search_pattern &&
	    escTableName && '\0' == escTableName[0] &&
	    escCatName && escSchemaName)
	{
		if ('\0' == escSchemaName[0])
		{
			if (stricmp(escCatName, SQL_ALL_CATALOGS) == 0)
				list_cat = TRUE;
			else if ('\0' == escCatName[0] &&
				 stricmp(tableType, SQL_ALL_TABLE_TYPES) == 0)
				list_table_types = TRUE;
		}
		else if ('\0' == escCatName[0] &&
			 stricmp(escSchemaName, SQL_ALL_SCHEMAS) == 0)
			list_schemas = TRUE;
	}
	list_some = (list_cat || list_schemas || list_table_types);

	tables_query[0] = '\0';
	if (list_cat)
		STRCPY_FIXED(tables_query, "select NULL, NULL, NULL");
	else if (list_table_types)
	{
		/*
		 * Query relations depending on what is available:
		 * - 9.3 and newer versions have materialized views
		 * - 9.1 and newer versions have foreign tables
		 */
		STRCPY_FIXED(tables_query,
				"select NULL, NULL, relkind from (select 'r' as relkind "
				"union select 'v' "
				"union select 'm' "
				"union select 'f') as a");
	}
	else if (list_schemas)
	{
		STRCPY_FIXED(tables_query, "select NULL, nspname, NULL"
			" from pg_catalog.pg_namespace n where true");
	}
	else
	{
		/*
		 * View is represented by its relkind since 7.1,
		 * Materialized views are added in 9.3, and foreign
		 * tables in 9.1.
		 */
		STRCPY_FIXED(tables_query, "select relname, nspname, relkind "
			"from pg_catalog.pg_class c, pg_catalog.pg_namespace n "
			"where relkind in ('r', 'v', 'm', 'f')");
	}

	op_string = gen_opestr(like_or_eq, conn);
	if (!list_some)
	{
		schema_strcat1(tables_query, sizeof(tables_query), " and nspname %s'%.*s'", op_string, escSchemaName, szTableName, cbTableName, conn);
		if (IS_VALID_NAME(escTableName))
			SPRINTFCAT_FIXED(tables_query,
					 " and relname %s'%s'", op_string, escTableName);
	}

	/*
	 * Parse the extra systable prefix configuration variable into an array
	 * of prefixes.
	 */
	STRCPY_FIXED(prefixes, ci->drivers.extra_systable_prefixes);
	for (i = 0; i < MAX_PREFIXES; i++)
	{
		char	*str = (i == 0) ? prefixes : NULL;

#ifdef	HAVE_STRTOK_R
		prefix[i] = strtok_r(str, ";", &last);
#else
		prefix[i] = strtok(str, ";");
#endif /* HAVE_STRTOK_R */

		if (prefix[i] == NULL)
			break;
	}
	nprefixes = i;

	/* Parse the desired table types to return */
	show_system_tables = FALSE;
	show_regular_tables = FALSE;
	show_views = FALSE;
	show_foreign_tables = FALSE;
	show_matviews = FALSE;

	/* TABLE_TYPE */
	if (!tableType)
	{
		show_regular_tables = TRUE;
		show_views = TRUE;
		show_foreign_tables = TRUE;
		show_matviews = TRUE;
	}
	else if (list_some || stricmp(tableType, SQL_ALL_TABLE_TYPES) == 0)
	{
		show_regular_tables = TRUE;
		show_views = TRUE;
		show_foreign_tables = TRUE;
		show_matviews = TRUE;
	}
	else
	{
		/* Check for desired table types to return */
		char *srcstr;
		for (srcstr = tableType;; srcstr = NULL)
		{
			char *typestr;

#ifdef	HAVE_STRTOK_R
			typestr = strtok_r(srcstr, ",", &last);
#else
			typestr = strtok(srcstr, ",");
#endif /* HAVE_STRTOK_R */

			if (typestr == NULL)
				break;

			while (isspace((unsigned char) *typestr))
				typestr++;
			if (*typestr == '\'')
				typestr++;
			if (strnicmp(typestr, CSTR_SYS_TABLE, strlen(CSTR_SYS_TABLE)) == 0)
				show_system_tables = TRUE;
			else if (strnicmp(typestr, CSTR_TABLE, strlen(CSTR_TABLE)) == 0)
				show_regular_tables = TRUE;
			else if (strnicmp(typestr, CSTR_VIEW, strlen(CSTR_VIEW)) == 0)
				show_views = TRUE;
			else if (strnicmp(typestr, CSTR_FOREIGN_TABLE, strlen(CSTR_FOREIGN_TABLE)) == 0)
				show_foreign_tables = TRUE;
			else if (strnicmp(typestr, CSTR_MATVIEW, strlen(CSTR_MATVIEW)) == 0)
				show_matviews = TRUE;
		}
	}

	/*
	 * If not interested in SYSTEM TABLES then filter them out to save
	 * some time on the query.	If treating system tables as regular
	 * tables, then dont filter either.
	 */
	if ((list_schemas || !list_some) && !atoi(ci->show_system_tables) && !show_system_tables)
		STRCAT_FIXED(tables_query, " and nspname not in ('pg_catalog', 'information_schema', 'pg_toast', 'pg_temp_1')");

	if (!list_some)
	{
		if (CC_accessible_only(conn))
			STRCAT_FIXED(tables_query, " and has_table_privilege(c.oid, 'select')");
	}
	if (list_schemas)
		STRCAT_FIXED(tables_query, " order by nspname");
	else if (list_some)
		;
	else
		STRCAT_FIXED(tables_query, " and n.oid = relnamespace order by nspname, relname");

	result = PGAPI_ExecDirect(htbl_stmt, (SQLCHAR *) tables_query, SQL_NTS, PODBC_RDONLY);
	if (!SQL_SUCCEEDED(result))
	{
		SC_full_error_copy(stmt, htbl_stmt, FALSE);
		goto cleanup;
	}

	/* If not found */
	if ((res = SC_get_Result(tbl_stmt)) &&
	    0 == QR_get_num_total_tuples(res))
	{
		if (allow_public_schema(conn, szSchemaName, cbSchemaName))
		{
			szSchemaName = pubstr;
			cbSchemaName = SQL_NTS;
			goto retry_public_schema;
		}
	}
#ifdef	UNICODE_SUPPORT
	if (CC_is_in_unicode_driver(conn))
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */
	result = PGAPI_BindCol(htbl_stmt, 1, internal_asis_type,
			table_name, MAX_INFO_STRING, &cbRelname);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(htbl_stmt, 2, internal_asis_type,
						   table_owner, MAX_INFO_STRING, &cbSchName);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		goto cleanup;
	}
	result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
			relkind_or_hasrules, MAX_INFO_STRING, &cbRelkind);
	if (!SQL_SUCCEEDED(result))
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
	result_cols = NUM_OF_TABLES_FIELDS;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	/* set the field names */
	QR_set_num_fields(res, result_cols);
	if (EN_is_odbc3(env))
	{
		QR_set_field_info_v(res, TABLES_CATALOG_NAME, "TABLE_CAT", PG_TYPE_VARCHAR, MAX_INFO_STRING);
		QR_set_field_info_v(res, TABLES_SCHEMA_NAME, "TABLE_SCHEM", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	}
	else
	{
		QR_set_field_info_v(res, TABLES_CATALOG_NAME, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
		QR_set_field_info_v(res, TABLES_SCHEMA_NAME, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	}
	QR_set_field_info_v(res, TABLES_TABLE_NAME, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, TABLES_TABLE_TYPE, "TABLE_TYPE", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, TABLES_REMARKS, "REMARKS", PG_TYPE_VARCHAR, INFO_VARCHAR_SIZE);

	/* add the tuples */
	table_name[0] = '\0';
	table_owner[0] = '\0';
	result = PGAPI_Fetch(htbl_stmt);
	while (SQL_SUCCEEDED(result))
	{
		/*
		 * Determine if this table name is a system table. If treating
		 * system tables as regular tables, then no need to do this test.
		 */
		systable = FALSE;
		if (!atoi(ci->show_system_tables))
		{
			if (stricmp(table_owner, "pg_catalog") == 0 ||
			    stricmp(table_owner, "pg_toast") == 0 ||
			    strnicmp(table_owner, "pg_temp_", 8) == 0 ||
			    stricmp(table_owner, "information_schema") == 0)
				systable = TRUE;
			else
			{
				/* Check extra system table prefixes */
				for (i = 0; i < nprefixes; i++)
				{
					mylog("table_name='%s', prefix[%d]='%s'\n", table_name, i, prefix[i]);
					if (strncmp(table_name, prefix[i], strlen(prefix[i])) == 0)
					{
						systable = TRUE;
						break;
					}
				}
			}
		}

		/* Determine if the table name is a view */
		view = (relkind_or_hasrules[0] == 'v');

		/* Check for foreign tables and materialized views ... */
		foreign_table = (relkind_or_hasrules[0] == 'f');
		matview =  (relkind_or_hasrules[0] == 'm');

		/* It must be a regular table */
		regular_table = (!systable && !view);

		/* Include the row in the result set if meets all criteria */

		/*
		 * NOTE: Unsupported table types (i.e., LOCAL TEMPORARY, ALIAS,
		 * etc) will return nothing
		 */
		if ((systable && show_system_tables) ||
			(view && show_views) ||
			(foreign_table && show_foreign_tables) ||
			(matview && show_matviews) ||
			(regular_table && show_regular_tables))
		{
			tuple = QR_AddNew(res);

			if (list_cat || !list_some)
				set_tuplefield_string(&tuple[TABLES_CATALOG_NAME], CurrCat(conn));
			else
				set_tuplefield_null(&tuple[TABLES_CATALOG_NAME]);

			/*
			 * I have to hide the table owner from Access, otherwise it
			 * insists on referring to the table as 'owner.table'. (this
			 * is valid according to the ODBC SQL grammar, but Postgres
			 * won't support it.)
			 *
			 * set_tuplefield_string(&tuple[TABLES_SCHEMA_NAME], table_owner);
			 */

			mylog("%s: table_name = '%s'\n", func, table_name);

			if (list_schemas || !list_some)
				set_tuplefield_string(&tuple[TABLES_SCHEMA_NAME], GET_SCHEMA_NAME(table_owner));
			else
				set_tuplefield_null(&tuple[TABLES_SCHEMA_NAME]);
			if (list_some)
				set_tuplefield_null(&tuple[TABLES_TABLE_NAME]);
			else
				set_tuplefield_string(&tuple[TABLES_TABLE_NAME], table_name);
			if (list_table_types || !list_some)
			{
				if (systable)
					set_tuplefield_string(&tuple[TABLES_TABLE_TYPE], CSTR_SYS_TABLE);
				else if (view)
					set_tuplefield_string(&tuple[TABLES_TABLE_TYPE], CSTR_VIEW);
				else if (matview)
					set_tuplefield_string(&tuple[TABLES_TABLE_TYPE], CSTR_MATVIEW);
				else if (foreign_table)
					set_tuplefield_string(&tuple[TABLES_TABLE_TYPE], CSTR_FOREIGN_TABLE);
				else
					set_tuplefield_string(&tuple[TABLES_TABLE_TYPE], CSTR_TABLE);
			}
			else
				set_tuplefield_null(&tuple[TABLES_TABLE_TYPE]);
			set_tuplefield_string(&tuple[TABLES_REMARKS], NULL_STRING);
			/*** set_tuplefield_string(&tuple[TABLES_REMARKS], "TABLE"); ***/
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

	if (escCatName)
		free(escCatName);
	if (escSchemaName)
		free(escSchemaName);
	if (escTableName)
		free(escTableName);
	if (tableType)
		free(tableType);
	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	if (htbl_stmt)
		PGAPI_FreeStmt(htbl_stmt, SQL_DROP);

	mylog("%s: EXIT, stmt=%p, ret=%d\n", func, stmt, ret);
	return ret;
}

/*
 *	macros for pgtype_attr_xxxx() calls which have
 *		PG_ADT_UNSET or PG_UNKNOWNS_UNSET parameters
 */
#define PGTYPE_ATTR_COLUMN_SIZE(conn, pgType, atttypmod) pgtype_attr_column_size(conn, pgType, atttypmod, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_ATTR_TO_CONCISE_TYPE(conn, pgType, atttypmod) pgtype_attr_to_concise_type(conn, pgType, atttypmod, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_ATTR_TO_SQLDESCTYPE(conn, pgType, atttypmod) pgtype_attr_to_sqldesctype(conn, pgType, atttypmod, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_ATTR_DISPLAY_SIZE(conn, pgType, atttypmod) pgtype_attr_display_size(conn, pgType, atttypmod, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_ATTR_BUFFER_LENGTH(conn, pgType, atttypmod) pgtype_attr_buffer_length(conn, pgType, atttypmod, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_ATTR_DECIMAL_DIGITS(conn, pgType, atttypmod) pgtype_attr_decimal_digits(conn, pgType, atttypmod, PG_ADT_UNSET, PG_UNKNOWNS_UNSET)
#define PGTYPE_ATTR_TRANSFER_OCTET_LENGTH(conn, pgType, atttypmod) pgtype_attr_transfer_octet_length(conn, pgType, atttypmod, PG_UNKNOWNS_UNSET)

/*
 *	for oid or xmin
 */
static void
add_tuple_for_oid_or_xmin(TupleField *tuple, int ordinal, const char *colname, OID the_type, const char *typname, const ConnectionClass *conn, const char *table_owner, const char *table_name, OID greloid, int attnum, BOOL auto_increment)
{
	int	sqltype;
	const int atttypmod = -1;

	set_tuplefield_string(&tuple[COLUMNS_CATALOG_NAME], CurrCat(conn));
	/* see note in SQLTables() */
	set_tuplefield_string(&tuple[COLUMNS_SCHEMA_NAME], GET_SCHEMA_NAME(table_owner));
	set_tuplefield_string(&tuple[COLUMNS_TABLE_NAME], table_name);
	set_tuplefield_string(&tuple[COLUMNS_COLUMN_NAME], colname);
	sqltype = PGTYPE_ATTR_TO_CONCISE_TYPE(conn, the_type, atttypmod);
	set_tuplefield_int2(&tuple[COLUMNS_DATA_TYPE], sqltype);
	set_tuplefield_string(&tuple[COLUMNS_TYPE_NAME], typname);

	set_tuplefield_int4(&tuple[COLUMNS_PRECISION], PGTYPE_ATTR_COLUMN_SIZE(conn, the_type, atttypmod));
	set_tuplefield_int4(&tuple[COLUMNS_LENGTH], PGTYPE_ATTR_BUFFER_LENGTH(conn, the_type, atttypmod));
	set_nullfield_int2(&tuple[COLUMNS_SCALE], PGTYPE_ATTR_DECIMAL_DIGITS(conn, the_type, atttypmod));
	set_nullfield_int2(&tuple[COLUMNS_RADIX], pgtype_radix(conn, the_type));
	set_tuplefield_int2(&tuple[COLUMNS_NULLABLE], SQL_NO_NULLS);
	set_tuplefield_string(&tuple[COLUMNS_REMARKS], NULL_STRING);
	set_tuplefield_null(&tuple[COLUMNS_COLUMN_DEF]);
	set_tuplefield_int2(&tuple[COLUMNS_SQL_DATA_TYPE], sqltype);
	set_tuplefield_null(&tuple[COLUMNS_SQL_DATETIME_SUB]);
	set_tuplefield_null(&tuple[COLUMNS_CHAR_OCTET_LENGTH]);
	set_tuplefield_int4(&tuple[COLUMNS_ORDINAL_POSITION], ordinal);
	set_tuplefield_string(&tuple[COLUMNS_IS_NULLABLE], "No");
	set_tuplefield_int4(&tuple[COLUMNS_DISPLAY_SIZE], PGTYPE_ATTR_DISPLAY_SIZE(conn, the_type, atttypmod));
	set_tuplefield_int4(&tuple[COLUMNS_FIELD_TYPE], the_type);
	set_tuplefield_int4(&tuple[COLUMNS_AUTO_INCREMENT], auto_increment);
	set_tuplefield_int2(&tuple[COLUMNS_PHYSICAL_NUMBER], attnum);
	set_tuplefield_int4(&tuple[COLUMNS_TABLE_OID], greloid);
	set_tuplefield_int4(&tuple[COLUMNS_BASE_TYPEID], 0);
	set_tuplefield_int4(&tuple[COLUMNS_ATTTYPMOD], -1);
}

RETCODE		SQL_API
PGAPI_Columns(HSTMT hstmt,
			  const SQLCHAR * szTableQualifier, /* OA X*/
			  SQLSMALLINT cbTableQualifier,
			  const SQLCHAR * szTableOwner, /* PV E*/
			  SQLSMALLINT cbTableOwner,
			  const SQLCHAR * szTableName, /* PV E*/
			  SQLSMALLINT cbTableName,
			  const SQLCHAR * szColumnName, /* PV E*/
			  SQLSMALLINT cbColumnName,
			  UWORD	flag,
			  OID	reloid,
			  Int2	attnum)
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
				result_cols;
	Int4		mod_length,
				ordinal,
				typmod, relhasoids;
	OID		field_type, greloid, basetype;
	char		not_null[MAX_INFO_STRING],
				relhasrules[MAX_INFO_STRING], relkind[8];
	char	*escSchemaName = NULL, *escTableName = NULL, *escColumnName = NULL;
	BOOL	search_pattern = TRUE, search_by_ids, relisaview, show_oid_column, row_versioning;
	ConnInfo   *ci;
	ConnectionClass *conn;
	SQLSMALLINT	internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const char	*like_or_eq = likeop, *op_string;
	const SQLCHAR *szSchemaName;
	BOOL	setIdentity = FALSE;

	mylog("%s: entering...stmt=%p scnm=%p len=%d\n", func, stmt, szTableOwner, cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
#ifdef	UNICODE_SUPPORT
	if (CC_is_in_unicode_driver(conn))
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */

#define	return	DONT_CALL_RETURN_FROM_HERE???
	show_oid_column = ((flag & PODBC_SHOW_OID_COLUMN) != 0);
	row_versioning = ((flag & PODBC_ROW_VERSIONING) != 0);
	search_by_ids = ((flag & PODBC_SEARCH_BY_IDS) != 0);
	if (search_by_ids)
	{
		szSchemaName = NULL;
		cbSchemaName = SQL_NULL_DATA;
	}
	else
	{
		szSchemaName = szTableOwner;
		cbSchemaName = cbTableOwner;
		reloid = 0;
		attnum = 0;
		/*
		 *	TableName or ColumnName is ordinarily an pattern value,
		 */
		search_pattern = ((flag & PODBC_NOT_SEARCH_PATTERN) == 0);
		if (search_pattern)
		{
			like_or_eq = likeop;
			escTableName = adjustLikePattern(szTableName, cbTableName, conn);
			escColumnName = adjustLikePattern(szColumnName, cbColumnName, conn);
		}
		else
		{
			like_or_eq = eqop;
			escTableName = simpleCatalogEscape(szTableName, cbTableName, conn);
			escColumnName = simpleCatalogEscape(szColumnName, cbColumnName, conn);
		}
	}
retry_public_schema:
	if (!search_by_ids)
	{
		if (escSchemaName)
			free(escSchemaName);
		if (search_pattern)
			escSchemaName = adjustLikePattern(szSchemaName, cbSchemaName, conn);
		else
			escSchemaName = simpleCatalogEscape(szSchemaName, cbSchemaName, conn);
	}
	/*
	 * Create the query to find out the columns (Note: pre 6.3 did not
	 * have the atttypmod field)
	 */
	op_string = gen_opestr(like_or_eq, conn);
	SPRINTF_FIXED(columns_query,
		"select n.nspname, c.relname, a.attname, a.atttypid, "
		"t.typname, a.attnum, a.attlen, a.atttypmod, a.attnotnull, "
		"c.relhasrules, c.relkind, c.oid, pg_get_expr(d.adbin, d.adrelid), "
        "case t.typtype when 'd' then t.typbasetype else 0 end, t.typtypmod, "
        "c.relhasoids "
		"from (((pg_catalog.pg_class c "
		"inner join pg_catalog.pg_namespace n on n.oid = c.relnamespace");
	if (search_by_ids)
		SPRINTFCAT_FIXED(columns_query, " and c.oid = %u", reloid);
	else
	{
		if (escTableName)
			SPRINTFCAT_FIXED(columns_query, " and c.relname %s'%s'", op_string, escTableName);
		schema_strcat1(columns_query, sizeof(columns_query), " and n.nspname %s'%.*s'", op_string, escSchemaName, szTableName, cbTableName, conn);
	}
	STRCAT_FIXED(columns_query, ") inner join pg_catalog.pg_attribute a"
		" on (not a.attisdropped)");
	if (0 == attnum && (NULL == escColumnName || like_or_eq != eqop))
		STRCAT_FIXED(columns_query, " and a.attnum > 0");
	if (search_by_ids)
	{
		if (attnum != 0)
			SPRINTFCAT_FIXED(columns_query, " and a.attnum = %d", attnum);
	}
	else if (escColumnName)
		SPRINTFCAT_FIXED(columns_query, " and a.attname %s'%s'", op_string, escColumnName);
	STRCAT_FIXED(columns_query,
		" and a.attrelid = c.oid) inner join pg_catalog.pg_type t"
		" on t.oid = a.atttypid) left outer join pg_attrdef d"
		" on a.atthasdef and d.adrelid = a.attrelid and d.adnum = a.attnum");
	STRCAT_FIXED(columns_query, " order by n.nspname, c.relname, attnum");

	result = PGAPI_AllocStmt(conn, &hcol_stmt, 0);
	if (!SQL_SUCCEEDED(result))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_Columns result.", func);
		result = SQL_ERROR;
		goto cleanup;
	}
	col_stmt = (StatementClass *) hcol_stmt;

	mylog("%s: hcol_stmt = %p, col_stmt = %p\n", func, hcol_stmt, col_stmt);

	result = PGAPI_ExecDirect(hcol_stmt, (SQLCHAR *) columns_query, SQL_NTS, PODBC_RDONLY);
	if (!SQL_SUCCEEDED(result))
	{
		SC_full_error_copy(stmt, col_stmt, FALSE);
		goto cleanup;
	}

	/* If not found */
	if ((flag & PODBC_SEARCH_PUBLIC_SCHEMA) != 0 &&
	    (res = SC_get_Result(col_stmt)) &&
	    0 == QR_get_num_total_tuples(res))
	{
		if (!search_by_ids &&
		    allow_public_schema(conn, szSchemaName, cbSchemaName))
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
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 2, internal_asis_type,
						   table_name, MAX_INFO_STRING, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 3, internal_asis_type,
						   field_name, MAX_INFO_STRING, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 4, SQL_C_ULONG,
						   &field_type, 4, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 5, internal_asis_type,
						   field_type_name, MAX_INFO_STRING, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 6, SQL_C_SHORT,
						   &field_number, MAX_INFO_STRING, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

#ifdef	NOT_USED
	result = PGAPI_BindCol(hcol_stmt, 7, SQL_C_LONG,
						   &field_length, MAX_INFO_STRING, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}
#endif /* NOT_USED */

	result = PGAPI_BindCol(hcol_stmt, 8, SQL_C_LONG,
						   &mod_length, MAX_INFO_STRING, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 9, internal_asis_type,
						   not_null, MAX_INFO_STRING, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 10, internal_asis_type,
						   relhasrules, MAX_INFO_STRING, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 11, internal_asis_type,
						   relkind, sizeof(relkind), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 12, SQL_C_LONG,
					&greloid, sizeof(greloid), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 14, SQL_C_ULONG,
					&basetype, sizeof(basetype), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 15, SQL_C_LONG,
					&typmod, sizeof(typmod), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 16, SQL_C_LONG,
					&relhasoids, sizeof(relhasoids), NULL);
	if (!SQL_SUCCEEDED(result))
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
	result_cols = NUM_OF_COLUMNS_FIELDS;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	/*
	 * Setting catalog_result here affects the behavior of
	 * pgtype_xxx() functions. So set it later.
	 * stmt->catalog_result = TRUE;
	 */
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
	QR_set_field_info_v(res, COLUMNS_REMARKS, "REMARKS", PG_TYPE_VARCHAR, INFO_VARCHAR_SIZE);
	QR_set_field_info_v(res, COLUMNS_COLUMN_DEF, "COLUMN_DEF", PG_TYPE_VARCHAR, INFO_VARCHAR_SIZE);
	QR_set_field_info_v(res, COLUMNS_SQL_DATA_TYPE, "SQL_DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, COLUMNS_SQL_DATETIME_SUB, "SQL_DATETIME_SUB", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, COLUMNS_CHAR_OCTET_LENGTH, "CHAR_OCTET_LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, COLUMNS_ORDINAL_POSITION, "ORDINAL_POSITION", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, COLUMNS_IS_NULLABLE, "IS_NULLABLE", PG_TYPE_VARCHAR, INFO_VARCHAR_SIZE);

	/* User defined fields */
	QR_set_field_info_v(res, COLUMNS_DISPLAY_SIZE, "DISPLAY_SIZE", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, COLUMNS_FIELD_TYPE, "FIELD_TYPE", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, COLUMNS_AUTO_INCREMENT, "AUTO_INCREMENT", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, COLUMNS_PHYSICAL_NUMBER, "PHYSICAL NUMBER", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, COLUMNS_TABLE_OID, "TABLE OID", PG_TYPE_OID, 4);
	QR_set_field_info_v(res, COLUMNS_BASE_TYPEID, "BASE TYPEID", PG_TYPE_OID, 4);
	QR_set_field_info_v(res, COLUMNS_ATTTYPMOD, "TYPMOD", PG_TYPE_INT4, 4);

	ordinal = 1;
	result = PGAPI_Fetch(hcol_stmt);

	/*
	 * Only show oid if option AND there are other columns AND it's not
	 * being called by SQLStatistics . Always show OID if it's a system
	 * table
	 */
	relisaview = (relkind[0] == 'v');

	if (result != SQL_ERROR)
	{
		if (!relisaview &&
			relhasoids &&
			(show_oid_column ||
			 strncmp(table_name, POSTGRES_SYS_PREFIX, strlen(POSTGRES_SYS_PREFIX)) == 0) &&
			(NULL == escColumnName ||
			 0 == strcmp(escColumnName, OID_NAME)))
		{
			const char *typname;

			/* For OID fields */
			tuple = QR_AddNew(res);

			if (CC_fake_mss(conn))
			{
				typname = "OID identity";
				setIdentity = TRUE;
			}
			else
				typname = OID_NAME;
			add_tuple_for_oid_or_xmin(tuple, ordinal, OID_NAME, PG_TYPE_OID, typname, conn, table_owner, table_name, greloid, OID_ATTNUM, TRUE);
			ordinal++;
		}
	}

	while (SQL_SUCCEEDED(result))
	{
		int	auto_unique;
		SQLLEN	len_needed;
		char	*attdef;

		attdef = NULL;
		PGAPI_SetPos(hcol_stmt, 1, SQL_POSITION, 0);
		PGAPI_GetData(hcol_stmt, 13, internal_asis_type, NULL, 0, &len_needed);
		if (len_needed > 0)
		{
mylog("len_needed=%d\n", len_needed);
			attdef = malloc(len_needed + 1);
			if (!attdef)
			{
				SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for attdef.", func);
				goto cleanup;
			}

			PGAPI_GetData(hcol_stmt, 13, internal_asis_type, attdef, len_needed + 1, &len_needed);
mylog(" and the data=%s\n", attdef);
		}
		tuple = QR_AddNew(res);

		sqltype = SQL_TYPE_NULL;	/* unspecified */
		set_tuplefield_string(&tuple[COLUMNS_CATALOG_NAME], CurrCat(conn));
		/* see note in SQLTables() */
		set_tuplefield_string(&tuple[COLUMNS_SCHEMA_NAME], GET_SCHEMA_NAME(table_owner));
		set_tuplefield_string(&tuple[COLUMNS_TABLE_NAME], table_name);
		set_tuplefield_string(&tuple[COLUMNS_COLUMN_NAME], field_name);
		auto_unique = SQL_FALSE;
		if (field_type = pg_true_type(conn, field_type, basetype), field_type == basetype)
			mod_length = typmod;
		switch (field_type)
		{
			case PG_TYPE_OID:
				if (0 != atoi(ci->fake_oid_index))
				{
					auto_unique = SQL_TRUE;
					set_tuplefield_string(&tuple[COLUMNS_TYPE_NAME], "identity");
					break;
				}
			case PG_TYPE_INT4:
			case PG_TYPE_INT8:
				if (attdef && strnicmp(attdef, "nextval(", 8) == 0 &&
				    not_null[0] != '0')
				{
					auto_unique = SQL_TRUE;
					if (!setIdentity &&
					    CC_fake_mss(conn))
					{
						char	tmp[32];

						SPRINTF_FIXED(tmp, "%s identity", field_type_name);
						set_tuplefield_string(&tuple[COLUMNS_TYPE_NAME], tmp);
						break;
					}
				}
			default:
				set_tuplefield_string(&tuple[COLUMNS_TYPE_NAME], field_type_name);
				break;
		}

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

		/* Subtract the header length */
		switch (field_type)
		{
			case PG_TYPE_DATETIME:
			case PG_TYPE_TIMESTAMP_NO_TMZONE:
			case PG_TYPE_TIME:
			case PG_TYPE_TIME_WITH_TMZONE:
			case PG_TYPE_BIT:
				break;
			default:
				if (mod_length >= 4)
					mod_length -= 4;
		}
		set_tuplefield_int4(&tuple[COLUMNS_PRECISION], PGTYPE_ATTR_COLUMN_SIZE(conn, field_type, mod_length));
		set_tuplefield_int4(&tuple[COLUMNS_LENGTH], PGTYPE_ATTR_BUFFER_LENGTH(conn, field_type, mod_length));
		set_tuplefield_int4(&tuple[COLUMNS_DISPLAY_SIZE], PGTYPE_ATTR_DISPLAY_SIZE(conn, field_type, mod_length));
		set_nullfield_int2(&tuple[COLUMNS_SCALE], PGTYPE_ATTR_DECIMAL_DIGITS(conn, field_type, mod_length));

		sqltype = PGTYPE_ATTR_TO_CONCISE_TYPE(conn, field_type, mod_length);
		concise_type = PGTYPE_ATTR_TO_SQLDESCTYPE(conn, field_type, mod_length);

		set_tuplefield_int2(&tuple[COLUMNS_DATA_TYPE], sqltype);

		set_nullfield_int2(&tuple[COLUMNS_RADIX], pgtype_radix(conn, field_type));
		set_tuplefield_int2(&tuple[COLUMNS_NULLABLE], (Int2) (not_null[0] != '0' ? SQL_NO_NULLS : pgtype_nullable(conn, field_type)));
		set_tuplefield_string(&tuple[COLUMNS_REMARKS], NULL_STRING);
		if (attdef && strlen(attdef) > INFO_VARCHAR_SIZE)
			set_tuplefield_string(&tuple[COLUMNS_COLUMN_DEF], "TRUNCATE");
		else
			set_tuplefield_string(&tuple[COLUMNS_COLUMN_DEF], attdef);
		set_tuplefield_int2(&tuple[COLUMNS_SQL_DATA_TYPE], concise_type);
		set_nullfield_int2(&tuple[COLUMNS_SQL_DATETIME_SUB], pgtype_attr_to_datetime_sub(conn, field_type, mod_length));
		set_tuplefield_int4(&tuple[COLUMNS_CHAR_OCTET_LENGTH], PGTYPE_ATTR_TRANSFER_OCTET_LENGTH(conn, field_type, mod_length));
		set_tuplefield_int4(&tuple[COLUMNS_ORDINAL_POSITION], ordinal);
		set_tuplefield_null(&tuple[COLUMNS_IS_NULLABLE]);
		set_tuplefield_int4(&tuple[COLUMNS_FIELD_TYPE], field_type);
		set_tuplefield_int4(&tuple[COLUMNS_AUTO_INCREMENT], auto_unique);
		set_tuplefield_int2(&tuple[COLUMNS_PHYSICAL_NUMBER], field_number);
		set_tuplefield_int4(&tuple[COLUMNS_TABLE_OID], greloid);
		set_tuplefield_int4(&tuple[COLUMNS_BASE_TYPEID], basetype);
		set_tuplefield_int4(&tuple[COLUMNS_ATTTYPMOD], mod_length);
		ordinal++;

		result = PGAPI_Fetch(hcol_stmt);
		if (attdef)
			free(attdef);
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
	if (!relisaview && row_versioning &&
		(NULL == escColumnName ||
		 0 == strcmp(escColumnName, XMIN_NAME)))
	{
		/* For Row Versioning fields */
		tuple = QR_AddNew(res);

		add_tuple_for_oid_or_xmin(tuple, ordinal, XMIN_NAME, PG_TYPE_XID, "xid", conn, table_owner, table_name, greloid, XMIN_ATTNUM, FALSE);
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
	stmt->catalog_result = TRUE;

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	if (escSchemaName)
		free(escSchemaName);
	if (escTableName)
		free(escTableName);
	if (escColumnName)
		free(escColumnName);
	if (hcol_stmt)
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	mylog("%s: EXIT,  stmt=%p\n", func, stmt);
	return result;
}


RETCODE		SQL_API
PGAPI_SpecialColumns(HSTMT hstmt,
					 SQLUSMALLINT fColType,
					 const SQLCHAR * szTableQualifier,
					 SQLSMALLINT cbTableQualifier,
					 const SQLCHAR * szTableOwner, /* OA E*/
					 SQLSMALLINT cbTableOwner,
					 const SQLCHAR * szTableName, /* OA(R) E*/
					 SQLSMALLINT cbTableName,
					 SQLUSMALLINT fScope,
					 SQLUSMALLINT fNullable)
{
	CSTR func = "PGAPI_SpecialColumns";
	TupleField	*tuple;
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	QResultClass	*res;
	HSTMT		hcol_stmt = NULL;
	StatementClass *col_stmt;
	char		columns_query[INFO_INQUIRY_LEN];
	char		*escSchemaName = NULL, *escTableName = NULL;
	RETCODE		result = SQL_SUCCESS;
	char		relhasrules[MAX_INFO_STRING], relkind[8], relhasoids[8];
	BOOL		relisaview;
	SQLSMALLINT	internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const SQLCHAR	*szSchemaName;
	const char *eq_string;
	int		result_cols;

	mylog("%s: entering...stmt=%p scnm=%p len=%d colType=%d scope=%d\n", func, stmt, szTableOwner, cbTableOwner, fColType, fScope);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;
	conn = SC_get_conn(stmt);
#ifdef	UNICODE_SUPPORT
	if (CC_is_in_unicode_driver(conn))
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */

	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

	escTableName = simpleCatalogEscape(szTableName, cbTableName, conn);
	if (!escTableName)
	{
		SC_set_error(stmt, STMT_INVALID_NULL_ARG, "The table name is required", func);
		return SQL_ERROR;
	}
#define	return	DONT_CALL_RETURN_FROM_HERE???

retry_public_schema:
	if (escSchemaName)
		free(escSchemaName);
	escSchemaName = simpleCatalogEscape(szSchemaName, cbSchemaName, conn);
	eq_string = gen_opestr(eqop, conn);
	/*
	 * Create the query to find out if this is a view or not...
	 */
	STRCPY_FIXED(columns_query, "select c.relhasrules, c.relkind, c.relhasoids");
	STRCAT_FIXED(columns_query, " from pg_catalog.pg_namespace u,"
					" pg_catalog.pg_class c where "
					"u.oid = c.relnamespace");

	/* TableName cannot contain a string search pattern */
	if (escTableName)
		SPRINTFCAT_FIXED(columns_query,
					 " and c.relname %s'%s'", eq_string, escTableName);
	/* SchemaName cannot contain a string search pattern */
	schema_strcat1(columns_query, sizeof(columns_query), " and u.nspname %s'%.*s'", eq_string, escSchemaName, szTableName, cbTableName, conn);

	result = PGAPI_AllocStmt(conn, &hcol_stmt, 0);
	if (!SQL_SUCCEEDED(result))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for SQLSpecialColumns result.", func);
		result = SQL_ERROR;
		goto cleanup;
	}
	col_stmt = (StatementClass *) hcol_stmt;

	mylog("%s: hcol_stmt = %p, col_stmt = %p\n", func, hcol_stmt, col_stmt);

	result = PGAPI_ExecDirect(hcol_stmt, (SQLCHAR *) columns_query, SQL_NTS, PODBC_RDONLY);
	if (!SQL_SUCCEEDED(result))
	{
		SC_full_error_copy(stmt, col_stmt, FALSE);
		result = SQL_ERROR;
		goto cleanup;
	}

	/* If not found */
	if ((res = SC_get_Result(col_stmt)) &&
	    0 == QR_get_num_total_tuples(res))
	{
		if (allow_public_schema(conn, szSchemaName, cbSchemaName))
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
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		result = SQL_ERROR;
		goto cleanup;
	}

	result = PGAPI_BindCol(hcol_stmt, 2, internal_asis_type,
					relkind, sizeof(relkind), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		result = SQL_ERROR;
		goto cleanup;
	}
	relhasoids[0] = '1';
	result = PGAPI_BindCol(hcol_stmt, 3, internal_asis_type,
				relhasoids, sizeof(relhasoids), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		result = SQL_ERROR;
		goto cleanup;
	}

	result = PGAPI_Fetch(hcol_stmt);
	relisaview = (relkind[0] == 'v');
	PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	hcol_stmt = NULL;

	res = QR_Constructor();
	if (!res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for query.", func);
		result = SQL_ERROR;
		goto cleanup;
	}
	SC_set_Result(stmt, res);
	extend_column_bindings(SC_get_ARDF(stmt), 8);

	stmt->catalog_result = TRUE;
	result_cols = NUM_OF_SPECOLS_FIELDS;
	QR_set_num_fields(res, result_cols);
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
			goto cleanup;
		}
		else if (fColType == SQL_ROWVER)
		{
			Int2		the_type = PG_TYPE_TID;
			int	atttypmod = -1;

			tuple = QR_AddNew(res);

			set_tuplefield_null(&tuple[SPECOLS_SCOPE]);
			set_tuplefield_string(&tuple[SPECOLS_COLUMN_NAME], "ctid");
			set_tuplefield_int2(&tuple[SPECOLS_DATA_TYPE], PGTYPE_ATTR_TO_CONCISE_TYPE(conn, the_type, atttypmod));
			set_tuplefield_string(&tuple[SPECOLS_TYPE_NAME], pgtype_attr_to_name(conn, the_type, atttypmod, FALSE));
			set_tuplefield_int4(&tuple[SPECOLS_COLUMN_SIZE], PGTYPE_ATTR_COLUMN_SIZE(conn, the_type, atttypmod));
			set_tuplefield_int4(&tuple[SPECOLS_BUFFER_LENGTH], PGTYPE_ATTR_BUFFER_LENGTH(conn, the_type, atttypmod));
			set_tuplefield_int2(&tuple[SPECOLS_DECIMAL_DIGITS], PGTYPE_ATTR_DECIMAL_DIGITS(conn, the_type, atttypmod));
			set_tuplefield_int2(&tuple[SPECOLS_PSEUDO_COLUMN], SQL_PC_NOT_PSEUDO);
inolog("Add ctid\n");
		}
	}
	else
	{
		/* use the oid value for the rowid */
		if (fColType == SQL_BEST_ROWID)
		{
			Int2	the_type = PG_TYPE_OID;
			int	atttypmod = -1;

			if (relhasoids[0] != '1')
			{
				goto cleanup;
			}
			tuple = QR_AddNew(res);

			set_tuplefield_int2(&tuple[SPECOLS_SCOPE], SQL_SCOPE_SESSION);
			set_tuplefield_string(&tuple[SPECOLS_COLUMN_NAME], OID_NAME);
			set_tuplefield_int2(&tuple[SPECOLS_DATA_TYPE], PGTYPE_ATTR_TO_CONCISE_TYPE(conn, the_type, atttypmod));
			set_tuplefield_string(&tuple[SPECOLS_TYPE_NAME], pgtype_attr_to_name(conn, the_type, atttypmod, TRUE));
			set_tuplefield_int4(&tuple[SPECOLS_COLUMN_SIZE], PGTYPE_ATTR_COLUMN_SIZE(conn, the_type, atttypmod));
			set_tuplefield_int4(&tuple[SPECOLS_BUFFER_LENGTH], PGTYPE_ATTR_BUFFER_LENGTH(conn, the_type, atttypmod));
			set_tuplefield_int2(&tuple[SPECOLS_DECIMAL_DIGITS], PGTYPE_ATTR_DECIMAL_DIGITS(conn, the_type, atttypmod));
			set_tuplefield_int2(&tuple[SPECOLS_PSEUDO_COLUMN], SQL_PC_PSEUDO);
		}
		else if (fColType == SQL_ROWVER)
		{
			Int2		the_type = PG_TYPE_XID;
			int	atttypmod = -1;

			tuple = QR_AddNew(res);

			set_tuplefield_null(&tuple[SPECOLS_SCOPE]);
			set_tuplefield_string(&tuple[SPECOLS_COLUMN_NAME], XMIN_NAME);
			set_tuplefield_int2(&tuple[SPECOLS_DATA_TYPE], PGTYPE_ATTR_TO_CONCISE_TYPE(conn, the_type, atttypmod));
			set_tuplefield_string(&tuple[SPECOLS_TYPE_NAME], pgtype_attr_to_name(conn, the_type, atttypmod, FALSE));
			set_tuplefield_int4(&tuple[SPECOLS_COLUMN_SIZE], PGTYPE_ATTR_COLUMN_SIZE(conn, the_type, atttypmod));
			set_tuplefield_int4(&tuple[SPECOLS_BUFFER_LENGTH], PGTYPE_ATTR_BUFFER_LENGTH(conn, the_type, atttypmod));
			set_tuplefield_int2(&tuple[SPECOLS_DECIMAL_DIGITS], PGTYPE_ATTR_DECIMAL_DIGITS(conn, the_type, atttypmod));
			set_tuplefield_int2(&tuple[SPECOLS_PSEUDO_COLUMN], SQL_PC_PSEUDO);
		}
	}

cleanup:
#undef	return
	if (escSchemaName)
		free(escSchemaName);
	if (escTableName)
		free(escTableName);
	stmt->status = STMT_FINISHED;
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);
	if (hcol_stmt)
		PGAPI_FreeStmt(hcol_stmt, SQL_DROP);
	mylog("%s: EXIT,  stmt=%p\n", func, stmt);
	return result;
}


#define INDOPTION_DESC		0x0001	/* values are in reverse order */
RETCODE		SQL_API
PGAPI_Statistics(HSTMT hstmt,
				 const SQLCHAR * szTableQualifier, /* OA X*/
				 SQLSMALLINT cbTableQualifier,
				 const SQLCHAR * szTableOwner, /* OA E*/
				 SQLSMALLINT cbTableOwner,
				 const SQLCHAR * szTableName, /* OA(R) E*/
				 SQLSMALLINT cbTableName,
				 SQLUSMALLINT fUnique,
				 SQLUSMALLINT fAccuracy)
{
	CSTR func = "PGAPI_Statistics";
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn;
	QResultClass	*res;
	char		index_query[INFO_INQUIRY_LEN];
	HSTMT		hcol_stmt = NULL, hindx_stmt = NULL;
	RETCODE		ret = SQL_ERROR, result;
	char		*escSchemaName = NULL, *table_name = NULL, *escTableName = NULL;
	char		index_name[MAX_INFO_STRING];
	short		fields_vector[INDEX_KEYS_STORAGE_COUNT + 1];
	short		indopt_vector[INDEX_KEYS_STORAGE_COUNT + 1];
	char		isunique[10],
				isclustered[10],
				ishash[MAX_INFO_STRING];
	SQLLEN		index_name_len, fields_vector_len;
	TupleField	*tuple;
	int			i;
	StatementClass *col_stmt,
			   *indx_stmt;
	char		column_name[MAX_INFO_STRING],
			table_schemaname[MAX_INFO_STRING],
				relhasrules[10];
	struct columns_idx {
		int	pnum;
		char	*col_name;
	} *column_names = NULL;
	/* char	  **column_names = NULL; */
	SQLLEN		column_name_len;
	int		total_columns = 0, alcount;
	ConnInfo   *ci;
	char		buf[256];
	SQLSMALLINT	internal_asis_type = SQL_C_CHAR, cbSchemaName, field_number;
	const SQLCHAR *szSchemaName;
	const char *eq_string;
	OID		ioid;
	Int4		relhasoids;

	mylog("%s: entering...stmt=%p scnm=%p len=%d\n", func, stmt, szTableOwner, cbTableOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	table_name = make_string(szTableName, cbTableName, NULL, 0);
	if (!table_name)
	{
		SC_set_error(stmt, STMT_INVALID_NULL_ARG, "The table name is required", func);
		return result;
	}
	conn = SC_get_conn(stmt);
	ci = &(conn->connInfo);
#ifdef	UNICODE_SUPPORT
	if (CC_is_in_unicode_driver(conn))
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */

	if (res = QR_Constructor(), !res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for PGAPI_Statistics result.", func);
		free(table_name);
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
	QR_set_num_fields(res, NUM_OF_STATS_FIELDS);
	QR_set_field_info_v(res, STATS_CATALOG_NAME, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, STATS_SCHEMA_NAME, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, STATS_TABLE_NAME, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, STATS_NON_UNIQUE, "NON_UNIQUE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, STATS_INDEX_QUALIFIER, "INDEX_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, STATS_INDEX_NAME, "INDEX_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, STATS_TYPE, "TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, STATS_SEQ_IN_INDEX, "SEQ_IN_INDEX", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, STATS_COLUMN_NAME, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, STATS_COLLATION, "COLLATION", PG_TYPE_CHAR, 1);
	QR_set_field_info_v(res, STATS_CARDINALITY, "CARDINALITY", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, STATS_PAGES, "PAGES", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, STATS_FILTER_CONDITION, "FILTER_CONDITION", PG_TYPE_VARCHAR, MAX_INFO_STRING);

#define	return	DONT_CALL_RETURN_FROM_HERE???
	szSchemaName = szTableOwner;
	cbSchemaName = cbTableOwner;

	table_schemaname[0] = '\0';
	schema_strcat(table_schemaname, sizeof(table_schemaname), "%.*s", szSchemaName, cbSchemaName, szTableName, cbTableName, conn);

	/*
	 * we need to get a list of the field names first, so we can return
	 * them later.
	 */
	result = PGAPI_AllocStmt(conn, &hcol_stmt, 0);
	if (!SQL_SUCCEEDED(result))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in PGAPI_Statistics for columns.", func);
		goto cleanup;
	}

	col_stmt = (StatementClass *) hcol_stmt;

	/*
	 * table_name parameter cannot contain a string search pattern.
	 */
	result = PGAPI_Columns(hcol_stmt,
						   NULL, 0,
						   (SQLCHAR *) table_schemaname, SQL_NTS,
						   (SQLCHAR *) table_name, SQL_NTS,
						   NULL, 0,
						   PODBC_NOT_SEARCH_PATTERN | PODBC_SEARCH_PUBLIC_SCHEMA, 0, 0);

	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}
	result = PGAPI_BindCol(hcol_stmt, COLUMNS_COLUMN_NAME + 1, internal_asis_type,
						 column_name, sizeof(column_name), &column_name_len);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}
	result = PGAPI_BindCol(hcol_stmt, COLUMNS_PHYSICAL_NUMBER + 1, SQL_C_SHORT,
			&field_number, sizeof(field_number), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, col_stmt, TRUE);
		goto cleanup;
	}

	alcount = 0;
	result = PGAPI_Fetch(hcol_stmt);
	while (SQL_SUCCEEDED(result))
	{
		if (0 == total_columns)
			PGAPI_GetData(hcol_stmt, 2, internal_asis_type, table_schemaname, sizeof(table_schemaname), NULL);

		if (total_columns >= alcount)
		{
			if (0 == alcount)
				alcount = 4;
			else
				alcount *= 2;
			SC_REALLOC_gexit_with_error(column_names, struct columns_idx, alcount * sizeof(struct columns_idx), stmt, "Couldn't allocate memory for column names.", (result = SQL_ERROR));
		}
		column_names[total_columns].col_name = strdup(column_name);
		if (!column_names[total_columns].col_name)
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for column name.", func);
			result = SQL_ERROR;
			goto cleanup;
		}
		column_names[total_columns].pnum = field_number;
		total_columns++;

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
	result = PGAPI_AllocStmt(conn, &hindx_stmt, 0);
	if (!SQL_SUCCEEDED(result))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "PGAPI_AllocStmt failed in SQLStatistics for indices.", func);
		goto cleanup;

	}
	indx_stmt = (StatementClass *) hindx_stmt;

	/* TableName cannot contain a string search pattern */
	escTableName = simpleCatalogEscape((SQLCHAR *) table_name, SQL_NTS, conn);
	eq_string = gen_opestr(eqop, conn);
	escSchemaName = simpleCatalogEscape((SQLCHAR *) table_schemaname, SQL_NTS, conn);
	SPRINTF_FIXED(index_query, "select c.relname, i.indkey, i.indisunique"
		", i.indisclustered, a.amname, c.relhasrules, n.nspname"
		", c.oid, d.relhasoids, %s"
		" from pg_catalog.pg_index i, pg_catalog.pg_class c,"
		" pg_catalog.pg_class d, pg_catalog.pg_am a,"
		" pg_catalog.pg_namespace n"
		" where d.relname %s'%s'"
		" and n.nspname %s'%s'"
		" and n.oid = d.relnamespace"
		" and d.oid = i.indrelid"
		" and i.indexrelid = c.oid"
		" and c.relam = a.oid order by"
		, PG_VERSION_GE(conn, 8.3) ? "i.indoption" : "0"
		, eq_string, escTableName, eq_string, escSchemaName);
	STRCAT_FIXED(index_query, " i.indisprimary desc,");
	STRCAT_FIXED(index_query, " i.indisunique, n.nspname, c.relname");

	result = PGAPI_ExecDirect(hindx_stmt, (SQLCHAR *) index_query, SQL_NTS, PODBC_RDONLY);
	if (!SQL_SUCCEEDED(result))
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
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);	/* "Couldn't bind column
						* in SQLStatistics."; */
		goto cleanup;

	}
	/* bind the vector column */
	result = PGAPI_BindCol(hindx_stmt, 2, SQL_C_DEFAULT,
			fields_vector, sizeof(fields_vector), &fields_vector_len);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, indx_stmt, TRUE); /* "Couldn't bind column
						 * in SQLStatistics."; */
		goto cleanup;

	}
	/* bind the "is unique" column */
	result = PGAPI_BindCol(hindx_stmt, 3, internal_asis_type,
						   isunique, sizeof(isunique), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);	/* "Couldn't bind column
						 * in SQLStatistics."; */
		goto cleanup;
	}

	/* bind the "is clustered" column */
	result = PGAPI_BindCol(hindx_stmt, 4, internal_asis_type,
						   isclustered, sizeof(isclustered), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);	/* "Couldn't bind column *
						 * in SQLStatistics."; */
		goto cleanup;

	}

	/* bind the "is hash" column */
	result = PGAPI_BindCol(hindx_stmt, 5, internal_asis_type,
						   ishash, sizeof(ishash), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);	/* "Couldn't bind column *
						 * in SQLStatistics."; */
		goto cleanup;

	}

	result = PGAPI_BindCol(hindx_stmt, 6, internal_asis_type,
					relhasrules, sizeof(relhasrules), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hindx_stmt, 8, SQL_C_ULONG,
					&ioid, sizeof(ioid), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);
		goto cleanup;
	}

	result = PGAPI_BindCol(hindx_stmt, 9, SQL_C_ULONG,
					&relhasoids, sizeof(relhasoids), NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, indx_stmt, TRUE);
		goto cleanup;
	}

	/* bind the vector column */
	result = PGAPI_BindCol(hindx_stmt, 10, SQL_C_DEFAULT,
			indopt_vector, sizeof(fields_vector), &fields_vector_len);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, indx_stmt, TRUE); /* "Couldn't bind column
						 * in SQLStatistics."; */
		goto cleanup;

	}

	relhasrules[0] = '0';
	result = PGAPI_Fetch(hindx_stmt);
	/* fake index of OID */
	if (relhasoids && relhasrules[0] != '1' && atoi(ci->show_oid_column) && atoi(ci->fake_oid_index))
	{
		tuple = QR_AddNew(res);

		/* no table qualifier */
		set_tuplefield_string(&tuple[STATS_CATALOG_NAME], CurrCat(conn));
		/* don't set the table owner, else Access tries to use it */
		set_tuplefield_string(&tuple[STATS_SCHEMA_NAME], GET_SCHEMA_NAME(table_schemaname));
		set_tuplefield_string(&tuple[STATS_TABLE_NAME], table_name);

		/* non-unique index? */
		set_tuplefield_int2(&tuple[STATS_NON_UNIQUE], (Int2) (ci->drivers.unique_index ? FALSE : TRUE));

		/* no index qualifier */
		set_tuplefield_string(&tuple[STATS_INDEX_QUALIFIER], GET_SCHEMA_NAME(table_schemaname));

		SPRINTF_FIXED(buf, "%s_idx_fake_oid", table_name);
		set_tuplefield_string(&tuple[STATS_INDEX_NAME], buf);

		/*
		 * Clustered/HASH index?
		 */
		set_tuplefield_int2(&tuple[STATS_TYPE], (Int2) SQL_INDEX_OTHER);
		set_tuplefield_int2(&tuple[STATS_SEQ_IN_INDEX], (Int2) 1);

		set_tuplefield_string(&tuple[STATS_COLUMN_NAME], OID_NAME);
		set_tuplefield_string(&tuple[STATS_COLLATION], "A");
		set_tuplefield_null(&tuple[STATS_CARDINALITY]);
		set_tuplefield_null(&tuple[STATS_PAGES]);
		set_tuplefield_null(&tuple[STATS_FILTER_CONDITION]);
	}

	while (SQL_SUCCEEDED(result))
	{
		/* If only requesting unique indexs, then just return those. */
		if (fUnique == SQL_INDEX_ALL ||
			(fUnique == SQL_INDEX_UNIQUE && atoi(isunique)))
		{
			int	colcnt, attnum;

			/* add a row in this table for each field in the index */
			colcnt = fields_vector[0];
			for (i = 1; i <= colcnt; i++)
			{
				tuple = QR_AddNew(res);

				/* no table qualifier */
				set_tuplefield_string(&tuple[STATS_CATALOG_NAME], CurrCat(conn));
				/* don't set the table owner, else Access tries to use it */
				set_tuplefield_string(&tuple[STATS_SCHEMA_NAME], GET_SCHEMA_NAME(table_schemaname));
				set_tuplefield_string(&tuple[STATS_TABLE_NAME], table_name);

				/* non-unique index? */
				if (ci->drivers.unique_index)
					set_tuplefield_int2(&tuple[STATS_NON_UNIQUE], (Int2) (atoi(isunique) ? FALSE : TRUE));
				else
					set_tuplefield_int2(&tuple[STATS_NON_UNIQUE], TRUE);

				/* no index qualifier */
				set_tuplefield_string(&tuple[STATS_INDEX_QUALIFIER], GET_SCHEMA_NAME(table_schemaname));
				set_tuplefield_string(&tuple[STATS_INDEX_NAME], index_name);

				/*
				 * Clustered/HASH index?
				 */
				set_tuplefield_int2(&tuple[STATS_TYPE], (Int2)
							   (atoi(isclustered) ? SQL_INDEX_CLUSTERED :
								(!strncmp(ishash, "hash", 4)) ? SQL_INDEX_HASHED : SQL_INDEX_OTHER));
				set_tuplefield_int2(&tuple[STATS_SEQ_IN_INDEX], (Int2) i);

				attnum = fields_vector[i];
				if (OID_ATTNUM == attnum)
				{
					set_tuplefield_string(&tuple[STATS_COLUMN_NAME], OID_NAME);
					mylog("%s: column name = oid\n", func);
				}
				else if (0 == attnum)
				{
					char	cmd[64];

					QResultClass *res;

					SPRINTF_FIXED(cmd, "select pg_get_indexdef(%u, %d, true)", ioid, i);
					res = CC_send_query(conn, cmd, NULL, READ_ONLY_QUERY, stmt);
					if (QR_command_maybe_successful(res))
						set_tuplefield_string(&tuple[STATS_COLUMN_NAME], QR_get_value_backend_text(res, 0, 0));
					QR_Destructor(res);
				}
				else
				{
					int j, matchidx;
					BOOL	unknownf = TRUE;

					if (attnum > 0)
					{
						for (j = 0; j < total_columns; j++)
						{
							if (attnum == column_names[j].pnum)
							{
								matchidx = j;
								unknownf = FALSE;
								break;
							}
						}
					}
					if (unknownf)
					{
						set_tuplefield_string(&tuple[STATS_COLUMN_NAME], "UNKNOWN");
						mylog("%s: column name = UNKNOWN\n", func);
					}
					else
					{
						set_tuplefield_string(&tuple[STATS_COLUMN_NAME], column_names[matchidx].col_name);
						mylog("%s: column name = '%s'\n", func, column_names[matchidx].col_name);
					}
				}

				if (i <= indopt_vector[0] &&
				    (indopt_vector[i] & INDOPTION_DESC) != 0)
					set_tuplefield_string(&tuple[STATS_COLLATION], "D");
				else
					set_tuplefield_string(&tuple[STATS_COLLATION], "A");
				set_tuplefield_null(&tuple[STATS_CARDINALITY]);
				set_tuplefield_null(&tuple[STATS_PAGES]);
				set_tuplefield_null(&tuple[STATS_FILTER_CONDITION]);
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
	if (escSchemaName)
		free(escSchemaName);
	if (column_names)
	{
		for (i = 0; i < total_columns; i++)
			free(column_names[i].col_name);
		free(column_names);
	}

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	mylog("%s: EXIT, stmt=%p, ret=%d\n", func, stmt, ret);

	return ret;
}


RETCODE		SQL_API
PGAPI_ColumnPrivileges(HSTMT hstmt,
					   const SQLCHAR * szTableQualifier, /* OA X*/
					   SQLSMALLINT cbTableQualifier,
					   const SQLCHAR * szTableOwner, /* OA E*/
					   SQLSMALLINT cbTableOwner,
					   const SQLCHAR * szTableName, /* OA(R) E*/
					   SQLSMALLINT cbTableName,
					   const SQLCHAR * szColumnName, /* PV E*/
					   SQLSMALLINT cbColumnName,
					   UWORD flag)
{
	CSTR func = "PGAPI_ColumnPrivileges";
	StatementClass	*stmt = (StatementClass *) hstmt;
	ConnectionClass	*conn = SC_get_conn(stmt);
	RETCODE	result = SQL_ERROR;
	char	*escSchemaName = NULL, *escTableName = NULL, *escColumnName = NULL;
	const char	*like_or_eq, *op_string, *eq_string;
	char	column_query[INFO_INQUIRY_LEN];
	size_t		cq_len,cq_size;
	char		*col_query;
	BOOL	search_pattern;
	QResultClass	*res = NULL;

	mylog("%s: entering...\n", func);

	/* Neither Access or Borland care about this. */

	if (SC_initialize_and_recycle(stmt) != SQL_SUCCESS)
		return SQL_ERROR;
	escSchemaName = simpleCatalogEscape(szTableOwner, cbTableOwner, conn);
	escTableName = simpleCatalogEscape(szTableName, cbTableName, conn);
	search_pattern = (0 == (flag & PODBC_NOT_SEARCH_PATTERN));
	if (search_pattern)
	{
		like_or_eq = likeop;
		escColumnName = adjustLikePattern(szColumnName, cbColumnName, conn);
	}
	else
	{
		like_or_eq = eqop;
		escColumnName = simpleCatalogEscape(szColumnName, cbColumnName, conn);
	}
	STRCPY_FIXED(column_query, "select '' as TABLE_CAT, table_schema as TABLE_SCHEM,"
			" table_name, column_name, grantor, grantee,"
			" privilege_type as PRIVILEGE, is_grantable from"
			" information_schema.column_privileges where true");
	cq_len = strlen(column_query);
	cq_size = sizeof(column_query);
	col_query = column_query;
	op_string = gen_opestr(like_or_eq, conn);
	eq_string = gen_opestr(eqop, conn);
	if (escSchemaName)
	{
		col_query += cq_len;
		cq_size -= cq_len;
		cq_len = snprintf_len(col_query, cq_size,
			" and table_schem %s'%s'", eq_string, escSchemaName);
	}
	if (escTableName)
	{
		col_query += cq_len;
		cq_size -= cq_len;
		cq_len += snprintf_len(col_query, cq_size,
			" and table_name %s'%s'", eq_string, escTableName);
	}
	if (escColumnName)
	{
		col_query += cq_len;
		cq_size -= cq_len;
		cq_len += snprintf_len(col_query, cq_size,
			" and column_name %s'%s'", op_string, escColumnName);
	}
	if (res = CC_send_query(conn, column_query, NULL, READ_ONLY_QUERY, stmt), !QR_command_maybe_successful(res))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_ColumnPrivileges query error", func);
		goto cleanup;
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
	if (!SQL_SUCCEEDED(result))
		QR_Destructor(res);
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
PGAPI_PrimaryKeys(HSTMT hstmt,
				  const SQLCHAR * szTableQualifier, /* OA X*/
				  SQLSMALLINT cbTableQualifier,
				  const SQLCHAR * szTableOwner, /* OA E*/
				  SQLSMALLINT cbTableOwner,
				  const SQLCHAR * szTableName, /* OA(R) E*/
				  SQLSMALLINT cbTableName,
				  OID	reloid)
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
	SQLLEN		attname_len;
	char		*pktab = NULL, *pktbname;
	char		pkscm[SCHEMA_NAME_STORAGE_LEN + 1];
	SQLLEN		pkscm_len;
	char		tabname[TABLE_NAME_STORAGE_LEN + 1];
	SQLLEN		tabname_len;
	char		pkname[TABLE_NAME_STORAGE_LEN + 1];
	Int2		result_cols;
	int			qno,
				qstart,
				qend;
	SQLSMALLINT	internal_asis_type = SQL_C_CHAR, cbSchemaName;
	const SQLCHAR *szSchemaName;
	const char *eq_string;
	char	*escSchemaName = NULL, *escTableName = NULL;

	mylog("%s: entering...stmt=%p scnm=%p len=%d\n", func, stmt, szTableOwner, cbTableOwner);

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
	result_cols = NUM_OF_PKS_FIELDS;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info_v(res, PKS_TABLE_CAT, "TABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PKS_TABLE_SCHEM, "TABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PKS_TABLE_NAME, "TABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PKS_COLUMN_NAME, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PKS_KEY_SQ, "KEY_SEQ", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, PKS_PK_NAME, "PK_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);

	conn = SC_get_conn(stmt);
	result = PGAPI_AllocStmt(conn, &htbl_stmt, 0);
	if (!SQL_SUCCEEDED(result))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for Primary Key result.", func);
		ret = SQL_ERROR;
		goto cleanup;
	}
	tbl_stmt = (StatementClass *) htbl_stmt;

#ifdef	UNICODE_SUPPORT
	if (CC_is_in_unicode_driver(conn))
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */

#define	return	DONT_CALL_RETURN_FROM_HERE???
	if (0 != reloid)
	{
		szSchemaName = NULL;
		cbSchemaName = SQL_NULL_DATA;
	}
	else
	{
		pktab = make_string(szTableName, cbTableName, NULL, 0);
		if (!pktab || pktab[0] == '\0')
		{
			SC_set_error(stmt, STMT_INTERNAL_ERROR, "No Table specified to PGAPI_PrimaryKeys.", func);
			ret = SQL_ERROR;
			goto cleanup;
		}
		szSchemaName = szTableOwner;
		cbSchemaName = cbTableOwner;
		escTableName = simpleCatalogEscape(szTableName, cbTableName, conn);
	}
	eq_string = gen_opestr(eqop, conn);

retry_public_schema:
	pkscm[0] = '\0';
	if (0 == reloid)
	{
		if (escSchemaName)
			free(escSchemaName);
		escSchemaName = simpleCatalogEscape(szSchemaName, cbSchemaName, conn);
		schema_strcat(pkscm, sizeof(pkscm), "%.*s", (SQLCHAR *) escSchemaName, SQL_NTS, szTableName, cbTableName, conn);
	}

	result = PGAPI_BindCol(htbl_stmt, 1, internal_asis_type,
						   attname, MAX_INFO_STRING, &attname_len);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		ret = SQL_ERROR;
		goto cleanup;
	}
	result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
			pkname, TABLE_NAME_STORAGE_LEN, NULL);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		ret = SQL_ERROR;
		goto cleanup;
	}
	result = PGAPI_BindCol(htbl_stmt, 4, internal_asis_type,
			pkscm, SCHEMA_NAME_STORAGE_LEN, &pkscm_len);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		ret = SQL_ERROR;
		goto cleanup;
	}
	result = PGAPI_BindCol(htbl_stmt, 5, internal_asis_type,
			tabname, TABLE_NAME_STORAGE_LEN, &tabname_len);
	if (!SQL_SUCCEEDED(result))
	{
		SC_error_copy(stmt, tbl_stmt, TRUE);
		ret = SQL_ERROR;
		goto cleanup;
	}

	qstart = 1;
	if (0 == reloid)
		qend = 2;
	else
		qend = 1;
	for (qno = qstart; qno <= qend; qno++)
	{
		size_t	qsize, tsize;
		char	*tbqry;

		switch (qno)
		{
			case 1:

				/*
				 * Simplified query to remove assumptions about number of
				 * possible index columns. Courtesy of Tom Lane - thomas
				 * 2000-03-21
				 */
				STRCPY_FIXED(tables_query,
					"select ta.attname, ia.attnum, ic.relname, n.nspname, tc.relname"
					" from pg_catalog.pg_attribute ta,"
					" pg_catalog.pg_attribute ia, pg_catalog.pg_class tc,"
					" pg_catalog.pg_index i, pg_catalog.pg_namespace n"
					", pg_catalog.pg_class ic");
				qsize = strlen(tables_query);
				tsize = sizeof(tables_query) - qsize;
				tbqry = tables_query + qsize;
				if (0 == reloid)
					snprintf(tbqry, tsize,
					" where tc.relname %s'%s'"
					" AND n.nspname %s'%s'"
					, eq_string, escTableName, eq_string, pkscm);
				else
					snprintf(tbqry, tsize, " where tc.oid = %u", reloid);

				STRCAT_FIXED(tables_query,
					" AND tc.oid = i.indrelid"
					" AND n.oid = tc.relnamespace"
					" AND i.indisprimary = 't'"
					" AND ia.attrelid = i.indexrelid"
					" AND ta.attrelid = i.indrelid"
					" AND ta.attnum = i.indkey[ia.attnum-1]"
					" AND (NOT ta.attisdropped)"
					" AND (NOT ia.attisdropped)"
					" AND ic.oid = i.indexrelid"
					" order by ia.attnum");
				break;
			case 2:

				/*
				 * Simplified query to search old fashoned primary key
				 */
				SPRINTF_FIXED(tables_query, "select ta.attname, ia.attnum, ic.relname, n.nspname, NULL"
					" from pg_catalog.pg_attribute ta,"
					" pg_catalog.pg_attribute ia, pg_catalog.pg_class ic,"
					" pg_catalog.pg_index i, pg_catalog.pg_namespace n"
					" where ic.relname %s'%s_pkey'"
					" AND n.nspname %s'%s'"
					" AND ic.oid = i.indexrelid"
					" AND n.oid = ic.relnamespace"
					" AND ia.attrelid = i.indexrelid"
					" AND ta.attrelid = i.indrelid"
					" AND ta.attnum = i.indkey[ia.attnum-1]"
					" AND (NOT ta.attisdropped)"
					" AND (NOT ia.attisdropped)"
					" order by ia.attnum", eq_string, escTableName, eq_string, pkscm);
				break;
		}
		mylog("%s: tables_query='%s'\n", func, tables_query);

		result = PGAPI_ExecDirect(htbl_stmt, (SQLCHAR *) tables_query, SQL_NTS, PODBC_RDONLY);
		if (!SQL_SUCCEEDED(result))
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
	if (SQL_NO_DATA_FOUND == result)
	{
		if (0 == reloid &&
		    allow_public_schema(conn, szSchemaName, cbSchemaName))
		{
			szSchemaName = pubstr;
			cbSchemaName = SQL_NTS;
			goto retry_public_schema;
		}
	}

	while (SQL_SUCCEEDED(result))
	{
		tuple = QR_AddNew(res);

		set_tuplefield_string(&tuple[PKS_TABLE_CAT], CurrCat(conn));

		/*
		 * I have to hide the table owner from Access, otherwise it
		 * insists on referring to the table as 'owner.table'. (this is
		 * valid according to the ODBC SQL grammar, but Postgres won't
		 * support it.)
		 */
		if (SQL_NULL_DATA == pkscm_len)
			pkscm[0] = '\0';
		set_tuplefield_string(&tuple[PKS_TABLE_SCHEM], GET_SCHEMA_NAME(pkscm));
		if (SQL_NULL_DATA == tabname_len)
			tabname[0] = '\0';
		pktbname = pktab ? pktab : tabname;
		set_tuplefield_string(&tuple[PKS_TABLE_NAME], pktbname);
		set_tuplefield_string(&tuple[PKS_COLUMN_NAME], attname);
		set_tuplefield_int2(&tuple[PKS_KEY_SQ], (Int2) (++seq));
		set_tuplefield_string(&tuple[PKS_PK_NAME], pkname);

		mylog(">> primaryKeys: schema ='%s', pktab = '%s', attname = '%s', seq = %d\n", pkscm, pktbname, attname, seq);

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
	if (escSchemaName)
		free(escSchemaName);
	if (escTableName)
		free(escTableName);
	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	mylog("%s: EXIT, stmt=%p, ret=%d\n", func, stmt, ret);
	return ret;
}


/*
 *	Multibyte support stuff for SQLForeignKeys().
 *	There may be much more effective way in the
 *	future version. The way is very forcible currently.
 */
static BOOL
isMultibyte(const char *str)
{
	for (; *str; str++)
	{
		if ((unsigned char) *str >= 0x80)
			return TRUE;
	}
	return FALSE;
}
static char *
getClientColumnName(ConnectionClass *conn, UInt4 relid, char *serverColumnName, BOOL *nameAlloced)
{
	char		query[1024], saveattnum[16],
			   *ret = serverColumnName;
	const char *eq_string;
	BOOL		continueExec = TRUE,
				bError = FALSE;
	QResultClass *res = NULL;
	UWORD	flag = READ_ONLY_QUERY;

	*nameAlloced = FALSE;
	if (!conn->original_client_encoding || !isMultibyte(serverColumnName))
		return ret;
	if (!conn->server_encoding)
	{
		if (res = CC_send_query(conn, "select getdatabaseencoding()", NULL, flag, NULL), QR_command_maybe_successful(res))
		{
			if (QR_get_num_cached_tuples(res) > 0)
				conn->server_encoding = strdup(QR_get_value_backend_text(res, 0, 0));
		}
		QR_Destructor(res);
		res = NULL;
	}
	if (!conn->server_encoding)
		return ret;
	SPRINTF_FIXED(query, "SET CLIENT_ENCODING TO '%s'", conn->server_encoding);
	bError = (!QR_command_maybe_successful((res = CC_send_query(conn, query, NULL, flag, NULL))));
	QR_Destructor(res);
	eq_string = gen_opestr(eqop, conn);
	if (!bError && continueExec)
	{
		SPRINTF_FIXED(query, "select attnum from pg_attribute "
			"where attrelid = %u and attname %s'%s'",
			relid, eq_string, serverColumnName);
		if (res = CC_send_query(conn, query, NULL, flag, NULL), QR_command_maybe_successful(res))
		{
			if (QR_get_num_cached_tuples(res) > 0)
			{
				STRCPY_FIXED(saveattnum, QR_get_value_backend_text(res, 0, 0));
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
	SPRINTF_FIXED(query, "SET CLIENT_ENCODING TO '%s'", conn->original_client_encoding);
	bError = (!QR_command_maybe_successful((res = CC_send_query(conn, query, NULL, flag, NULL))));
	QR_Destructor(res);
	if (bError || !continueExec)
		return ret;
	SPRINTF_FIXED(query, "select attname from pg_attribute where attrelid = %u and attnum = %s", relid, saveattnum);
	if (res = CC_send_query(conn, query, NULL, flag, NULL), QR_command_maybe_successful(res))
	{
		if (QR_get_num_cached_tuples(res) > 0)
		{
			char *tmp;

			tmp = strdup(QR_get_value_backend_text(res, 0, 0));
			if (tmp)
			{
				ret = tmp;
				*nameAlloced = TRUE;
			}
		}
	}
	QR_Destructor(res);
	return ret;
}

static RETCODE          SQL_API
PGAPI_ForeignKeys_new(HSTMT hstmt,
					  const SQLCHAR * szPkTableQualifier, /* OA X*/
					  SQLSMALLINT cbPkTableQualifier,
					  const SQLCHAR * szPkTableOwner, /* OA E*/
					  SQLSMALLINT cbPkTableOwner,
					  const SQLCHAR * szPkTableName, /* OA(R) E*/
					  SQLSMALLINT cbPkTableName,
					  const SQLCHAR * szFkTableQualifier, /* OA X*/
					  SQLSMALLINT cbFkTableQualifier,
					  const SQLCHAR * szFkTableOwner, /* OA E*/
					  SQLSMALLINT cbFkTableOwner,
					  const SQLCHAR * szFkTableName, /* OA(R) E*/
					  SQLSMALLINT cbFkTableName);

static RETCODE		SQL_API
PGAPI_ForeignKeys_old(HSTMT hstmt,
					  const SQLCHAR * szPkTableQualifier, /* OA X*/
					  SQLSMALLINT cbPkTableQualifier,
					  const SQLCHAR * szPkTableOwner, /* OA E*/
					  SQLSMALLINT cbPkTableOwner,
					  const SQLCHAR * szPkTableName, /* OA(R) E*/
					  SQLSMALLINT cbPkTableName,
					  const SQLCHAR * szFkTableQualifier, /* OA X*/
					  SQLSMALLINT cbFkTableQualifier,
					  const SQLCHAR * szFkTableOwner, /* OA E*/
					  SQLSMALLINT cbFkTableOwner,
					  const SQLCHAR * szFkTableName, /* OA(R) E*/
					  SQLSMALLINT cbFkTableName)
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
	char		*pk_table_needed = NULL, *escPkTableName = NULL;
	char		fk_table_fetched[TABLE_NAME_STORAGE_LEN + 1];
	char		*fk_table_needed = NULL, *escFkTableName = NULL;
	char		pk_table_fetched[TABLE_NAME_STORAGE_LEN + 1];
	char		schema_needed[SCHEMA_NAME_STORAGE_LEN + 1];
	char		schema_fetched[SCHEMA_NAME_STORAGE_LEN + 1];
	char		constrname[NAMESTORAGELEN + 1], pkname[TABLE_NAME_STORAGE_LEN + 1];
	char	   *pkey_ptr,
			   *pkey_text = NULL,
			   *fkey_ptr,
			   *fkey_text = NULL;

	ConnectionClass *conn;
	BOOL		pkey_alloced,
			fkey_alloced, got_pkname;
	int			i,
				j,
				k,
				num_keys;
	SQLSMALLINT		trig_nargs,
				upd_rule_type = 0,
				del_rule_type = 0;
	SQLSMALLINT	internal_asis_type = SQL_C_CHAR;
	SQLSMALLINT	defer_type;
	char		pkey[MAX_INFO_STRING];
	Int2		result_cols;
	UInt4		relid1, relid2;
	const char *eq_string;

	mylog("%s: entering...stmt=%p\n", func, stmt);

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
	result_cols = NUM_OF_FKS_FIELDS;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info_v(res, FKS_PKTABLE_CAT, "PKTABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_PKTABLE_SCHEM, "PKTABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_PKTABLE_NAME, "PKTABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_PKCOLUMN_NAME, "PKCOLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_FKTABLE_CAT, "FKTABLE_QUALIFIER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_FKTABLE_SCHEM, "FKTABLE_OWNER", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_FKTABLE_NAME, "FKTABLE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_FKCOLUMN_NAME, "FKCOLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_KEY_SEQ, "KEY_SEQ", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, FKS_UPDATE_RULE, "UPDATE_RULE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, FKS_DELETE_RULE, "DELETE_RULE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, FKS_FK_NAME, "FK_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_PK_NAME, "PK_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, FKS_DEFERRABILITY, "DEFERRABILITY", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, FKS_TRIGGER_NAME, "TRIGGER_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	conn = SC_get_conn(stmt);
	result = PGAPI_AllocStmt(conn, &htbl_stmt, 0);
	if (!SQL_SUCCEEDED(result))
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_ForeignKeys result.", func);
		return SQL_ERROR;
	}

#define	return	DONT_CALL_RETURN_FROM_HERE???

	tbl_stmt = (StatementClass *) htbl_stmt;
	schema_needed[0] = '\0';
	schema_fetched[0] = '\0';

	pk_table_needed = make_string(szPkTableName, cbPkTableName, NULL, 0);
	fk_table_needed = make_string(szFkTableName, cbFkTableName, NULL, 0);

#ifdef	UNICODE_SUPPORT
	if (CC_is_in_unicode_driver(conn))
		internal_asis_type = INTERNAL_ASIS_TYPE;
#endif /* UNICODE_SUPPORT */
	pkey_alloced = fkey_alloced = FALSE;

	eq_string = gen_opestr(eqop, conn);
	/*
	 * Case #2 -- Get the foreign keys in the specified table (fktab) that
	 * refer to the primary keys of other table(s).
	 */
	if (fk_table_needed && fk_table_needed[0] != '\0')
	{
		char    *escSchemaName;

		mylog("%s: entering Foreign Key Case #2", func);
		escFkTableName = simpleCatalogEscape((SQLCHAR *) fk_table_needed, SQL_NTS, conn);
		schema_strcat(schema_needed, sizeof(schema_needed), "%.*s", szFkTableOwner, cbFkTableOwner, szFkTableName, cbFkTableName, conn);
		escSchemaName = simpleCatalogEscape((SQLCHAR *) schema_needed, SQL_NTS, conn);
		SPRINTF_FIXED(tables_query, "SELECT	pt.tgargs, "
			"		pt.tgnargs, "
			"		pt.tgdeferrable, "
			"		pt.tginitdeferred, "
			"		pp1.proname, "
			"		pp2.proname, "
			"		pc.oid, "
			"		pc1.oid, "
			"		pc1.relname, "
			"		pt.tgconstrname, pn.nspname "
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
			"AND ((pc.relname %s'%s') "
			"AND (pn1.oid = pc.relnamespace) "
			"AND (pn1.nspname %s'%s') "
			"AND (pp.proname LIKE '%%ins') "
			"AND (pp1.proname LIKE '%%upd') "
			"AND (pp1.proname not LIKE '%%check%%') "
			"AND (pp2.proname LIKE '%%del') "
			"AND (pt1.tgrelid=pt.tgconstrrelid) "
			"AND (pt1.tgconstrname=pt.tgconstrname) "
			"AND (pt2.tgrelid=pt.tgconstrrelid) "
			"AND (pt2.tgconstrname=pt.tgconstrname) "
			"AND (pt.tgconstrrelid=pc1.oid) "
			"AND (pc1.relnamespace=pn.oid))"
			" order by pt.tgconstrname",
			eq_string, escFkTableName, eq_string, escSchemaName);
		free(escSchemaName);

		result = PGAPI_ExecDirect(htbl_stmt, (SQLCHAR *) tables_query, SQL_NTS, PODBC_RDONLY);

		if (!SQL_SUCCEEDED(result))
		{
			SC_full_error_copy(stmt, tbl_stmt, FALSE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 1, SQL_C_BINARY,
							   trig_args, sizeof(trig_args), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 2, SQL_C_SHORT,
							   &trig_nargs, 0, NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
						 trig_deferrable, sizeof(trig_deferrable), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 4, internal_asis_type,
					 trig_initdeferred, sizeof(trig_initdeferred), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 5, internal_asis_type,
							   upd_rule, sizeof(upd_rule), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 6, internal_asis_type,
							   del_rule, sizeof(del_rule), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 7, SQL_C_ULONG,
							   &relid1, sizeof(relid1), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 8, SQL_C_ULONG,
							   &relid2, sizeof(relid2), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 9, internal_asis_type,
					pk_table_fetched, TABLE_NAME_STORAGE_LEN, NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 10, internal_asis_type,
					constrname, NAMESTORAGELEN, NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 11, internal_asis_type,
				schema_fetched, SCHEMA_NAME_STORAGE_LEN, NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
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

		keyresult = PGAPI_AllocStmt(conn, &hpkey_stmt, 0);
		if (!SQL_SUCCEEDED(keyresult))
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

			got_pkname = FALSE;
			keyresult = PGAPI_PrimaryKeys(hpkey_stmt, NULL, 0,
										  (SQLCHAR *) schema_fetched, SQL_NTS,
										  (SQLCHAR *) pk_table_fetched, SQL_NTS,
										  0);
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
				if (!got_pkname)
				{
					PGAPI_GetData(hpkey_stmt, 6, internal_asis_type, pkname, sizeof(pkname), NULL);
					got_pkname = TRUE;
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
				pkey_alloced = FALSE;
				/* Get to next primary key */
				for (k = 0; k < 2; k++)
					pkey_ptr += strlen(pkey_ptr) + 1;

			}
			PGAPI_FreeStmt(hpkey_stmt, SQL_CLOSE);

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

			if (!strcmp(del_rule, "RI_FKey_cascade_del"))
				del_rule_type = SQL_CASCADE;
			else if (!strcmp(del_rule, "RI_FKey_noaction_del"))
				del_rule_type = SQL_NO_ACTION;
			else if (!strcmp(del_rule, "RI_FKey_restrict_del"))
				del_rule_type = SQL_NO_ACTION;
			else if (!strcmp(del_rule, "RI_FKey_setdefault_del"))
				del_rule_type = SQL_SET_DEFAULT;
			else if (!strcmp(del_rule, "RI_FKey_setnull_del"))
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
				tuple = QR_AddNew(res);

				pkey_text = getClientColumnName(conn, relid2, pkey_ptr, &pkey_alloced);
				fkey_text = getClientColumnName(conn, relid1, fkey_ptr, &fkey_alloced);

				mylog("%s: pk_table = '%s', pkey_ptr = '%s'\n", func, pk_table_fetched, pkey_text);
				set_tuplefield_string(&tuple[FKS_PKTABLE_CAT], CurrCat(conn));
				set_tuplefield_string(&tuple[FKS_PKTABLE_SCHEM], GET_SCHEMA_NAME(schema_fetched));
				set_tuplefield_string(&tuple[FKS_PKTABLE_NAME], pk_table_fetched);
				set_tuplefield_string(&tuple[FKS_PKCOLUMN_NAME], pkey_text);

				mylog("%s: fk_table_needed = '%s', fkey_ptr = '%s'\n", func, fk_table_needed, fkey_text);
				set_tuplefield_string(&tuple[FKS_FKTABLE_CAT], CurrCat(conn));
				set_tuplefield_string(&tuple[FKS_FKTABLE_SCHEM], GET_SCHEMA_NAME(schema_needed));
				set_tuplefield_string(&tuple[FKS_FKTABLE_NAME], fk_table_needed);
				set_tuplefield_string(&tuple[FKS_FKCOLUMN_NAME], fkey_text);

				mylog("%s: upd_rule_type = '%i', del_rule_type = '%i'\n, trig_name = '%s'", func, upd_rule_type, del_rule_type, trig_args);
				set_tuplefield_int2(&tuple[FKS_KEY_SEQ], (Int2) (k + 1));
				set_tuplefield_int2(&tuple[FKS_UPDATE_RULE], upd_rule_type);
				set_tuplefield_int2(&tuple[FKS_DELETE_RULE], del_rule_type);
				set_tuplefield_string(&tuple[FKS_FK_NAME], constrname);
				set_tuplefield_string(&tuple[FKS_PK_NAME], pkname);
				set_tuplefield_int2(&tuple[FKS_DEFERRABILITY], defer_type);
				set_tuplefield_string(&tuple[FKS_TRIGGER_NAME], trig_args);

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
	}

	/*
	 * Case #1 -- Get the foreign keys in other tables that refer to the
	 * primary key in the specified table (pktab).	i.e., Who points to
	 * me?
	 */
	else if (pk_table_needed[0] != '\0')
	{
		char	*escSchemaName;

		escPkTableName = simpleCatalogEscape((SQLCHAR *) pk_table_needed, SQL_NTS, conn);
		schema_strcat(schema_needed, sizeof(schema_needed), "%.*s", szPkTableOwner, cbPkTableOwner, szPkTableName, cbPkTableName, conn);
		escSchemaName = simpleCatalogEscape((SQLCHAR *) schema_needed, SQL_NTS, conn);
		SPRINTF_FIXED(tables_query, "SELECT	pt.tgargs, "
			"	pt.tgnargs, "
			"	pt.tgdeferrable, "
			"	pt.tginitdeferred, "
			"	pp1.proname, "
			"	pp2.proname, "
			"	pc.oid, "
			"	pc1.oid, "
			"	pc1.relname, "
			"	pt.tgconstrname, pn1.nspname "
			"FROM	pg_catalog.pg_class pc, "
			"	pg_catalog.pg_class pc1, "
			"	pg_catalog.pg_proc pp, "
			"	pg_catalog.pg_proc pp1, "
			"	pg_catalog.pg_proc pp2, "
			"	pg_catalog.pg_trigger pt, "
			"	pg_catalog.pg_trigger pt1, "
			"	pg_catalog.pg_trigger pt2, "
			"	pg_catalog.pg_namespace pn, "
			"	pg_catalog.pg_namespace pn1 "
			"WHERE  pc.relname %s'%s' "
			"	AND pn.nspname %s'%s' "
			"	AND pc.relnamespace = pn.oid "
			"	AND pt.tgconstrrelid = pc.oid "
			"	AND pp.oid = pt.tgfoid "
			"	AND pp.proname Like '%%ins' "
			"	AND pt1.tgconstrname = pt.tgconstrname "
			"	AND pt1.tgconstrrelid = pt.tgrelid "
			"	AND pt1.tgrelid = pc.oid "
			"	AND pc1.oid = pt.tgrelid "
			"	AND pp1.oid = pt1.tgfoid "
			"	AND pp1.proname like '%%upd' "
			"	AND (pp1.proname not like '%%check%%') "
			"	AND pt2.tgconstrname = pt.tgconstrname "
			"	AND pt2.tgconstrrelid = pt.tgrelid "
			"	AND pt2.tgrelid = pc.oid "
			"	AND pp2.oid = pt2.tgfoid "
			"	AND pp2.proname Like '%%del' "
			"	AND pn1.oid = pc1.relnamespace "
			" order by pt.tgconstrname",
			eq_string, escPkTableName, eq_string, escSchemaName);
		free(escSchemaName);

		result = PGAPI_ExecDirect(htbl_stmt, (SQLCHAR *) tables_query, SQL_NTS, PODBC_RDONLY);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 1, SQL_C_BINARY,
							   trig_args, sizeof(trig_args), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 2, SQL_C_SHORT,
							   &trig_nargs, 0, NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 3, internal_asis_type,
						 trig_deferrable, sizeof(trig_deferrable), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 4, internal_asis_type,
					 trig_initdeferred, sizeof(trig_initdeferred), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 5, internal_asis_type,
							   upd_rule, sizeof(upd_rule), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 6, internal_asis_type,
							   del_rule, sizeof(del_rule), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 7, SQL_C_ULONG,
						&relid1, sizeof(relid1), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 8, SQL_C_ULONG,
						&relid2, sizeof(relid2), NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 9, internal_asis_type,
					fk_table_fetched, TABLE_NAME_STORAGE_LEN, NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}
		result = PGAPI_BindCol(htbl_stmt, 10, internal_asis_type,
					constrname, NAMESTORAGELEN, NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
		}

		result = PGAPI_BindCol(htbl_stmt, 11, internal_asis_type,
				schema_fetched, SCHEMA_NAME_STORAGE_LEN, NULL);
		if (!SQL_SUCCEEDED(result))
		{
			SC_error_copy(stmt, tbl_stmt, TRUE);
			goto cleanup;
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

		/*
		 *	get pk_name here
		 */
		keyresult = PGAPI_AllocStmt(conn, &hpkey_stmt, 0);
		if (!SQL_SUCCEEDED(keyresult))
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate statement for PGAPI_ForeignKeys (pkeys) result.", func);
			goto cleanup;
		}
		keyresult = PGAPI_BindCol(hpkey_stmt, 6, internal_asis_type,
				pkname, sizeof(pkname), NULL);
		if (keyresult != SQL_SUCCESS)
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't bindcol for primary keys for PGAPI_ForeignKeys result.", func);
			goto cleanup;
		}
		keyresult = PGAPI_PrimaryKeys(hpkey_stmt, NULL, 0,
									  (SQLCHAR *) schema_needed, SQL_NTS,
									  (SQLCHAR *) pk_table_needed, SQL_NTS, 0);
		if (keyresult != SQL_SUCCESS)
		{
			SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't get primary keys for PGAPI_ForeignKeys result.", func);
			goto cleanup;
		}
		pkname[0] = '\0';
		keyresult = PGAPI_Fetch(hpkey_stmt);
		PGAPI_FreeStmt(hpkey_stmt, SQL_CLOSE);
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

			if (!strcmp(del_rule, "RI_FKey_cascade_del"))
				del_rule_type = SQL_CASCADE;
			else if (!strcmp(del_rule, "RI_FKey_noaction_del"))
				del_rule_type = SQL_NO_ACTION;
			else if (!strcmp(del_rule, "RI_FKey_restrict_del"))
				del_rule_type = SQL_NO_ACTION;
			else if (!strcmp(del_rule, "RI_FKey_setdefault_del"))
				del_rule_type = SQL_SET_DEFAULT;
			else if (!strcmp(del_rule, "RI_FKey_setnull_del"))
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

				tuple = QR_AddNew(res);

				mylog("pk_table_needed = '%s', pkey_ptr = '%s'\n", pk_table_needed, pkey_text);
				set_tuplefield_string(&tuple[FKS_PKTABLE_CAT], CurrCat(conn));
				set_tuplefield_string(&tuple[FKS_PKTABLE_SCHEM], GET_SCHEMA_NAME(schema_needed));
				set_tuplefield_string(&tuple[FKS_PKTABLE_NAME], pk_table_needed);
				set_tuplefield_string(&tuple[FKS_PKCOLUMN_NAME], pkey_text);

				mylog("fk_table = '%s', fkey_ptr = '%s'\n", fk_table_fetched, fkey_text);
				set_tuplefield_string(&tuple[FKS_FKTABLE_CAT], CurrCat(conn));
				set_tuplefield_string(&tuple[FKS_FKTABLE_SCHEM], GET_SCHEMA_NAME(schema_fetched));
				set_tuplefield_string(&tuple[FKS_FKTABLE_NAME], fk_table_fetched);
				set_tuplefield_string(&tuple[FKS_FKCOLUMN_NAME], fkey_text);

				set_tuplefield_int2(&tuple[FKS_KEY_SEQ], (Int2) (k + 1));

				mylog("upd_rule = %d, del_rule= %d", upd_rule_type, del_rule_type);
				set_nullfield_int2(&tuple[FKS_UPDATE_RULE], upd_rule_type);
				set_nullfield_int2(&tuple[FKS_DELETE_RULE], del_rule_type);

				set_tuplefield_string(&tuple[FKS_FK_NAME], constrname);
				set_tuplefield_string(&tuple[FKS_PK_NAME], pkname);

				set_tuplefield_string(&tuple[FKS_TRIGGER_NAME], trig_args);

				mylog(" defer_type = %d\n", defer_type);
				set_tuplefield_int2(&tuple[FKS_DEFERRABILITY], defer_type);

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
	if (escPkTableName)
		free(escPkTableName);
	if (fk_table_needed)
		free(fk_table_needed);
	if (escFkTableName)
		free(escFkTableName);

	if (htbl_stmt)
		PGAPI_FreeStmt(htbl_stmt, SQL_DROP);
	if (hpkey_stmt)
		PGAPI_FreeStmt(hpkey_stmt, SQL_DROP);

	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	mylog("%s(): EXIT, stmt=%p, ret=%d\n", func, stmt, ret);
	return ret;
}

RETCODE		SQL_API
PGAPI_ForeignKeys(HSTMT hstmt,
				  const SQLCHAR * szPkTableQualifier, /* OA X*/
				  SQLSMALLINT cbPkTableQualifier,
				  const SQLCHAR * szPkTableOwner, /* OA E*/
				  SQLSMALLINT cbPkTableOwner,
				  const SQLCHAR * szPkTableName, /* OA(R) E*/
				  SQLSMALLINT cbPkTableName,
				  const SQLCHAR * szFkTableQualifier, /* OA X*/
				  SQLSMALLINT cbFkTableQualifier,
				  const SQLCHAR * szFkTableOwner, /* OA E*/
				  SQLSMALLINT cbFkTableOwner,
				  const SQLCHAR * szFkTableName, /* OA(R) E*/
				  SQLSMALLINT cbFkTableName)
{
	ConnectionClass	*conn = SC_get_conn(((StatementClass *) hstmt));
	if (PG_VERSION_GE(conn, 8.1))
		return PGAPI_ForeignKeys_new(hstmt,
				szPkTableQualifier, cbPkTableQualifier,
				szPkTableOwner, cbPkTableOwner,
				szPkTableName, cbPkTableName,
				szFkTableQualifier, cbFkTableQualifier,
				szFkTableOwner, cbFkTableOwner,
				szFkTableName, cbFkTableName);
	else
		return PGAPI_ForeignKeys_old(hstmt,
				szPkTableQualifier, cbPkTableQualifier,
				szPkTableOwner, cbPkTableOwner,
				szPkTableName, cbPkTableName,
				szFkTableQualifier, cbFkTableQualifier,
				szFkTableOwner, cbFkTableOwner,
				szFkTableName, cbFkTableName);
}


#define	PRORET_COUNT
#define	DISPLAY_ARGNAME
static BOOL
has_outparam(const char *proargmodes)
{
	const char *ptr;

	if (!proargmodes)
		return FALSE;
	for (ptr = proargmodes; *ptr; ptr++)
	{
		switch (*ptr)
		{
			case 'o':
			case 'b':
			case 't':
				return TRUE;
		}
	}

	return FALSE;
}

RETCODE		SQL_API
PGAPI_ProcedureColumns(HSTMT hstmt,
					   const SQLCHAR * szProcQualifier, /* OA X*/
					   SQLSMALLINT cbProcQualifier,
					   const SQLCHAR * szProcOwner, /* PV E*/
					   SQLSMALLINT cbProcOwner,
					   const SQLCHAR * szProcName, /* PV E*/
					   SQLSMALLINT cbProcName,
					   const SQLCHAR * szColumnName, /* PV X*/
					   SQLSMALLINT cbColumnName,
					   UWORD flag)
{
	CSTR func = "PGAPI_ProcedureColumns";
	StatementClass	*stmt = (StatementClass *) hstmt;
	ConnectionClass *conn = SC_get_conn(stmt);
	char		proc_query[INFO_INQUIRY_LEN];
	Int2		result_cols;
	TupleField	*tuple;
	char		*schema_name, *procname;
	char		*escSchemaName = NULL, *escProcName = NULL;
	char		*params, *proargnames;
	char		*proargmodes;
	char		*delim = NULL;
	char		*atttypid, *attname, *column_name;
	QResultClass *res, *tres;
	SQLLEN		tcount;
	OID			pgtype;
	Int4		paramcount, column_size, i, j;
	RETCODE		result;
	BOOL		search_pattern, bRetset, outpara_exist;
	const char	*like_or_eq, *op_string, *retset;
	int		ret_col = -1, ext_pos = -1, poid_pos = -1, attid_pos = -1, attname_pos = -1;
	UInt4		poid = 0, newpoid;

	mylog("%s: entering...\n", func);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;
	search_pattern = (0 == (flag & PODBC_NOT_SEARCH_PATTERN));
	if (search_pattern)
	{
		like_or_eq = likeop;
		escSchemaName = adjustLikePattern(szProcOwner, cbProcOwner, conn);
		escProcName = adjustLikePattern(szProcName, cbProcName, conn);
	}
	else
	{
		like_or_eq = eqop;
		escSchemaName = simpleCatalogEscape(szProcOwner, cbProcOwner, conn);
		escProcName = simpleCatalogEscape(szProcName, cbProcName, conn);
	}
	op_string = gen_opestr(like_or_eq, conn);
	STRCPY_FIXED(proc_query, "select proname, proretset, prorettype, "
			"pronargs, proargtypes, nspname, p.oid");
	ret_col = ext_pos = 7;
	poid_pos = 6;
#ifdef	PRORET_COUNT
	STRCAT_FIXED(proc_query, ", atttypid, attname");
	attid_pos = ext_pos;
	attname_pos = ext_pos + 1;
	ret_col += 2;
	ext_pos = ret_col;
#endif /* PRORET_COUNT */
	if (PG_VERSION_GE(conn, 8.0))
	{
		STRCAT_FIXED(proc_query, ", proargnames");
		ret_col++;
	}
	if (PG_VERSION_GE(conn, 8.1))
	{
		STRCAT_FIXED(proc_query, ", proargmodes, proallargtypes");
		ret_col += 2;
	}
#ifdef	PRORET_COUNT
	STRCAT_FIXED(proc_query, " from ((pg_catalog.pg_namespace n inner join"
			   " pg_catalog.pg_proc p on p.pronamespace = n.oid)"
		" inner join pg_type t on t.oid = p.prorettype)"
		" left outer join pg_attribute a on a.attrelid = t.typrelid "
		" and attnum > 0 and not attisdropped where");
#else
	STRCAT_FIXED(proc_query, " from pg_catalog.pg_namespace n,"
			   " pg_catalog.pg_proc p where");
			   " p.pronamespace = n.oid  and"
			   " (not proretset) and");
#endif /* PRORET_COUNT */
	SPRINTFCAT_FIXED(proc_query,
				 " has_function_privilege(p.oid, 'EXECUTE')");
	if (IS_VALID_NAME(escSchemaName))
		SPRINTFCAT_FIXED(proc_query,
				 " and nspname %s'%s'",
				 op_string, escSchemaName);
	if (escProcName)
		SPRINTFCAT_FIXED(proc_query,
					 " and proname %s'%s'", op_string, escProcName);
	SPRINTFCAT_FIXED(proc_query,
				 " order by nspname, proname, p.oid, attnum");

	if (escSchemaName)
		free(escSchemaName);
	if (escProcName)
		free(escProcName);

	tres = CC_send_query(conn, proc_query, NULL, READ_ONLY_QUERY, stmt);
	if (!QR_command_maybe_successful(tres))
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
	result_cols = NUM_OF_PROCOLS_FIELDS;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	/* set the field names */
	QR_set_num_fields(res, result_cols);
	QR_set_field_info_v(res, PROCOLS_PROCEDURE_CAT, "PROCEDURE_CAT", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PROCOLS_PROCEDURE_SCHEM, "PROCEDUR_SCHEM", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PROCOLS_PROCEDURE_NAME, "PROCEDURE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PROCOLS_COLUMN_NAME, "COLUMN_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PROCOLS_COLUMN_TYPE, "COLUMN_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, PROCOLS_DATA_TYPE, "DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, PROCOLS_TYPE_NAME, "TYPE_NAME", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PROCOLS_COLUMN_SIZE, "COLUMN_SIZE", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, PROCOLS_BUFFER_LENGTH, "BUFFER_LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, PROCOLS_DECIMAL_DIGITS, "DECIMAL_DIGITS", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, PROCOLS_NUM_PREC_RADIX, "NUM_PREC_RADIX", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, PROCOLS_NULLABLE, "NULLABLE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, PROCOLS_REMARKS, "REMARKS", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PROCOLS_COLUMN_DEF, "COLUMN_DEF", PG_TYPE_VARCHAR, MAX_INFO_STRING);
	QR_set_field_info_v(res, PROCOLS_SQL_DATA_TYPE, "SQL_DATA_TYPE", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, PROCOLS_SQL_DATETIME_SUB, "SQL_DATETIME_SUB", PG_TYPE_INT2, 2);
	QR_set_field_info_v(res, PROCOLS_CHAR_OCTET_LENGTH, "CHAR_OCTET_LENGTH", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, PROCOLS_ORDINAL_POSITION, "ORDINAL_POSITION", PG_TYPE_INT4, 4);
	QR_set_field_info_v(res, PROCOLS_IS_NULLABLE, "IS_NULLABLE", PG_TYPE_VARCHAR, MAX_INFO_STRING);

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
		schema_name = GET_SCHEMA_NAME(QR_get_value_backend_text(tres, i, 5));
		procname = QR_get_value_backend_text(tres, i, 0);
		retset = QR_get_value_backend_text(tres, i, 1);
		pgtype = QR_get_value_backend_int(tres, i, 2, NULL);
		bRetset = retset && (retset[0] == 't' || retset[0] == 'y');
		outpara_exist = FALSE;
		newpoid = 0;
		if (poid_pos >= 0)
			newpoid = QR_get_value_backend_int(tres, i, poid_pos, NULL);
mylog("newpoid=%d\n", newpoid);
		atttypid = NULL;
		if (attid_pos >= 0)
		{
			atttypid = QR_get_value_backend_text(tres, i, attid_pos);
mylog("atttypid=%s\n", atttypid ? atttypid : "(null)");
		}
		if (poid == 0 || newpoid != poid)
		{
			poid = newpoid;
			proargmodes = NULL;
			proargnames = NULL;
			if (ext_pos >=0)
			{
#ifdef	DISPLAY_ARGNAME /* !! named parameter is unavailable !! */
				if (PG_VERSION_GE(conn, 8.0))
					proargnames = QR_get_value_backend_text(tres, i, ext_pos);
#endif /* DISPLAY_ARGNAME */
				if (PG_VERSION_GE(conn, 8.1))
					proargmodes = QR_get_value_backend_text(tres, i, ext_pos + 1);
			}
			outpara_exist = has_outparam(proargmodes);
			/* RETURN_VALUE info */
			if (0 != pgtype && PG_TYPE_VOID != pgtype && !bRetset && !atttypid && !outpara_exist)
			{
				tuple = QR_AddNew(res);
				set_tuplefield_string(&tuple[PROCOLS_PROCEDURE_CAT], CurrCat(conn));
				set_nullfield_string(&tuple[PROCOLS_PROCEDURE_SCHEM], schema_name);
				set_tuplefield_string(&tuple[PROCOLS_PROCEDURE_NAME], procname);
				set_tuplefield_string(&tuple[PROCOLS_COLUMN_NAME], NULL_STRING);
				set_tuplefield_int2(&tuple[PROCOLS_COLUMN_TYPE], SQL_RETURN_VALUE);
				set_tuplefield_int2(&tuple[PROCOLS_DATA_TYPE], PGTYPE_TO_CONCISE_TYPE(conn, pgtype));
				set_tuplefield_string(&tuple[PROCOLS_TYPE_NAME], PGTYPE_TO_NAME(conn, pgtype, FALSE));
				column_size = PGTYPE_COLUMN_SIZE(conn, pgtype);
				set_nullfield_int4(&tuple[PROCOLS_COLUMN_SIZE], column_size);
				set_tuplefield_int4(&tuple[PROCOLS_BUFFER_LENGTH], PGTYPE_BUFFER_LENGTH(conn, pgtype));
				set_nullfield_int2(&tuple[PROCOLS_DECIMAL_DIGITS], PGTYPE_DECIMAL_DIGITS(conn, pgtype));
				set_nullfield_int2(&tuple[PROCOLS_NUM_PREC_RADIX], pgtype_radix(conn, pgtype));
				set_tuplefield_int2(&tuple[PROCOLS_NULLABLE], SQL_NULLABLE_UNKNOWN);
				set_tuplefield_null(&tuple[PROCOLS_REMARKS]);
				set_tuplefield_null(&tuple[PROCOLS_COLUMN_DEF]);
				set_tuplefield_int2(&tuple[PROCOLS_SQL_DATA_TYPE], PGTYPE_TO_SQLDESCTYPE(conn, pgtype));
				set_nullfield_int2(&tuple[PROCOLS_SQL_DATETIME_SUB], PGTYPE_TO_DATETIME_SUB(conn, pgtype));
				set_nullfield_int4(&tuple[PROCOLS_CHAR_OCTET_LENGTH], PGTYPE_TRANSFER_OCTET_LENGTH(conn, pgtype));
				set_tuplefield_int4(&tuple[PROCOLS_ORDINAL_POSITION], 0);
				set_tuplefield_string(&tuple[PROCOLS_IS_NULLABLE], NULL_STRING);
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
				params = QR_get_value_backend_text(tres, i, ext_pos + 2);
				if ('{' == *proargmodes)
					proargmodes++;
				if ('{' == *params)
					params++;
			}
			else
			{
				paramcount = QR_get_value_backend_int(tres, i, 3, NULL);
				params = QR_get_value_backend_text(tres, i, 4);
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
					while (isspace((unsigned char) *params) || ',' == *params)
						params++;
					if ('\0' == *params || '}' == *params)
						params = NULL;
					else
					{
						sscanf(params, "%u", &pgtype);
						while (isdigit((unsigned char) *params))
							params++;
					}
				}
				/* input/output type of parameters */
				if (proargmodes)
				{
					while (isspace((unsigned char) *proargmodes) || ',' == *proargmodes)
						proargmodes++;
					if ('\0' == *proargmodes || '}' == *proargmodes)
						proargmodes = NULL;
				}
				/* name of parameters */
				if (proargnames)
				{
					while (isspace((unsigned char) *proargnames) || ',' == *proargnames)
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
						for (delim = proargnames; *delim && !isspace((unsigned char) *delim) && ',' != *delim && '}' != *delim; delim++)
							;
					}
					if (proargnames && '\0' == *delim) /* discard the incomplete name */
						proargnames = NULL;
				}

				tuple = QR_AddNew(res);
				set_tuplefield_string(&tuple[PROCOLS_PROCEDURE_CAT], CurrCat(conn));
				set_nullfield_string(&tuple[PROCOLS_PROCEDURE_SCHEM], schema_name);
				set_tuplefield_string(&tuple[PROCOLS_PROCEDURE_NAME], procname);
				if (proargnames)
				{
					*delim = '\0';
					set_tuplefield_string(&tuple[PROCOLS_COLUMN_NAME], proargnames);
					proargnames = delim + 1;
				}
				else
					set_tuplefield_string(&tuple[PROCOLS_COLUMN_NAME], NULL_STRING);
				if (proargmodes)
				{
					int	ptype;

					switch (*proargmodes)
					{
						case 'o':
						case 't':
							ptype = bRetset ? SQL_RESULT_COL : SQL_PARAM_OUTPUT;
							break;
						case 'b':
							ptype = SQL_PARAM_INPUT_OUTPUT;
							break;
						default:
							ptype = SQL_PARAM_INPUT;
							break;
					}
					set_tuplefield_int2(&tuple[PROCOLS_COLUMN_TYPE], ptype);
					proargmodes++;
				}
				else
					set_tuplefield_int2(&tuple[PROCOLS_COLUMN_TYPE], SQL_PARAM_INPUT);
				set_tuplefield_int2(&tuple[PROCOLS_DATA_TYPE], PGTYPE_TO_CONCISE_TYPE(conn, pgtype));
				set_tuplefield_string(&tuple[PROCOLS_TYPE_NAME], PGTYPE_TO_NAME(conn, pgtype, FALSE));
				column_size = PGTYPE_COLUMN_SIZE(conn, pgtype);
				set_nullfield_int4(&tuple[PROCOLS_COLUMN_SIZE], column_size);
				set_tuplefield_int4(&tuple[PROCOLS_BUFFER_LENGTH], PGTYPE_BUFFER_LENGTH(conn, pgtype));
				set_nullfield_int2(&tuple[PROCOLS_DECIMAL_DIGITS], PGTYPE_DECIMAL_DIGITS(conn, pgtype));
				set_nullfield_int2(&tuple[PROCOLS_NUM_PREC_RADIX], pgtype_radix(conn, pgtype));
				set_tuplefield_int2(&tuple[PROCOLS_NULLABLE], SQL_NULLABLE_UNKNOWN);
				set_tuplefield_null(&tuple[PROCOLS_REMARKS]);
				set_tuplefield_null(&tuple[PROCOLS_COLUMN_DEF]);
				set_tuplefield_int2(&tuple[PROCOLS_SQL_DATA_TYPE], PGTYPE_TO_SQLDESCTYPE(conn, pgtype));
				set_nullfield_int2(&tuple[PROCOLS_SQL_DATETIME_SUB], PGTYPE_TO_DATETIME_SUB(conn, pgtype));
				set_nullfield_int4(&tuple[PROCOLS_CHAR_OCTET_LENGTH], PGTYPE_TRANSFER_OCTET_LENGTH(conn, pgtype));
				set_tuplefield_int4(&tuple[PROCOLS_ORDINAL_POSITION], j + 1);
				set_tuplefield_string(&tuple[PROCOLS_IS_NULLABLE], NULL_STRING);
			}
		}
		/* RESULT Columns info */
		if (!outpara_exist &&
		    (NULL != atttypid || bRetset))
		{
			int	typid;

			if (!atttypid)
			{
				typid = pgtype;
				attname = NULL;
			}
			else
			{
				typid = atoi(atttypid);
				attname = QR_get_value_backend_text(tres, i, attname_pos);
			}
			tuple = QR_AddNew(res);
			set_tuplefield_string(&tuple[PROCOLS_PROCEDURE_CAT], CurrCat(conn));
			set_nullfield_string(&tuple[PROCOLS_PROCEDURE_SCHEM], schema_name);
			set_tuplefield_string(&tuple[PROCOLS_PROCEDURE_NAME], procname);
			set_tuplefield_string(&tuple[PROCOLS_COLUMN_NAME], attname);
			set_tuplefield_int2(&tuple[PROCOLS_COLUMN_TYPE], bRetset ? SQL_RESULT_COL : SQL_PARAM_OUTPUT);
			set_tuplefield_int2(&tuple[PROCOLS_DATA_TYPE], PGTYPE_TO_CONCISE_TYPE(conn, typid));
			set_tuplefield_string(&tuple[PROCOLS_TYPE_NAME], PGTYPE_TO_NAME(conn, typid, FALSE));
			column_size = PGTYPE_COLUMN_SIZE(conn, typid);
			set_nullfield_int4(&tuple[PROCOLS_COLUMN_SIZE], column_size);
			set_tuplefield_int4(&tuple[PROCOLS_BUFFER_LENGTH], PGTYPE_BUFFER_LENGTH(conn, typid));
			set_nullfield_int2(&tuple[PROCOLS_DECIMAL_DIGITS], PGTYPE_DECIMAL_DIGITS(conn, typid));
			set_nullfield_int2(&tuple[PROCOLS_NUM_PREC_RADIX], pgtype_radix(conn, typid));
			set_tuplefield_int2(&tuple[PROCOLS_NULLABLE], SQL_NULLABLE_UNKNOWN);
			set_tuplefield_null(&tuple[PROCOLS_REMARKS]);
			set_tuplefield_null(&tuple[PROCOLS_COLUMN_DEF]);
			set_tuplefield_int2(&tuple[PROCOLS_SQL_DATA_TYPE], PGTYPE_TO_SQLDESCTYPE(conn, typid));
			set_nullfield_int2(&tuple[PROCOLS_SQL_DATETIME_SUB], PGTYPE_TO_DATETIME_SUB(conn, typid));
			set_nullfield_int4(&tuple[PROCOLS_CHAR_OCTET_LENGTH], PGTYPE_TRANSFER_OCTET_LENGTH(conn, typid));
			set_tuplefield_int4(&tuple[PROCOLS_ORDINAL_POSITION], 0);
			set_tuplefield_string(&tuple[PROCOLS_IS_NULLABLE], NULL_STRING);
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
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_Procedures(HSTMT hstmt,
				 const SQLCHAR * szProcQualifier, /* OA X*/
				 SQLSMALLINT cbProcQualifier,
				 const SQLCHAR * szProcOwner, /* PV E*/
				 SQLSMALLINT cbProcOwner,
				 const SQLCHAR * szProcName, /* PV E*/
				 SQLSMALLINT cbProcName,
				 UWORD flag)
{
	CSTR func = "PGAPI_Procedures";
	StatementClass *stmt = (StatementClass *) hstmt;
	ConnectionClass *conn = SC_get_conn(stmt);
	char		proc_query[INFO_INQUIRY_LEN];
	char	*escSchemaName = NULL, *escProcName = NULL;
	QResultClass *res;
	RETCODE		result;
	const char	*like_or_eq, *op_string;
	BOOL	search_pattern;

	mylog("%s: entering... scnm=%p len=%d\n", func, szProcOwner, cbProcOwner);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	search_pattern = (0 == (flag & PODBC_NOT_SEARCH_PATTERN));
	if (search_pattern)
	{
		like_or_eq = likeop;
		escSchemaName = adjustLikePattern(szProcOwner, cbProcOwner, conn);
		escProcName = adjustLikePattern(szProcName, cbProcName, conn);
	}
	else
	{
		like_or_eq = eqop;
		escSchemaName = simpleCatalogEscape(szProcOwner, cbProcOwner, conn);
		escProcName = simpleCatalogEscape(szProcName, cbProcName, conn);
	}
	/*
	 * The following seems the simplest implementation
	 */
	op_string = gen_opestr(like_or_eq, conn);
	STRCPY_FIXED(proc_query, "select '' as " "PROCEDURE_CAT" ", nspname as " "PROCEDURE_SCHEM" ","
	" proname as " "PROCEDURE_NAME" ", '' as " "NUM_INPUT_PARAMS" ","
	   " '' as " "NUM_OUTPUT_PARAMS" ", '' as " "NUM_RESULT_SETS" ","
	   " '' as " "REMARKS" ","
	   " case when prorettype = 0 then 1::int2 else 2::int2 end"
	   " as "		  "PROCEDURE_TYPE" " from pg_catalog.pg_namespace,"
	   " pg_catalog.pg_proc"
	  " where pg_proc.pronamespace = pg_namespace.oid");
	schema_strcat1(proc_query, sizeof(proc_query), " and nspname %s'%.*s'", op_string, escSchemaName, szProcName, cbProcName, conn);
	if (IS_VALID_NAME(escProcName))
		SPRINTFCAT_FIXED(proc_query,
				 " and proname %s'%s'", op_string, escProcName);

	res = CC_send_query(conn, proc_query, NULL, READ_ONLY_QUERY, stmt);
	if (!QR_command_maybe_successful(res))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_Procedures query error", func);
		QR_Destructor(res);
		if (escSchemaName)
			free(escSchemaName);
		if (escProcName)
			free(escProcName);
		return SQL_ERROR;
	}
	SC_set_Result(stmt, res);

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	stmt->status = STMT_FINISHED;
	extend_column_bindings(SC_get_ARDF(stmt), 8);
	if (escSchemaName)
		free(escSchemaName);
	if (escProcName)
		free(escProcName);
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
	int usercount = (int) QR_get_num_cached_tuples(allures), i, addcnt = 0;

mylog("user=%s auth=%s\n", user, auth);
	if (user[0])
		for (i = 0; i < usercount; i++)
		{
			if (strcmp(QR_get_value_backend_text(allures, i, 0), user) == 0)
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
PGAPI_TablePrivileges(HSTMT hstmt,
					  const SQLCHAR * szTableQualifier, /* OA X*/
					  SQLSMALLINT cbTableQualifier,
					  const SQLCHAR * szTableOwner, /* PV E*/
					  SQLSMALLINT cbTableOwner,
					  const SQLCHAR * szTableName, /* PV E*/
					  SQLSMALLINT cbTableName,
					  UWORD flag)
{
	StatementClass *stmt = (StatementClass *) hstmt;
	CSTR func = "PGAPI_TablePrivileges";
	ConnectionClass *conn = SC_get_conn(stmt);
	Int2		result_cols;
	char		proc_query[INFO_INQUIRY_LEN];
	QResultClass	*res, *wres = NULL, *allures = NULL;
	TupleField	*tuple;
	Int4		tablecount, usercount, i, j, k;
	BOOL		grpauth, sys, su;
	char		(*useracl)[ACLMAX] = NULL, *acl, *user, *delim, *auth;
	const char	*reln, *owner, *priv, *schnm = NULL;
	RETCODE		result, ret = SQL_SUCCESS;
	const char	*like_or_eq, *op_string;
	const SQLCHAR *szSchemaName;
	SQLSMALLINT	cbSchemaName;
	char		*escSchemaName = NULL, *escTableName = NULL;
	BOOL		search_pattern;

	mylog("%s: entering... scnm=%p len-%d\n", func, szTableOwner, cbTableOwner);
	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	/*
	 * a statement is actually executed, so we'll have to do this
	 * ourselves.
	 */
	result_cols = NUM_OF_TABPRIV_FIELDS;
	extend_column_bindings(SC_get_ARDF(stmt), result_cols);

	stmt->catalog_result = TRUE;
	/* set the field names */
	res = QR_Constructor();
	if (!res)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for query.", func);
		return SQL_ERROR;
	}
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
	search_pattern = (0 == (flag & PODBC_NOT_SEARCH_PATTERN));
	if (search_pattern)
	{
		like_or_eq = likeop;
		escTableName = adjustLikePattern(szTableName, cbTableName, conn);
	}
	else
	{
		like_or_eq = eqop;
		escTableName = simpleCatalogEscape(szTableName, cbTableName, conn);
	}

retry_public_schema:
	if (escSchemaName)
		free(escSchemaName);
	if (search_pattern)
		escSchemaName = adjustLikePattern(szSchemaName, cbSchemaName, conn);
	else
		escSchemaName = simpleCatalogEscape(szSchemaName, cbSchemaName, conn);

	op_string = gen_opestr(like_or_eq, conn);
	STRCPY_FIXED(proc_query, "select relname, usename, relacl, nspname"
	" from pg_catalog.pg_namespace, pg_catalog.pg_class ,"
	" pg_catalog.pg_user where");
	if (escSchemaName)
		schema_strcat1(proc_query, sizeof(proc_query), " nspname %s'%.*s' and", op_string, escSchemaName, szTableName, cbTableName, conn);

	if (escTableName)
		SPRINTFCAT_FIXED(proc_query, " relname %s'%s' and", op_string, escTableName);
	STRCAT_FIXED(proc_query, " pg_namespace.oid = relnamespace and relkind in ('r', 'v') and");
	if ((!escTableName) && (!escSchemaName))
		STRCAT_FIXED(proc_query, " nspname not in ('pg_catalog', 'information_schema') and");

	STRCAT_FIXED(proc_query, " pg_user.usesysid = relowner");
	if (wres = CC_send_query(conn, proc_query, NULL, READ_ONLY_QUERY, stmt), !QR_command_maybe_successful(wres))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_TablePrivileges query error", func);
		ret = SQL_ERROR;
		goto cleanup;
	}
	tablecount = (Int4) QR_get_num_cached_tuples(wres);
	/* If not found */
	if ((flag & PODBC_SEARCH_PUBLIC_SCHEMA) != 0 &&
	    0 == tablecount)
	{
		if (allow_public_schema(conn, szSchemaName, cbSchemaName))
		{
			QR_Destructor(wres);
			wres = NULL;
			szSchemaName = pubstr;
			cbSchemaName = SQL_NTS;
			goto retry_public_schema;
		}
	}

	STRCPY_FIXED(proc_query, "select usename, usesysid, usesuper from pg_user");
	if (allures = CC_send_query(conn, proc_query, NULL, READ_ONLY_QUERY, stmt), !QR_command_maybe_successful(allures))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_TablePrivileges query error", func);
		ret = SQL_ERROR;
		goto cleanup;
	}
	usercount = (Int4) QR_get_num_cached_tuples(allures);
	useracl = (char (*)[ACLMAX]) malloc(usercount * sizeof(char [ACLMAX]));
	if (!useracl)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Couldn't allocate memory for user acl.", func);
		ret = SQL_ERROR;
		goto cleanup;
	}
	for (i = 0; i < tablecount; i++)
	{
		memset(useracl, 0, usercount * sizeof(char[ACLMAX]));
		acl = (char *) QR_get_value_backend_text(wres, i, 2);
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

				SPRINTF_FIXED(proc_query, "select grolist from pg_group where groname = '%s'", user);
				if (gres = CC_send_query(conn, proc_query, NULL, READ_ONLY_QUERY, stmt), !QR_command_maybe_successful(gres))
				{
					grolist = QR_get_value_backend_text(gres, 0, 0);
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
								if (strcmp(QR_get_value_backend_text(allures, i, 1), uid) == 0)
									useracl_upd(useracl, allures, QR_get_value_backend_text(allures, i, 0), auth);
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
		reln = QR_get_value_backend_text(wres, i, 0);
		owner = QR_get_value_backend_text(wres, i, 1);
		schnm = QR_get_value_backend_text(wres, i, 3);
		/* The owner has all privileges */
		useracl_upd(useracl, allures, owner, ALL_PRIVILIGES);
		for (j = 0; j < usercount; j++)
		{
			user = QR_get_value_backend_text(allures, j, 0);
			su = (strcmp(QR_get_value_backend_text(allures, j, 2), "t") == 0);
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
				set_tuplefield_string(&tuple[TABPRIV_TABLE_CAT], CurrCat(conn));
				set_tuplefield_string(&tuple[TABPRIV_TABLE_SCHEM], GET_SCHEMA_NAME(schnm));
				set_tuplefield_string(&tuple[TABPRIV_TABLE_NAME], reln);
				if (su || sys)
					set_tuplefield_string(&tuple[TABPRIV_GRANTOR], "_SYSTEM");
				else
					set_tuplefield_string(&tuple[TABPRIV_GRANTOR], owner);
				mylog("user=%s\n", user);
				set_tuplefield_string(&tuple[TABPRIV_GRANTEE], user);
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
						priv = NULL_STRING;
				}
				set_tuplefield_string(&tuple[TABPRIV_PRIVILEGE], priv);
				/* The owner and the super user are grantable */
				if (sys || su)
					set_tuplefield_string(&tuple[TABPRIV_IS_GRANTABLE], "YES");
				else
					set_tuplefield_string(&tuple[TABPRIV_IS_GRANTABLE], "NO");
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
	return ret;
}


static RETCODE		SQL_API
PGAPI_ForeignKeys_new(HSTMT hstmt,
					  const SQLCHAR * szPkTableQualifier, /* OA X*/
					  SQLSMALLINT cbPkTableQualifier,
					  const SQLCHAR * szPkTableOwner, /* OA E*/
					  SQLSMALLINT cbPkTableOwner,
					  const SQLCHAR * szPkTableName, /* OA(R) E*/
					  SQLSMALLINT cbPkTableName,
					  const SQLCHAR * szFkTableQualifier, /* OA X*/
					  SQLSMALLINT cbFkTableQualifier,
					  const SQLCHAR * szFkTableOwner, /* OA E*/
					  SQLSMALLINT cbFkTableOwner,
					  const SQLCHAR * szFkTableName, /* OA(R) E*/
					  SQLSMALLINT cbFkTableName)
{
	CSTR func = "PGAPI_ForeignKeys";
	StatementClass	*stmt = (StatementClass *) hstmt;
	QResultClass	*res = NULL;
	RETCODE		ret = SQL_ERROR, result;
	char		tables_query[INFO_INQUIRY_LEN];
	char		*pk_table_needed = NULL, *escTableName = NULL;
	char		*fk_table_needed = NULL;
	char		schema_needed[SCHEMA_NAME_STORAGE_LEN + 1];
	char		*escSchemaName;
	char		catName[SCHEMA_NAME_STORAGE_LEN],
			scmName1[SCHEMA_NAME_STORAGE_LEN],
			scmName2[SCHEMA_NAME_STORAGE_LEN];
	const char	*relqual;
	ConnectionClass *conn = SC_get_conn(stmt);

	const char *eq_string;

	mylog("%s: entering...stmt=%p\n", func, stmt);

	if (result = SC_initialize_and_recycle(stmt), SQL_SUCCESS != result)
		return result;

	schema_needed[0] = '\0';
#define	return	DONT_CALL_RETURN_FROM_HERE???

	pk_table_needed = make_string(szPkTableName, cbPkTableName, NULL, 0);
	fk_table_needed = make_string(szFkTableName, cbFkTableName, NULL, 0);

	eq_string = gen_opestr(eqop, conn);

	/*
	 * Case #2 -- Get the foreign keys in the specified table (fktab) that
	 * refer to the primary keys of other table(s).
	 */
	if (NULL != fk_table_needed)
	{
		mylog("%s: entering Foreign Key Case #2", func);
		escTableName = simpleCatalogEscape((SQLCHAR *) fk_table_needed, SQL_NTS, conn);
		schema_strcat(schema_needed, sizeof(schema_needed), "%.*s", szFkTableOwner, cbFkTableOwner, szFkTableName, cbFkTableName, conn);
		relqual = "\n   and  conrelid = c.oid";
	}
	/*
	 * Case #1 -- Get the foreign keys in other tables that refer to the
	 * primary key in the specified table (pktab).	i.e., Who points to
	 * me?
	 */
	else if (NULL != pk_table_needed)
	{
		escTableName = simpleCatalogEscape((SQLCHAR *) pk_table_needed, SQL_NTS, conn);
		schema_strcat(schema_needed, sizeof(schema_needed), "%.*s", szPkTableOwner, cbPkTableOwner, szPkTableName, cbPkTableName, conn);
		relqual = "\n   and  confrelid = c.oid";
	}
	else
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "No tables specified to PGAPI_ForeignKeys.", func);
		goto cleanup;
	}

	if (NULL != CurrCat(conn))
		SPRINTF_FIXED(catName, "'%s'::name", CurrCat(conn));
	else
		STRCPY_FIXED(catName, "NULL::name");
	STRCPY_FIXED(scmName1, "n2.nspname");
	STRCPY_FIXED(scmName2, "n1.nspname");
	escSchemaName = simpleCatalogEscape((SQLCHAR *) schema_needed, SQL_NTS, conn);

	SPRINTF_FIXED(tables_query,
		"select"
		"	%s as PKTABLE_CAT"
		",\n	%s as PKTABLE_SCHEM"
		",\n	c2.relname as PKTABLE_NAME"
		",\n	a2.attname as PKCOLUMN_NAME"
		",\n	%s as FKTABLE_CAT"
		",\n	%s as FKTABLE_SCHEM"
		",\n	c1.relname as FKTABLE_NAME"
		",\n	a1.attname as FKCOLUMN_NAME"
		",\n	i::int2 as KEY_SEQ"
		",\n	case ref.confupdtype"
		"\n		when 'c' then %d::int2"
		"\n		when 'n' then %d::int2"
		"\n		when 'd' then %d::int2"
		"\n		when 'r' then %d::int2"
		"\n		else %d::int2"
		"\n	end as UPDATE_RULE"
		",\n	case ref.confdeltype"
		"\n		when 'c' then %d::int2"
		"\n		when 'n' then %d::int2"
		"\n		when 'd' then %d::int2"
		"\n		when 'r' then %d::int2"
		"\n		else %d::int2"
		"\n	end as DELETE_RULE"
		",\n	ref.conname as FK_NAME"
		",\n	cn.conname as PK_NAME"
		",\n	case"
		"\n		when ref.condeferrable then"
		"\n			case"
		"\n			when ref.condeferred then %d::int2"
		"\n			else %d::int2"
		"\n			end"
		"\n		else %d::int2"
		"\n	end as DEFERRABLITY"
		"\n from"
		"\n ((((((("
		" (select cn.oid, conrelid, conkey, confrelid, confkey"
		",\n	 generate_series(array_lower(conkey, 1), array_upper(conkey, 1)) as i"
		",\n	 confupdtype, confdeltype, conname"
		",\n	 condeferrable, condeferred"
		"\n  from pg_catalog.pg_constraint cn"
		",\n	pg_catalog.pg_class c"
		",\n	pg_catalog.pg_namespace n"
		"\n  where contype = 'f' %s"
		"\n   and  relname %s'%s'"
		"\n   and  n.oid = c.relnamespace"
		"\n   and  n.nspname %s'%s'"
		"\n ) ref"
		"\n inner join pg_catalog.pg_class c1"
		"\n  on c1.oid = ref.conrelid)"
		"\n inner join pg_catalog.pg_namespace n1"
		"\n  on  n1.oid = c1.relnamespace)"
		"\n inner join pg_catalog.pg_attribute a1"
		"\n  on  a1.attrelid = c1.oid"
		"\n  and  a1.attnum = conkey[i])"
		"\n inner join pg_catalog.pg_class c2"
		"\n  on  c2.oid = ref.confrelid)"
		"\n inner join pg_catalog.pg_namespace n2"
		"\n  on  n2.oid = c2.relnamespace)"
		"\n inner join pg_catalog.pg_attribute a2"
		"\n  on  a2.attrelid = c2.oid"
		"\n  and  a2.attnum = confkey[i])"
		"\n left outer join pg_catalog.pg_constraint cn"
		"\n  on cn.conrelid = ref.confrelid"
		"\n  and cn.contype = 'p')"
		, catName
		, scmName1
		, catName
		, scmName2
		, SQL_CASCADE
		, SQL_SET_NULL
		, SQL_SET_DEFAULT
		, SQL_RESTRICT
		, SQL_NO_ACTION
		, SQL_CASCADE
		, SQL_SET_NULL
		, SQL_SET_DEFAULT
		, SQL_RESTRICT
		, SQL_NO_ACTION
		, SQL_INITIALLY_DEFERRED
		, SQL_INITIALLY_IMMEDIATE
		, SQL_NOT_DEFERRABLE
		, relqual
		, eq_string, escTableName
		, eq_string, escSchemaName);

	free(escSchemaName);
	if (NULL != pk_table_needed &&
	    NULL != fk_table_needed)
	{
		free(escTableName);
		escTableName = simpleCatalogEscape((SQLCHAR *) pk_table_needed, SQL_NTS, conn);
		SPRINTFCAT_FIXED(tables_query,
				"\n where c2.relname %s'%s'",
				eq_string, escTableName);
	}
	STRCAT_FIXED(tables_query, "\n  order by ref.oid, ref.i");

	if (res = CC_send_query(conn, tables_query, NULL, READ_ONLY_QUERY, stmt), !QR_command_maybe_successful(res))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "PGAPI_ForeignKeys query error", func);
		QR_Destructor(res);
		goto cleanup;
	}
	SC_set_Result(stmt, res);
	ret = SQL_SUCCESS;

cleanup:
#undef	return

	/*
	 * also, things need to think that this statement is finished so the
	 * results can be retrieved.
	 */
	if (SQL_SUCCEEDED(ret))
	{
		stmt->status = STMT_FINISHED;
		extend_column_bindings(SC_get_ARDF(stmt), QR_NumResultCols(res));
	}
	if (pk_table_needed)
		free(pk_table_needed);
	if (escTableName)
		free(escTableName);
	if (fk_table_needed)
		free(fk_table_needed);
	/* set up the current tuple pointer for SQLFetch */
	stmt->currTuple = -1;
	SC_set_rowset_start(stmt, -1, FALSE);
	SC_set_current_col(stmt, -1);

	mylog("%s(): EXIT, stmt=%p, ret=%d\n", func, stmt, ret);
	return ret;
}
