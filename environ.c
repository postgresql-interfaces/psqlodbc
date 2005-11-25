/*-------
 * Module:			environ.c
 *
 * Description:		This module contains routines related to
 *					the environment, such as storing connection handles,
 *					and returning errors.
 *
 * Classes:			EnvironmentClass (Functions prefix: "EN_")
 *
 * API functions:	SQLAllocEnv, SQLFreeEnv, SQLError
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#include "environ.h"
#include "connection.h"
#include "dlg_specific.h"
#include "statement.h"
#include <stdlib.h>
#include <string.h>
#include "pgapifunc.h"

extern GLOBAL_VALUES globals;

/* The one instance of the handles */
ConnectionClass *conns[MAX_CONNECTIONS];
#if defined(WIN_MULTITHREAD_SUPPORT)
CRITICAL_SECTION	conns_cs;
#elif defined(POSIX_MULTITHREAD_SUPPORT)
pthread_mutex_t     conns_cs;
#endif /* WIN_MULTITHREAD_SUPPORT */


RETCODE		SQL_API
PGAPI_AllocEnv(HENV FAR * phenv)
{
	CSTR func = "PGAPI_AllocEnv";

	mylog("**** in PGAPI_AllocEnv ** \n");

	/*
	 * Hack for systems on which none of the constructor-making techniques
	 * in psqlodbc.c work: if globals appears not to have been
	 * initialized, then cause it to be initialized.  Since this should be
	 * the first function called in this shared library, doing it here
	 * should work.
	 */
	if (globals.socket_buffersize <= 0)
	{
		initialize_global_cs();
		getCommonDefaults(DBMS_NAME, ODBCINST_INI, NULL);
	}

	*phenv = (HENV) EN_Constructor();
	if (!*phenv)
	{
		*phenv = SQL_NULL_HENV;
		EN_log_error(func, "Error allocating environment", NULL);
		return SQL_ERROR;
	}

	mylog("** exit PGAPI_AllocEnv: phenv = %u **\n", *phenv);
	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_FreeEnv(HENV henv)
{
	CSTR func = "PGAPI_FreeEnv";
	EnvironmentClass *env = (EnvironmentClass *) henv;

	mylog("**** in PGAPI_FreeEnv: env = %u ** \n", env);

	if (env && EN_Destructor(env))
	{
		mylog("   ok\n");
		return SQL_SUCCESS;
	}

	mylog("    error\n");
	EN_log_error(func, "Error freeing environment", env);
	return SQL_ERROR;
}


static void
pg_sqlstate_set(const EnvironmentClass *env, UCHAR *szSqlState, const UCHAR *ver3str, const UCHAR *ver2str)
{
	strcpy(szSqlState, EN_is_odbc3(env) ? ver3str : ver2str);
}

PG_ErrorInfo	*ER_Constructor(SDWORD errnumber, const char *msg)
{
	PG_ErrorInfo	*error;
	Int4		aladd, errsize;

	if (DESC_OK == errnumber)
		return NULL;
	if (msg)
	{
		errsize = strlen(msg);
		aladd = errsize;
	}
	else
	{
		errsize = -1;
		aladd = 0;
	}
	error = (PG_ErrorInfo *) malloc(sizeof(PG_ErrorInfo) + aladd);
	if (error)
	{
		memset(error, 0, sizeof(PG_ErrorInfo));
		error->status = errnumber;
		error->errorsize = errsize;
		if (errsize > 0)
			memcpy(error->__error_message, msg, errsize);
		error->__error_message[aladd] = '\0';
        	error->recsize = -1;
	}
	return error;
}
void
ER_Destructor(PG_ErrorInfo *self)
{
        if (self->__error_message)
                free(self->__error_message);
	free(self);
}

#define	DRVMNGRDIV	511
/*		Returns the next SQL error information. */
RETCODE		SQL_API
ER_ReturnError(PG_ErrorInfo *error,
		SWORD	RecNumber,
		UCHAR FAR * szSqlState,
		SDWORD FAR * pfNativeError,
		UCHAR FAR * szErrorMsg,
		SWORD cbErrorMsgMax,
		SWORD FAR * pcbErrorMsg,
		UWORD flag)
{
	/* CC: return an error of a hstmt  */
	BOOL		partial_ok = ((flag & PODBC_ALLOW_PARTIAL_EXTRACT) != 0),
			clear_str = ((flag & PODBC_ERROR_CLEAR) != 0);
	const char	*msg;
	SWORD		msglen, stapos, wrtlen, pcblen;

	if (!error)
		return SQL_NO_DATA_FOUND;
	msg = error->__error_message;
	mylog("ER_GetError: status = %d, msg = #%s#\n", error->status, msg);
	msglen = (SWORD) strlen(msg);
	/*
	 *	Even though an application specifies a larger error message
	 *	buffer, the driver manager changes it silently.
	 *	Therefore we divide the error message into ...
	 */
	if (error->recsize < 0)
	{
		if (cbErrorMsgMax > 0)
			error->recsize = cbErrorMsgMax - 1; /* apply the first request */
		else
			error->recsize = DRVMNGRDIV;
	}
	if (RecNumber < 0)
	{
		if (0 == error->errorpos)
			RecNumber = 1;
		else
			RecNumber = 2 + (error->errorpos - 1) / error->recsize;
	}
	stapos = (RecNumber - 1) * error->recsize;
	if (stapos > msglen)
		return SQL_NO_DATA_FOUND;
	pcblen = wrtlen = msglen - stapos;
	if (pcblen > error->recsize)
		pcblen = error->recsize;
	if (0 == cbErrorMsgMax)
		wrtlen = 0;
	else if (wrtlen >= cbErrorMsgMax)
	{
		if (partial_ok)
			wrtlen = cbErrorMsgMax - 1;
		else if (cbErrorMsgMax <= error->recsize)
			wrtlen = 0;
		else
			wrtlen = error->recsize;
	}
	if (wrtlen > pcblen)
		wrtlen = pcblen;
	if (NULL != pcbErrorMsg)
		*pcbErrorMsg = pcblen;

	if ((NULL != szErrorMsg) && (cbErrorMsgMax > 0))
	{
		memcpy(szErrorMsg, msg + stapos, wrtlen);
		szErrorMsg[wrtlen] = '\0';
	}

	if (NULL != pfNativeError)
		*pfNativeError = error->status;

	if (NULL != szSqlState)
		strncpy(szSqlState, error->sqlstate, 6);

	mylog("	     szSqlState = '%s',len=%d, szError='%s'\n", szSqlState, pcblen, szErrorMsg);
	if (clear_str)
	{
		error->errorpos = stapos + wrtlen;
		if (error->errorpos >= msglen)
			ER_Destructor(error);
	}
	if (wrtlen == 0)
		return SQL_SUCCESS_WITH_INFO;
	else
		return SQL_SUCCESS;
}

#define	DRVMNGRDIV	511
/*		Returns the next SQL error information. */
RETCODE		SQL_API
PGAPI_StmtError(	HSTMT hstmt, 
			SWORD RecNumber,
			SQLCHAR *szSqlState, 
			SQLINTEGER *pfNativeError,
			SQLCHAR *szErrorMsg, 
			SQLSMALLINT cbErrorMsgMax,
			SQLSMALLINT *pcbErrorMsg, 
			UWORD flag)
{
	/* CC: return an error of a hstmt  */
	StatementClass *stmt = (StatementClass *) hstmt;
	EnvironmentClass *env = (EnvironmentClass *) SC_get_conn(stmt)->henv;
	char		*msg;
	int		status;
	BOOL		partial_ok = ((flag & PODBC_ALLOW_PARTIAL_EXTRACT) != 0),
			clear_str = ((flag & PODBC_ERROR_CLEAR) != 0);
	SWORD		msglen, stapos, wrtlen, pcblen;

	mylog("**** PGAPI_StmtError: hstmt=%u <%d>\n", hstmt, cbErrorMsgMax);

	if (cbErrorMsgMax < 0)
		return SQL_ERROR;

	if (STMT_EXECUTING == stmt->status || !SC_get_error(stmt, &status, &msg)
		 || NULL == msg || !msg[0])
	{
		mylog("SC_Get_error returned nothing.\n");
		if (NULL != szSqlState)
			strcpy(szSqlState, "00000");
		if (NULL != pcbErrorMsg)
			*pcbErrorMsg = 0;
		if ((NULL != szErrorMsg) && (cbErrorMsgMax > 0))
			szErrorMsg[0] = '\0';

		return SQL_NO_DATA_FOUND;
	}
	mylog("SC_get_error: status = %d, msg = #%s#\n", status, msg);
	msglen = (SWORD) strlen(msg);
	/*
	 *	Even though an application specifies a larger error message
	 *	buffer, the driver manager changes it silently.
	 *	Therefore we divide the error message into ...
	 */
	if (stmt->error_recsize < 0)
	{
		if (cbErrorMsgMax > 0)
			stmt->error_recsize = cbErrorMsgMax - 1; /* apply the first request */
		else
			stmt->error_recsize = DRVMNGRDIV;
	}
	if (RecNumber < 0)
	{
		if (0 == stmt->errorpos)
			RecNumber = 1;
		else
			RecNumber = 2 + (stmt->errorpos - 1) / stmt->error_recsize;
	}
	stapos = (RecNumber - 1) * stmt->error_recsize;
	if (stapos > msglen)
		return SQL_NO_DATA_FOUND;
	pcblen = wrtlen = msglen - stapos;
	if (pcblen > stmt->error_recsize)
		pcblen = stmt->error_recsize;
	if (0 == cbErrorMsgMax)
		wrtlen = 0;
	else if (wrtlen >= cbErrorMsgMax)
	{
		if (partial_ok)
			wrtlen = cbErrorMsgMax - 1;
		else if (cbErrorMsgMax <= stmt->error_recsize)
			wrtlen = 0;
		else
			wrtlen = stmt->error_recsize;
	}
	if (wrtlen > pcblen)
		wrtlen = pcblen;
	if (NULL != pcbErrorMsg)
		*pcbErrorMsg = pcblen;

	if ((NULL != szErrorMsg) && (cbErrorMsgMax > 0))
	{
		memcpy(szErrorMsg, msg + stapos, wrtlen);
		szErrorMsg[wrtlen] = '\0';
	}

	if (NULL != pfNativeError)
		*pfNativeError = status;

	if (NULL != szSqlState)

		switch (status)
		{
				/* now determine the SQLSTATE to be returned */
			case STMT_ROW_VERSION_CHANGED:
				pg_sqlstate_set(env, szSqlState, "01001", "01001");
				/* data truncated */
				break;
			case STMT_TRUNCATED:
				pg_sqlstate_set(env, szSqlState, "01004", "01004");
				/* data truncated */
				break;
			case STMT_INFO_ONLY:
				pg_sqlstate_set(env, szSqlState, "00000", "0000");
				/* just information that is returned, no error */
				break;
			case STMT_BAD_ERROR:
				pg_sqlstate_set(env, szSqlState, "08S01", "08S01");
				/* communication link failure */
				break;
			case STMT_CREATE_TABLE_ERROR:
				pg_sqlstate_set(env, szSqlState, "42S01", "S0001");
				/* table already exists */
				break;
			case STMT_STATUS_ERROR:
			case STMT_SEQUENCE_ERROR:
				pg_sqlstate_set(env, szSqlState, "HY010", "S1010");
				/* Function sequence error */
				break;
			case STMT_NO_MEMORY_ERROR:
				pg_sqlstate_set(env, szSqlState, "HY001", "S1001");
				/* memory allocation failure */
				break;
			case STMT_COLNUM_ERROR:
				pg_sqlstate_set(env, szSqlState, "07009", "S1002");
				/* invalid column number */
				break;
			case STMT_NO_STMTSTRING:
				pg_sqlstate_set(env, szSqlState, "HY001", "S1001");
				/* having no stmtstring is also a malloc problem */
				break;
			case STMT_ERROR_TAKEN_FROM_BACKEND:
				pg_sqlstate_set(env, szSqlState, SC_get_sqlstate(stmt), "S1000");
				/* Use the ODBC 3 sqlstate reported by the backend. */
				break;
			case STMT_INTERNAL_ERROR:
				pg_sqlstate_set(env, szSqlState, "HY000", "S1000");
				/* general error */
				break;
			case STMT_FETCH_OUT_OF_RANGE:
				pg_sqlstate_set(env, szSqlState, "HY106", "S1106");
				break;

			case STMT_ROW_OUT_OF_RANGE:
				pg_sqlstate_set(env, szSqlState, "HY107", "S1107");
				break;

			case STMT_OPERATION_CANCELLED:
				pg_sqlstate_set(env, szSqlState, "HY008", "S1008");
				break;

			case STMT_NOT_IMPLEMENTED_ERROR:
				pg_sqlstate_set(env, szSqlState, "HYC00", "S1C00");	/* == 'driver not
													 * capable' */
				break;
			case STMT_OPTION_OUT_OF_RANGE_ERROR:
				pg_sqlstate_set(env, szSqlState, "HY092", "S1092");
				break;
			case STMT_BAD_PARAMETER_NUMBER_ERROR:
				pg_sqlstate_set(env, szSqlState, "07009", "S1093");
				break;
			case STMT_INVALID_COLUMN_NUMBER_ERROR:
				pg_sqlstate_set(env, szSqlState, "07009", "S1002");
				break;
			case STMT_RESTRICTED_DATA_TYPE_ERROR:
				pg_sqlstate_set(env, szSqlState, "07006", "07006");
				break;
			case STMT_INVALID_CURSOR_STATE_ERROR:
				pg_sqlstate_set(env, szSqlState, "07005", "24000");
				break;
			case STMT_ERROR_IN_ROW:
				pg_sqlstate_set(env, szSqlState, "01S01", "01S01");
				break;
			case STMT_OPTION_VALUE_CHANGED:
				pg_sqlstate_set(env, szSqlState, "01S02", "01S02");
				break;
			case STMT_POS_BEFORE_RECORDSET:
				pg_sqlstate_set(env, szSqlState, "01S06", "01S06");
				break;
			case STMT_INVALID_CURSOR_NAME:
				pg_sqlstate_set(env, szSqlState, "34000", "34000");
				break;
			case STMT_NO_CURSOR_NAME:
				pg_sqlstate_set(env, szSqlState, "S1015", "S1015");
				break;
			case STMT_INVALID_ARGUMENT_NO:
				pg_sqlstate_set(env, szSqlState, "HY024", "S1009");
				/* invalid argument value */
				break;
			case STMT_INVALID_CURSOR_POSITION:
				pg_sqlstate_set(env, szSqlState, "HY109", "S1109");
				break;
			case STMT_RETURN_NULL_WITHOUT_INDICATOR:
				pg_sqlstate_set(env, szSqlState, "22002", "22002");
				break;
			case STMT_VALUE_OUT_OF_RANGE:
				pg_sqlstate_set(env, szSqlState, "HY019", "22003");
				break;
			case STMT_OPERATION_INVALID:
				pg_sqlstate_set(env, szSqlState, "HY011", "S1011");
				break;
			case STMT_INVALID_DESCRIPTOR_IDENTIFIER:
				pg_sqlstate_set(env, szSqlState, "HY091", "HY091");
				break;
			case STMT_INVALID_OPTION_IDENTIFIER:
				pg_sqlstate_set(env, szSqlState, "HY092", "HY092");
				break;
			case STMT_OPTION_NOT_FOR_THE_DRIVER:
				pg_sqlstate_set(env, szSqlState, "HYC00", "HYC00");
				break;
			case STMT_COUNT_FIELD_INCORRECT:
				pg_sqlstate_set(env, szSqlState, "07002", "07002");
				break;
			case STMT_EXEC_ERROR:
			default:
				pg_sqlstate_set(env, szSqlState, "HY000", "S1000");
				/* also a general error */
				break;
		}
	mylog("	     szSqlState = '%s',len=%d, szError='%s'\n", szSqlState, pcblen, szErrorMsg);
	if (clear_str)
	{
		stmt->errorpos = stapos + wrtlen;
		if (stmt->errorpos >= msglen)
			SC_clear_error(stmt);
	}
	if (wrtlen == 0)
		return SQL_SUCCESS_WITH_INFO;
	else
		return SQL_SUCCESS;
}

RETCODE		SQL_API
PGAPI_ConnectError(HDBC hdbc, 
			SWORD RecNumber,
			SQLCHAR *szSqlState, 
			SQLINTEGER *pfNativeError,
			SQLCHAR *szErrorMsg, 
			SQLSMALLINT cbErrorMsgMax,
			SQLSMALLINT *pcbErrorMsg, 
			UWORD flag)
{
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	EnvironmentClass *env = (EnvironmentClass *) conn->henv;
	char		*msg;
	int		status;
	BOOL	once_again = FALSE;
	SWORD		msglen;

	mylog("**** PGAPI_ConnectError: hdbc=%u <%d>\n", hdbc, cbErrorMsgMax);
	if (RecNumber != 1 && RecNumber != -1)
		return SQL_NO_DATA_FOUND;
	if (cbErrorMsgMax < 0)
		return SQL_ERROR;
	if (CONN_EXECUTING == conn->status || !CC_get_error(conn, &status, &msg) || NULL == msg)
	{
		mylog("CC_Get_error returned nothing.\n");
		if (NULL != szSqlState)
			strcpy(szSqlState, "00000");
		if (NULL != pcbErrorMsg)
			*pcbErrorMsg = 0;
		if ((NULL != szErrorMsg) && (cbErrorMsgMax > 0))
			szErrorMsg[0] = '\0';

		return SQL_NO_DATA_FOUND;
	}
	mylog("CC_get_error: status = %d, msg = #%s#\n", status, msg);

	msglen = strlen(msg);
	if (NULL != pcbErrorMsg)
	{
		*pcbErrorMsg = msglen;
		if (cbErrorMsgMax == 0)
			once_again = TRUE;
		else if (msglen >= cbErrorMsgMax)
			*pcbErrorMsg = cbErrorMsgMax - 1;
	}
	if ((NULL != szErrorMsg) && (cbErrorMsgMax > 0))
		strncpy_null(szErrorMsg, msg, cbErrorMsgMax);
	if (NULL != pfNativeError)
		*pfNativeError = status;

	if (NULL != szSqlState)
		switch (status)
		{
			case STMT_OPTION_VALUE_CHANGED:
			case CONN_OPTION_VALUE_CHANGED:
				pg_sqlstate_set(env, szSqlState, "01S02", "01S02");
				break;
			case STMT_TRUNCATED:
			case CONN_TRUNCATED:
				pg_sqlstate_set(env, szSqlState, "01004", "01004");
				/* data truncated */
				break;
			case CONN_INIREAD_ERROR:
				pg_sqlstate_set(env, szSqlState, "IM002", "IM002");
				/* data source not found */
				break;
			case CONNECTION_SERVER_NOT_REACHED:
			case CONN_OPENDB_ERROR:
				pg_sqlstate_set(env, szSqlState, "08001", "08001");
				/* unable to connect to data source */
				break;
			case CONN_INVALID_AUTHENTICATION:
			case CONN_AUTH_TYPE_UNSUPPORTED:
				pg_sqlstate_set(env, szSqlState, "28000", "28000");
				break;
			case CONN_STMT_ALLOC_ERROR:
				pg_sqlstate_set(env, szSqlState, "HY001", "S1001");
				/* memory allocation failure */
				break;
			case CONN_IN_USE:
				pg_sqlstate_set(env, szSqlState, "HY000", "S1000");
				/* general error */
				break;
			case CONN_UNSUPPORTED_OPTION:
				pg_sqlstate_set(env, szSqlState, "IM001", "IM001");
				/* driver does not support this function */
			case CONN_INVALID_ARGUMENT_NO:
				pg_sqlstate_set(env, szSqlState, "HY009", "S1009");
				/* invalid argument value */
				break;
			case CONN_TRANSACT_IN_PROGRES:
				pg_sqlstate_set(env, szSqlState, "HY010", "S1010");

				/*
				 * when the user tries to switch commit mode in a
				 * transaction
				 */
				/* -> function sequence error */
				break;
			case CONN_NO_MEMORY_ERROR:
				pg_sqlstate_set(env, szSqlState, "HY001", "S1001");
				break;
			case CONN_NOT_IMPLEMENTED_ERROR:
			case STMT_NOT_IMPLEMENTED_ERROR:
				pg_sqlstate_set(env, szSqlState, "HYC00", "S1C00");
				break;
			case STMT_RETURN_NULL_WITHOUT_INDICATOR:
				pg_sqlstate_set(env, szSqlState, "22002", "22002");
				break;
			case CONN_VALUE_OUT_OF_RANGE:
			case STMT_VALUE_OUT_OF_RANGE:
				pg_sqlstate_set(env, szSqlState, "HY019", "22003");
				break;
			case CONNECTION_COULD_NOT_SEND:
			case CONNECTION_COULD_NOT_RECEIVE:
				pg_sqlstate_set(env, szSqlState, "08S01", "08S01");
				break;
			default:
				pg_sqlstate_set(env, szSqlState, "HY000", "S1000");
				/* general error */
				break;
		}

	mylog("	     szSqlState = '%s',len=%d, szError='%s'\n", szSqlState, msglen, szErrorMsg);
	if (once_again)
	{
		CC_set_errornumber(conn, status);
		return SQL_SUCCESS_WITH_INFO;
	}
	else
		return SQL_SUCCESS;
}

RETCODE		SQL_API
PGAPI_EnvError(		HENV henv, 
			SWORD RecNumber,
			SQLCHAR *szSqlState, 
			SQLINTEGER *pfNativeError,
			SQLCHAR *szErrorMsg, 
			SQLSMALLINT cbErrorMsgMax,
			SQLSMALLINT *pcbErrorMsg, 
			UWORD flag)
{
	EnvironmentClass *env = (EnvironmentClass *) henv;
	char		*msg;
	int		status;

	mylog("**** PGAPI_EnvError: henv=%u <%d>\n", henv, cbErrorMsgMax);
	if (RecNumber != 1 && RecNumber != -1)
		return SQL_NO_DATA_FOUND;
	if (cbErrorMsgMax < 0)
		return SQL_ERROR;
	if (!EN_get_error(env, &status, &msg) || NULL == msg)
	{
			mylog("EN_get_error: status = %d, msg = #%s#\n", status, msg);

		if (NULL != szSqlState)
			pg_sqlstate_set(env, szSqlState, "00000", "00000");
		if (NULL != pcbErrorMsg)
			*pcbErrorMsg = 0;
		if ((NULL != szErrorMsg) && (cbErrorMsgMax > 0))
			szErrorMsg[0] = '\0';

		return SQL_NO_DATA_FOUND;
	}
	mylog("EN_get_error: status = %d, msg = #%s#\n", status, msg);

	if (NULL != pcbErrorMsg)
		*pcbErrorMsg = (SWORD) strlen(msg);
	if ((NULL != szErrorMsg) && (cbErrorMsgMax > 0))
		strncpy_null(szErrorMsg, msg, cbErrorMsgMax);
	if (NULL != pfNativeError)
		*pfNativeError = status;

	if (szSqlState)
	{
		switch (status)
		{
			case ENV_ALLOC_ERROR:
				/* memory allocation failure */
				pg_sqlstate_set(env, szSqlState, "HY001", "S1001");
				break;
			default:
				pg_sqlstate_set(env, szSqlState, "HY000", "S1000");
				/* general error */
				break;
		}
	}

	return SQL_SUCCESS;
}


/*		Returns the next SQL error information. */
RETCODE		SQL_API
PGAPI_Error(		HENV henv,
			HDBC hdbc, 
			HSTMT hstmt,
			SQLCHAR *szSqlState,
			SQLINTEGER *pfNativeError,
			SQLCHAR *szErrorMsg, 
			SQLSMALLINT cbErrorMsgMax,
			SQLSMALLINT *pcbErrorMsg)
{
	RETCODE	ret;
	UWORD	flag = PODBC_ALLOW_PARTIAL_EXTRACT | PODBC_ERROR_CLEAR;

	mylog("**** PGAPI_Error: henv=%u, hdbc=%u hstmt=%d\n", henv, hdbc, hstmt);

	if (cbErrorMsgMax < 0)
		return SQL_ERROR;
	if (SQL_NULL_HSTMT != hstmt)
		ret = PGAPI_StmtError(hstmt, -1, szSqlState, pfNativeError,
			 szErrorMsg, cbErrorMsgMax, pcbErrorMsg, flag);
	else if (SQL_NULL_HDBC != hdbc)
		ret = PGAPI_ConnectError(hdbc, -1, szSqlState, pfNativeError,
			 szErrorMsg, cbErrorMsgMax, pcbErrorMsg, flag);
	else if (SQL_NULL_HENV != henv)
		ret = PGAPI_EnvError(henv, -1, szSqlState, pfNativeError,
			 szErrorMsg, cbErrorMsgMax, pcbErrorMsg, flag);
	else
	{
		if (NULL != szSqlState)
			strcpy(szSqlState, "00000");
		if (NULL != pcbErrorMsg)
			*pcbErrorMsg = 0;
		if ((NULL != szErrorMsg) && (cbErrorMsgMax > 0))
			szErrorMsg[0] = '\0';

		ret = SQL_NO_DATA_FOUND;
	}
	mylog("**** PGAPI_Error exit code=%d\n", ret);
	return ret;
}

/*
 * EnvironmentClass implementation
 */
EnvironmentClass *
EN_Constructor(void)
{
	EnvironmentClass *rv;

	rv = (EnvironmentClass *) malloc(sizeof(EnvironmentClass));
	if (rv)
	{
		rv->errormsg = 0;
		rv->errornumber = 0;
		rv->flag = 0;
		INIT_ENV_CS(rv);
	}

	return rv;
}


char
EN_Destructor(EnvironmentClass *self)
{
	int			lf;
	char		rv = 1;

	mylog("in EN_Destructor, self=%u\n", self);

	/*
	 * the error messages are static strings distributed throughout the
	 * source--they should not be freed
	 */

	/* Free any connections belonging to this environment */
	ENTER_CONNS_CS;
	for (lf = 0; lf < MAX_CONNECTIONS; lf++)
	{
		if (conns[lf] && conns[lf]->henv == self)
		{
			rv = rv && CC_Destructor(conns[lf]);
			conns[lf] = NULL;
		}
	}
	LEAVE_CONNS_CS;
	DELETE_ENV_CS(self);
	free(self);

	mylog("exit EN_Destructor: rv = %d\n", rv);
#ifdef	_MEMORY_DEBUG_
	debug_memory_check();
#endif   /* _MEMORY_DEBUG_ */
	return rv;
}


char
EN_get_error(EnvironmentClass *self, int *number, char **message)
{
	if (self && self->errormsg && self->errornumber)
	{
		*message = self->errormsg;
		*number = self->errornumber;
		self->errormsg = 0;
		self->errornumber = 0;
		return 1;
	}
	else
		return 0;
}


char
EN_add_connection(EnvironmentClass *self, ConnectionClass *conn)
{
	int			i;

	mylog("EN_add_connection: self = %u, conn = %u\n", self, conn);

	ENTER_CONNS_CS;
	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!conns[i])
		{
			conn->henv = self;
			conns[i] = conn;
			LEAVE_CONNS_CS;

			mylog("       added at i =%d, conn->henv = %u, conns[i]->henv = %u\n", i, conn->henv, conns[i]->henv);

			return TRUE;
		}
	}
	LEAVE_CONNS_CS;

	return FALSE;
}


char
EN_remove_connection(EnvironmentClass *self, ConnectionClass *conn)
{
	int			i;

	for (i = 0; i < MAX_CONNECTIONS; i++)
		if (conns[i] == conn && conns[i]->status != CONN_EXECUTING)
		{
			ENTER_CONNS_CS;
			conns[i] = NULL;
			LEAVE_CONNS_CS;
			return TRUE;
		}

	return FALSE;
}


void
EN_log_error(const char *func, char *desc, EnvironmentClass *self)
{
	if (self)
		qlog("ENVIRON ERROR: func=%s, desc='%s', errnum=%d, errmsg='%s'\n", func, desc, self->errornumber, self->errormsg);
	else
		qlog("INVALID ENVIRON HANDLE ERROR: func=%s, desc='%s'\n", func, desc);
}
