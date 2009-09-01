/*-------
 * Module:			odbcapi30w.c
 *
 * Description:		This module contains UNICODE routines
 *
 * Classes:			n/a
 *
 * API functions:	SQLColAttributeW, SQLGetStmtAttrW, SQLSetStmtAttrW,
 			SQLSetConnectAttrW, SQLGetConnectAttrW,
			SQLGetDescFieldW, SQLGetDescRecW, SQLGetDiagFieldW,
			SQLGetDiagRecW,
 *-------
 */

#include "psqlodbc.h"

#if (ODBCVER >= 0x0300)
#include <stdio.h>
#include <string.h>

#include "pgapifunc.h"
#include "connection.h"
#include "statement.h"


RETCODE	SQL_API	SQLGetStmtAttrW(SQLHSTMT hstmt,
		SQLINTEGER	fAttribute,
		PTR		rgbValue,
		SQLINTEGER	cbValueMax,
		SQLINTEGER	*pcbValue)
{
	CSTR func = "SQLGetStmtAttrW";
	RETCODE	ret;
	StatementClass	*stmt = (StatementClass *) hstmt;

	mylog("[%s]", func);
	ENTER_STMT_CS((StatementClass *) hstmt);
	SC_clear_error((StatementClass *) hstmt);
	StartRollbackState(stmt);
	ret = PGAPI_GetStmtAttr(hstmt, fAttribute, rgbValue,
		cbValueMax, pcbValue);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS((StatementClass *) hstmt);
	return ret;
}

RETCODE SQL_API	SQLSetStmtAttrW(SQLHSTMT hstmt,
		SQLINTEGER	fAttribute,
		PTR		rgbValue,
		SQLINTEGER	cbValueMax)
{
	CSTR func = "SQLSetStmtAttrW";
	RETCODE	ret;
	StatementClass	*stmt = (StatementClass *) hstmt;

	mylog("[%s]", func);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	ret = PGAPI_SetStmtAttr(hstmt, fAttribute, rgbValue,
		cbValueMax);
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);
	return ret;
}

RETCODE SQL_API	SQLGetConnectAttrW(HDBC hdbc,
		SQLINTEGER	fAttribute,
		PTR		rgbValue,
		SQLINTEGER	cbValueMax,
		SQLINTEGER	*pcbValue)
{
	CSTR func = "SQLGetConnectAttrW";
	RETCODE	ret;

	mylog("[%s]", func);
	ENTER_CONN_CS((ConnectionClass *) hdbc);
	CC_clear_error((ConnectionClass *) hdbc);
	ret = PGAPI_GetConnectAttr(hdbc, fAttribute, rgbValue,
		cbValueMax, pcbValue);
	LEAVE_CONN_CS((ConnectionClass *) hdbc);
	return ret;
}

RETCODE SQL_API	SQLSetConnectAttrW(HDBC hdbc,
		SQLINTEGER	fAttribute,
		PTR		rgbValue,
		SQLINTEGER	cbValue)
{
	CSTR func = "SQLSetConnectAttrW";
	RETCODE	ret;
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	mylog("[%s]", func);
	ENTER_CONN_CS(conn);
	CC_clear_error(conn);
	CC_set_in_unicode_driver(conn);
	ret = PGAPI_SetConnectAttr(hdbc, fAttribute, rgbValue,
		cbValue);
	LEAVE_CONN_CS(conn);
	return ret;
}

/*      new function */
RETCODE  SQL_API
SQLSetDescFieldW(SQLHDESC DescriptorHandle, SQLSMALLINT RecNumber,
				SQLSMALLINT FieldIdentifier, PTR Value, 
				SQLINTEGER BufferLength)
{
	CSTR func = "SQLSetDescFieldW";
	RETCODE	ret;
	SQLLEN	vallen;
        char    *uval = NULL;
	BOOL	val_alloced = FALSE;

	mylog("[%s]", func);
	if (BufferLength > 0 || SQL_NTS == BufferLength)
	{
		switch (FieldIdentifier)
		{
			case SQL_DESC_BASE_COLUMN_NAME:
			case SQL_DESC_BASE_TABLE_NAME:
			case SQL_DESC_CATALOG_NAME:
			case SQL_DESC_LABEL:
			case SQL_DESC_LITERAL_PREFIX:
			case SQL_DESC_LITERAL_SUFFIX:
			case SQL_DESC_LOCAL_TYPE_NAME:
			case SQL_DESC_NAME:
			case SQL_DESC_SCHEMA_NAME:
			case SQL_DESC_TABLE_NAME:
			case SQL_DESC_TYPE_NAME:
				uval = ucs2_to_utf8(Value, BufferLength > 0 ? BufferLength / WCLEN : BufferLength, &vallen, FALSE);
				val_alloced = TRUE;
			break;
		}
	}
	if (!val_alloced)
	{
		uval = Value;
		vallen = BufferLength;
	}
	ret = PGAPI_SetDescField(DescriptorHandle, RecNumber, FieldIdentifier,
				uval, (SQLINTEGER) vallen);
	if (val_alloced)
		free(uval);
	return ret;
}
RETCODE SQL_API
SQLGetDescFieldW(SQLHDESC hdesc, SQLSMALLINT iRecord, SQLSMALLINT iField,
				PTR rgbValue, SQLINTEGER cbValueMax,
    				SQLINTEGER *pcbValue)
{
	CSTR func = "SQLGetDescFieldW";
	RETCODE	ret;
	SQLINTEGER		blen = 0, bMax,	*pcbV;
        char    *rgbV = NULL;

	mylog("[%s]", func);
	switch (iField)
	{
		case SQL_DESC_BASE_COLUMN_NAME:
		case SQL_DESC_BASE_TABLE_NAME:
		case SQL_DESC_CATALOG_NAME:
		case SQL_DESC_LABEL:
		case SQL_DESC_LITERAL_PREFIX:
		case SQL_DESC_LITERAL_SUFFIX:
		case SQL_DESC_LOCAL_TYPE_NAME:
		case SQL_DESC_NAME:
		case SQL_DESC_SCHEMA_NAME:
		case SQL_DESC_TABLE_NAME:
		case SQL_DESC_TYPE_NAME:
			bMax = cbValueMax * 3 / WCLEN;
			rgbV = malloc(bMax + 1);
			pcbV = &blen;
			for (;; bMax = blen + 1, rgbV = realloc(rgbV, bMax))
			{
				ret = PGAPI_GetDescField(hdesc, iRecord, iField, rgbV, bMax, pcbV);
				if (SQL_SUCCESS_WITH_INFO != ret || blen < bMax)
					break;
			}
			if (SQL_SUCCEEDED(ret))
			{
				blen = (SQLINTEGER) utf8_to_ucs2(rgbV, blen, (SQLWCHAR *) rgbValue, cbValueMax / WCLEN);
				if (SQL_SUCCESS == ret && blen * WCLEN >= cbValueMax)
				{
					ret = SQL_SUCCESS_WITH_INFO;
					DC_set_error(hdesc, STMT_TRUNCATED, "The buffer was too small for the rgbDesc.");
				}
				if (pcbValue)
					*pcbValue = blen * WCLEN;
			}
			if (rgbV)
				free(rgbV);
			break;
		default:
			rgbV = rgbValue;
			bMax = cbValueMax;
			pcbV = pcbValue;
			ret = PGAPI_GetDescField(hdesc, iRecord, iField, rgbV, bMax, pcbV);
			break;
	}

	return ret;
}

RETCODE SQL_API	SQLGetDiagRecW(SQLSMALLINT fHandleType,
		SQLHANDLE	handle,
		SQLSMALLINT	iRecord,
		SQLWCHAR	*szSqlState,
		SQLINTEGER	*pfNativeError,
		SQLWCHAR	*szErrorMsg,
		SQLSMALLINT	cbErrorMsgMax,
		SQLSMALLINT	*pcbErrorMsg)
{
	CSTR func = "SQLGetDiagRecW";
	RETCODE	ret;
        SQLSMALLINT	buflen, tlen;
        char    *qstr = NULL, *mtxt = NULL;

	mylog("[%s]", func);
        if (szSqlState)
                qstr = malloc(8);
	buflen = 0;
        if (szErrorMsg && cbErrorMsgMax > 0)
	{
		buflen = cbErrorMsgMax;
                mtxt = malloc(buflen);
	}
        ret = PGAPI_GetDiagRec(fHandleType, handle, iRecord, qstr,
                pfNativeError, mtxt, buflen, &tlen);
	if (SQL_SUCCEEDED(ret))
	{
        	if (qstr)
                	utf8_to_ucs2(qstr, strlen(qstr), szSqlState, 6);
		if (mtxt && tlen <= cbErrorMsgMax)
		{
			SQLULEN ulen = utf8_to_ucs2_lf1(mtxt, tlen, FALSE, szErrorMsg, cbErrorMsgMax);
			if (ulen == (SQLULEN) -1)
			{
				tlen = (SQLSMALLINT) msgtowstr(NULL, mtxt,
					(int) tlen, (LPWSTR) szErrorMsg, (int) cbErrorMsgMax);
			}
			else
				tlen = (SQLSMALLINT) ulen;
			if (tlen >= cbErrorMsgMax)
				ret = SQL_SUCCESS_WITH_INFO;
		}
		if (pcbErrorMsg)
        		*pcbErrorMsg = tlen;
	}
        if (qstr)
        	free(qstr);
	if (mtxt)
        	free(mtxt);
        return ret;
}

SQLRETURN SQL_API SQLColAttributeW(
	SQLHSTMT	hstmt,
	SQLUSMALLINT	iCol,
	SQLUSMALLINT	iField,
	SQLPOINTER	pCharAttr,
	SQLSMALLINT	cbCharAttrMax,	
	SQLSMALLINT	*pcbCharAttr,
#if defined(_WIN64) || defined(SQLCOLATTRIBUTE_SQLLEN)
	SQLLEN		*pNumAttr
#else
	SQLPOINTER	pNumAttr
#endif
	)
{
	CSTR func = "SQLColAttributeW";
	RETCODE	ret;
	StatementClass	*stmt = (StatementClass *) hstmt;
	SQLSMALLINT	*rgbL, blen = 0, bMax;
        char    *rgbD = NULL;

	mylog("[%s]", func);
	ENTER_STMT_CS(stmt);
	SC_clear_error(stmt);
	StartRollbackState(stmt);
	switch (iField)
	{ 
		case SQL_DESC_BASE_COLUMN_NAME:
		case SQL_DESC_BASE_TABLE_NAME:
		case SQL_DESC_CATALOG_NAME:
		case SQL_DESC_LABEL:
		case SQL_DESC_LITERAL_PREFIX:
		case SQL_DESC_LITERAL_SUFFIX:
		case SQL_DESC_LOCAL_TYPE_NAME:
		case SQL_DESC_NAME:
		case SQL_DESC_SCHEMA_NAME:
		case SQL_DESC_TABLE_NAME:
		case SQL_DESC_TYPE_NAME:
		case SQL_COLUMN_NAME:
			bMax = cbCharAttrMax * 3 / WCLEN;
			rgbD = malloc(bMax);
			rgbL = &blen;
			for (;; bMax = blen + 1, rgbD = realloc(rgbD, bMax))
			{
				ret = PGAPI_ColAttributes(hstmt, iCol, iField, rgbD,
					bMax, rgbL, pNumAttr);
				if (SQL_SUCCESS_WITH_INFO != ret || blen < bMax)
					break;
			}
			if (SQL_SUCCEEDED(ret))
			{
				blen = (SQLSMALLINT) utf8_to_ucs2(rgbD, blen, (SQLWCHAR *) pCharAttr, cbCharAttrMax / WCLEN);
				if (SQL_SUCCESS == ret && blen * WCLEN >= cbCharAttrMax)
				{
					ret = SQL_SUCCESS_WITH_INFO;
					SC_set_error(stmt, STMT_TRUNCATED, "The buffer was too small for the pCharAttr.", func);
				}
				if (pcbCharAttr)
					*pcbCharAttr = blen * WCLEN;
			}
			if (rgbD)
				free(rgbD);
			break;
		default:
			rgbD = pCharAttr;
			bMax = cbCharAttrMax;
			rgbL = pcbCharAttr;
			ret = PGAPI_ColAttributes(hstmt, iCol, iField, rgbD,
					bMax, rgbL, pNumAttr);
			break;
	}
	ret = DiscardStatementSvp(stmt, ret, FALSE);
	LEAVE_STMT_CS(stmt);

	return ret;
}

RETCODE SQL_API SQLGetDiagFieldW(
    SQLSMALLINT        fHandleType,
    SQLHANDLE          handle,
    SQLSMALLINT        iRecord,
    SQLSMALLINT        fDiagField,
    SQLPOINTER         rgbDiagInfo,
    SQLSMALLINT        cbDiagInfoMax,
    SQLSMALLINT    *pcbDiagInfo)
{
	CSTR func = "SQLGetDiagFieldW";
	RETCODE	ret;
	SQLSMALLINT	*rgbL, blen = 0, bMax;
        char    *rgbD = NULL;

	mylog("[[%s]] Handle=(%u,%p) Rec=%d Id=%d info=(%p,%d)\n", func, fHandleType,
			handle, iRecord, fDiagField, rgbDiagInfo, cbDiagInfoMax);
	switch (fDiagField)
	{ 
		case SQL_DIAG_DYNAMIC_FUNCTION:
		case SQL_DIAG_CLASS_ORIGIN:
		case SQL_DIAG_CONNECTION_NAME:
		case SQL_DIAG_MESSAGE_TEXT:
		case SQL_DIAG_SERVER_NAME:
		case SQL_DIAG_SQLSTATE:
		case SQL_DIAG_SUBCLASS_ORIGIN:
			bMax = cbDiagInfoMax * 3 / WCLEN + 1;
			if (rgbD = malloc(bMax), !rgbD)
				return SQL_ERROR;
			rgbL = &blen;
			for (;; bMax = blen + 1, rgbD = realloc(rgbD, bMax))
			{
				ret = PGAPI_GetDiagField(fHandleType, handle, iRecord, fDiagField, rgbD,
					bMax, rgbL);
				if (SQL_SUCCESS_WITH_INFO != ret || blen < bMax)
					break;
			}
			if (SQL_SUCCEEDED(ret))
			{
				SQLULEN ulen = (SQLSMALLINT) utf8_to_ucs2_lf1(rgbD, blen, FALSE, (SQLWCHAR *) rgbDiagInfo, cbDiagInfoMax / WCLEN);
				if (ulen == (SQLULEN) -1)
				{
					blen = (SQLSMALLINT) msgtowstr(NULL, rgbD,
						(int) blen, (LPWSTR) rgbDiagInfo, (int) cbDiagInfoMax / WCLEN);
				}
				else
					blen = (SQLSMALLINT) ulen;
				if (SQL_SUCCESS == ret && blen * WCLEN >= cbDiagInfoMax)
					ret = SQL_SUCCESS_WITH_INFO;
				if (pcbDiagInfo)
				{
#ifdef	WIN32
					extern int	platformId;

					if (VER_PLATFORM_WIN32_WINDOWS == platformId && NULL == rgbDiagInfo && 0 == cbDiagInfoMax)
						blen++;
#endif /* WIN32 */
					*pcbDiagInfo = blen * WCLEN;
				}
			}
			if (rgbD)
				free(rgbD);
			break;
		default:
			rgbD = rgbDiagInfo;
			bMax = cbDiagInfoMax;
			rgbL = pcbDiagInfo;
			ret = PGAPI_GetDiagField(fHandleType, handle, iRecord, fDiagField, rgbD,
				bMax, rgbL);
			break;
	}

	return ret;
}

#endif /* ODBCVER >= 0x0300 */
