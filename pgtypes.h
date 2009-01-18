/* File:			pgtypes.h
 *
 * Description:		See "pgtypes.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __PGTYPES_H__
#define __PGTYPES_H__

#include "psqlodbc.h"

/* the type numbers are defined by the OID's of the types' rows */
/* in table pg_type */


#ifdef NOT_USED
#define PG_TYPE_LO				????	/* waiting for permanent type */
#endif

#define	MS_ACCESS_SERIAL		"int identity"
#define PG_TYPE_BOOL			16
#define PG_TYPE_BYTEA			17
#define PG_TYPE_CHAR			18
#define PG_TYPE_NAME			19
#define PG_TYPE_INT8			20
#define PG_TYPE_INT2			21
#define PG_TYPE_INT2VECTOR		22
#define PG_TYPE_INT4			23
#define PG_TYPE_REGPROC			24
#define PG_TYPE_TEXT			25
#define PG_TYPE_OID				26
#define PG_TYPE_TID				27
#define PG_TYPE_XID				28
#define PG_TYPE_CID				29
#define PG_TYPE_OIDVECTOR		30
#define PG_TYPE_SET				32
#define PG_TYPE_XML			142
#define PG_TYPE_XMLARRAY		143
#define PG_TYPE_CHAR2			409
#define PG_TYPE_CHAR4			410
#define PG_TYPE_CHAR8			411
#define PG_TYPE_POINT			600
#define PG_TYPE_LSEG			601
#define PG_TYPE_PATH			602
#define PG_TYPE_BOX				603
#define PG_TYPE_POLYGON			604
#define PG_TYPE_FILENAME		605
#define PG_TYPE_CIDR			650
#define PG_TYPE_FLOAT4			700
#define PG_TYPE_FLOAT8			701
#define PG_TYPE_ABSTIME			702
#define PG_TYPE_RELTIME			703
#define PG_TYPE_TINTERVAL		704
#define PG_TYPE_UNKNOWN			705
#define PG_TYPE_MONEY			790
#define PG_TYPE_OIDINT2			810
#define PG_TYPE_MACADDR			829
#define PG_TYPE_INET			869
#define PG_TYPE_OIDINT4			910
#define PG_TYPE_OIDNAME			911
#define PG_TYPE_TEXTARRAY		1009
#define PG_TYPE_BPCHARARRAY		1014
#define PG_TYPE_VARCHARARRAY		1015
#define PG_TYPE_BPCHAR			1042
#define PG_TYPE_VARCHAR			1043
#define PG_TYPE_DATE			1082
#define PG_TYPE_TIME			1083
#define PG_TYPE_TIMESTAMP_NO_TMZONE	1114		/* since 7.2 */
#define PG_TYPE_DATETIME		1184
#define PG_TYPE_TIME_WITH_TMZONE	1266		/* since 7.1 */
#define PG_TYPE_TIMESTAMP		1296	/* deprecated since 7.0 */
#define PG_TYPE_NUMERIC			1700
#define PG_TYPE_RECORD			2249
#define PG_TYPE_VOID			2278
#define PG_TYPE_UUID			2950
#define INTERNAL_ASIS_TYPE		(-9999)

/* extern Int4 pgtypes_defined[]; */
extern SQLSMALLINT sqlTypes[];

/*	Defines for pgtype_precision */
#define PG_STATIC				(-1)

OID		pg_true_type(const ConnectionClass *, OID, OID);
OID		sqltype_to_pgtype(StatementClass *stmt, SQLSMALLINT fSqlType);

SQLSMALLINT	pgtype_to_concise_type(StatementClass *stmt, OID type, int col);
SQLSMALLINT	pgtype_to_sqldesctype(StatementClass *stmt, OID type, int col);
SQLSMALLINT	pgtype_to_datetime_sub(StatementClass *stmt, OID type);
SQLSMALLINT	pgtype_to_ctype(StatementClass *stmt, OID type);
const char	*pgtype_to_name(StatementClass *stmt, OID type, BOOL auto_increment);

/*	These functions can use static numbers or result sets(col parameter) */
Int4		pgtype_column_size(StatementClass *stmt, OID type, int col, int handle_unknown_size_as); /* corresponds to "precision" in ODBC 2.x */
SQLSMALLINT	pgtype_precision(StatementClass *stmt, OID type, int col, int handle_unknown_size_as); /* "precsion in ODBC 3.x */
/* the following size/length are of Int4 due to PG restriction */ 
Int4		pgtype_display_size(StatementClass *stmt, OID type, int col, int handle_unknown_size_as);
Int4		pgtype_buffer_length(StatementClass *stmt, OID type, int col, int handle_unknown_size_as);
Int4		pgtype_desclength(StatementClass *stmt, OID type, int col, int handle_unknown_size_as);
Int4		pgtype_transfer_octet_length(StatementClass *stmt, OID type, int column_size);

SQLSMALLINT	pgtype_decimal_digits(StatementClass *stmt, OID type, int col); /* corresponds to "scale" in ODBC 2.x */
SQLSMALLINT	pgtype_min_decimal_digits(StatementClass *stmt, OID type); /* corresponds to "min_scale" in ODBC 2.x */
SQLSMALLINT	pgtype_max_decimal_digits(StatementClass *stmt, OID type); /* corresponds to "max_scale" in ODBC 2.x */
SQLSMALLINT	pgtype_scale(StatementClass *stmt, OID type, int col); /* ODBC 3.x " */
Int2		pgtype_radix(StatementClass *stmt, OID type);
Int2		pgtype_nullable(StatementClass *stmt, OID type);
Int2		pgtype_auto_increment(StatementClass *stmt, OID type);
Int2		pgtype_case_sensitive(StatementClass *stmt, OID type);
Int2		pgtype_money(StatementClass *stmt, OID type);
Int2		pgtype_searchable(StatementClass *stmt, OID type);
Int2		pgtype_unsigned(StatementClass *stmt, OID type);
const char	*pgtype_literal_prefix(StatementClass *stmt, OID type);
const char	*pgtype_literal_suffix(StatementClass *stmt, OID type);
const char	*pgtype_create_params(StatementClass *stmt, OID type);

SQLSMALLINT	sqltype_to_default_ctype(const ConnectionClass *stmt, SQLSMALLINT sqltype);
Int4		ctype_length(SQLSMALLINT ctype);

#define	USE_ZONE	FALSE
#endif
