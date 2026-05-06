/*
 * Test that SQL_DBMS_VER returns the correct version format.
 * For PG >= 10, the format should be "major.minor.0" (e.g. "18.3.0"),
 * not "major.0.minor" (e.g. "18.0.3").
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	char		ver[32];
	int		major = 0, minor = 0, patch = 0;

	test_connect();

	rc = SQLGetInfo(conn, SQL_DBMS_VER, ver, sizeof(ver), NULL);
	CHECK_CONN_RESULT(rc, "SQLGetInfo for SQL_DBMS_VER failed", conn);

	if (sscanf(ver, "%d.%d.%d", &major, &minor, &patch) != 3)
	{
		printf("FAIL: version string not in expected ##.##.## format\n");
		exit(1);
	}

	/* For PG >= 10, minor should not be 0 unless it's truly a .0 release */
	if (major >= 10)
	{
		if (minor == 0 && patch != 0)
		{
			printf("FAIL: version %s looks like old-style parsing (major.0.minor instead of major.minor.0)\n", ver);
			exit(1);
		}
		printf("ok: version format correct for PG >= 10\n");
	}
	else
	{
		printf("ok: PG < 10 version format\n");
	}

	test_disconnect();

	return 0;
}
