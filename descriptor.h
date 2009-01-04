/* File:			descriptor.h
 *
 * Description:		This file contains defines and declarations that are related to
 *					the entire driver.
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 * $Id: descriptor.h,v 1.22 2009/01/04 02:40:02 hinoue Exp $
 *
 */

#ifndef __DESCRIPTOR_H__
#define __DESCRIPTOR_H__

#include "psqlodbc.h"

typedef struct
{
	char	*name;
} pgNAME;
#define	GET_NAME(the_name)	((the_name).name)
#define	SAFE_NAME(the_name)	((the_name).name ? (the_name).name : NULL_STRING)
#define	PRINT_NAME(the_name)	((the_name).name ? (the_name).name : PRINT_NULL)
#define	NAME_IS_NULL(the_name)	(NULL == (the_name).name)
#define	NAME_IS_VALID(the_name)	(NULL != (the_name).name)
#define	INIT_NAME(the_name) ((the_name).name = NULL)
#define	NULL_THE_NAME(the_name) \
do { \
	if ((the_name).name) free((the_name).name); \
	(the_name).name = NULL; \
} while (0)
#define	STR_TO_NAME(the_name, str) \
do { \
	if ((the_name).name) \
		free((the_name).name); \
	(the_name).name = (str ? strdup((str)) : NULL); \
} while (0)
/*
 * a modified version of macro STR_TO_NAME to suppress compiler warnings
 * when the compiler may confirm str != NULL.
 */ 
#define	STRX_TO_NAME(the_name, str) \
do { \
	if ((the_name).name) \
		free((the_name).name); \
	(the_name).name = strdup((str)); \
} while (0)

#define	STRN_TO_NAME(the_name, str, n) \
do { \
	if ((the_name).name) \
		free((the_name).name); \
	if (str) \
	{ \
		(the_name).name = malloc(n + 1); \
		memcpy((the_name).name, str, n); \
		(the_name).name[n] = '\0'; \
	} \
	else \
		(the_name).name = NULL; \
} while (0)
#define	NAME_TO_STR(str, the_name) \
do {\
	if ((the_name).name) strcpy(str, (the_name).name); \
	else *str = '\0'; \
} while (0)
#define	NAME_TO_NAME(to, from) \
do { \
	if ((to).name) \
		free((to).name); \
	if ((from).name) \
		(to).name = strdup(from.name); \
	else \
		(to).name = NULL; \
} while (0)
#define	MOVE_NAME(to, from) \
do { \
	if ((to).name) \
		free((to).name); \
	(to).name = (from).name; \
	(from).name = NULL; \
} while (0)
#define	SET_NAME(the_name, str) ((the_name).name = (str))

#define	NAMECMP(name1, name2) (strcmp(SAFE_NAME(name1), SAFE_NAME(name2)))
#define	NAMEICMP(name1, name2) (stricmp(SAFE_NAME(name1), SAFE_NAME(name2)))

enum {
	TI_UPDATABLE	=	1L
	,TI_HASOIDS_CHECKED	=	(1L << 1)
	,TI_HASOIDS	=	(1L << 2)
	,TI_COLATTRIBUTE	=	(1L << 3)
};
typedef struct
{
	OID		table_oid;
	COL_INFO	*col_info; /* cached SQLColumns info for this table */
	pgNAME		schema_name;
	pgNAME		table_name;
	pgNAME		table_alias;
	pgNAME		bestitem;
	pgNAME		bestqual;
	UInt4		flags;
} TABLE_INFO;
#define	TI_set_updatable(ti)	(ti->flags |= TI_UPDATABLE)
#define	TI_is_updatable(ti)	(0 != (ti->flags &= TI_UPDATABLE))
#define	TI_no_updatable(ti)	(ti->flags &= (~TI_UPDATABLE))
#define	TI_set_hasoids_checked(ti)	(ti->flags |= TI_HASOIDS_CHECKED)
#define	TI_checked_hasoids(ti)		(0 != (ti->flags &= TI_HASOIDS))
#define	TI_set_hasoids(ti)	(ti->flags |= TI_HASOIDS)
#define	TI_has_oids(ti)		(0 != (ti->flags &= TI_HASOIDS))
#define	TI_set_has_no_oids(ti)	(ti->flags &= (~TI_HASOIDS))
void	TI_Constructor(TABLE_INFO *, const ConnectionClass *);
void	TI_Destructor(TABLE_INFO **, int);

enum {
	FIELD_INITIALIZED 	= 0
	,FIELD_PARSING		= 1L
	,FIELD_TEMP_SET		= (1L << 1)
	,FIELD_COL_ATTRIBUTE	= (1L << 2)
	,FIELD_PARSED_OK	= (1L << 3)
	,FIELD_PARSED_INCOMPLETE = (1L << 4)
};
typedef struct
{
	char		flag;
	char		updatable;
	Int2		attnum;
	pgNAME		schema_name;
	TABLE_INFO *ti;	/* to resolve explicit table names */
	pgNAME		column_name;
	pgNAME		column_alias;
	char		nullable;
	char		auto_increment;
	char		func;
	char		columnkey;
	int		column_size; /* precision in 2.x */
	int		decimal_digits; /* scale in 2.x */
	int		display_size;
	SQLLEN		length;
	OID		columntype;
	OID		basetype; /* may be the basetype when the column type is a domain */ 
	char		expr;
	char		quote;
	char		dquote;
	char		numeric;
	pgNAME		before_dot;
} FIELD_INFO;
Int4 FI_precision(const FIELD_INFO *);
Int4 FI_scale(const FIELD_INFO *);
void	FI_Constructor(FIELD_INFO *, BOOL reuse);
void	FI_Destructor(FIELD_INFO **, int, BOOL freeFI);
#define	FI_is_applicable(fi) (NULL != fi && (fi->flag & (FIELD_PARSED_OK | FIELD_COL_ATTRIBUTE)) != 0)
#define	FI_type(fi) (0 == (fi)->basetype ? (fi)->columntype : (fi)->basetype)

typedef struct DescriptorHeader_
{
	ConnectionClass	*conn_conn;
	char	embedded;
	char	type_defined;
	UInt4	desc_type;
	UInt4	error_row;	/* 1-based row */
	UInt4	error_index;	/* 1-based index */
	Int4	__error_number;
	char	*__error_message;
	PG_ErrorInfo	*pgerror;
} DescriptorClass;

/*
 *	ARD and APD are(must be) of the same format
 */
struct ARDFields_
{
#if (ODBCVER >= 0x0300)
	SQLLEN		size_of_rowset; /* for ODBC3 fetch operation */
#endif /* ODBCVER */
	SQLUINTEGER	bind_size;	/* size of each structure if using
					 * Row-wise Binding */
	SQLUSMALLINT	*row_operation_ptr;
	SQLULEN		*row_offset_ptr;
	BindInfoClass	*bookmark;
	BindInfoClass	*bindings;
	SQLSMALLINT	allocated;
	SQLLEN		size_of_rowset_odbc2; /* for SQLExtendedFetch */
};

/*
 *	APD must be of the same format as ARD
 */
struct APDFields_
{
	SQLLEN		paramset_size;	/* really an SQLINTEGER type */ 
	SQLUINTEGER	param_bind_type; /* size of each structure if using
					  * Row-wise Parameter Binding */
	SQLUSMALLINT	*param_operation_ptr;
	SQLULEN		*param_offset_ptr;
	ParameterInfoClass	*bookmark; /* dummy item to fit APD to ARD */
	ParameterInfoClass	*parameters;
	SQLSMALLINT	allocated;
	SQLLEN		paramset_size_dummy; /* dummy item to fit APD to ARD */
};

struct IRDFields_
{
	StatementClass	*stmt;
	SQLULEN		*rowsFetched;
	SQLUSMALLINT	*rowStatusArray;
	UInt4		nfields;
	SQLSMALLINT	allocated;
	FIELD_INFO	**fi;
};

struct IPDFields_
{
#if (ODBCVER >= 0x0300)
	SQLUINTEGER		*param_processed_ptr;
#else
	SQLULEN			*param_processed_ptr; /* SQLParamOptions */
#endif /* ODBCVER */
	SQLUSMALLINT		*param_status_ptr;
	SQLSMALLINT		allocated;
	ParameterImplClass	*parameters;
};

typedef	struct
{
	DescriptorClass	deschd;
	union {
		ARDFields	ard;
		APDFields	apd;
		IRDFields	ird;
		IPDFields	ipd;
	} flds;
}	DescriptorAlloc;
typedef struct
{
	DescriptorClass	deschd;
	ARDFields	ardopts;
}	ARDClass;
typedef struct
{
	DescriptorClass	deschd;
	APDFields	apdopts;
}	APDClass;
typedef struct
{
	DescriptorClass	deschd;
	IRDFields	irdopts;
}	IRDClass;
typedef struct
{
	DescriptorClass	deschd;
	IPDFields	ipdopts;
}	IPDClass;

#define	DC_get_conn(a)	(a->conn_conn)

void InitializeEmbeddedDescriptor(DescriptorClass *, StatementClass *stmt,
				UInt4 desc_type);
void	DC_Destructor(DescriptorClass *desc);
void	InitializeARDFields(ARDFields *self);
void	InitializeAPDFields(APDFields *self);
/* void	InitializeIRDFields(IRDFields *self);
void	InitializeIPDFiedls(IPDFields *self); */
BindInfoClass	*ARD_AllocBookmark(ARDFields *self);
void	ARD_unbind_cols(ARDFields *self, BOOL freeall);
void	APD_free_params(APDFields *self, char option);
void	IPD_free_params(IPDFields *self, char option);
BOOL	getCOLIfromTI(const char *, ConnectionClass *, StatementClass *, const OID, TABLE_INFO **);
#if (ODBCVER >= 0x0300)
RETCODE	DC_set_stmt(DescriptorClass *desc, StatementClass *stmt);
void	DC_clear_error(DescriptorClass *desc);
void	DC_set_error(DescriptorClass *desc, int errornumber, const char * errormsg);
void	DC_set_errormsg(DescriptorClass *desc, const char * errormsg);
PG_ErrorInfo *DC_get_error(DescriptorClass *self);
int	DC_get_errornumber(const DescriptorClass *self);
const char *DC_get_errormsg(const DescriptorClass *self);
void	DC_log_error(const char *func, const char *desc, const DescriptorClass *self);
#endif /* ODBCVER */

/*	Error numbers about descriptor handle */
enum {
	LOWEST_DESC_ERROR		= -2
	/* minus means warning/notice message */
	,DESC_ERROR_IN_ROW		= -2
	,DESC_OPTION_VALUE_CHANGED	= -1
	,DESC_OK			= 0
	,DESC_EXEC_ERROR
	,DESC_STATUS_ERROR
	,DESC_SEQUENCE_ERROR
	,DESC_NO_MEMORY_ERROR
	,DESC_COLNUM_ERROR
	,DESC_NO_STMTSTRING
	,DESC_ERROR_TAKEN_FROM_BACKEND
	,DESC_INTERNAL_ERROR
	,DESC_STILL_EXECUTING
	,DESC_NOT_IMPLEMENTED_ERROR
	,DESC_BAD_PARAMETER_NUMBER_ERROR
	,DESC_OPTION_OUT_OF_RANGE_ERROR
	,DESC_INVALID_COLUMN_NUMBER_ERROR
	,DESC_RESTRICTED_DATA_TYPE_ERROR
	,DESC_INVALID_CURSOR_STATE_ERROR
	,DESC_CREATE_TABLE_ERROR
	,DESC_NO_CURSOR_NAME
	,DESC_INVALID_CURSOR_NAME
	,DESC_INVALID_ARGUMENT_NO
	,DESC_ROW_OUT_OF_RANGE
	,DESC_OPERATION_CANCELLED
	,DESC_INVALID_CURSOR_POSITION
	,DESC_VALUE_OUT_OF_RANGE
	,DESC_OPERATION_INVALID	
	,DESC_PROGRAM_TYPE_OUT_OF_RANGE
	,DESC_BAD_ERROR
	,DESC_INVALID_OPTION_IDENTIFIER
	,DESC_RETURN_NULL_WITHOUT_INDICATOR
	,DESC_INVALID_DESCRIPTOR_IDENTIFIER
	,DESC_OPTION_NOT_FOR_THE_DRIVER
	,DESC_FETCH_OUT_OF_RANGE
	,DESC_COUNT_FIELD_INCORRECT
};
#endif /* __DESCRIPTOR_H__ */
