/* File:			descriptor.h
 *
 * Description:		This file contains defines and declarations that are related to
 *					the entire driver.
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 * $Id: descriptor.h,v 1.13 2004/07/21 12:29:58 dpage Exp $
 *
 */

#ifndef __DESCRIPTOR_H__
#define __DESCRIPTOR_H__

#include "psqlodbc.h"

typedef struct
{
	COL_INFO	*col_info; /* cached SQLColumns info for this table */
	char		schema[SCHEMA_NAME_STORAGE_LEN + 1];
	char		name[TABLE_NAME_STORAGE_LEN + 1];
	char		alias[TABLE_NAME_STORAGE_LEN + 1];
	char		updatable;
} TABLE_INFO;

typedef struct
{
	TABLE_INFO *ti;		/* resolve to explicit table names */
	int			column_size; /* precision in 2.x */
	int			decimal_digits; /* scale in 2.x */
	int			display_size;
	int			length;
	int			type;
	char		nullable;
	char		func;
	char		expr;
	char		quote;
	char		dquote;
	char		numeric;
	char		updatable;
	char		dot[TABLE_NAME_STORAGE_LEN + 1];
	char		name[COLUMN_NAME_STORAGE_LEN + 1];
	char		alias[COLUMN_NAME_STORAGE_LEN + 1];
	char		*schema;
} FIELD_INFO;
Int4 FI_precision(const FIELD_INFO *);
Int4 FI_scale(const FIELD_INFO *);

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
	int		size_of_rowset; /* for ODBC3 fetch operation */
	int		bind_size;	/* size of each structure if using
					 * Row-wise Binding */
	UInt2		*row_operation_ptr;
	UInt4		*row_offset_ptr;
	BindInfoClass	*bookmark;
	BindInfoClass	*bindings;
	int		allocated;
	int		size_of_rowset_odbc2; /* for SQLExtendedFetch */
};

/*
 *	APD must be of the same format as ARD
 */
struct APDFields_
{
	int		paramset_size;
	int		param_bind_type; /* size of each structure if using
					  * Row-wsie Parameter Binding */
	UInt2		*param_operation_ptr;
	UInt4		*param_offset_ptr;
	ParameterInfoClass	*bookmark; /* dummy item to fit APD to ARD */
	ParameterInfoClass	*parameters;
	int		allocated;
	int		paramset_size_dummy; /* dummy item to fit APD to ARD */
};

struct IRDFields_
{
	StatementClass	*stmt;
	UInt4		*rowsFetched;
	UInt2		*rowStatusArray;
	UInt4		nfields;
	FIELD_INFO	**fi;
};

struct IPDFields_
{
	UInt4		*param_processed_ptr;
	UInt2		*param_status_ptr;
	ParameterImplClass	*parameters;
	int			allocated;
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
RETCODE	DC_set_stmt(DescriptorClass *desc, StatementClass *stmt);
void	DC_clear_error(DescriptorClass *desc);
void	DC_set_error(DescriptorClass *desc, int errornumber, const char * errormsg);
void	DC_set_errormsg(DescriptorClass *desc, const char * errormsg);
PG_ErrorInfo *DC_get_error(DescriptorClass *self);
int	DC_get_errornumber(const DescriptorClass *self);
const char *DC_get_errormsg(const DescriptorClass *self);
void	DC_log_error(const char *func, const char *desc, const DescriptorClass *self);

/*	Error numbers about descriptor handle */
#define DESC_OK						0
#define DESC_EXEC_ERROR					1
#define DESC_STATUS_ERROR				2
#define DESC_SEQUENCE_ERROR				3
#define DESC_NO_MEMORY_ERROR				4
#define DESC_COLNUM_ERROR				5
#define DESC_NO_STMTSTRING				6
#define DESC_ERROR_TAKEN_FROM_BACKEND			7
#define DESC_INTERNAL_ERROR				8
#define DESC_STILL_EXECUTING				9
#define DESC_NOT_IMPLEMENTED_ERROR			10
#define DESC_BAD_PARAMETER_NUMBER_ERROR			11
#define DESC_OPTION_OUT_OF_RANGE_ERROR			12
#define DESC_INVALID_COLUMN_NUMBER_ERROR		13
#define DESC_RESTRICTED_DATA_TYPE_ERROR			14
#define DESC_INVALID_CURSOR_STATE_ERROR			15
#define DESC_OPTION_VALUE_CHANGED			16
#define DESC_CREATE_TABLE_ERROR				17
#define DESC_NO_CURSOR_NAME				18
#define DESC_INVALID_CURSOR_NAME			19
#define DESC_INVALID_ARGUMENT_NO			20
#define DESC_ROW_OUT_OF_RANGE				21
#define DESC_OPERATION_CANCELLED			22
#define DESC_INVALID_CURSOR_POSITION			23
#define DESC_VALUE_OUT_OF_RANGE				24
#define DESC_OPERATION_INVALID				25
#define DESC_PROGRAM_TYPE_OUT_OF_RANGE			26
#define DESC_BAD_ERROR					27
#define DESC_INVALID_OPTION_IDENTIFIER			28
#define DESC_RETURN_NULL_WITHOUT_INDICATOR		29
#define DESC_ERROR_IN_ROW				30
#define DESC_INVALID_DESCRIPTOR_IDENTIFIER		31
#define DESC_OPTION_NOT_FOR_THE_DRIVER			32
#define DESC_FETCH_OUT_OF_RANGE				33
#define DESC_COUNT_FIELD_INCORRECT			34

#endif /* __DESCRIPTOR_H__ */
