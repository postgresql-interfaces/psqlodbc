/*
 * Test conversion of values received from server to SQL datatypes.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#ifdef WIN32
#include <float.h>
#endif

#include "common.h"

/* Visual Studio didn't have isinf and isinf until VS2013. */
#if defined (WIN32) && (_MSC_VER < 1800)
#define isinf(x) ((_fpclass(x) == _FPCLASS_PINF) || (_fpclass(x) == _FPCLASS_NINF))
#define isnan(x) _isnan(x)
#endif

static const char *pgtypes[] =
{
	/*
	 * This query uses every PostgreSQL data type the driver recognizes,
	 * i.e. that has a PG_TYPE_* code.
	 */
	"true",			"boolean",
	"\\x464F4F",	"bytea",
	"x",			"char",
	"namedata",		"name",
	"1234567890",	"int8",
	"12345",		"int2",
	"1 2 3 4 5",	"int2vector",
	"1234567",		"int4",
	"int4pl",		"regproc",
	"textdata",		"text",
/*	"3234567901",	"oid", */ /* OID can be confused with large objects */
	"(1,2)",		"tid",
	"1234",			"xid",
	"4321",			"cid",
	"1 2 3",		"oidvector",
	"<foo>bar</foo>", "xml",
	"{<foo>bar</foo>}", "_xml",
	"10.0.0.1", "cidr",
	"1.234", "float4",
	"1.23456789012", "float8",
	"2011-01-14 16:49:18+03", "abstime",
	"foo", "unknown",
	"1.23", "money",
	"08-00-2b-01-02-03", "macaddr",
	"10.0.0.1", "inet",
	"{foo, bar}", "_text",
	"{foo, bar}", "_bpchar",
	"{foo, bar}", "_varchar",
	"foobar", "bpchar",
	"foobar", "varchar",
	"2011-02-13", "date",
	"13:23:34", "time",
	"2011-02-15 15:49:18", "timestamp",
	"2011-02-16 17:49:18+03", "timestamptz",
	"10 years -11 months -12 days +13:14", "interval",
	"1", "bit",
	"1234.567890", "numeric",
	"foocur", "refcursor",
	NULL
};

#define X(sqltype) { sqltype, #sqltype }

static const struct
{
	int sqltype;
	char *str;
} sqltypes[] =
{
	X(SQL_C_CHAR),
	X(SQL_C_WCHAR),
	X(SQL_C_SSHORT),
	X(SQL_C_USHORT),
	X(SQL_C_SLONG),
	X(SQL_C_ULONG),
	X(SQL_C_FLOAT),
	X(SQL_C_DOUBLE),
	X(SQL_C_BIT),
	X(SQL_C_STINYINT),
	X(SQL_C_UTINYINT),
	X(SQL_C_SBIGINT),
	X(SQL_C_UBIGINT),
	X(SQL_C_BINARY),
	X(SQL_C_BOOKMARK),
	X(SQL_C_VARBOOKMARK),
	/*
	 * Converting arbitrary things to date and timestamp produces results that
	 * depend on the current timestamp, because the driver substitutes the
	 * current year/month/datefor missing values. Disable for now, to get a
	 * reproducible result.
	 */
/*	X(SQL_C_TYPE_DATE), */
	X(SQL_C_TYPE_TIME),
/*	X(SQL_C_TYPE_TIMESTAMP), */
	X(SQL_C_NUMERIC),
	X(SQL_C_GUID),
	X(SQL_C_INTERVAL_YEAR),
	X(SQL_C_INTERVAL_MONTH),
	X(SQL_C_INTERVAL_DAY),
	X(SQL_C_INTERVAL_HOUR),
	X(SQL_C_INTERVAL_MINUTE),
	X(SQL_C_INTERVAL_SECOND),
	X(SQL_C_INTERVAL_YEAR_TO_MONTH),
	X(SQL_C_INTERVAL_DAY_TO_HOUR),
	X(SQL_C_INTERVAL_DAY_TO_MINUTE),
	X(SQL_C_INTERVAL_DAY_TO_SECOND),
	X(SQL_C_INTERVAL_HOUR_TO_MINUTE),
	X(SQL_C_INTERVAL_HOUR_TO_SECOND),
	X(SQL_C_INTERVAL_MINUTE_TO_SECOND),
	{ 0, NULL }
};

static HSTMT hstmt = SQL_NULL_HSTMT;

void
printwchar(SQLWCHAR *wstr)
{
	int			i = 0;
	/*
	 * a backstop to make sure we terminate if the string isn't null-terminated
	 * properly
	 */
	int			MAXLEN = 50;

	while(*wstr && i < MAXLEN)
	{
		if ((*wstr & 0xFFFFFF00) == 0 && isprint(*wstr))
			printf("%c", *wstr);
		else
			printf("\\%4X", *wstr);
		wstr++;
		i++;
	}
}


void
printhex(unsigned char *b, SQLLEN len)
{
	SQLLEN i;

	printf("hex: ");
	for (i = 0; i < len; i++)
		printf("%02X", b[i]);
}

void
printdouble(double d)
{
	/*
	 * printf() can print NaNs and infinite values too, but the output is
	 * platform dependent.
	 */
	if (isnan(d))
		printf("nan");
	else if (d < 0 && isinf(d))
		printf("-inf");
	else if (isinf(d))
		printf("inf");
	else
		printf("%f", d);
}

void
print_sql_type(int sql_c_type, void *buf, SQLLEN strlen_or_ind, int use_time)
{
	switch(sql_c_type)
	{
		case SQL_C_CHAR:
			printf("%s", (char *) buf);
			break;
		case SQL_C_WCHAR:
			printwchar((SQLWCHAR *) buf);
			break;
		case SQL_C_SSHORT:
			printf("%hd", *((short *) buf));
			break;
		case SQL_C_USHORT:
			printf("%hu", *((unsigned short *) buf));
			break;
		case SQL_C_SLONG:
			/* always 32-bits, regardless of native 'long' type */
			printf("%d", (int) *((SQLINTEGER *) buf));
			break;
		case SQL_C_ULONG:
			printf("%u", (unsigned int) *((SQLUINTEGER *) buf));
			break;
		case SQL_C_FLOAT:
			printdouble(*((SQLREAL *) buf));
			break;
		case SQL_C_DOUBLE:
			printdouble(*((SQLDOUBLE *) buf));
			break;
		case SQL_C_BIT:
			printf("%u", *((unsigned char *) buf));
			break;
		case SQL_C_STINYINT:
			printf("%d", *((signed char *) buf));
			break;
		case SQL_C_UTINYINT:
			printf("%u", *((unsigned char *) buf));
			break;
		case SQL_C_SBIGINT:
			/* XXX: the %ld format string won't handle the full 64-bit range on
			 * all platforms. */
			printf("%ld", (long) *((SQLBIGINT *) buf));
			break;
		case SQL_C_UBIGINT:
			/* XXX: the %lu format string won't handle the full 64-bit range on
			 * all platforms. */
			printf("%lu", (unsigned long) *((SQLUBIGINT *) buf));
			break;
		case SQL_C_BINARY:
			printhex((unsigned char *) buf, strlen_or_ind);
			break;
		/* SQL_C_BOOKMARK has same value as SQL_C_UBIGINT */
		/* SQL_C_VARBOOKMARK has same value as SQL_C_BINARY */
		case SQL_C_TYPE_DATE:
			{
				DATE_STRUCT *ds = (DATE_STRUCT *) buf;
				if (use_time != 0)
				{
					time_t  t = 0;
					struct tm  *tim;

					t = time(NULL);
					tim = localtime(&t);
					printf("y: %d m: %u d: %u",
						   ds->year - (tim->tm_year + 1900),
						   ds->month - (tim->tm_mon + 1),
						   ds->day - tim->tm_mday);
				}
				else
					printf("y: %d m: %u d: %u", ds->year, ds->month, ds->day);
			}
			break;
		case SQL_C_TYPE_TIME:
			{
				TIME_STRUCT *ts = (TIME_STRUCT *) buf;

				if (use_time != 0)
				{
					time_t  t = 0;
					struct tm  *tim;

					t = time(NULL);
					tim = localtime(&t);
				}
				else
					printf("h: %d m: %u s: %u", ts->hour, ts->minute, ts->second);
			}
			break;
		case SQL_C_TYPE_TIMESTAMP:
			{
				TIMESTAMP_STRUCT *tss = (TIMESTAMP_STRUCT *) buf;

				if (use_time)
				{
					time_t  t = 0;
					struct tm  *tim;

					t = time(NULL);
					tim = localtime(&t);
					printf("y: %d m: %u d: %u h: %d m: %u s: %u f: %u",
						   tss->year - (tim->tm_year + 1900),
						   tss->month - (tim->tm_mon + 1),
						   tss->day - tim->tm_mday,
						   tss->hour, tss->minute, tss->second,
						   (unsigned int) tss->fraction);
				}
				else
					printf("y: %d m: %u d: %u h: %d m: %u s: %u f: %u",
						   tss->year, tss->month, tss->day,
						   tss->hour, tss->minute, tss->second,
						   (unsigned int) tss->fraction);
			}
			break;
		case SQL_C_NUMERIC:
			{
				SQL_NUMERIC_STRUCT *ns = (SQL_NUMERIC_STRUCT *) buf;
				int i;
				printf("precision: %u scale: %d sign: %d val: ",
					   ns->precision, ns->scale, ns->scale);
				for (i = 0; i < SQL_MAX_NUMERIC_LEN; i++)
					printf("%02x", ns->val[i]);
			}
			break;
		case SQL_C_GUID:
			{
				SQLGUID *g = (SQLGUID *) buf;
				printf("d1: %04X d2: %04X d3: %04X d4: %02X%02X%02X%02X%02X%02X%02X%02X",
					   (unsigned int) g->Data1, g->Data2, g->Data3,
					   g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3],
					   g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
			}
			break;
		case SQL_C_INTERVAL_YEAR:
		case SQL_C_INTERVAL_MONTH:
		case SQL_C_INTERVAL_DAY:
		case SQL_C_INTERVAL_HOUR:
		case SQL_C_INTERVAL_MINUTE:
		case SQL_C_INTERVAL_SECOND:
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
		case SQL_C_INTERVAL_DAY_TO_HOUR:
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
		case SQL_C_INTERVAL_DAY_TO_SECOND:
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
			{
				SQL_INTERVAL_STRUCT *s = (SQL_INTERVAL_STRUCT *) buf;

				printf("interval sign: %u ", s->interval_sign);
				switch(s->interval_type)
				{
					case SQL_IS_YEAR:
						printf("year: %u", (unsigned int) s->intval.year_month.year);
						break;
					case SQL_IS_MONTH:
						printf("year: %u", (unsigned int) s->intval.year_month.month);
						break;
					case SQL_IS_DAY:
						printf("day: %u", (unsigned int) s->intval.day_second.day);
						break;
					case SQL_IS_HOUR:
						printf("hour: %u", (unsigned int) s->intval.day_second.hour);
						break;
					case SQL_IS_MINUTE:
						printf("minute: %u", (unsigned int) s->intval.day_second.minute);
						break;
					case SQL_IS_SECOND:
						printf("second: %u", (unsigned int) s->intval.day_second.second);
						break;
					case SQL_IS_YEAR_TO_MONTH:
						printf("year %u month: %u",
							   (unsigned int) s->intval.year_month.year,
							   (unsigned int) s->intval.year_month.month);
						break;
					case SQL_IS_DAY_TO_HOUR:
						printf("day: %u hour: %u",
							   (unsigned int) s->intval.day_second.day,
							   (unsigned int) s->intval.day_second.hour);
						break;
					case SQL_IS_DAY_TO_MINUTE:
						printf("day: %u hour: %u minute: %u",
							   (unsigned int) s->intval.day_second.day,
							   (unsigned int) s->intval.day_second.hour,
							   (unsigned int) s->intval.day_second.minute);
						break;
					case SQL_IS_DAY_TO_SECOND:
						printf("day: %u hour: %u minute: %u second: %u fraction: %u",
							   (unsigned int) s->intval.day_second.day,
							   (unsigned int) s->intval.day_second.hour,
							   (unsigned int) s->intval.day_second.minute,
							   (unsigned int) s->intval.day_second.second,
							   (unsigned int) s->intval.day_second.fraction);
						break;
					case SQL_IS_HOUR_TO_MINUTE:
						printf("hour: %u minute: %u",
							   (unsigned int) s->intval.day_second.hour,
							   (unsigned int) s->intval.day_second.minute);
						break;
					case SQL_IS_HOUR_TO_SECOND:
						printf("hour: %u minute: %u second: %u fraction: %u",
							   (unsigned int) s->intval.day_second.hour,
							   (unsigned int) s->intval.day_second.minute,
							   (unsigned int) s->intval.day_second.second,
							   (unsigned int) s->intval.day_second.fraction);
						break;
					case SQL_IS_MINUTE_TO_SECOND:
						printf("minute: %u second: %u fraction: %u",
							   (unsigned int) s->intval.day_second.minute,
							   (unsigned int) s->intval.day_second.second,
							   (unsigned int) s->intval.day_second.fraction);
						break;
					default:
						printf("unknown interval type: %u", s->interval_type);
						break;
				}
			}
			break;
		default:
			printf("unknown SQL C type: %u", sql_c_type);
			break;
	}
}


/*
 * Get the size of fixed-length data types, or -1 for var-length types
 */
int
get_sql_type_size(int sql_c_type)
{
	switch(sql_c_type)
	{
		case SQL_C_CHAR:
			return -1;
		case SQL_C_WCHAR:
			return -1;
		case SQL_C_SSHORT:
			return sizeof(short);
		case SQL_C_USHORT:
			return sizeof(unsigned short);
		case SQL_C_SLONG:
			/* always 32-bits, regardless of native 'long' type */
			return sizeof(SQLINTEGER);
		case SQL_C_ULONG:
			return sizeof(SQLUINTEGER);
		case SQL_C_FLOAT:
			return sizeof(SQLREAL);
		case SQL_C_DOUBLE:
			return sizeof(SQLDOUBLE);
		case SQL_C_BIT:
			return sizeof(unsigned char);
		case SQL_C_STINYINT:
			return sizeof(signed char);
		case SQL_C_UTINYINT:
			return sizeof(unsigned char);
		case SQL_C_SBIGINT:
			return sizeof(SQLBIGINT);
		case SQL_C_UBIGINT:
			return sizeof(SQLUBIGINT);
		case SQL_C_BINARY:
			return -1;
		/* SQL_C_BOOKMARK has same value as SQL_C_UBIGINT */
		/* SQL_C_VARBOOKMARK has same value as SQL_C_BINARY */
		case SQL_C_TYPE_DATE:
			return sizeof(DATE_STRUCT);
		case SQL_C_TYPE_TIME:
			return sizeof(TIME_STRUCT);
		case SQL_C_TYPE_TIMESTAMP:
			return sizeof(TIMESTAMP_STRUCT);
		case SQL_C_NUMERIC:
			return sizeof(SQL_NUMERIC_STRUCT);
		case SQL_C_GUID:
			return sizeof(SQLGUID);
		case SQL_C_INTERVAL_YEAR:
		case SQL_C_INTERVAL_MONTH:
		case SQL_C_INTERVAL_DAY:
		case SQL_C_INTERVAL_HOUR:
		case SQL_C_INTERVAL_MINUTE:
		case SQL_C_INTERVAL_SECOND:
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
		case SQL_C_INTERVAL_DAY_TO_HOUR:
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
		case SQL_C_INTERVAL_DAY_TO_SECOND:
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
			return sizeof(SQL_INTERVAL_STRUCT);
		default:
			printf("unknown SQL C type: %u", sql_c_type);
			return -1;
	}
}

static char *resultbuf = NULL;

void
test_conversion(const char *pgtype, const char *pgvalue, int sqltype, const char *sqltypestr, int buflen, int use_time)
{
	char		sql[500];
	SQLRETURN	rc;
	SQLLEN		len_or_ind;
	int			fixed_len;

	printf("'%s' (%s) as %s: ", pgvalue, pgtype, sqltypestr);

	if (resultbuf == NULL)
		resultbuf = malloc(500);

	memset(resultbuf, 0xFF, 500);

	fixed_len = get_sql_type_size(sqltype);
	if (fixed_len != -1)
		buflen = fixed_len;
	if (buflen == -1)
	{
		printf("no buffer length given!");
		exit(1);
	}

	/*
	 * Use dollar-quotes to make the test case insensitive to
	 * standards_conforming_strings. Some of the test values we use contain
	 * backslashes.
	 */
	snprintf(sql, sizeof(sql),
			 "SELECT $$%s$$::%s AS %s_col /* convert to %s */",
			 pgvalue, pgtype, pgtype, sqltypestr);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetData(hstmt, 1, sqltype, resultbuf, buflen, &len_or_ind);
	if (SQL_SUCCEEDED(rc))
	{
		print_sql_type(sqltype, resultbuf, len_or_ind, use_time);

		if (rc == SQL_SUCCESS_WITH_INFO)
		{
			char sqlstate[10];

			rc = SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, NULL, NULL, 0, NULL);
			if (!SQL_SUCCEEDED(rc) && SQL_NO_DATA != rc)
				print_diag(" SQLGetDiagRec failed", SQL_HANDLE_STMT, hstmt);
			else
			{
				if (memcmp(sqlstate, "01004", 5) == 0)
					printf(" (truncated)");
				else if (SQL_NO_DATA == rc && IsAnsi()) /* maybe */
					printf(" (truncated)");
				else
					print_diag("SQLGetData success with info", SQL_HANDLE_STMT, hstmt);
			}
		}
		/* just in order to fix ansi driver test on Windows */
		else if (1 <=  buflen &&
			 SQL_C_WCHAR == sqltype &&
			 0 == len_or_ind &&
			 0 == strcmp(pgtype, "text") &&
			 IsAnsi())
			printf(" (truncated)");

		printf("\n");
		/* Check that the driver didn't write past the buffer */
		if ((unsigned char) resultbuf[buflen] != 0xFF)
			printf("Driver wrote byte %02X past result buffer of size %d!\n", (unsigned char) resultbuf[buflen], buflen);
	}
	else
	{
		/* some of the conversions throw an error; that's OK */
		print_diag("SQLGetData failed", SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	fflush(stdout);
}

/* A helper function to execute one command */
void
exec_cmd(char *sql)
{
	SQLRETURN	rc;

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	printf("Executed: %s\n", sql);
}

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	int			sqltype_i;
	int			pgtype_i;

	test_connect();

	rc = SQLAllocHandle(SQL_HANDLE_STMT, conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("failed to allocate stmt handle", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * The interval stuff requires intervalstyle=postgres at the momemnt.
	 * Someone should fix the driver to understand other formats,
	 * postgres_verbose in particular...
	 */
	exec_cmd("SET intervalstyle=postgres");
	SQLExecDirect(hstmt, (SQLCHAR *) "SET timezone=-08", SQL_NTS);

	/*
	 * Use octal escape bytea format in the tests. We will test the conversion
	 * from the hex format separately later.
	 */
	exec_cmd("SET bytea_output=escape");
	exec_cmd("SET lc_monetary='C'");

	/* Test all combinations of PostgreSQL data types and SQL datatypes */
	for (pgtype_i = 0; pgtypes[pgtype_i * 2] != NULL; pgtype_i++)
	{
		const char *value = pgtypes[pgtype_i * 2];
		const char *pgtype = pgtypes[pgtype_i * 2 +1];

		for (sqltype_i = 0; sqltypes[sqltype_i].str != NULL; sqltype_i++)
		{
			int sqltype = sqltypes[sqltype_i].sqltype;
			const char *sqltypestr = sqltypes[sqltype_i].str;

			test_conversion(pgtype, value, sqltype, sqltypestr, 100, 0);
		}
	}

	/*
	 * Test a few things separately that were not part of the exhaustive test
	 * above.
	 */

	/* Conversions from bytea, in the hex format */

	/*
	 * Use octal escape bytea format in the tests. We will test the conversion
	 * from the hex format separately later.
	 */
	exec_cmd("SET bytea_output=hex");
	test_conversion("bytea", "\\x464F4F", SQL_C_CHAR, "SQL_C_CHAR", 100, 0);
	test_conversion("bytea", "\\x464F4F", SQL_C_WCHAR, "SQL_C_WCHAR", 100, 0);

	/* Conversion to GUID throws error if the string is not of correct form */
	test_conversion("text", "543c5e21-435a-440b-943c-64af1ad571f1", SQL_C_GUID, "SQL_C_GUID", -1, 0);

	/* Date/timestamp tests of non-date input depends on current date */
	test_conversion("date", "2011-02-13", SQL_C_TYPE_DATE, "SQL_C_DATE", -1, 0);
	test_conversion("date", "2011-02-13", SQL_C_TYPE_TIMESTAMP, "SQL_C_TIMESTAMP", -1, 0);
	test_conversion("timestamp", "2011-02-15 15:49:18", SQL_C_TYPE_DATE, "SQL_C_DATE", -1, 0);
	test_conversion("timestamp", "2011-02-15 15:49:18", SQL_C_TYPE_TIMESTAMP, "SQL_C_TIMESTAMP", -1, 0);
	test_conversion("timestamptz", "2011-02-16 17:49:18+03", SQL_C_TYPE_DATE, "SQL_C_DATE", -1, 0);
	test_conversion("timestamptz", "2011-02-16 17:49:18+03", SQL_C_TYPE_TIMESTAMP, "SQL_C_TIMESTAMP", -1, 0);

	/* cast of empty text values using localtime() */
	test_conversion("text", "", SQL_C_TYPE_DATE, "SQL_C_TYPE_DATE", -1, 1);
	test_conversion("text", "", SQL_C_TYPE_TIME, "SQL_C_TYPE_TIME", -1, 0);
	test_conversion("text", "", SQL_C_TYPE_TIMESTAMP, "SQL_C_TYPE_TIMESTAMP", -1, 1);

	/*
	 * Test for truncations.
	 */
	test_conversion("text", "foobar", SQL_C_CHAR, "SQL_C_CHAR", 5, 0);
	test_conversion("text", "foobar", SQL_C_CHAR, "SQL_C_CHAR", 6, 0);
	test_conversion("text", "foobar", SQL_C_CHAR, "SQL_C_CHAR", 7, 0);
	test_conversion("text", "foobar", SQL_C_WCHAR, "SQL_C_WCHAR", 10, 0);
	test_conversion("text", "foobar", SQL_C_WCHAR, "SQL_C_WCHAR", 11, 0);
	test_conversion("text", "foobar", SQL_C_WCHAR, "SQL_C_WCHAR", 12, 0);
	test_conversion("text", "foobar", SQL_C_WCHAR, "SQL_C_WCHAR", 13, 0);
	test_conversion("text", "foobar", SQL_C_WCHAR, "SQL_C_WCHAR", 14, 0);

	test_conversion("text", "", SQL_C_CHAR, "SQL_C_CHAR", 1, 0);
	test_conversion("text", "", SQL_C_WCHAR, "SQL_C_WCHAR", 1, 0);

	test_conversion("timestamp", "2011-02-15 15:49:18", SQL_C_CHAR, "SQL_C_CHAR", 19, 0);

	/*
	 * Test for a specific bug, where the driver used to overrun the output
	 * buffer because it assumed that a timestamp value is always max 20 bytes
	 * long (not true for BC values, or with years > 10000)
	 */
	test_conversion("timestamp", "2011-02-15 15:49:18 BC", SQL_C_CHAR, "SQL_C_CHAR", 20, 0);

	/* Test special float values */
	test_conversion("float4", "NaN", SQL_C_FLOAT, "SQL_C_FLOAT", 20, 0);
	test_conversion("float4", "Infinity", SQL_C_FLOAT, "SQL_C_FLOAT", 20, 0);
	test_conversion("float4", "-Infinity", SQL_C_FLOAT, "SQL_C_FLOAT", 20, 0);
	test_conversion("float8", "NaN", SQL_C_FLOAT, "SQL_C_FLOAT", 20, 0);
	test_conversion("float8", "Infinity", SQL_C_FLOAT, "SQL_C_FLOAT", 20, 0);
	test_conversion("float8", "-Infinity", SQL_C_FLOAT, "SQL_C_FLOAT", 20, 0);
	test_conversion("float4", "NaN", SQL_C_DOUBLE, "SQL_C_DOUBLE", 20, 0);
	test_conversion("float4", "Infinity", SQL_C_DOUBLE, "SQL_C_DOUBLE", 20, 0);
	test_conversion("float4", "-Infinity", SQL_C_DOUBLE, "SQL_C_DOUBLE", 20, 0);
	test_conversion("float8", "NaN", SQL_C_DOUBLE, "SQL_C_DOUBLE", 20, 0);
	test_conversion("float8", "Infinity", SQL_C_DOUBLE, "SQL_C_DOUBLE", 20, 0);
	test_conversion("float8", "-Infinity", SQL_C_DOUBLE, "SQL_C_DOUBLE", 20, 0);

	/* Clean up */
	test_disconnect();

	return 0;
}
