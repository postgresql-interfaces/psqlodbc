/*
 * Test cases for connection string bracket parsing in drvconn.c
 * Targets: dconn_get_attributes() switch(*value) OPENING_BRACKET case
 *
 * These tests exercise various bracket-enclosed value patterns in
 * ODBC connection strings to verify robustness against crashes/core dumps.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/*
 * Helper: attempt SQLDriverConnect with a given connection string.
 * Returns SQL_SUCCESS/SQL_ERROR etc. Does NOT exit on failure.
 */
static SQLRETURN
try_connect(const char *connstr, const char *test_label)
{
	SQLRETURN ret;
	SQLCHAR outstr[1024];
	SQLSMALLINT outlen;
	SQLHENV local_env = SQL_NULL_HENV;
	SQLHDBC local_conn = SQL_NULL_HDBC;

	printf("Test: %s\n", test_label);
	printf("  ConnStr: %s\n", connstr);

	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &local_env);
	SQLSetEnvAttr(local_env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
	SQLAllocHandle(SQL_HANDLE_DBC, local_env, &local_conn);

	ret = SQLDriverConnect(local_conn, NULL,
						   (SQLCHAR *)connstr, SQL_NTS,
						   outstr, sizeof(outstr), &outlen,
						   SQL_DRIVER_NOPROMPT);

	if (SQL_SUCCEEDED(ret))
	{
		printf("  Result: Connected OK\n");
		SQLDisconnect(local_conn);
	}
	else
	{
		SQLCHAR sqlstate[6], msg[1024];
		SQLINTEGER native_err;
		SQLSMALLINT msglen;

		SQLGetDiagRec(SQL_HANDLE_DBC, local_conn, 1,
					  sqlstate, &native_err, msg, sizeof(msg), &msglen);
		printf("  Result: Failed (SQLSTATE=%s): %s\n", sqlstate, msg);
	}

	SQLFreeHandle(SQL_HANDLE_DBC, local_conn);
	SQLFreeHandle(SQL_HANDLE_ENV, local_env);

	printf("  [No crash - PASS]\n\n");
	return ret;
}

/*
 * TC01: Normal braced value - basic path
 * Covers: OPENING_BRACKET case entry, closep found, closep[1]=='\0' branch
 */
static void
test_normal_braced_value(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={simple_password}", get_test_dsn());
	try_connect(connstr, "TC01: Normal braced value");
}

/*
 * TC02: Braced value containing semicolons
 * Covers: delp != termp, strtok splits at ';' inside braces,
 *         closep found after delimiter restoration
 */
static void
test_braced_with_semicolons(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={pass;word;123}", get_test_dsn());
	try_connect(connstr, "TC02: Braced value with semicolons");
}

/*
 * TC03: Braced value at end of string (eoftok path)
 * Covers: delp >= termp -> eoftok = TRUE
 */
static void
test_braced_at_end_eoftok(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={lastvalue}", get_test_dsn());
	try_connect(connstr, "TC03: Braced value at end (eoftok)");
}

/*
 * TC04: Escaped closing bracket (}} -> literal })
 * Covers: CLOSING_BRACKET == closep[1] branch, valuen = closep + 2, continue
 */
static void
test_escaped_closing_bracket(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={pass}}word}", get_test_dsn());
	try_connect(connstr, "TC04: Escaped closing bracket }}");
}

/*
 * TC05: Multiple escaped brackets
 * Covers: multiple iterations of CLOSING_BRACKET == closep[1]
 */
static void
test_multiple_escaped_brackets(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={a}}b}}c}}", get_test_dsn());
	try_connect(connstr, "TC05: Multiple escaped brackets }}}}");
}

/*
 * TC06: Missing closing bracket - error path 1
 * Covers: NULL == closep with !delp -> "closing bracket doesn't exist 1"
 */
static void
test_missing_closing_bracket_1(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={no_close;Server=host", get_test_dsn());
	try_connect(connstr, "TC06: Missing closing bracket (error path 1)");
}

/*
 * TC07: Missing closing bracket - error path 2
 * Covers: closep = strchr(delp+1, CLOSING_BRACKET) fails ->
 *         "closing bracket doesn't exist 2"
 */
static void
test_missing_closing_bracket_2(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={no_close_at_all", get_test_dsn());
	try_connect(connstr, "TC07: Missing closing bracket (error path 2)");
}

/*
 * TC08: Invalid character after closing bracket
 * Covers: else branch after checking ';', '\0', delp==closep+1
 *         -> "subsequent char to the closing bracket is %c"
 */
static void
test_invalid_char_after_bracket(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={value}x;Server=host", get_test_dsn());
	try_connect(connstr, "TC08: Invalid char after closing bracket");
}

/*
 * TC09: Empty braced value
 * Covers: closep found immediately after OPENING_BRACKET
 *         closep[1] == ATTRIBUTE_DELIMITER path
 */
static void
test_empty_braced_value(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={};Server=host", get_test_dsn());
	try_connect(connstr, "TC09: Empty braced value {}");
}

/*
 * TC10: Braced value with closing bracket followed by null (end of string)
 * Covers: closep[1] == '\0' in the for loop
 */
static void
test_braced_value_closep_null_term(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;Extra=val;PWD={password}", get_test_dsn());
	try_connect(connstr, "TC10: Braced value where closep[1]=='\\0'");
}

/*
 * TC11: Braced value spanning multiple strtok segments
 * Covers: delp restoration (*delp = ATTRIBUTE_DELIMITER),
 *         multiple iterations searching for closing bracket across segments
 */
static void
test_braced_spanning_segments(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={semi;colon;in;value};Server=host", get_test_dsn());
	try_connect(connstr, "TC11: Braced value spanning multiple segments");
}

/*
 * TC12: Escaped bracket at segment boundary
 * Covers: valuen == delp after closep + 2, restore delimiter
 */
static void
test_escaped_bracket_at_boundary(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={val}};more}", get_test_dsn());
	try_connect(connstr, "TC12: Escaped bracket near segment boundary");
}

/*
 * TC13: Closing bracket where delp == closep + 1
 * Covers: delp == closep + 1 condition in else-if
 */
static void
test_closep_plus_one_is_delp(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={value};", get_test_dsn());
	try_connect(connstr, "TC13: Closing bracket + delimiter (delp==closep+1)");
}

/*
 * TC14: Very long braced value (buffer boundary stress)
 * Covers: potential buffer overflows near allocation boundaries
 */
static void
test_long_braced_value(void)
{
	char connstr[4096];
	char longval[2048];
	int i;

	memset(longval, 'A', sizeof(longval) - 1);
	longval[sizeof(longval) - 1] = '\0';

	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={%s}", get_test_dsn(), longval);
	try_connect(connstr, "TC14: Very long braced value (2KB)");
}

/*
 * TC15: Braced value with only opening bracket and immediate end
 * Covers: delp >= termp path (value is just "{" at the very end)
 */
static void
test_only_opening_bracket(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={", get_test_dsn());
	try_connect(connstr, "TC15: Only opening bracket at end");
}

/*
 * TC16: valuen >= termp after escaped bracket
 * Covers: valuen >= termp break in the }} handling
 */
static void
test_escaped_bracket_at_termp(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={x}}", get_test_dsn());
	try_connect(connstr, "TC16: Escaped bracket at string end (valuen>=termp)");
}

/*
 * TC17: strtok_arg + 1 >= termp after successful bracket parse
 * Covers: eoftok = TRUE in the ATTRIBUTE_DELIMITER == closep[1] branch
 */
static void
test_strtok_arg_at_termp(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={val};", get_test_dsn());
	try_connect(connstr, "TC17: strtok_arg at end after bracket parse");
}

/*
 * TC18: Multiple braced values in one connection string
 * Covers: full parse loop repeated, bracket handling re-entry
 */
static void
test_multiple_braced_values(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={pass;1};Description={test;desc}", get_test_dsn());
	try_connect(connstr, "TC18: Multiple braced values in string");
}

/*
 * TC19: Braced value with embedded null-like content (all printable)
 * Covers: stress test for string boundary detection
 */
static void
test_braced_special_chars(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD={=;{nested};==}", get_test_dsn());
	try_connect(connstr, "TC19: Special characters inside braces");
}

/*
 * TC20: Connection string with no braces (non-bracket path)
 * Covers: switch default - verifies bracket code is NOT triggered
 */
static void
test_no_braces_baseline(void)
{
	char connstr[1024];
	snprintf(connstr, sizeof(connstr),
			 "DSN=%s;PWD=plaintext", get_test_dsn());
	try_connect(connstr, "TC20: No braces baseline (control test)");
}

int main(int argc, char **argv)
{
	printf("=== Bracket Parsing Test Suite for drvconn.c ===\n");
	printf("Target: dconn_get_attributes() OPENING_BRACKET case\n\n");

	/* Basic functional tests */
	test_no_braces_baseline();          /* TC20 - baseline */
	test_normal_braced_value();         /* TC01 */
	test_empty_braced_value();          /* TC09 */
	test_braced_at_end_eoftok();        /* TC03 */
	test_braced_value_closep_null_term(); /* TC10 */

	/* Semicolon inside braces */
	test_braced_with_semicolons();      /* TC02 */
	test_braced_spanning_segments();    /* TC11 */
	test_multiple_braced_values();      /* TC18 */

	/* Escaped bracket tests */
	test_escaped_closing_bracket();     /* TC04 */
	test_multiple_escaped_brackets();   /* TC05 */
	test_escaped_bracket_at_boundary(); /* TC12 */
	test_escaped_bracket_at_termp();    /* TC16 */

	/* Boundary conditions */
	test_closep_plus_one_is_delp();     /* TC13 */
	test_strtok_arg_at_termp();         /* TC17 */
	test_only_opening_bracket();        /* TC15 */
	test_long_braced_value();           /* TC14 */

	/* Error path tests (should NOT crash, should return error) */
	test_missing_closing_bracket_1();   /* TC06 */
	test_missing_closing_bracket_2();   /* TC07 */
	test_invalid_char_after_bracket();  /* TC08 */

	/* Special content */
	test_braced_special_chars();        /* TC19 */

	printf("=== All tests completed without crash ===\n");
	return 0;
}
