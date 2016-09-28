#include <Windows.h>
#include <odbcinst.h>
#include <stdio.h>

#define RETURN_1 \
{ \
	char	gtlin[80]; \
	fprintf(stderr, "Hit return key to continue ...\n"); \
	fflush(stderr); \
	gets(gtlin); \
	return 1; \
}

void err()
{
	RETCODE	ret;
	DWORD	ErrorCode;
	char	szMsg[256];
	WORD	cbMsg;

	for (int i = 1; i <= 8; i++)
	{
		ret = SQLInstallerError(i, &ErrorCode, szMsg, sizeof(szMsg), &cbMsg);
		if (!SQL_SUCCEEDED(ret))
			break;
		szMsg[cbMsg] = '\0';
		fprintf(stderr, "SQLInstallDriverEx ErrorCode=%d:%s\n", ErrorCode, szMsg);
	}
}

static int inst_driver(const char *driver, int argc, const char **argv)
{
	char	szDriver[256], szOut[256];
	DWORD	usageCount;
	size_t	pcnt;
	WORD	cbMsg;
	char	*psz, *pchr;

	if (argc < 5) {
		fprintf(stderr, "install driver needs 4 parameters\n", argv[0]);
		return 1;
	}
	memset(szDriver, 0, sizeof(szDriver));
	psz = szDriver;
	strncpy(szDriver, driver, sizeof(szDriver));
	pcnt = strlen(psz) + 1;
	psz += pcnt;
	strncpy(psz, argv[4], sizeof(szDriver) - pcnt);
	for (pchr = psz; *pchr; pchr++)
	{
		if (*pchr == '|')
			*pchr = '\0';
	}
	psz += (strlen(psz) + 1);
	if (!SQLInstallDriverEx(szDriver, argv[3], szOut, sizeof(szOut), &cbMsg, ODBC_INSTALL_COMPLETE, &usageCount))
	{
		err();
		fprintf(stderr, "SQLInstallDriverEx %s:%s:%s error\n", szDriver,  szDriver + strlen(szDriver) + 2, argv[3]);
		RETURN_1
	}
	if (!SQLConfigDriver(NULL, ODBC_INSTALL_DRIVER, driver, "", szOut, sizeof(szOut), &cbMsg))
	{
		err();
		fprintf(stderr, "SQLConfigDriver %s error\n", driver);
		RETURN_1
	}

	fprintf(stderr, "SQLInstallDriverEx driver=%s\n", driver);
	return 0;
}
static int uninst_driver(const char *driver)
{
	char	szOut[256];
	DWORD	usageCount;
	WORD	cbMsg;

	if (!SQLConfigDriver(NULL, ODBC_REMOVE_DRIVER, driver, "", szOut, sizeof(szOut), &cbMsg))
	{
		err();
		fprintf(stderr, "SQLConfigDriver REMOVE_DRIVER %s error\n", driver);
		RETURN_1
	}
	if (!SQLRemoveDriver(driver, FALSE, &usageCount))
	{
		err();
		fprintf(stderr, "SQLRemoveDriver %s error\n", driver);
		RETURN_1
	}

	return 0;
}

static int add_dsn(const char *driver, int argc, const char **argv)
{
	char	szDsn[256];
	char	*psz, *pchr;
	size_t	pcnt;
	const char *dsn = argv[3];

	if (argc < 5) {
		fprintf(stderr, "add dsn needs 4 parameters\n");
		return 1;
	}
	memset(szDsn, 0, sizeof(szDsn));
	psz = szDsn;
	_snprintf(psz, sizeof(szDsn), "DSN=%s", dsn);
	pcnt = strlen(psz) + 1;
	psz += pcnt;
	strncpy(psz, argv[4], sizeof(szDsn) - pcnt);
	for (pchr = psz; *pchr; pchr++)
	{
		if (*pchr == '|')
			*pchr = '\0';
	}
	psz += (strlen(psz) + 1);
	if (!SQLConfigDataSource(NULL, ODBC_ADD_SYS_DSN, driver, szDsn))
	{
		err();
		fprintf(stderr, "SQLConfigDataSource ADD_SYS_DSN %s error\n", driver);
		RETURN_1
	}
	fprintf(stderr, "SQLConfigDataSource ADD_SYS_DSN %s\n", dsn);

	return 0;
}

/*
 *	SQLInstallDriverEx -> SQLConfigDriver
 *
 *	SQLRemoveDriver	<- SQConfigDriver(ODBC_REMOVE_DRIVER)
 */
int main(int argc, char **argv)
{
	const char *driver = argv[2];

	if (argc < 3)
	{
		fprintf(stderr, "%s needs at least 2 parameters\n", argv[0]);
		return 1;
	}
	switch (argv[1][0])
	{
		case 'i':
			return inst_driver(driver, argc, argv);
		case 'u':
			return uninst_driver(driver);
		case 'a':
			return add_dsn(driver, argc, argv);
	}

	fprintf(stderr, "mode %s is invalid\n", argv[1]);
	return 1;
}
