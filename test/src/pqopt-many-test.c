/*
 * Test that no libpq keyword given in pqopt is dropped.
 *
 * The driver used to build the PQconnectdbParams() arrays in a fixed
 * array of 30 entries and silently skip the pqopt keywords that did not
 * fit, in libpq's keyword order, so the last ones (target_session_attrs,
 * gssencmode, require_auth, ...) were lost without an error.  A pqopt
 * keyword that repeated one of the driver's own settings with the same
 * value also took a second entry.
 *
 * Each case connects with about 25 pqopt keywords, some of them repeating
 * the driver's settings.  The harmless ones are there to fill the array;
 * target_session_attrs, near the end of libpq's table, decides whether the
 * connection succeeds.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define FILLER_PQOPT \
	"application_name=pqopt_many " \
	"fallback_application_name=pqopt_fallback " \
	"options=-cwork_mem=4MB " \
	"sslmode=disable " \
	"connect_timeout=15 " \
	"keepalives=1 " \
	"keepalives_idle=30 " \
	"keepalives_interval=10 " \
	"keepalives_count=5 " \
	"tcp_user_timeout=0 " \
	"sslcompression=0 " \
	"sslsni=1 " \
	"ssl_min_protocol_version=TLSv1.2 " \
	"ssl_max_protocol_version=TLSv1.3 " \
	"gssencmode=disable " \
	"channel_binding=disable " \
	"passfile=no-such-pgpass " \
	"sslpassword=unused " \
	"sslcert=no-such-cert " \
	"sslkey=no-such-key " \
	"sslrootcert=no-such-root " \
	"sslcrl=no-such-crl " \
	"sslcrldir=no-such-crldir "

static int
try_connect(const char *target, SQLHENV *phenv, SQLHDBC *phdbc)
{
	SQLRETURN	rc;
	SQLCHAR		outstr[1024];
	SQLSMALLINT	outlen = 0;
	char		connstr[2048];

	/*
	 * Give the driver its own keepalive and timeout settings too, equal to
	 * the ones in pqopt, so that they fill their entries twice over in the
	 * old code.  SSLmode=disable is the driver's default.
	 */
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;SSLmode=disable;KeepaliveTime=30;KeepaliveInterval=10;"
			 "pqopt={" FILLER_PQOPT "target_session_attrs=%s}",
			 get_test_dsn(), target);

	rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, phenv);
	if (!SQL_SUCCEEDED(rc))
	{
		fprintf(stderr, "SQLAllocHandle(SQL_HANDLE_ENV) failed\n");
		exit(1);
	}
	SQLSetEnvAttr(*phenv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
	rc = SQLAllocHandle(SQL_HANDLE_DBC, *phenv, phdbc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(SQL_HANDLE_DBC) failed", SQL_HANDLE_ENV, *phenv);
		exit(1);
	}
	SQLSetConnectAttr(*phdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER) 15, 0);

	rc = SQLDriverConnect(*phdbc, NULL, (SQLCHAR *) connstr, SQL_NTS,
						  outstr, sizeof(outstr), &outlen,
						  SQL_DRIVER_NOPROMPT);
	return SQL_SUCCEEDED(rc);
}

static void
done(SQLHENV henv, SQLHDBC hdbc)
{
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

static void
show_setting(SQLHDBC hdbc, const char *name)
{
	SQLRETURN	rc;
	SQLHSTMT	hstmt = SQL_NULL_HSTMT;
	char		sql[100];
	SQLCHAR		buf[100];
	SQLLEN		ind = 0;

	rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	CHECK_CONN_RESULT(rc, "SQLAllocHandle(SQL_HANDLE_STMT) failed", hdbc);
	snprintf(sql, sizeof(sql), "SELECT current_setting('%s')", name);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);
	rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("%s: %s\n", name, (char *) buf);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

int
main(void)
{
	SQLHENV		henv;
	SQLHDBC		hdbc;

	/* every keyword is passed and the connection works */
	if (!try_connect("any", &henv, &hdbc))
	{
		print_diag("SQLDriverConnect with target_session_attrs=any failed",
				   SQL_HANDLE_DBC, hdbc);
		exit(1);
	}
	printf("target_session_attrs=any: connected\n");
	show_setting(hdbc, "application_name");
	show_setting(hdbc, "work_mem");
	SQLDisconnect(hdbc);
	done(henv, hdbc);

	/*
	 * The test server is a primary, so a read-only session can only be
	 * refused.  Before the fix the keyword was dropped and this connected.
	 */
	if (try_connect("read-only", &henv, &hdbc))
	{
		printf("target_session_attrs=read-only: connected, the keyword was dropped\n");
		SQLDisconnect(hdbc);
	}
	else
		printf("target_session_attrs=read-only: refused\n");
	done(henv, hdbc);

	return 0;
}
