/* File:			bind.h
 *
 * Description:		See "bind.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __BIND_H__
#define __BIND_H__

#include "psqlodbc.h"
#ifdef USE_LIBPQ
#include <libpq-fe.h>
#endif /* USE_LIBPQ */
/*
 * BindInfoClass -- stores information about a bound column
 */
struct BindInfoClass_
{
	Int4	buflen;			/* size of buffer */
	char	*buffer;		/* pointer to the buffer */
	Int4	*used;			/* used space in the buffer (for strings
					 * not counting the '\0') */
	Int2	returntype;		/* kind of conversion to be applied when
					 * returning (SQL_C_DEFAULT,
					 * SQL_C_CHAR... etc) */
	Int2	precision;		/* the precision for numeric or timestamp type */
	Int2	scale;			/* the scale for numeric type */
	/* area for work variables */
	char	dummy_data;		/* currently not used */		
};
typedef struct
{
	char	*ttlbuf;		/* to save the large result */
	Int4	ttlbuflen;		/* the buffer length */
	Int4	ttlbufused;		/* used length of the buffer */
	Int4	data_left;		/* amount of data left to read
					 * (SQLGetData) */
}	GetDataClass;

/*
 * ParameterInfoClass -- stores information about a bound parameter
 */
struct ParameterInfoClass_
{
	Int4	buflen;
	char	*buffer;
	Int4	*used;
	Int2	CType;
	Int2	precision;	/* the precision for numeric or timestamp type */
	Int2	scale;		/* the scale for numeric type */
	/* area for work variables */
	char	data_at_exec;
};

typedef struct 
{
	Int4	*EXEC_used;	/* amount of data */
	char	*EXEC_buffer; 	/* the data */
	Oid	lobj_oid;
}	PutDataClass;

/*
 * ParameterImplClass -- stores implemntation information about a parameter
 */
struct ParameterImplClass_
{
	Int2		paramType;
	Int2		SQLType;
	Int4		PGType;
	UInt4		column_size;
	Int2		decimal_digits;
	Int2		precision;	/* the precision for numeric or timestamp type */
	Int2		scale;		/* the scale for numeric type */
};

typedef struct
{
	GetDataClass	fdata;
	Int4		allocated;
	GetDataClass	*gdata;
}	GetDataInfo;
typedef struct
{
	Int4		allocated;
	PutDataClass	*pdata;
}	PutDataInfo;

void	extend_column_bindings(ARDFields *opts, int num_columns);
void	reset_a_column_binding(ARDFields *opts, int icol);
void	extend_parameter_bindings(APDFields *opts, int num_params);
void	extend_iparameter_bindings(IPDFields *opts, int num_params);
void	reset_a_parameter_binding(APDFields *opts, int ipar);
void	reset_a_iparameter_binding(IPDFields *opts, int ipar);
void	GetDataInfoInitialize(GetDataInfo *gdata);
void	extend_getdata_info(GetDataInfo *gdata, int num_columns, BOOL shrink);
void	reset_a_getdata_info(GetDataInfo *gdata, int icol);
void	GDATA_unbind_cols(GetDataInfo *gdata, BOOL freeall);
void	PutDataInfoInitialize(PutDataInfo *pdata);
void	extend_putdata_info(PutDataInfo *pdata, int num_params, BOOL shrink);
void	reset_a_putdata_info(PutDataInfo *pdata, int ipar);
void	PDATA_free_params(PutDataInfo *pdata, char option);

#endif
