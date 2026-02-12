/*
 * Test that SQLGetInfo returns SQLSTATE HY096 for invalid info types,
 * and that SQLSetConnectAttr returns SQLSTATE HY092 for invalid attributes.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int
main(int argc, char **argv)
{
	SQLRETURN	rc;
	char		buf[256];
	SQLSMALLINT	len;
	char		sqlstate[6];
	SQLINTEGER	nativeerror;
	SQLSMALLINT	textlen;

	test_connect();

	/*
	 * Test 1: SQLGetInfo with invalid info type should return HY096
	 */
	printf("Testing SQLGetInfo with invalid info type 99999\n");
	rc = SQLGetInfo(conn, (SQLUSMALLINT) 99999, buf, sizeof(buf), &len);
	if (rc == SQL_ERROR)
	{
		rc = SQLGetDiagRec(SQL_HANDLE_DBC, conn, 1, (SQLCHAR *) sqlstate,
						   &nativeerror, NULL, 0, &textlen);
		printf("SQLSTATE: %s\n", sqlstate);
	}
	else
	{
		printf("unexpected result: %d\n", rc);
	}

	/*
	 * Test 2: SQLSetConnectAttr with invalid attribute should return HY092
	 */
	printf("Testing SQLSetConnectAttr with invalid attribute 99999\n");
	rc = SQLSetConnectAttr(conn, (SQLINTEGER) 99999, (SQLPOINTER) 0, SQL_IS_UINTEGER);
	if (rc == SQL_ERROR)
	{
		rc = SQLGetDiagRec(SQL_HANDLE_DBC, conn, 1, (SQLCHAR *) sqlstate,
						   &nativeerror, NULL, 0, &textlen);
		printf("SQLSTATE: %s\n", sqlstate);
	}
	else
	{
		printf("unexpected result: %d\n", rc);
	}

	/* Clean up */
	test_disconnect();

	return 0;
}
