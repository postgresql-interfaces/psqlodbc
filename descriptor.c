/*-------
 * Module:			descriptor.c
 *
 * Description:		This module contains functions related to creating
 *					and manipulating a statement.
 *
 * Classes:			DescriptorClass (Functions prefix: "DC_")
 *
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#include "environ.h"
#include "connection.h"
#include "descriptor.h"
#include "statement.h"
#include "qresult.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pgapifunc.h"


void	TI_Constructor(TABLE_INFO *self, const ConnectionClass *conn)
{
	memset(self, 0, sizeof(TABLE_INFO));
	TI_set_updatable(self);
	if (PG_VERSION_LT(conn, 7.2))
	{
		char	qual[32];

		STR_TO_NAME(self->bestitem, OID_NAME);
		sprintf(qual, "\"%s\" = %%u", OID_NAME);
		STRX_TO_NAME(self->bestqual, qual);
		TI_set_hasoids(self);
		TI_set_hasoids_checked(self);
	}
}
void	TI_Destructor(TABLE_INFO **ti, int count)
{
	int	i;

inolog("TI_Destructor count=%d\n", count);
	if (ti)
	{
		for (i = 0; i < count; i++)
		{
			if (ti[i])
			{
				COL_INFO *coli = ti[i]->col_info;
				if (coli)
				{
mylog("!!!refcnt %p:%d -> %d\n", coli, coli->refcnt, coli->refcnt - 1);
					coli->refcnt--;
					if (coli->refcnt <= 0 && 0 == coli->acc_time) /* acc_time == 0 means the table is dropped */
						free_col_info_contents(coli);
				}
				NULL_THE_NAME(ti[i]->schema_name);
				NULL_THE_NAME(ti[i]->table_name);
				NULL_THE_NAME(ti[i]->table_alias);
				NULL_THE_NAME(ti[i]->bestitem);
				NULL_THE_NAME(ti[i]->bestqual);
				free(ti[i]);
				ti[i] = NULL;
			}
		}
	}
}
void	FI_Constructor(FIELD_INFO *self, BOOL reuse)
{
inolog("FI_Constructor reuse=%d\n", reuse);
	if (reuse)
		FI_Destructor(&self, 1, FALSE);
	memset(self, 0, sizeof(FIELD_INFO));
	self->nullable = TRUE;
	self->columnkey = -1;
	self->typmod = -1;
}
void	FI_Destructor(FIELD_INFO **fi, int count, BOOL freeFI)
{
	int	i;

inolog("FI_Destructor count=%d\n", count);
	if (fi)
	{
		for (i = 0; i < count; i++)
		{
			if (fi[i])
			{
				NULL_THE_NAME(fi[i]->column_name);
				NULL_THE_NAME(fi[i]->column_alias);
				NULL_THE_NAME(fi[i]->schema_name);
				NULL_THE_NAME(fi[i]->before_dot);
				if (freeFI)
				{
					free(fi[i]);
					fi[i] = NULL;
				}
			}
		}
		if (freeFI)
			free(fi);
	}
}

void	DC_Constructor(DescriptorClass *self, BOOL embedded, StatementClass *stmt)
{
	memset(self, 0, sizeof(DescriptorClass));
	self->embedded = embedded;
}

static void ARDFields_free(ARDFields * self)
{
inolog("ARDFields_free %p bookmark=%p", self, self->bookmark);
	if (self->bookmark)
	{
		free(self->bookmark);
		self->bookmark = NULL;
	}
inolog(" hey");
	/*
	 * the memory pointed to by the bindings is not deallocated by the
	 * driver but by the application that uses that driver, so we don't
	 * have to care
	 */
	ARD_unbind_cols(self, TRUE);
}

static void APDFields_free(APDFields * self)
{
	if (self->bookmark)
	{
		free(self->bookmark);
		self->bookmark = NULL;
	}
	/* param bindings */
	APD_free_params(self, STMT_FREE_PARAMS_ALL);
}

static void IRDFields_free(IRDFields * self)
{
	/* Free the parsed field information */
	if (self->fi)
	{
		FI_Destructor(self->fi, self->allocated, TRUE);
		self->fi = NULL;
	}
	self->allocated = 0;
	self->nfields = 0;
}

static void IPDFields_free(IPDFields * self)
{
	/* param bindings */
	IPD_free_params(self, STMT_FREE_PARAMS_ALL);
}

void	DC_Destructor(DescriptorClass *self)
{
	if (self->__error_message)
	{
		free(self->__error_message);
		self->__error_message = NULL;
	}
	if (self->pgerror)
	{
		ER_Destructor(self->pgerror);
		self->pgerror = NULL;
	}
	if (self->type_defined)
	{
		switch (self->desc_type)
		{
			case SQL_ATTR_APP_ROW_DESC:
				ARDFields_free((ARDFields *) (self + 1));
				break;
			case SQL_ATTR_APP_PARAM_DESC:
				APDFields_free((APDFields *) (self + 1));
				break;
			case SQL_ATTR_IMP_ROW_DESC:
				IRDFields_free((IRDFields *) (self + 1));
				break;
			case SQL_ATTR_IMP_PARAM_DESC:
				IPDFields_free((IPDFields *) (self + 1));
				break;
		}
	}
}

void InitializeEmbeddedDescriptor(DescriptorClass *desc, StatementClass *stmt,
		 UInt4 desc_type) 
{
	DC_Constructor(desc, TRUE, stmt);
	DC_get_conn(desc) = SC_get_conn(stmt);
	desc->type_defined = TRUE;
	desc->desc_type = desc_type;
	switch (desc_type)
	{
		case SQL_ATTR_APP_ROW_DESC:
			memset(desc + 1, 0, sizeof(ARDFields));
			stmt->ard = (ARDClass *) desc;
			break; 
		case SQL_ATTR_APP_PARAM_DESC:
			memset(desc + 1, 0, sizeof(APDFields));
			stmt->apd = (APDClass *) desc;
			break; 
		case SQL_ATTR_IMP_ROW_DESC:
			memset(desc + 1, 0, sizeof(IRDFields));
			stmt->ird = (IRDClass *) desc;
			stmt->ird->irdopts.stmt = stmt;
			break; 
		case SQL_ATTR_IMP_PARAM_DESC:
			memset(desc + 1, 0, sizeof(IPDFields));
			stmt->ipd = (IPDClass *) desc;
			break; 
	}
}

/*
 * ARDFields initialize
 */
void
InitializeARDFields(ARDFields *opt)
{
	memset(opt, 0, sizeof(ARDFields));
#if (ODBCVER >= 0x0300)
	opt->size_of_rowset = 1;
#endif /* ODBCVER */
	opt->bind_size = 0;		/* default is to bind by column */
	opt->size_of_rowset_odbc2 = 1;
}
/*
 * APDFields initialize
 */
void
InitializeAPDFields(APDFields *opt)
{
	memset(opt, 0, sizeof(APDFields));
	opt->paramset_size = 1;
	opt->param_bind_type = 0;	/* default is to bind by column */
	opt->paramset_size_dummy = 1;	/* dummy setting */
}

BindInfoClass	*ARD_AllocBookmark(ARDFields *ardopts)
{
	if (!ardopts->bookmark)
	{
		ardopts->bookmark = (BindInfoClass *) malloc(sizeof(BindInfoClass));
		memset(ardopts->bookmark, 0, sizeof(BindInfoClass));
	}
	return ardopts->bookmark;
}

#if (ODBCVER >= 0x0300)

#define	DESC_INCREMENT	10
char CC_add_descriptor(ConnectionClass *self, DescriptorClass *desc)
{
	int	i;

	mylog("CC_add_descriptor: self=%p, desc=%p\n", self, desc);

	for (i = 0; i < self->num_descs; i++)
	{
		if (!self->descs[i])
		{
			DC_get_conn(desc) = self;
			self->descs[i] = desc;
			return TRUE;
		}
	}
        /* no more room -- allocate more memory */
	self->descs = (DescriptorClass **) realloc(self->descs, sizeof(DescriptorClass *) * (DESC_INCREMENT + self->num_descs));
	if (!self->descs)
		return FALSE;

	memset(&self->descs[self->num_descs], 0, sizeof(DescriptorClass *) *
				DESC_INCREMENT);
        DC_get_conn(desc) = self;
	self->descs[self->num_descs] = desc;
	self->num_descs += DESC_INCREMENT;

	return TRUE;
}

/*
 *	This API allocates a Application descriptor.
 */
RETCODE SQL_API PGAPI_AllocDesc(HDBC ConnectionHandle,
					SQLHDESC *DescriptorHandle)
{
	CSTR func = "PGAPI_AllocDesc";
	ConnectionClass	*conn = (ConnectionClass *) ConnectionHandle;
	RETCODE	ret = SQL_SUCCESS;
	DescriptorClass	*desc = (DescriptorClass *) malloc(sizeof(DescriptorAlloc));

	mylog("%s: entering...\n", func);
	if (desc)
	{
		memset(desc, 0, sizeof(DescriptorAlloc));
		DC_get_conn(desc) = conn;
		if (CC_add_descriptor(conn, desc))
			*DescriptorHandle = desc;
		else
		{
			free(desc);
			CC_set_error(conn, CONN_STMT_ALLOC_ERROR, "Maximum number of descriptors exceeded", func);
			ret = SQL_ERROR;
		} 
	}
	else
	{
		CC_set_error(conn, CONN_STMT_ALLOC_ERROR, "No more memory ti allocate a further descriptor", func);
		ret = SQL_ERROR;
	}
	return ret;
}

RETCODE SQL_API PGAPI_FreeDesc(SQLHDESC DescriptorHandle)
{
	CSTR func = "PGAPI_FreeDesc";
	DescriptorClass	*desc = (DescriptorClass *) DescriptorHandle;
	RETCODE	ret = SQL_SUCCESS;

	mylog("%s: entering...\n", func);
	DC_Destructor(desc);
	if (!desc->embedded)
	{
		int	i;
		ConnectionClass	*conn = DC_get_conn(desc);

		for (i = 0; i < conn->num_descs; i++)
		{
			if (conn->descs[i] == desc)
			{
				conn->descs[i] = NULL;
				break;
			}
		}
		free(desc);
	}
	return ret;
}

static void BindInfoClass_copy(const BindInfoClass *src, BindInfoClass *target)
{
	memcpy(target, src, sizeof(BindInfoClass));
}
static void ARDFields_copy(const ARDFields *src, ARDFields *target)
{
	memcpy(target, src, sizeof(ARDFields));
	target->bookmark = NULL;
	if (src->bookmark)
	{
		BindInfoClass *bookmark = ARD_AllocBookmark(target);
		BindInfoClass_copy(src->bookmark, bookmark);
	}
	if (src->allocated <= 0)
	{
		target->allocated = 0;
		target->bindings = NULL;
	}
	else
	{
		int	i;

		target->bindings = malloc(target->allocated * sizeof(BindInfoClass));
		for (i = 0; i < target->allocated; i++)
			BindInfoClass_copy(&src->bindings[i], &target->bindings[i]);
	}
}

static void ParameterInfoClass_copy(const ParameterInfoClass *src, ParameterInfoClass *target)
{
	memcpy(target, src, sizeof(ParameterInfoClass));
}
static void APDFields_copy(const APDFields *src, APDFields *target)
{
	memcpy(target, src, sizeof(APDFields));
	if (src->bookmark)
	{
		target->bookmark = malloc(sizeof(BindInfoClass));
		ParameterInfoClass_copy(src->bookmark, target->bookmark);
	}
	if (src->allocated <= 0)
	{
		target->allocated = 0;
		target->parameters = NULL;
	}
	else
	{
		int	i;

		target->parameters = malloc(target->allocated * sizeof(ParameterInfoClass));
		for (i = 0; i < target->allocated; i++)
			ParameterInfoClass_copy(&src->parameters[i], &target->parameters[i]);
	}
}

static void ParameterImplClass_copy(const ParameterImplClass *src, ParameterImplClass *target)
{
	memcpy(target, src, sizeof(ParameterImplClass));
}
static void IPDFields_copy(const IPDFields *src, IPDFields *target)
{
	memcpy(target, src, sizeof(IPDFields));
	if (src->allocated <= 0)
	{
		target->allocated = 0;
		target->parameters = NULL;
	}
	else
	{
		int	i;

		target->parameters = (ParameterImplClass *) malloc(target->allocated * sizeof(ParameterInfoClass));
		for (i = 0; i < target->allocated; i++)
			ParameterImplClass_copy(&src->parameters[i], &target->parameters[i]);
	}
}

RETCODE	SQL_API
PGAPI_CopyDesc(SQLHDESC SourceDescHandle,
			SQLHDESC TargetDescHandle)
{
	CSTR func = "PGAPI_CopyDesc";
	RETCODE ret = SQL_ERROR;
	DescriptorClass	*src, *target;
	ARDFields	*ard_src, *ard_tgt;
	APDFields	*apd_src, *apd_tgt;
	IPDFields	*ipd_src, *ipd_tgt;

	mylog("%s: entering...\n", func);
	src = (DescriptorClass *) SourceDescHandle;
	target = (DescriptorClass *) TargetDescHandle;
	if (!src->type_defined)
	{
		mylog("source type undefined\n");
		DC_set_error(target, DESC_EXEC_ERROR, "source handle type undefined");
		return ret;
	}
	if (target->type_defined)
	{
inolog("source type=%d -> target type=%d\n", src->desc_type, target->desc_type);
		if (SQL_ATTR_IMP_ROW_DESC == target->desc_type)
		{
			mylog("can't modify IRD\n");
			DC_set_error(target, DESC_EXEC_ERROR, "can't copy to IRD");
			return ret;
		}
		else if (target->desc_type != src->desc_type)
		{
			mylog("src type != target type\n");
			DC_set_error(target, DESC_EXEC_ERROR, "src descriptor != target type");
			return ret;
		}
		DC_Destructor(target);
	}
	ret = SQL_SUCCESS;
	switch (src->desc_type)
	{
		case SQL_ATTR_APP_ROW_DESC:
inolog("src=%p target=%p type=%d", src, target, src->desc_type);
			if (!target->type_defined)
			{
				target->desc_type = src->desc_type;
			}
			ard_src = (ARDFields *) (src + 1);
inolog(" rowset_size=%d bind_size=%d ope_ptr=%p off_ptr=%p\n",
ard_src->size_of_rowset, ard_src->bind_size,
ard_src->row_operation_ptr, ard_src->row_offset_ptr);
			ard_tgt = (ARDFields *) (target + 1);
inolog(" target=%p", ard_tgt);
			ARDFields_copy(ard_src, ard_tgt);
inolog(" offset_ptr=%p\n", ard_tgt->row_offset_ptr);
			break;
		case SQL_ATTR_APP_PARAM_DESC:
			if (!target->type_defined)
			{
				target->desc_type = src->desc_type;
			}
			apd_src = (APDFields *) (src + 1);
			apd_tgt = (APDFields *) (target + 1);
			APDFields_copy(apd_src, apd_tgt);
			break;
		case SQL_ATTR_IMP_PARAM_DESC:
			if (!target->type_defined)
			{
				target->desc_type = src->desc_type;
			}
			ipd_src = (IPDFields *) (src + 1);
			ipd_tgt = (IPDFields *) (target + 1);
			IPDFields_copy(ipd_src, ipd_tgt);
			break;
		default:
			mylog("invalid descriptor handle type=%d\n", src->desc_type);
			DC_set_error(target, DESC_EXEC_ERROR, "invalid descriptor type");
			ret = SQL_ERROR;
	}

	if (SQL_SUCCESS == ret)
		target->type_defined = TRUE;
        return ret;
}

void	DC_clear_error(DescriptorClass *self)
{
	if (self->__error_message)
	{
		free(self->__error_message);
		self->__error_message = NULL;
	}
	if (self->pgerror)
	{
		ER_Destructor(self->pgerror);
		self->pgerror = NULL;
	}
	self->__error_number = 0;
	self->error_row = 0;
	self->error_index = 0;
}

void    DC_set_error(DescriptorClass *desc, int errornumber, const char *errormsg)
{
	if (desc->__error_message)
		free(desc->__error_message);
	desc->__error_number = errornumber;
	desc->__error_message = errormsg ? strdup(errormsg) : NULL;
}
void    DC_set_errormsg(DescriptorClass *desc, const char *errormsg)
{
	if (desc->__error_message)
		free(desc->__error_message);
	desc->__error_message = errormsg ? strdup(errormsg) : NULL;
}
const char	*DC_get_errormsg(const DescriptorClass *desc)
{
        return desc->__error_message;
}
int	DC_get_errornumber(const DescriptorClass *desc)
{
        return desc->__error_number;
}

/*	Map sql commands to statement types */
static struct
{
	int	number;
	const	char	* ver3str;
	const	char	* ver2str;
}	Descriptor_sqlstate[] =

{
	{ DESC_ERROR_IN_ROW, "01S01", "01S01" },
	{ DESC_OPTION_VALUE_CHANGED, "01S02", "01S02" },
	{ DESC_OK,  "00000", "00000" }, /* OK */
	{ DESC_EXEC_ERROR, "HY000", "S1000" }, /* also a general error */
	{ DESC_STATUS_ERROR, "HY010", "S1010" },
	{ DESC_SEQUENCE_ERROR, "HY010", "S1010" }, /* Function sequence error */
	{ DESC_NO_MEMORY_ERROR, "HY001", "S1001" }, /* memory allocation failure */
	{ DESC_COLNUM_ERROR, "07009", "S1002" }, /* invalid column number */
	{ DESC_NO_STMTSTRING, "HY001", "S1001" }, /* having no stmtstring is also a malloc problem */
	{ DESC_ERROR_TAKEN_FROM_BACKEND, "HY000", "S1000" }, /* general error */
	{ DESC_INTERNAL_ERROR, "HY000", "S1000" }, /* general error */
	{ DESC_STILL_EXECUTING, "HY010", "S1010" },
	{ DESC_NOT_IMPLEMENTED_ERROR, "HYC00", "S1C00" }, /* == 'driver not 
							  * capable' */
	{ DESC_BAD_PARAMETER_NUMBER_ERROR, "07009", "S1093" },
	{ DESC_OPTION_OUT_OF_RANGE_ERROR, "HY092", "S1092" },
	{ DESC_INVALID_COLUMN_NUMBER_ERROR, "07009", "S1002" },
	{ DESC_RESTRICTED_DATA_TYPE_ERROR, "07006", "07006" },
	{ DESC_INVALID_CURSOR_STATE_ERROR, "07005", "24000" },
	{ DESC_CREATE_TABLE_ERROR, "42S01", "S0001" }, /* table already exists */
	{ DESC_NO_CURSOR_NAME, "S1015", "S1015" },
	{ DESC_INVALID_CURSOR_NAME, "34000", "34000" },
	{ DESC_INVALID_ARGUMENT_NO, "HY024", "S1009" }, /* invalid argument value */
	{ DESC_ROW_OUT_OF_RANGE, "HY107", "S1107" },
	{ DESC_OPERATION_CANCELLED, "HY008", "S1008" },
	{ DESC_INVALID_CURSOR_POSITION, "HY109", "S1109" },
	{ DESC_VALUE_OUT_OF_RANGE, "HY019", "22003" },
	{ DESC_OPERATION_INVALID, "HY011", "S1011" },
	{ DESC_PROGRAM_TYPE_OUT_OF_RANGE, "?????", "?????" }, 
	{ DESC_BAD_ERROR, "08S01", "08S01" }, /* communication link failure */
	{ DESC_INVALID_OPTION_IDENTIFIER, "HY092", "HY092" },
	{ DESC_RETURN_NULL_WITHOUT_INDICATOR, "22002", "22002" },
	{ DESC_INVALID_DESCRIPTOR_IDENTIFIER, "HY091", "HY091" },
	{ DESC_OPTION_NOT_FOR_THE_DRIVER, "HYC00", "HYC00" },
	{ DESC_FETCH_OUT_OF_RANGE, "HY106", "S1106" },
	{ DESC_COUNT_FIELD_INCORRECT, "07002", "07002" },
};

static	PG_ErrorInfo	*DC_create_errorinfo(const DescriptorClass *desc)
{
	PG_ErrorInfo	*error;
	ConnectionClass	*conn;
	EnvironmentClass	*env;
	Int4	errornum;
	BOOL		env_is_odbc3 = TRUE;

	if (desc->pgerror)
		return desc->pgerror;
	errornum = desc->__error_number;
	error = ER_Constructor(errornum, desc->__error_message);
	if (!error)
		return error;
	conn = DC_get_conn(desc);
	if (conn && (env = (EnvironmentClass *) conn->henv))
		env_is_odbc3 = EN_is_odbc3(env);
	errornum -= LOWEST_DESC_ERROR;
	if (errornum < 0 ||
	    errornum >= sizeof(Descriptor_sqlstate) / sizeof(Descriptor_sqlstate[0]))
		errornum = 1 - LOWEST_DESC_ERROR;
	strcpy(error->sqlstate, env_is_odbc3 ? Descriptor_sqlstate[errornum].ver3str : Descriptor_sqlstate[errornum].ver2str); 
        return error;
}
void
DC_log_error(const char *func, const char *desc, const DescriptorClass *self)
{
#define nullcheck(a) (a ? a : "(NULL)")
	if (self)
	{
		qlog("DESCRIPTOR ERROR: func=%s, desc='%s', errnum=%d, errmsg='%s'\n", func, desc, self->__error_number, nullcheck(self->__error_message));
		mylog("DESCRIPTOR ERROR: func=%s, desc='%s', errnum=%d, errmsg='%s'\n", func, desc, self->__error_number, nullcheck(self->__error_message));
	}
}

/*		Returns the next SQL error information. */
RETCODE		SQL_API
PGAPI_DescError(	SQLHDESC hdesc,
			SQLSMALLINT	RecNumber,
			SQLCHAR FAR * szSqlState,
			SQLINTEGER FAR * pfNativeError,
			SQLCHAR FAR * szErrorMsg,
			SQLSMALLINT cbErrorMsgMax,
			SQLSMALLINT FAR * pcbErrorMsg,
			UWORD flag)
{
	CSTR func = "PGAPI_DescError";
	/* CC: return an error of a hdesc  */
	DescriptorClass *desc = (DescriptorClass *) hdesc;

	mylog("%s RecN=%d\n", func);
	desc->pgerror = DC_create_errorinfo(desc);
	return ER_ReturnError(&(desc->pgerror), RecNumber, szSqlState,
				pfNativeError, szErrorMsg, cbErrorMsgMax,
				pcbErrorMsg, flag);
}

#endif /* ODBCVER */
