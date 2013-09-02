#include "common.h"

SQLHENV env;
SQLHDBC conn;

void
print_diag(char *msg, SQLSMALLINT htype, SQLHANDLE handle)
{
	char sqlstate[32];
	char message[1000];
	SQLINTEGER nativeerror;
	SQLSMALLINT textlen;
	SQLRETURN ret;

	if (msg)
		printf("%s\n", msg);

	ret = SQLGetDiagRec(htype, handle, 1, sqlstate, &nativeerror,
						message, 256, &textlen);

	if (ret != SQL_ERROR)
		printf("%s=%s\n", (CHAR *)sqlstate, (CHAR *)message);
}

void
test_connect_ext(char *extraparams)
{
	SQLRETURN ret;
	SQLCHAR str[1024];
	SQLSMALLINT strl;
	SQLCHAR dsn[1024];

	snprintf(dsn, sizeof(dsn), "DSN=psqlodbc_test_dsn;%s",
			 extraparams ? extraparams : "");

	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);

	SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);

	SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);
	ret = SQLDriverConnect(conn, NULL, dsn, SQL_NTS,
						   str, sizeof(str), &strl,
						   SQL_DRIVER_COMPLETE);
	if (SQL_SUCCEEDED(ret)) {
		printf("connected\n");
	} else {
		print_diag("SQLDriverConnect failed.", SQL_HANDLE_DBC, conn);
		exit(1);
	}
}

void
test_connect(void)
{
	test_connect_ext(NULL);
}

void
test_disconnect(void)
{
	SQLRETURN rc;

	printf("disconnecting\n");
	rc = SQLDisconnect(conn);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLDisconnect failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	rc = SQLFreeConnect(conn);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeConnect failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}
	conn = NULL;

	rc = SQLFreeEnv(env);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLFreeEnv failed", SQL_HANDLE_ENV, env);
		exit(1);
	}
	env = NULL;
}

const char *
datatype_str(SQLSMALLINT datatype)
{
	static char buf[100];

	switch (datatype)
	{
		case SQL_CHAR:
			return "CHAR";
		case SQL_VARCHAR:
			return "VARCHAR";
		case SQL_LONGVARCHAR:
			return "LONGVARCHAR";
		case SQL_WCHAR:
			return "WCHAR";
		case SQL_WVARCHAR:
			return "WVARCHAR";
		case SQL_WLONGVARCHAR:
			return "WLONGVARCHAR";
		case SQL_DECIMAL:
			return "DECIMAL";
		case SQL_NUMERIC:
			return "NUMERIC";
		case SQL_SMALLINT:
			return "SMALLINT";
		case SQL_INTEGER:
			return "INTEGER";
		case SQL_REAL:
			return "REAL";
		case SQL_FLOAT:
			return "FLOAT";
		case SQL_DOUBLE:
			return "DOUBLE";
		case SQL_BIT:
			return "BIT";
		case SQL_TINYINT:
			return "TINYINT";
		case SQL_BIGINT:
			return "BIGINT";
		case SQL_BINARY:
			return "BINARY";
		case SQL_VARBINARY:
			return "VARBINARY";
		case SQL_LONGVARBINARY:
			return "LONGVARBINARY";
		case SQL_TYPE_DATE:
			return "TYPE_DATE";
		case SQL_TYPE_TIME:
			return "TYPE_TIME";
		case SQL_TYPE_TIMESTAMP:
			return "TYPE_TIMESTAMP";
		case SQL_GUID:
			return "GUID";
		default:
			snprintf(buf, sizeof(buf), "unknown sql type %d", datatype);
			return buf;
	}
}

const char *nullable_str(SQLSMALLINT nullable)
{
	static char buf[100];

	switch(nullable)
	{
		case SQL_NO_NULLS:
			return "not nullable";
		case SQL_NULLABLE:
			return "nullable";
		case SQL_NULLABLE_UNKNOWN:
			return "nullable_unknown";
		default:
			snprintf(buf, sizeof(buf), "unknown nullable value %d", nullable);
			return buf;
	}
}

void
print_result_meta(HSTMT hstmt)
{
	SQLRETURN rc;
	SQLSMALLINT numcols;
	int i;

	rc = SQLNumResultCols(hstmt, &numcols);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLNumResultCols failed", SQL_HANDLE_STMT, hstmt);
		return;
	}

	printf("Result set metadata:\n");

	for (i = 1; i <= numcols; i++)
	{
		SQLCHAR colname[50];
		SQLSMALLINT colnamelen;
		SQLSMALLINT datatype;
		SQLULEN colsize;
		SQLSMALLINT decdigits;
		SQLSMALLINT nullable;

		rc = SQLDescribeCol(hstmt, i,
							colname, sizeof(colname),
							&colnamelen,
							&datatype,
							&colsize,
							&decdigits,
							&nullable);
		if (!SQL_SUCCEEDED(rc))
		{
			print_diag("SQLDescribeCol failed", SQL_HANDLE_STMT, hstmt);
			return;
		}
		printf("%s: %s(%d) digits: %d, %s\n",
			   colname, datatype_str(datatype), colsize,
			   decdigits, nullable_str(nullable));
	}
}

void
print_result(HSTMT hstmt)
{
	SQLRETURN rc;
	SQLSMALLINT numcols;

	rc = SQLNumResultCols(hstmt, &numcols);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLNumResultCols failed", SQL_HANDLE_STMT, hstmt);
		return;
	}

	printf("Result set:\n");
	while(1)
	{
		rc = SQLFetch(hstmt);
		if (rc == SQL_NO_DATA) 
			break;
		if (rc == SQL_SUCCESS)
		{
			char buf[40];
			int i;
			SQLLEN ind;

			for (i = 1; i <= numcols; i++)
			{
				rc = SQLGetData(hstmt,i, SQL_C_CHAR, buf, sizeof(buf), &ind);
				if (!SQL_SUCCEEDED(rc))
				{
					print_diag("SQLGetData failed", SQL_HANDLE_STMT, hstmt);
					return;
				}
				if (ind == SQL_NULL_DATA)
					strcpy(buf, "NULL");
				printf("%s%s", (i > 1) ? "\t" : "", buf);
			}
			printf("\n");
		}
		else
		{
			print_diag("SQLFetch failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}
	}
}
