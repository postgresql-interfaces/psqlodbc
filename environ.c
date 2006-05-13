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
CRITICAL_SECTION	common_cs; /* commonly used for short term blocking */
#elif defined(POSIX_MULTITHREAD_SUPPORT)
pthread_mutex_t     conns_cs;
pthread_mutex_t     common_cs;
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

	mylog("** exit PGAPI_AllocEnv: phenv = %x **\n", *phenv);
	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_FreeEnv(HENV henv)
{
	CSTR func = "PGAPI_FreeEnv";
	EnvironmentClass *env = (EnvironmentClass *) henv;

	mylog("**** in PGAPI_FreeEnv: env = %x ** \n", env);

	if (env && EN_Destructor(env))
	{
#ifdef	_HANDLE_ENLIST_IN_DTC_
		DtcOnRelease();
#endif /* _HANDLE_ENLIST_IN_DTC_ */
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
	free(self);
}

PG_ErrorInfo *ER_Dup(const PG_ErrorInfo *self)
{
	PG_ErrorInfo	*new;
	Int4		alsize;

	if (!self)
		return NULL;
	alsize = sizeof(PG_ErrorInfo);
	if (self->errorsize  > 0)
		alsize += self->errorsize;
	new = (PG_ErrorInfo *) malloc(alsize);
	memcpy(new, self, alsize);

	return new;
}

#define	DRVMNGRDIV	511
/*		Returns the next SQL error information. */
RETCODE		SQL_API
ER_ReturnError(PG_ErrorInfo **pgerror,
		SQLSMALLINT	RecNumber,
		SQLCHAR FAR * szSqlState,
		SQLINTEGER FAR * pfNativeError,
		SQLCHAR FAR * szErrorMsg,
		SQLSMALLINT cbErrorMsgMax,
		SQLSMALLINT FAR * pcbErrorMsg,
		UWORD flag)
{
	CSTR func = "ER_ReturnError";
	/* CC: return an error of a hstmt  */
	PG_ErrorInfo	*error;
	BOOL		partial_ok = ((flag & PODBC_ALLOW_PARTIAL_EXTRACT) != 0),
			clear_str = ((flag & PODBC_ERROR_CLEAR) != 0);
	const char	*msg;
	SWORD		msglen, stapos, wrtlen, pcblen;

	if (!pgerror || !*pgerror)
		return SQL_NO_DATA_FOUND;
	error = *pgerror;
	msg = error->__error_message;
	mylog("%s: status = %d, msg = #%s#\n", func, error->status, msg);
	msglen = (SQLSMALLINT) strlen(msg);
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
		{
			ER_Destructor(error);
			*pgerror = NULL;
		}
	}
	if (wrtlen == 0)
		return SQL_SUCCESS_WITH_INFO;
	else
		return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_ConnectError(	HDBC hdbc,
			SQLSMALLINT	RecNumber,
			SQLCHAR FAR * szSqlState,
			SQLINTEGER FAR * pfNativeError,
			SQLCHAR FAR * szErrorMsg,
			SQLSMALLINT cbErrorMsgMax,
			SQLSMALLINT FAR * pcbErrorMsg,
			UWORD flag)
{
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	EnvironmentClass *env = (EnvironmentClass *) conn->henv;
	char		*msg;
	int		status;
	BOOL	once_again = FALSE;
	SWORD		msglen;

	mylog("**** PGAPI_ConnectError: hdbc=%x <%d>\n", hdbc, cbErrorMsgMax);
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
	{
		if (conn->sqlstate[0])
			strcpy(szSqlState, conn->sqlstate);
		else	
		switch (status)
		{
			case CONN_OPTION_VALUE_CHANGED:
				pg_sqlstate_set(env, szSqlState, "01S02", "01S02");
				break;
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
				pg_sqlstate_set(env, szSqlState, "HYC00", "S1C00");
				break;
			case CONN_VALUE_OUT_OF_RANGE:
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
			SQLSMALLINT	RecNumber,
			SQLCHAR FAR * szSqlState,
			SQLINTEGER FAR * pfNativeError,
			SQLCHAR FAR * szErrorMsg,
			SQLSMALLINT cbErrorMsgMax,
			SQLSMALLINT FAR * pcbErrorMsg,
			UWORD flag)
{
	EnvironmentClass *env = (EnvironmentClass *) henv;
	char		*msg;
	int		status;

	mylog("**** PGAPI_EnvError: henv=%x <%d>\n", henv, cbErrorMsgMax);
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
		*pcbErrorMsg = (SQLSMALLINT) strlen(msg);
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
PGAPI_Error(
			HENV henv,
			HDBC hdbc,
			HSTMT hstmt,
			SQLCHAR FAR * szSqlState,
			SQLINTEGER FAR * pfNativeError,
			SQLCHAR FAR * szErrorMsg,
			SQLSMALLINT cbErrorMsgMax,
			SQLSMALLINT FAR * pcbErrorMsg)
{
	RETCODE	ret;
	UWORD	flag = PODBC_ALLOW_PARTIAL_EXTRACT | PODBC_ERROR_CLEAR;

	mylog("**** PGAPI_Error: henv=%x, hdbc=%x hstmt=%d\n", henv, hdbc, hstmt);

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

	mylog("in EN_Destructor, self=%x\n", self);

	/*
	 * the error messages are static strings distributed throughout the
	 * source--they should not be freed
	 */

	/* Free any connections belonging to this environment */
	for (lf = 0; lf < MAX_CONNECTIONS; lf++)
	{
		if (conns[lf] && conns[lf]->henv == self)
		{
			if (CC_Destructor(conns[lf]))
				conns[lf] = NULL;
			else
				rv = 0;
		}
	}
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

	mylog("EN_add_connection: self = %x, conn = %x\n", self, conn);

	ENTER_CONNS_CS;
	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (!conns[i])
		{
			conn->henv = self;
			conns[i] = conn;
			LEAVE_CONNS_CS;

			mylog("       added at i =%d, conn->henv = %x, conns[i]->henv = %x\n", i, conn->henv, conns[i]->henv);

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
