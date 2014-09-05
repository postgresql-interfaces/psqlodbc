/*
 * Test conversion of values received from server to SQL datatypes.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "common.h"

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
	"3234567901",	"oid",
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
	while(*wstr)
	{
		if (isprint(*wstr))
			printf("%c", *wstr);
		else
			printf("\\%4X", *wstr);
		wstr++;
	}
}


void
printhex(unsigned char *b, int len)
{
	int i;

	printf("hex: ");
	for (i = 0; i < len; i++)
		printf("%02X", b[i]);
}

void
print_sql_type(int sql_c_type, void *buf, SQLLEN strlen_or_ind)
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
			printf("%d", *((SQLINTEGER *) buf));
			break;
		case SQL_C_ULONG:
			printf("%u", *((SQLUINTEGER *) buf));
			break;
		case SQL_C_FLOAT:
			printf("%f", *((SQLREAL *) buf));
			break;
		case SQL_C_DOUBLE:
			printf("%f", *((SQLDOUBLE *) buf));
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
			printf("%ld", *((SQLBIGINT *) buf));
			break;
		case SQL_C_UBIGINT:
			/* XXX: the %lu format string won't handle the full 64-bit range on
			 * all platforms. */
			printf("%lu", *((SQLUBIGINT *) buf));
			break;
		case SQL_C_BINARY:
			printhex((unsigned char *) buf, strlen_or_ind);
			break;
		/* SQL_C_BOOKMARK has same value as SQL_C_UBIGINT */
		/* SQL_C_VARBOOKMARK has same value as SQL_C_BINARY */
		case SQL_C_TYPE_DATE:
			{
				DATE_STRUCT *ds = (DATE_STRUCT *) buf;
				printf("y: %d m: %u d: %u", ds->year, ds->month, ds->day);
			}
			break;
		case SQL_C_TYPE_TIME:
			{
				TIME_STRUCT *ts = (TIME_STRUCT *) buf;
				printf("h: %d m: %u s: %u", ts->hour, ts->minute, ts->second);
			}
			break;
		case SQL_C_TYPE_TIMESTAMP:
			{
				TIMESTAMP_STRUCT *tss = (TIMESTAMP_STRUCT *) buf;
				printf("y: %d m: %u d: %u h: %d m: %u s: %u f: %u",
					   tss->year, tss->month, tss->day,
					   tss->hour, tss->minute, tss->second, tss->fraction);
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
					   g->Data1, g->Data2, g->Data3,
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
						printf("year: %u", s->intval.year_month.year);
						break;
					case SQL_IS_MONTH:
						printf("year: %u", s->intval.year_month.month);
						break;
					case SQL_IS_DAY:
						printf("day: %u", s->intval.day_second.day);
						break;
					case SQL_IS_HOUR:
						printf("hour: %u", s->intval.day_second.hour);
						break;
					case SQL_IS_MINUTE:
						printf("minute: %u", s->intval.day_second.minute);
						break;
					case SQL_IS_SECOND:
						printf("second: %u", s->intval.day_second.second);
						break;
					case SQL_IS_YEAR_TO_MONTH:
						printf("year %u month: %u", s->intval.year_month.year,
							   s->intval.year_month.month);
						break;
					case SQL_IS_DAY_TO_HOUR:
						printf("day: %u hour: %u", s->intval.day_second.day,
							   s->intval.day_second.hour);
						break;
					case SQL_IS_DAY_TO_MINUTE:
						printf("day: %u hour: %u minute: %u",
							   s->intval.day_second.day,
							   s->intval.day_second.hour,
							   s->intval.day_second.minute);
						break;
					case SQL_IS_DAY_TO_SECOND:
						printf("day: %u hour: %u minute: %u second: %u fraction: %u",
							   s->intval.day_second.day,
							   s->intval.day_second.hour,
							   s->intval.day_second.minute,
							   s->intval.day_second.second,
							   s->intval.day_second.fraction);
						break;
					case SQL_IS_HOUR_TO_MINUTE:
						printf("hour: %u minute: %u",
							   s->intval.day_second.hour,
							   s->intval.day_second.minute);
						break;
					case SQL_IS_HOUR_TO_SECOND:
						printf("hour: %u minute: %u second: %u fraction: %u",
							   s->intval.day_second.hour,
							   s->intval.day_second.minute,
							   s->intval.day_second.second,
							   s->intval.day_second.fraction);
						break;
					case SQL_IS_MINUTE_TO_SECOND:
						printf("minute: %u second: %u fraction: %u",
							   s->intval.day_second.minute,
							   s->intval.day_second.second,
							   s->intval.day_second.fraction);
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

static char *resultbuf = NULL;

void
test_conversion(const char *pgtype, const char *pgvalue, int sqltype, const char *sqltypestr)
{
	char		sql[500];
	SQLRETURN	rc;
	SQLLEN		len_or_ind;

	if (resultbuf == NULL)
		resultbuf = malloc(500);

	memset(resultbuf, 0xFF, 500);

	printf("'%s' (%s) as %s: ", pgvalue, pgtype, sqltypestr);

	snprintf(sql, sizeof(sql),
			 "SELECT '%s'::%s AS %s_col /* convert to %s */",
			 pgvalue, pgtype, pgtype, sqltypestr);

	rc = SQLExecDirect(hstmt, (SQLCHAR *) sql, SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	rc = SQLFetch(hstmt);
	CHECK_STMT_RESULT(rc, "SQLFetch failed", hstmt);

	rc = SQLGetData(hstmt, 1, sqltype, resultbuf, 100, &len_or_ind);
	if (SQL_SUCCEEDED(rc))
	{
		print_sql_type(sqltype, resultbuf, len_or_ind);
		printf("\n");
	}
	else
	{
		/* some of the conversions throw an error; that's OK */
		print_diag("SQLGetData failed", SQL_HANDLE_STMT, hstmt);
	}

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
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
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SET intervalstyle=postgres", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);
	print_result(hstmt);

	rc = SQLFreeStmt(hstmt, SQL_CLOSE);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);

	/* Test all combinations of PostgreSQL data types and SQL datatypes */
	for (pgtype_i = 0; pgtypes[pgtype_i * 2] != NULL; pgtype_i++)
	{
		const char *value = pgtypes[pgtype_i * 2];
		const char *pgtype = pgtypes[pgtype_i * 2 +1];

		for (sqltype_i = 0; sqltypes[sqltype_i].str != NULL; sqltype_i++)
		{
			int sqltype = sqltypes[sqltype_i].sqltype;
			const char *sqltypestr = sqltypes[sqltype_i].str;

			test_conversion(pgtype, value, sqltype, sqltypestr);
		}
	}

	/*
	 * Test a few things separately that were not part of the exhaustive test
	 * above.
	 */

	/* Conversion to GUID throws error if the string is not of correct form */
	test_conversion("text", "543c5e21-435a-440b-943c-64af1ad571f1", SQL_C_GUID, "SQL_C_GUID");

	/* Date/timestamp tests of non-date input depends on current date */
	test_conversion("date", "2011-02-13", SQL_C_DATE, "SQL_C_DATE");
	test_conversion("date", "2011-02-13", SQL_C_TIMESTAMP, "SQL_C_TIMESTAMP");
	test_conversion("timestamp", "2011-02-15 15:49:18", SQL_C_DATE, "SQL_C_DATE");
	test_conversion("timestamp", "2011-02-15 15:49:18", SQL_C_TIMESTAMP, "SQL_C_TIMESTAMP");
	test_conversion("timestamptz", "2011-02-16 17:49:18+03", SQL_C_DATE, "SQL_C_DATE");
	test_conversion("timestamptz", "2011-02-16 17:49:18+03", SQL_C_TIMESTAMP, "SQL_C_TIMESTAMP");

	/* Clean up */
	test_disconnect();

	return 0;
}
