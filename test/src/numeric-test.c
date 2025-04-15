/*
 * Test cases for dealing with SQL_NUMERIC_STRUCT
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

static unsigned char
hex_to_int(char c)
{
	char		result;

	if (c >= '0' && c <= '9')
		result = c - '0';
	else if (c >= 'a' && c <= 'f')
		result = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		result = c - 'A' + 10;
	else
	{
		fprintf(stderr, "invalid hex-encoded numeric value\n");
		exit(1);
	}
	return (unsigned char) result;
}

static void
build_numeric_struct(SQL_NUMERIC_STRUCT *numericparam,
					 unsigned char sign, char *hexstr,
					 unsigned char precision, unsigned char scale)
{
	int			len;

	/* parse the hex-encoded value */
	memset(numericparam, 0, sizeof(SQL_NUMERIC_STRUCT));

	numericparam->sign = sign;
	numericparam->precision = precision;
	numericparam->scale = scale;

	len = 0;
	while (*hexstr)
	{
		if (*hexstr == ' ')
		{
			hexstr++;
			continue;
		}
		if (len >= SQL_MAX_NUMERIC_LEN)
		{
			fprintf(stderr, "hex-encoded numeric value too long\n");
			exit(1);
		}
		numericparam->val[len] =
			hex_to_int(*hexstr) << 4 | hex_to_int(*(hexstr + 1));
		hexstr += 2;
		len++;
	}
}

static void
test_numeric_param(HSTMT hstmt, unsigned char sign, char *hexval,
				   unsigned char precision, unsigned char scale)
{
	SQL_NUMERIC_STRUCT numericparam;
	SQLLEN		cbParam1;
	SQLRETURN	rc;
	char		buf[200];

	build_numeric_struct(&numericparam, sign, hexval, precision, scale);

	cbParam1 = sizeof(numericparam);
	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_NUMERIC,	/* value type */
						  SQL_NUMERIC,	/* param type */
						  0,			/* column size (ignored for SQL_INTERVAL_SECOND) */
						  0,			/* dec digits */
						  &numericparam, /* param value ptr */
						  sizeof(numericparam), /* buffer len (ignored for SQL_C_INTERVAL_SECOND) */
						  &cbParam1 /* StrLen_or_IndPtr (ignored for SQL_C_INTERVAL_SECOND) */);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	/* Execute */
	rc = SQLExecute(hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLExecute failed", SQL_HANDLE_STMT, hstmt);
    }
	else
	{
		/* print result */
		rc = SQLFetch(hstmt);
		CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

		rc = SQLGetData(hstmt, 1, SQL_C_CHAR, buf, sizeof(buf), NULL);
		CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
		printf("sign %u prec %u scale %d val %s:\n    %s\n",
			   sign, precision, scale, hexval, buf);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

static void
test_numeric_result(HSTMT hstmt, char *numstr)
{
	char		sql[100];
	SQL_NUMERIC_STRUCT numericres;
	SQLRETURN	rc;

	/*
	 * assume 'numstr' param to be well-formed (we're testing how the
	 * results come out, not the input handling)
	 */
	snprintf(sql, sizeof(sql), "SELECT '%s'::numeric", numstr);
	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetData(hstmt, 1, SQL_C_NUMERIC, &numericres, sizeof(numericres), NULL);
	CHECK_STMT_RESULT(rc, "SQLGetData failed", hstmt);
	printf("%s:\n     sign %u prec %u scale %d val %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		   numstr, numericres.sign, numericres.precision, numericres.scale,
		   numericres.val[0], numericres.val[1],
		   numericres.val[2], numericres.val[3],
		   numericres.val[4], numericres.val[5],
		   numericres.val[6], numericres.val[7],
		   numericres.val[8], numericres.val[9],
		   numericres.val[10], numericres.val[11],
		   numericres.val[12], numericres.val[13],
		   numericres.val[14], numericres.val[15]);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	HSTMT		hstmt = SQL_NULL_HSTMT;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/**** Test binding SQL_NUMERIC_STRUCT params (SQL_C_NUMERIC) ****/

	printf("Testing SQL_NUMERIC_STRUCT params...\n\n");

	rc = SQLPrepare(hstmt, (SQLCHAR *) "SELECT ?::numeric", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	/* 25.212 (per Microsoft KB 22831) */
	test_numeric_param(hstmt, 1, "7C62", 5, 3);

	/* 24197857161011715162171839636988778104 */
	test_numeric_param(hstmt, 1, "78563412 78563412 78563412 78563412", 41, 0);

	/* 12345678901234567890123456789012345678 */
	test_numeric_param(hstmt, 1, "4EF338DE509049C4133302F0F6B04909", 38, 0);

	/* highest possible non-scaled: 340282366920938463463374607431768211455 */
	test_numeric_param(hstmt, 1, "FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF", 50, 0);

	/* positive and negative zero */
	test_numeric_param(hstmt, 1, "00", 1, 0);
	test_numeric_param(hstmt, 0, "00", 1, 0);

	/* -7.70 */
	test_numeric_param(hstmt, 1, "0203", 3, 2);

	/* 0.12345, with 1-6 digit precision: */
	test_numeric_param(hstmt, 1, "3930", 1, 5);
	test_numeric_param(hstmt, 1, "3930", 2, 5);
	test_numeric_param(hstmt, 1, "3930", 3, 5);
	test_numeric_param(hstmt, 1, "3930", 4, 5);
	test_numeric_param(hstmt, 1, "3930", 5, 5);
	test_numeric_param(hstmt, 1, "3930", 6, 5);

	/* large scale with small value */
	test_numeric_param(hstmt, 1, "0203", 3, 50);

	/* medium-sized scale and precision */
	test_numeric_param(hstmt, 1, "0203", 25, 80);

	/* max length output; negative with max scale and decimal dot */
	test_numeric_param(hstmt, 0, "FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF", 40, 127);

	/* large scale with small value */
	test_numeric_param(hstmt, 1, "0203", 3, 50);


	/**** Test fetching SQL_NUMERIC_STRUCT results ****/
	printf("Testing SQL_NUMERIC_STRUCT results...\n\n");

	test_numeric_result(hstmt, "25.212");
	test_numeric_result(hstmt, "24197857161011715162171839636988778104");
	test_numeric_result(hstmt, "12345678901234567890123456789012345678");
	/* highest number */
	test_numeric_result(hstmt, "340282366920938463463374607431768211455");
	/* overflow */
	test_numeric_result(hstmt, "340282366920938463463374607431768211456");
	test_numeric_result(hstmt, "340282366920938463463374607431768211457");

	test_numeric_result(hstmt, "-0");
	test_numeric_result(hstmt, "0");
	test_numeric_result(hstmt, "-7.70");
	test_numeric_result(hstmt, "999999999999");

	/* Clean up */
	test_disconnect();

	return 0;
}
