/*
 * Test that the driver's debug and communication logs don't contain the
 * secrets of a connection.
 *
 * A first connection turns both logs on, with Debug=1 and CommLog=1, and
 * stays open, so that the connections after it are logged from the
 * start. Each of those connects with a secret written in one of the ways
 * the connection string grammar, or libpq's within pqopt, allows. Their
 * connects may fail, because the secrets are not the real password; the
 * driver logs them before it finds that out. At the end the test reads
 * its own log files and reports, for each secret, whether any part of it
 * is in them.
 *
 * The log files are found in the default log directory, /tmp, by the user
 * name and process ID in their names. On Windows the default directory
 * and the names differ, so the test is skipped there.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#ifndef WIN32
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#endif

/* Each secret's parts are the strings that must not be in the logs */
static const struct
{
	const char *what;
	const char *attributes;		/* after DSN=...; */
	const char *parts[3];
}			cases[] = {
	{"PWD", "PWD=Sx1aaaa", {"Sx1aaaa"}},
	{"Password with spaces", "Password=Sx2aaaa Sx2bbbb", {"Sx2aaaa", "Sx2bbbb"}},
	{"braced PWD with a ;", "PWD={Sx3aaaa;Sx3bbbb}", {"Sx3aaaa", "Sx3bbbb"}},
	{"braced PWD with }}", "PWD={Sx4aaaa}}Sx4bbbb}", {"Sx4aaaa", "Sx4bbbb"}},
	{"pqopt password with spaces around =",
	 "pqopt={password = Sx5aaaa}", {"Sx5aaaa"}},
	{"pqopt sslpassword with an escaped space",
	 "pqopt={sslpassword=Sx6aaaa\\ Sx6bbbb}", {"Sx6aaaa", "Sx6bbbb"}},
	{"pqopt sslpassword quoted with an escaped quote",
	 "pqopt={sslpassword='\\'Sx7aaaa Sx7bbbb'}", {"Sx7aaaa", "Sx7bbbb"}},
	{"pqopt oauth_client_secret",
	 "pqopt={oauth_client_secret=Sx8aaaa}", {"Sx8aaaa"}},
};
#define NCASES (sizeof(cases) / sizeof(cases[0]))

/* The secrets passed to SQLConnect, SQLDriverConnectW and SQLConnectW */
#define CONNECT_PASSWORD "Sx9aaaa"
#define DRIVERCONNECTW_PASSWORD "Sx10aaaa"
#define CONNECTW_PASSWORD "Sx11aaaa"

#ifndef WIN32
static void
to_wide(const char *s, SQLWCHAR *w)
{
	while (*s)
		*w++ = (unsigned char) *s++;
	*w = 0;
}

/*
 * Read all of the log files of this process into one buffer. Return NULL
 * when there are none.
 */
static char *
read_own_logs(int *nfiles)
{
	char		suffix[256];
	struct passwd *pw = getpwuid(getuid());
	DIR		   *dir;
	struct dirent *ent;
	char	   *all = NULL;
	size_t		alllen = 0;

	*nfiles = 0;
	snprintf(suffix, sizeof(suffix), "%s%u.log",
			 pw ? pw->pw_name : "", (unsigned) getpid());
	if (NULL == (dir = opendir("/tmp")))
		return NULL;
	while (NULL != (ent = readdir(dir)))
	{
		size_t		nlen = strlen(ent->d_name);
		size_t		slen = strlen(suffix);
		char		path[1024];
		FILE	   *f;
		char		buf[4096];
		size_t		n;

		if ((strncmp(ent->d_name, "mylog_", 6) != 0 &&
			 strncmp(ent->d_name, "psqlodbc_", 9) != 0) ||
			nlen < slen || strcmp(ent->d_name + nlen - slen, suffix) != 0)
			continue;
		snprintf(path, sizeof(path), "/tmp/%s", ent->d_name);
		if (NULL == (f = fopen(path, "r")))
			continue;
		(*nfiles)++;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		{
			char	   *grown = realloc(all, alllen + n + 1);

			if (!grown)
				break;
			all = grown;
			memcpy(all + alllen, buf, n);
			alllen += n;
			all[alllen] = '\0';
		}
		fclose(f);
		/* the files are this test's own */
		unlink(path);
	}
	closedir(dir);
	return all;
}
#endif

int
main(int argc, char **argv)
{
#ifdef WIN32
	printf("Skipped, the test reads the logs from /tmp\n");
	return 0;
#else
	SQLRETURN	rc;
	SQLHDBC		conn2 = SQL_NULL_HDBC;
	char		connstr[512];
	SQLWCHAR	wconnstr[512], wdsn[128], wpassword[64];
	char	   *logs;
	int			nfiles;
	size_t		i, j;

	test_connect_ext("Debug=1;CommLog=1");

	for (i = 0; i < NCASES; i++)
	{
		snprintf(connstr, sizeof(connstr), "DSN=%s;%s", get_test_dsn(),
				 cases[i].attributes);
		rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn2);
		CHECK_CONN_RESULT(rc, "failed to allocate connection handle", conn2);
		rc = SQLDriverConnect(conn2, NULL, (SQLCHAR *) connstr, SQL_NTS,
							  NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
		if (SQL_SUCCEEDED(rc))
			SQLDisconnect(conn2);
		SQLFreeHandle(SQL_HANDLE_DBC, conn2);
	}
	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn2);
	CHECK_CONN_RESULT(rc, "failed to allocate connection handle", conn2);
	rc = SQLConnect(conn2, (SQLCHAR *) get_test_dsn(), SQL_NTS, NULL, 0,
					(SQLCHAR *) CONNECT_PASSWORD, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		SQLDisconnect(conn2);
	SQLFreeHandle(SQL_HANDLE_DBC, conn2);

	/* the Unicode entry points convert the strings before they log them */
	snprintf(connstr, sizeof(connstr), "DSN=%s;PWD=%s", get_test_dsn(),
			 DRIVERCONNECTW_PASSWORD);
	to_wide(connstr, wconnstr);
	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn2);
	CHECK_CONN_RESULT(rc, "failed to allocate connection handle", conn2);
	rc = SQLDriverConnectW(conn2, NULL, wconnstr, SQL_NTS, NULL, 0, NULL,
						   SQL_DRIVER_NOPROMPT);
	if (SQL_SUCCEEDED(rc))
		SQLDisconnect(conn2);
	SQLFreeHandle(SQL_HANDLE_DBC, conn2);
	to_wide(get_test_dsn(), wdsn);
	to_wide(CONNECTW_PASSWORD, wpassword);
	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn2);
	CHECK_CONN_RESULT(rc, "failed to allocate connection handle", conn2);
	rc = SQLConnectW(conn2, wdsn, SQL_NTS, NULL, 0, wpassword, SQL_NTS);
	if (SQL_SUCCEEDED(rc))
		SQLDisconnect(conn2);
	SQLFreeHandle(SQL_HANDLE_DBC, conn2);

	/* closing the first connection writes out and closes the logs */
	test_disconnect();

	logs = read_own_logs(&nfiles);
	if (!logs || nfiles == 0)
	{
		printf("the logs of this process were not found in /tmp\n");
		return 1;
	}
	printf("found the logs\n");
	for (i = 0; i < NCASES; i++)
	{
		int			found = 0;

		for (j = 0; j < 3 && cases[i].parts[j]; j++)
			if (strstr(logs, cases[i].parts[j]))
				found = 1;
		printf("%s: %s\n", cases[i].what,
			   found ? "IN THE LOGS" : "not in the logs");
	}
	printf("SQLConnect password: %s\n",
		   strstr(logs, CONNECT_PASSWORD) ? "IN THE LOGS" : "not in the logs");
	printf("SQLDriverConnectW PWD: %s\n",
		   strstr(logs, DRIVERCONNECTW_PASSWORD) ? "IN THE LOGS" : "not in the logs");
	printf("SQLConnectW password: %s\n",
		   strstr(logs, CONNECTW_PASSWORD) ? "IN THE LOGS" : "not in the logs");
	free(logs);
	return 0;
#endif
}
