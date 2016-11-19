#include <Windows.h>
#include <odbcinst.h>
#include <stdio.h>

#define RETURN_1 \
{ \
	char	gtlin[80]; \
	fprintf(stderr, "Hit return key to continue ...\n"); \
	fflush(stderr); \
	fgets(gtlin, sizeof(gtlin), stdin); \
	return 1; \
}

DWORD err()
{
	RETCODE	ret;
	DWORD	ErrorCode = 0;
	char	szMsg[256];
	WORD	cbMsg;
	int	i;

	for (i = 1; i <= 8; i++)
	{
		ret = SQLInstallerError(i, &ErrorCode, szMsg, sizeof(szMsg), &cbMsg);
		if (!SQL_SUCCEEDED(ret))
			break;
		szMsg[cbMsg] = '\0';
		fprintf(stderr, "SQLInstallDriverEx ErrorCode=%d:%s\n", ErrorCode, szMsg);
		switch (ErrorCode)
		{
			case ODBC_ERROR_COMPONENT_NOT_FOUND:
			case ODBC_ERROR_INVALID_NAME:
				return ErrorCode;
		}
	}
	return ErrorCode;
}

static int inst_driver(const char *driver, const char *pathIn, const char *key_value_pairs)
{
	char	szDriver[256], szOut[256];
	DWORD	usageCount;
	size_t	pcnt;
	WORD	cbMsg;
	char	*psz, *pchr;

	memset(szDriver, 0, sizeof(szDriver));
	psz = szDriver;
	strncpy(szDriver, driver, sizeof(szDriver));
	pcnt = strlen(psz) + 1;
	psz += pcnt;
	strncpy(psz, key_value_pairs, sizeof(szDriver) - pcnt);
	for (pchr = psz; *pchr; pchr++)
	{
		if (*pchr == '|')
			*pchr = '\0';
	}
	psz += (strlen(psz) + 1);
	if (!SQLInstallDriverEx(szDriver, pathIn, szOut, sizeof(szOut), &cbMsg, ODBC_INSTALL_COMPLETE, &usageCount))
	{
		err();
		fprintf(stderr, "SQLInstallDriverEx %s:%s:%s error\n", szDriver,  szDriver + strlen(szDriver) + 2, pathIn);
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

static int add_dsn(const char *driver, const char *dsn, const char *key_value_pairs)
{
	char	szDsn[256];
	char	*psz, *pchr;
	size_t	pcnt;

	memset(szDsn, 0, sizeof(szDsn));
	psz = szDsn;
	_snprintf(psz, sizeof(szDsn), "DSN=%s", dsn);
	pcnt = strlen(psz) + 1;
	psz += pcnt;
	strncpy(psz, key_value_pairs, sizeof(szDsn) - pcnt);
	for (pchr = psz; *pchr; pchr++)
	{
		if (*pchr == '|')
			*pchr = '\0';
	}
	psz += (strlen(psz) + 1);
	if (!SQLConfigDataSource(NULL, ODBC_ADD_SYS_DSN, driver, szDsn))
	{
		switch (err())
		{
			case ODBC_ERROR_COMPONENT_NOT_FOUND:
			case ODBC_ERROR_INVALID_NAME:
				return -1;
		}
		fprintf(stderr, "SQLConfigDataSource ADD_SYS_DSN %s error\n", driver);
		RETURN_1
	}
	fprintf(stderr, "SQLConfigDataSource ADD_SYS_DSN %s\n", dsn);

	return 0;
}

typedef	SQLRETURN (SQL_API *SQLAPIPROC)();
static int
IsDsnExist(const char *dsn)
{
	HMODULE	hmodule;
	SQLHENV	henv;
	int	retcode = 1;
	char	dsnname[64], dsnatt[128];
	SQLUSMALLINT	direction = SQL_FETCH_FIRST;
	SQLSMALLINT	dsnncount, dsnacount;
	SQLRETURN	ret;
	SQLAPIPROC	addr;

	hmodule = LoadLibrary("ODBC32");
	if (!hmodule)
	{
		fprintf(stderr, "LoadLibrary ODBC32 failed\n");
		RETURN_1;
	}
	addr = (SQLAPIPROC) GetProcAddress(hmodule, "SQLAllocEnv");
	if (!addr)
	{
		fprintf(stderr, "GetProcAddress for SQLAllocEnv failed\n");
		RETURN_1;
	}
	ret = (*addr)(&henv);
	if (!SQL_SUCCEEDED(ret))
	{
		fprintf(stderr, "SQLAllocEnv failed\n");
		RETURN_1;
	}
	retcode = -1;
	do
	{
		ret = SQLDataSources(henv, direction,
						 dsnname, sizeof(dsnname), &dsnncount,
						 dsnatt, sizeof(dsnatt), &dsnacount);
		if (!SQL_SUCCEEDED(ret))
			break;
		if (_strnicmp(dsnname, dsn, sizeof(dsnname)) == 0)
		{
			retcode = 0;
			break;
		}
		direction = SQL_FETCH_NEXT;
	} while (1);
	addr = (SQLAPIPROC) GetProcAddress(hmodule, "SQLFreeEnv");
	if (addr)
		(*addr)(henv);
	FreeLibrary(hmodule);

	return retcode;
}

static int register_dsn(const char *driver, const char *dsn, const char *key_value_pairs, const char *pathIn, const char *driver_key_value_pairs)
{
	int	ret;

	if (ret = IsDsnExist(dsn), ret != -1)
		return ret;
	if (ret = add_dsn(driver, dsn, key_value_pairs), ret != -1)
		return ret;
	fprintf(stderr, "\tAdding driver %s of %s\n", driver, pathIn);
	if (ret = inst_driver(driver, pathIn, driver_key_value_pairs), ret != 0)
		return ret;
	return add_dsn(driver, dsn, key_value_pairs);
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
		case 'e':
			return IsDsnExist(argv[2]);
		case 'r':
			if (argc < 7) {
				fprintf(stderr, "register dsn needs 4 parameters\n");
				return 1;
			}
			return register_dsn(driver, argv[3], argv[4], argv[5], argv[6]);
		case 'i':
			if (argc < 5) {
				fprintf(stderr, "install driver needs 4 parameters\n");
				return 1;
			}
			return inst_driver(driver, argv[3], argv[4]);
		case 'u':
			return uninst_driver(driver);
		case 'a':
			if (argc < 5) {
				fprintf(stderr, "add dsn needs 4 parameters\n");
				return 1;
			}
			return add_dsn(driver, argv[3], argv[4]);
	}

	fprintf(stderr, "mode %s is invalid\n", argv[1]);
	return 1;
}
