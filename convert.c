/*-------
 * Module:				 convert.c
 *
 * Description:    This module contains routines related to
 *				   converting parameters and columns into requested data types.
 *				   Parameters are converted from their SQL_C data types into
 *				   the appropriate postgres type.  Columns are converted from
 *				   their postgres type (SQL type) into the appropriate SQL_C
 *				   data type.
 *
 * Classes:		   n/a
 *
 * API functions:  none
 *
 * Comments:	   See "readme.txt" for copyright and license information.
 *-------
 */
/* Multibyte support  Eiji Tokuya	2001-03-15	*/

#include "convert.h"
#include "unicode_support.h"
#include "misc.h"
#ifdef	WIN32
#include <float.h>
#define	HAVE_LOCALE_H
#endif /* WIN32 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "multibyte.h"

#include <time.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include "statement.h"
#include "qresult.h"
#include "bind.h"
#include "pgtypes.h"
#include "lobj.h"
#include "connection.h"
#include "catfunc.h"
#include "pgapifunc.h"

CSTR	NAN_STRING = "NaN";
CSTR	INFINITY_STRING = "Infinity";
CSTR	MINFINITY_STRING = "-Infinity";

#if	defined(WIN32) || defined(__CYGWIN__)
#define TIMEZONE_GLOBAL _timezone
#define TZNAME_GLOBAL _tzname
#define DAYLIGHT_GLOBAL _daylight
#elif	defined(HAVE_INT_TIMEZONE)
#define TIMEZONE_GLOBAL timezone
#define TZNAME_GLOBAL tzname
#define DAYLIGHT_GLOBAL daylight
#endif

/*
 *	How to map ODBC scalar functions {fn func(args)} to Postgres.
 *	This is just a simple substitution.  List augmented from:
 *	http://www.merant.com/datadirect/download/docs/odbc16/Odbcref/rappc.htm
 *	- thomas 2000-04-03
 */
static const struct
{
	/*
	 * There's a horrible hack in odbc_name field: if it begins with a %,
	 * the digit after the % indicates the number of arguments. Otherwise,
	 * the entry matches any number of args.
	 */
	char *odbc_name;
	char *pgsql_name;
} mapFuncs[] = {
/*	{ "ASCII",		 "ascii"	  }, built_in */
	{"CHAR", "chr($*)" },
	{"CONCAT", "concat($1::text, $2::text)" },
/*	{ "DIFFERENCE", "difference" }, how to ? */
	{"INSERT", "substring($1 from 1 for $2 - 1) || $4 || substring($1 from $2 + $3)" },
	{"LCASE", "lower($*)" },
/*	{"LEFT",		 "left"		  }, built_in */
	{"%2LOCATE", "strpos($2,  $1)" },	/* 2 parameters */
	{"%3LOCATE", "strpos(substring($2 from $3), $1) + $3 - 1" },	/* 3 parameters */
	{"LENGTH", "char_length($*)"},
/*	{ "LTRIM",		 "ltrim"	  }, built_in */
/*	{"RIGHT",		 "right"	  }, built_in */
	{"SPACE", "repeat(' ', $1)" },
/*	{ "REPEAT",		 "repeat"	  }, built_in */
/*	{ "REPLACE", "replace" }, ??? */
/*	{ "RTRIM",		 "rtrim"	  }, built_in */
/*	{ "SOUNDEX", "soundex" }, how to ? */
	{"SUBSTRING", "substr($*)" },
	{"UCASE", "upper($*)" },

/*	{ "ABS",		 "abs"		  }, built_in */
/*	{ "ACOS",		 "acos"		  }, built_in */
/*	{ "ASIN",		 "asin"		  }, built_in */
/*	{ "ATAN",		 "atan"		  }, built_in */
/*	{ "ATAN2",		 "atan2"	  }, built_in */
	{"CEILING", "ceil($*)" },
/*	{ "COS",		 "cos" 		  }, built_in */
/*	{ "COT",		 "cot" 		  }, built_in */
/*	{ "DEGREES",		 "degrees" 	  }, built_in */
/*	{ "EXP",		 "exp" 		  }, built_in */
/*	{ "FLOOR",		 "floor" 	  }, built_in */
	{"LOG", "ln($*)" },
	{"LOG10", "log($*)" },
/*	{ "MOD",		 "mod" 		  }, built_in */
/*	{ "PI",			 "pi" 		  }, built_in */
/*	{"POWER", 		 "power"	  }, built_in */
/*	{ "RADIANS",		 "radians"	  }, built_in */
	{"%0RAND", "random()" },	/* 0 parameters */
	{"%1RAND", "(setseed($1) * .0 + random())" },	/* 1 parameters */
/*	{ "ROUND",		 "round"	  }, built_in */
/*	{ "SIGN",		 "sign"		  }, built_in */
/*	{ "SIN",		 "sin"		  }, built_in */
/*	{ "SQRT",		 "sqrt"		  }, built_in */
/*	{ "TAN",		 "tan"		  }, built_in */
	{"TRUNCATE", "trunc($*)" },

	{"CURRENT_DATE", "current_date" },
	{"CURRENT_TIME", "current_time" },
	{"CURRENT_TIMESTAMP", "current_timestamp" },
	{"LOCALTIME", "localtime" },
	{"LOCALTIMESTAMP", "localtimestamp" },
	{"CURRENT_USER", "cast(current_user as text)" },
	{"SESSION_USER", "cast(session_user as text)" },
	{"CURDATE",	 "current_date" },
	{"CURTIME",	 "current_time" },
	{"DAYNAME",	 "to_char($1, 'Day')" },
	{"DAYOFMONTH",  "cast(extract(day from $1) as integer)" },
	{"DAYOFWEEK",	 "(cast(extract(dow from $1) as integer) + 1)" },
	{"DAYOFYEAR",	 "cast(extract(doy from $1) as integer)" },
	{"HOUR",	 "cast(extract(hour from $1) as integer)" },
	{"MINUTE",	"cast(extract(minute from $1) as integer)" },
	{"MONTH",	"cast(extract(month from $1) as integer)" },
	{"MONTHNAME",	 " to_char($1, 'Month')" },
/*	{ "NOW",		 "now"		  }, built_in */
	{"QUARTER",	 "cast(extract(quarter from $1) as integer)" },
	{"SECOND",	"cast(extract(second from $1) as integer)" },
	{"WEEK",	"cast(extract(week from $1) as integer)" },
	{"YEAR",	"cast(extract(year from $1) as integer)" },
	// TIMESTAMPADD()
	{"TIMESTAMPADD(SQL_TSI_YEAR", "($3+make_interval(years := $2))" },
	{"TIMESTAMPADD(SQL_TSI_MONTH", "($3+make_interval(months := $2))" },
	{"TIMESTAMPADD(SQL_TSI_WEEK", "($3+make_interval(weeks := $2))" },
	{"TIMESTAMPADD(SQL_TSI_DAY", "($3+make_interval(days := $2))" },
	{"TIMESTAMPADD(SQL_TSI_HOUR", "($3+make_interval(hours := $2))" },
	{"TIMESTAMPADD(SQL_TSI_MINUTE", "($3+make_interval(mins := $2))" },
	{"TIMESTAMPADD(SQL_TSI_SECOND", "($3+make_interval(secs := $2))" },
	{"TIMESTAMPADD(SQL_TSI_FRAC_SECOND", "($3+make_interval(secs := $2::float / 1000000))" },
	// TIMESTAMPDIFF()
/* doesn't work properly?
{"TIMESTAMPDIFF(SQL_TSI_YEAR", "cast(extract(year from ($3 - $2)) as integer)" },
{"TIMESTAMPDIFF(SQL_TSI_MONTH", "cast(extract(month from ($3 - $2)) as integer)" },
{"TIMESTAMPDIFF(SQL_TSI_QUARTER", "cast(extract(quarter from ($3 - $2)) as integer)" },
{"TIMESTAMPDIFF(SQL_TSI_WEEK", "cast(extract(week from ($3 - $2)) as integer)" },
*/
	/*
	{"TIMESTAMPDIFF(SQL_TSI_DAY", "cast(extract(day from ($3 - $2)) as integer)" },
	{"TIMESTAMPDIFF(SQL_TSI_HOUR", "cast(extract(hour from ($3 - $2)) as integer)" },
	{"TIMESTAMPDIFF(SQL_TSI_HOUR", "(extract(epoch from $3) - extract(epoch from $2)) / 3600" },
	{"TIMESTAMPDIFF(SQL_TSI_MINUTE", "cast(extract(minute from ($3 - $2)) as integer)" },
	{"TIMESTAMPDIFF(SQL_TSI_SECOND", "cast(extract(second from ($3 - $2)) as integer)" },
	*/
	{"TIMESTAMPDIFF(SQL_TSI_DAY", "cast((extract(epoch from $3) - extract(epoch from $2)) / (24*60*60) as int)" },
	{"TIMESTAMPDIFF(SQL_TSI_HOUR", "cast((extract(epoch from $3) - extract(epoch from $2)) / 3600 as int)" },
	{"TIMESTAMPDIFF(SQL_TSI_MINUTE", "cast((extract(epoch from $3) - extract(epoch from $2)) / 60 as int)" },
	{"TIMESTAMPDIFF(SQL_TSI_SECOND", "cast((extract(epoch from $3) - extract(epoch from $2)) as int)" },
	{"TIMESTAMPDIFF(SQL_TSI_FRAC_SECOND", "mod(cast(extract(second from ($3 - $2)) as numeric), 1.0) * 1000000" },

/*	{ "EXTRACT",		 "extract"		  }, built_in */

/*	{ "DATABASE",	 "database"   }, */
	{"IFNULL", "coalesce($*)" },
	{"USER", "cast(current_user as text)" },

	{0, 0}
};

typedef struct
{
	int		infinity;
	int			m;
	int			d;
	int			y;
	int			hh;
	int			mm;
	int			ss;
	int			fr;
} SIMPLE_TIME;

static const char *mapFunction(const char *func, int param_count, const char * keyword);
static BOOL convert_money(const char *s, char *sout, size_t soutmax);
static char parse_datetime(const char *buf, SIMPLE_TIME *st);
size_t convert_linefeeds(const char *s, char *dst, size_t max, BOOL convlf, BOOL *changed);
static size_t convert_from_pgbinary(const char *value, char *rgbValue, SQLLEN cbValueMax);
static int convert_lo(StatementClass *stmt, const void *value, SQLSMALLINT fCType,
	 PTR rgbValue, SQLLEN cbValueMax, SQLLEN *pcbValue);
static int conv_from_octal(const char *s);
static SQLLEN pg_bin2hex(const char *src, char *dst, SQLLEN length);
#ifdef	UNICODE_SUPPORT
static SQLLEN pg_bin2whex(const char *src, SQLWCHAR *dst, SQLLEN length);
#endif /* UNICODE_SUPPORT */

/*---------
 *			A Guide for date/time/timestamp conversions
 *
 *			field_type		fCType				Output
 *			----------		------				----------
 *			PG_TYPE_DATE	SQL_C_DEFAULT		SQL_C_DATE
 *			PG_TYPE_DATE	SQL_C_DATE			SQL_C_DATE
 *			PG_TYPE_DATE	SQL_C_TIMESTAMP		SQL_C_TIMESTAMP		(time = 0 (midnight))
 *			PG_TYPE_TIME	SQL_C_DEFAULT		SQL_C_TIME
 *			PG_TYPE_TIME	SQL_C_TIME			SQL_C_TIME
 *			PG_TYPE_TIME	SQL_C_TIMESTAMP		SQL_C_TIMESTAMP		(date = current date)
 *			PG_TYPE_ABSTIME SQL_C_DEFAULT		SQL_C_TIMESTAMP
 *			PG_TYPE_ABSTIME SQL_C_DATE			SQL_C_DATE			(time is truncated)
 *			PG_TYPE_ABSTIME SQL_C_TIME			SQL_C_TIME			(date is truncated)
 *			PG_TYPE_ABSTIME SQL_C_TIMESTAMP		SQL_C_TIMESTAMP
 *---------
 */


/*
 *	Macros for unsigned long handling.
 */
#ifdef	WIN32
#define	ATOI32U(val)	strtoul(val, NULL, 10)
#elif	defined(HAVE_STRTOUL)
#define	ATOI32U(val)	strtoul(val, NULL, 10)
#else /* HAVE_STRTOUL */
#define	ATOI32U	atol
#endif /* WIN32 */

/*
 *	Macros for BIGINT handling.
 */
#ifdef	ODBCINT64
#ifdef	WIN32
#define	ATOI64(val)	_strtoi64(val, NULL, 10)
#define	ATOI64U(val)	_strtoui64(val, NULL, 10)
#elif	(SIZEOF_LONG == 8)
#define	ATOI64(val)	strtol(val, NULL, 10)
#define	ATOI64U(val)	strtoul(val, NULL, 10)
#else
#if	defined(HAVE_STRTOLL)
#define	ATOI64(val)	strtoll(val, NULL, 10)
#define	ATOI64U(val)	strtoull(val, NULL, 10)
#else
static ODBCINT64 ATOI64(const char *val)
{
	ODBCINT64 ll;
	sscanf(val, "%lld", &ll);
	return ll;
}
static unsigned ODBCINT64 ATOI64U(const char *val)
{
	unsigned ODBCINT64 ll;
	sscanf(val, "%llu", &ll);
	return ll;
}
#endif /* HAVE_STRTOLL */
#endif /* WIN32 */
#endif /* ODBCINT64 */

static void ResolveNumericParam(const SQL_NUMERIC_STRUCT *ns, char *chrform);
static void parse_to_numeric_struct(const char *wv, SQL_NUMERIC_STRUCT *ns, BOOL *overflow);

/*
 *	TIMESTAMP <-----> SIMPLE_TIME
 *		precision support since 7.2.
 *		time zone support is unavailable(the stuff is unreliable)
 */
static BOOL
timestamp2stime(const char *str, SIMPLE_TIME *st, BOOL *bZone, int *zone)
{
	char		rest[64], bc[16],
			   *ptr;
	int			scnt,
				i;
	int			y, m, d, hh, mm, ss;
#ifdef	TIMEZONE_GLOBAL
	long		timediff;
#endif
	BOOL		withZone = *bZone;

	*bZone = FALSE;
	*zone = 0;
	st->fr = 0;
	st->infinity = 0;
	rest[0] = '\0';
	bc[0] = '\0';
	if ((scnt = sscanf(str, "%4d-%2d-%2d %2d:%2d:%2d%31s %15s", &y, &m, &d, &hh, &mm, &ss, rest, bc)) < 6)
	{
		if (scnt == 3) /* date */
		{
			st->y  = y;
			st->m  = m;
			st->d  = d;
			st->hh = 0;
			st->mm = 0;
			st->ss = 0;
			return TRUE;
		}
		if ((scnt = sscanf(str, "%2d:%2d:%2d%31s %15s", &hh, &mm, &ss, rest, bc)) < 3)
			return FALSE;
		else
		{
			st->hh = hh;
			st->mm = mm;
			st->ss = ss;
			if (scnt == 3) /* time */
				return TRUE;
		}
	}
	else
	{
		st->y  = y;
		st->m  = m;
		st->d  = d;
		st->hh = hh;
		st->mm = mm;
		st->ss = ss;
		if (scnt == 6)
			return TRUE;
	}
	switch (rest[0])
	{
		case '+':
			*bZone = TRUE;
			*zone = atoi(&rest[1]);
			break;
		case '-':
			*bZone = TRUE;
			*zone = -atoi(&rest[1]);
			break;
		case '.':
			if ((ptr = strchr(rest, '+')) != NULL)
			{
				*bZone = TRUE;
				*zone = atoi(&ptr[1]);
				*ptr = '\0';
			}
			else if ((ptr = strchr(rest, '-')) != NULL)
			{
				*bZone = TRUE;
				*zone = -atoi(&ptr[1]);
				*ptr = '\0';
			}
			for (i = 1; i < 10; i++)
			{
				if (!isdigit((UCHAR) rest[i]))
					break;
			}
			for (; i < 10; i++)
				rest[i] = '0';
			rest[i] = '\0';
			st->fr = atoi(&rest[1]);
			break;
		case 'B':
			if (stricmp(rest, "BC") == 0)
				st->y *= -1;
			return TRUE;
		default:
			return TRUE;
	}
	if (stricmp(bc, "BC") == 0)
	{
		st->y *= -1;
	}
	if (!withZone || !*bZone || st->y < 1970)
		return TRUE;
#ifdef	TIMEZONE_GLOBAL
	if (!TZNAME_GLOBAL[0] || !TZNAME_GLOBAL[0][0])
	{
		*bZone = FALSE;
		return TRUE;
	}
	timediff = TIMEZONE_GLOBAL + (*zone) * 3600;
	if (!DAYLIGHT_GLOBAL && timediff == 0)		/* the same timezone */
		return TRUE;
	else
	{
		struct tm	tm,
				   *tm2;
		time_t		time0;

		*bZone = FALSE;
		tm.tm_year = st->y - 1900;
		tm.tm_mon = st->m - 1;
		tm.tm_mday = st->d;
		tm.tm_hour = st->hh;
		tm.tm_min = st->mm;
		tm.tm_sec = st->ss;
		tm.tm_isdst = -1;
		time0 = mktime(&tm);
		if (time0 < 0)
			return TRUE;
		if (tm.tm_isdst > 0)
			timediff -= 3600;
		if (timediff == 0)		/* the same time zone */
			return TRUE;
		time0 -= timediff;
#ifdef	HAVE_LOCALTIME_R
		if (time0 >= 0 && (tm2 = localtime_r(&time0, &tm)) != NULL)
#else
		if (time0 >= 0 && (tm2 = localtime(&time0)) != NULL)
#endif /* HAVE_LOCALTIME_R */
		{
			st->y = tm2->tm_year + 1900;
			st->m = tm2->tm_mon + 1;
			st->d = tm2->tm_mday;
			st->hh = tm2->tm_hour;
			st->mm = tm2->tm_min;
			st->ss = tm2->tm_sec;
			*bZone = TRUE;
		}
	}
#endif /* TIMEZONE_GLOBAL */
	return TRUE;
}

static int
stime2timestamp(const SIMPLE_TIME *st, char *str, size_t bufsize, BOOL bZone,
				int precision)
{
	char		precstr[16],
				zonestr[16];
	int			i;

	precstr[0] = '\0';
	if (st->infinity > 0)
	{
		return snprintf(str, bufsize, "%s", INFINITY_STRING);
	}
	else if (st->infinity < 0)
	{
		return snprintf(str, bufsize, "%s", MINFINITY_STRING);
	}
	if (precision > 0 && st->fr)
	{
		SPRINTF_FIXED(precstr, ".%09d", st->fr);
		if (precision < 9)
			precstr[precision + 1] = '\0';
		else if (precision > 9)
			precision = 9;
		for (i = precision; i > 0; i--)
		{
			if (precstr[i] != '0')
				break;
			precstr[i] = '\0';
		}
		if (i == 0)
			precstr[i] = '\0';
	}
	zonestr[0] = '\0';
#ifdef	TIMEZONE_GLOBAL
	if (bZone && TZNAME_GLOBAL[0] && TZNAME_GLOBAL[0][0] && st->y >= 1970)
	{
		long		zoneint;
		struct tm	tm;
		time_t		time0;

		zoneint = TIMEZONE_GLOBAL;
		if (DAYLIGHT_GLOBAL && st->y >= 1900)
		{
			tm.tm_year = st->y - 1900;
			tm.tm_mon = st->m - 1;
			tm.tm_mday = st->d;
			tm.tm_hour = st->hh;
			tm.tm_min = st->mm;
			tm.tm_sec = st->ss;
			tm.tm_isdst = -1;
			time0 = mktime(&tm);
			if (time0 >= 0 && tm.tm_isdst > 0)
				zoneint -= 3600;
		}
		if (zoneint > 0)
			SPRINTF_FIXED(zonestr, "-%02d", (int) zoneint / 3600);
		else
			SPRINTF_FIXED(zonestr, "+%02d", -(int) zoneint / 3600);
	}
#endif /* TIMEZONE_GLOBAL */
	if (st->y < 0)
		return snprintf(str, bufsize, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d%s%s BC", -st->y, st->m, st->d, st->hh, st->mm, st->ss, precstr, zonestr);
	else
		return snprintf(str, bufsize, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d%s%s", st->y, st->m, st->d, st->hh, st->mm, st->ss, precstr, zonestr);
}

static
SQLINTERVAL	interval2itype(SQLSMALLINT ctype)
{
	SQLINTERVAL	sqlitv = 0;

	switch (ctype)
	{
		case SQL_C_INTERVAL_YEAR:
			sqlitv = SQL_IS_YEAR;
			break;
		case SQL_C_INTERVAL_MONTH:
			sqlitv = SQL_IS_MONTH;
			break;
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
			sqlitv = SQL_IS_YEAR_TO_MONTH;
			break;
		case SQL_C_INTERVAL_DAY:
			sqlitv = SQL_IS_DAY;
			break;
		case SQL_C_INTERVAL_HOUR:
			sqlitv = SQL_IS_HOUR;
			break;
		case SQL_C_INTERVAL_DAY_TO_HOUR:
			sqlitv = SQL_IS_DAY_TO_HOUR;
			break;
		case SQL_C_INTERVAL_MINUTE:
			sqlitv = SQL_IS_MINUTE;
			break;
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
			sqlitv = SQL_IS_DAY_TO_MINUTE;
			break;
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
			sqlitv = SQL_IS_HOUR_TO_MINUTE;
			break;
		case SQL_C_INTERVAL_SECOND:
			sqlitv = SQL_IS_SECOND;
			break;
		case SQL_C_INTERVAL_DAY_TO_SECOND:
			sqlitv = SQL_IS_DAY_TO_SECOND;
			break;
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
			sqlitv = SQL_IS_HOUR_TO_SECOND;
			break;
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
			sqlitv = SQL_IS_MINUTE_TO_SECOND;
			break;
	}
	return sqlitv;
}

/*
 *	Interval data <-----> SQL_INTERVAL_STRUCT
 */

static int getPrecisionPart(int precision, const char * precPart)
{
	char	fraction[] = "000000000";
	int		fracs = sizeof(fraction) - 1;
	size_t	cpys;

	if (precision < 0)
		precision = 6; /* default */
	if (precision == 0)
		return 0;
	cpys = strlen(precPart);
	if (cpys > fracs)
		cpys = fracs;
	memcpy(fraction, precPart, cpys);
	fraction[precision] = '\0';

	return atoi(fraction);
}

static BOOL
interval2istruct(SQLSMALLINT ctype, int precision, const char *str, SQL_INTERVAL_STRUCT *st)
{
	char	lit1[64], lit2[64];
	int	scnt, years, mons, days, hours, minutes, seconds;
	BOOL	sign;
	SQLINTERVAL	itype = interval2itype(ctype);

	memset(st, 0, sizeof(SQL_INTERVAL_STRUCT));
	if ((scnt = sscanf(str, "%d-%d", &years, &mons)) >=2)
	{
		if (SQL_IS_YEAR_TO_MONTH == itype)
		{
			sign = years < 0 ? SQL_TRUE : SQL_FALSE;
			st->interval_type = itype;
			st->interval_sign = sign;
			st->intval.year_month.year = sign ? (-years) : years;
			st->intval.year_month.month = mons;
			return TRUE;
		}
		return FALSE;
	}
	else if (scnt = sscanf(str, "%d %02d:%02d:%02d.%09s", &days, &hours, &minutes, &seconds, lit2), 5 == scnt || 4 == scnt)
	{
		sign = days < 0 ? SQL_TRUE : SQL_FALSE;
		st->interval_type = itype;
		st->interval_sign = sign;
		st->intval.day_second.day = sign ? (-days) : days;
		st->intval.day_second.hour = hours;
		st->intval.day_second.minute = minutes;
		st->intval.day_second.second = seconds;
		if (scnt > 4)
			st->intval.day_second.fraction = getPrecisionPart(precision, lit2);
		return TRUE;
	}
	else if ((scnt = sscanf(str, "%d %10s %d %10s", &years, lit1, &mons, lit2)) >=4)
	{
		if (strnicmp(lit1, "year", 4) == 0 &&
		    strnicmp(lit2, "mon", 2) == 0 &&
		    (SQL_IS_MONTH == itype ||
		     SQL_IS_YEAR_TO_MONTH == itype))
		{
			sign = years < 0 ? SQL_TRUE : SQL_FALSE;
			st->interval_type = itype;
			st->interval_sign = sign;
			st->intval.year_month.year = sign ? (-years) : years;
			st->intval.year_month.month = sign ? (-mons) : mons;
			return TRUE;
		}
		return FALSE;
	}
	if ((scnt = sscanf(str, "%d %10s %d", &years, lit1, &days)) == 2)
	{
		sign = years < 0 ? SQL_TRUE : SQL_FALSE;
		if (SQL_IS_YEAR == itype &&
		    (stricmp(lit1, "year") == 0 ||
		     stricmp(lit1, "years") == 0))
		{
			st->interval_type = itype;
			st->interval_sign = sign;
			st->intval.year_month.year = sign ? (-years) : years;
			return TRUE;
		}
		if (SQL_IS_MONTH == itype &&
		    (stricmp(lit1, "mon") == 0 ||
		     stricmp(lit1, "mons") == 0))
		{
			st->interval_type = itype;
			st->interval_sign = sign;
			st->intval.year_month.month = sign ? (-years) : years;
			return TRUE;
		}
		if (SQL_IS_DAY == itype &&
		    (stricmp(lit1, "day") == 0 ||
		     stricmp(lit1, "days") == 0))
		{
			st->interval_type = itype;
			st->interval_sign = sign;
			st->intval.day_second.day = sign ? (-years) : years;
			return TRUE;
		}
		return FALSE;
	}
	if (itype == SQL_IS_YEAR || itype == SQL_IS_MONTH || itype == SQL_IS_YEAR_TO_MONTH)
	{
		/* these formats should've been handled above already */
		return FALSE;
	}
	scnt = sscanf(str, "%d %10s %02d:%02d:%02d.%09s", &days, lit1, &hours, &minutes, &seconds, lit2);
	if (scnt == 5 || scnt == 6)
	{
		if (strnicmp(lit1, "day", 3) != 0)
			return FALSE;
		sign = days < 0 ? SQL_TRUE : SQL_FALSE;

		st->interval_type = itype;
		st->interval_sign = sign;
		st->intval.day_second.day = sign ? (-days) : days;
		st->intval.day_second.hour = sign ? (-hours) : hours;
		st->intval.day_second.minute = minutes;
		st->intval.day_second.second = seconds;
		if (scnt > 5)
			st->intval.day_second.fraction = getPrecisionPart(precision, lit2);
		return TRUE;
	}
	scnt = sscanf(str, "%02d:%02d:%02d.%09s", &hours, &minutes, &seconds, lit2);
	if (scnt == 3 || scnt == 4)
	{
		sign = hours < 0 ? SQL_TRUE : SQL_FALSE;

		st->interval_type = itype;
		st->interval_sign = sign;
		st->intval.day_second.hour = sign ? (-hours) : hours;
		st->intval.day_second.minute = minutes;
		st->intval.day_second.second = seconds;
		if (scnt > 3)
			st->intval.day_second.fraction = getPrecisionPart(precision, lit2);
		return TRUE;
	}

	return FALSE;
}


#ifdef	HAVE_LOCALE_H
/*
 * Get the decimal point of the current locale.
 *
 * XXX: This isn't thread-safe, if another thread changes the locale with
 * setlocale() concurrently. There are two problems with that:
 *
 * 1. The pointer returned by localeconv(), or the lc->decimal_point string,
 * might be invalidated by calls in other threads. Until someone comes up
 * with a thread-safe version of localeconv(), there isn't much we can do
 * about that. (libc implementations that return a static buffer (like glibc)
 * happen to be safe from the lconv struct being invalidated, but the
 * decimal_point string might still not point to a static buffer).
 *
 * 2. The between the call to sprintf() and get_current_decimal_point(), the
 * decimal point might change. That would cause set_server_decimal_point()
 * to fail to recognize a decimal separator, and we might send a numeric
 * string to the server that the server won't recognize. This would cause
 * the query to fail in the server.
 *
 * XXX: we only take into account the first byte of the decimal separator.
 */
static char get_current_decimal_point(void)
{
	struct lconv	*lc = localeconv();

	return lc->decimal_point[0];
}

/*
 * Modify the string representation of a numeric/float value, converting the
 * decimal point from '.' to the correct decimal separator of the current
 * locale.
 */
static void set_server_decimal_point(char *num, SQLLEN len)
{
	char current_decimal_point = get_current_decimal_point();
	char *str;
	SQLLEN i;

	if ('.' == current_decimal_point)
		return;
	i = 0;
	for (str = num; '\0' != *str; str++)
	{
		if (*str == current_decimal_point)
		{
			*str = '.';
			break;
		}

		if (len != SQL_NTS && i++ >= len)
			break;
	}
}

/*
 * Inverse of set_server_decimal_point.
 */
static void set_client_decimal_point(char *num)
{
	char current_decimal_point = get_current_decimal_point();
	char *str;

	if ('.' == current_decimal_point)
		return;
	for (str = num; '\0' != *str; str++)
	{
		if (*str == '.')
		{
			*str = current_decimal_point;
			break;
		}
	}
}
#else
static void set_server_decimal_point(char *num) {}
static void set_client_decimal_point(char *num, BOOL) {}
#endif /* HAVE_LOCALE_H */

/*	This is called by SQLFetch() */
int
copy_and_convert_field_bindinfo(StatementClass *stmt, OID field_type, int atttypmod, void *value, int col)
{
	ARDFields *opts = SC_get_ARDF(stmt);
	BindInfoClass *bic;
	SQLULEN	offset = opts->row_offset_ptr ? *opts->row_offset_ptr : 0;

	if (opts->allocated <= col)
		extend_column_bindings(opts, col + 1);
	bic = &(opts->bindings[col]);
	SC_set_current_col(stmt, -1);
	return copy_and_convert_field(stmt, field_type, atttypmod, value,
		bic->returntype, bic->precision,
		(PTR) (bic->buffer + offset), bic->buflen,
		LENADDR_SHIFT(bic->used, offset), LENADDR_SHIFT(bic->indicator, offset));
}

/*
 * Is 'str' a valid integer literal, consisting only of ASCII characters
 * 0-9 ?
 *
 * Also, *negative is set to TRUE if the value was negative.
 *
 * We don't check for overflow here. This is just to decide if we need to
 * quote the value.
 */
static BOOL
valid_int_literal(const char *str, SQLLEN len, BOOL *negative)
{
	SQLLEN i = 0;

	/* Check there is a minus sign in front */
	if ((len == SQL_NTS || len > 0) && str[0] == '-')
	{
		i++;
		*negative = TRUE;
	}
	else
		*negative = FALSE;

	/*
	 * Must begin with a digit. This also rejects empty strings and '-'.
	 */
	if (i == len || !(str[i] >= '0' && str[i] <= '9'))
		return FALSE;

	for (; str[i] && (len == SQL_NTS || i < len); i++)
	{
		if (!(str[i] >= '0' && str[i] <= '9'))
			return FALSE;
	}

	return TRUE;
}

static double get_double_value(const char *str)
{
	if (stricmp(str, NAN_STRING) == 0)
#ifdef	NAN
		return (double) NAN;
#else
	{
		double	a = .0;
		return .0 / a;
	}
#endif /* NAN */
	else if (stricmp(str, INFINITY_STRING) == 0)
#ifdef	INFINITY
		return (double) INFINITY;
#else
		return (double) (HUGE_VAL * HUGE_VAL);
#endif /* INFINITY */
	else if (stricmp(str, MINFINITY_STRING) == 0)
#ifdef	INFINITY
		return (double) -INFINITY;
#else
		return (double) -(HUGE_VAL * HUGE_VAL);
#endif /* INFINITY */
	return atof(str);
}

static int char2guid(const char *str, SQLGUID *g)
{
	/*
	 * SQLGUID.Data1 is an "unsigned long" on some platforms, and
	 * "unsigned int" on others. For format "%08X", it should be an
	 * "unsigned int", so use a temporary variable for it.
	 */
	unsigned int Data1;
	if (sscanf(str,
		"%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		&Data1,
		&g->Data2, &g->Data3,
		&g->Data4[0], &g->Data4[1], &g->Data4[2], &g->Data4[3],
		&g->Data4[4], &g->Data4[5], &g->Data4[6], &g->Data4[7]) < 11)
		return COPY_GENERAL_ERROR;
	g->Data1 = Data1;
	return COPY_OK;
}

static int	effective_fraction(int fraction, int *width)
{
	for (*width = 9; fraction % 10 == 0; (*width)--, fraction /= 10)
		;
	return fraction;
}


static int
get_terminator_len(SQLSMALLINT fCType)
{
	switch (fCType)
	{
#ifdef	UNICODE_SUPPORT
		case SQL_C_WCHAR:
			return WCLEN;
#endif /* UNICODE_SUPPORT */
		case SQL_C_BINARY:
			return 0;
	}

	/* SQL_C_CHAR or INTERNAL_ASIS_TYPE */
	return 1;
}

static SQLLEN
get_adjust_len(SQLSMALLINT fCType, SQLLEN len)
{
	switch (fCType)
	{
#ifdef	UNICODE_SUPPORT
		case SQL_C_WCHAR:
			return (len / WCLEN) * WCLEN;
#endif /* UNICODE_SUPPORT */
	}

	return len;
}

#define	BYTEA_PROCESS_ESCAPE	1
#define	BYTEA_PROCESS_BINARY	2

static int
setup_getdataclass(SQLLEN * const length_return, const char ** const ptr_return,
	int *needbuflen_return, GetDataClass * const pgdc, const char *neut_str,
	const OID field_type, const SQLSMALLINT fCType,
	const SQLLEN cbValueMax, const ConnectionClass * const conn)
{
	SQLLEN len = (-2);
	const char *ptr = NULL;
	int	needbuflen = 0;
	int	result = COPY_OK;

	BOOL	lf_conv = conn->connInfo.lf_conversion;
	int	bytea_process_kind = 0;
	BOOL	already_processed = FALSE;
	BOOL	changed = FALSE;
	int	len_for_wcs_term = 0;

#ifdef	UNICODE_SUPPORT
	char	*allocbuf = NULL;
	int	unicode_count = -1;
	BOOL	localize_needed = FALSE;
	BOOL	hybrid = FALSE;
#endif /* UNICODE_SUPPORT */

	if (PG_TYPE_BYTEA == field_type)
	{
		if (SQL_C_BINARY == fCType)
			bytea_process_kind = BYTEA_PROCESS_BINARY;
		else if (0 == strnicmp(neut_str, "\\x", 2)) /* hex format */
			neut_str += 2;
		else
			bytea_process_kind = BYTEA_PROCESS_ESCAPE;
	}

#ifdef	UNICODE_SUPPORT
	if (0 == bytea_process_kind)
	{
		if (get_convtype() > 0) /* coversion between the current locale is available */
		{
			BOOL	wcs_debug = conn->connInfo.wcs_debug;
			BOOL	same_encoding = (conn->ccsc == pg_CS_code(conn->locale_encoding));
			BOOL	is_utf8 = (UTF8 == conn->ccsc);

			switch (field_type)
			{
				case PG_TYPE_UNKNOWN:
				case PG_TYPE_BPCHAR:
				case PG_TYPE_VARCHAR:
				case PG_TYPE_TEXT:
				case PG_TYPE_BPCHARARRAY:
				case PG_TYPE_VARCHARARRAY:
				case PG_TYPE_TEXTARRAY:
					if (SQL_C_CHAR == fCType || SQL_C_BINARY == fCType)
						localize_needed = (!same_encoding || wcs_debug);
					if (SQL_C_WCHAR == fCType)
						hybrid = (!is_utf8 || (same_encoding && wcs_debug));
			}
			MYLOG(0, "localize=%d hybrid=%d is_utf8=%d same_encoding=%d wcs_debug=%d\n", localize_needed, hybrid, is_utf8, same_encoding, wcs_debug);
		}
	}
	if (fCType == SQL_C_WCHAR)
	{
		if (BYTEA_PROCESS_ESCAPE == bytea_process_kind)
			unicode_count = convert_from_pgbinary(neut_str, NULL, 0) * 2;
		else if (hybrid)
		{
			MYLOG(0, "hybrid estimate\n");
			if ((unicode_count = bindcol_hybrid_estimate(neut_str, lf_conv, &allocbuf)) < 0)
			{
				result = COPY_INVALID_STRING_CONVERSION;
				goto cleanup;
			}
		}
		else	/* normally */
		{
			unicode_count = utf8_to_ucs2_lf(neut_str, SQL_NTS, lf_conv, NULL, 0, FALSE);
		}
		len = WCLEN * unicode_count;
		already_processed = changed = TRUE;
	}
	else if (localize_needed)
	{
		if ((len = bindcol_localize_estimate(neut_str, lf_conv, &allocbuf)) < 0)
		{
			result = COPY_INVALID_STRING_CONVERSION;
			goto cleanup;
		}
		already_processed = changed = TRUE;
	}
#endif /* UNICODE_SUPPORT */

	if (already_processed)	/* skip */
		;
	else if (0 != bytea_process_kind)
	{
		len = convert_from_pgbinary(neut_str, NULL, 0);
		if (BYTEA_PROCESS_BINARY != bytea_process_kind)
			len *= 2;
		changed = TRUE;
	}
	else
		/* convert linefeeds to carriage-return/linefeed */
		len = convert_linefeeds(neut_str, NULL, 0, lf_conv, &changed);

	/* just returns length info */
	if (cbValueMax == 0)
	{
		result = COPY_RESULT_TRUNCATED;
		goto cleanup;
	}

	if (!pgdc->ttlbuf)
		pgdc->ttlbuflen = 0;
	needbuflen = len + get_terminator_len(fCType);
	if (SQL_C_BINARY == fCType)
	{
		/*
		 * Though Binary doesn't have NULL terminator,
		 * bindcol_localize_exec() needs output buffer
		 * for NULL terminator.
		 */
		len_for_wcs_term = 1;
	}
	if (changed || needbuflen > cbValueMax)
	{
		if (needbuflen > (SQLLEN) pgdc->ttlbuflen)
		{
			pgdc->ttlbuf = realloc(pgdc->ttlbuf, needbuflen + len_for_wcs_term);
			pgdc->ttlbuflen = needbuflen;
		}

		already_processed = FALSE;
#ifdef	UNICODE_SUPPORT
		if (fCType == SQL_C_WCHAR)
		{
			if (BYTEA_PROCESS_ESCAPE == bytea_process_kind)
			{
				len = convert_from_pgbinary(neut_str, pgdc->ttlbuf, pgdc->ttlbuflen);
				len = pg_bin2whex(pgdc->ttlbuf, (SQLWCHAR *) pgdc->ttlbuf, len);
			}
			else
			{
				if (!hybrid)	/* normally */
					utf8_to_ucs2_lf(neut_str, SQL_NTS, lf_conv, (SQLWCHAR *) pgdc->ttlbuf, unicode_count, FALSE);
				else /* hybrid */
				{
					MYLOG(0, "hybrid convert\n");
					if (bindcol_hybrid_exec((SQLWCHAR *) pgdc->ttlbuf, neut_str, unicode_count + 1, lf_conv, &allocbuf) < 0)
					{
						result = COPY_INVALID_STRING_CONVERSION;
						goto cleanup;
					}
				}
			}
			already_processed = TRUE;
		}
		else if (localize_needed)
		{
			if (bindcol_localize_exec(pgdc->ttlbuf, len + 1, lf_conv, &allocbuf) < 0)
			{
				result = COPY_INVALID_STRING_CONVERSION;
				goto cleanup;
			}
			already_processed = TRUE;
		}
#endif /* UNICODE_SUPPORT */

		if (already_processed)
			;
		else if (0 != bytea_process_kind)
		{
			len = convert_from_pgbinary(neut_str, pgdc->ttlbuf, pgdc->ttlbuflen);
			if (BYTEA_PROCESS_ESCAPE == bytea_process_kind)
				len = pg_bin2hex(pgdc->ttlbuf, pgdc->ttlbuf, len);
		}
		else
			convert_linefeeds(neut_str, pgdc->ttlbuf, pgdc->ttlbuflen, lf_conv, &changed);
		ptr = pgdc->ttlbuf;
		pgdc->ttlbufused = len;
	}
	else
	{
		if (pgdc->ttlbuf)
		{
			free(pgdc->ttlbuf);
			pgdc->ttlbuf = NULL;
		}
		ptr = neut_str;
	}
cleanup:
#ifdef	UNICODE_SUPPORT
	if (allocbuf)
		free(allocbuf);
#endif /* UNICODE_SUPPORT */

	*length_return = len;
	*ptr_return = ptr;
	*needbuflen_return = needbuflen;

	return result;
}

/*
	gdata		SC_get_GDTI(stmt)
	current_col	stmt->current_col
 */

/*
 *	fCType treated in the following function is
 *
 *	SQL_C_CHAR, SQL_C_BINARY, SQL_C_WCHAR or INTERNAL_ASIS_TYPE
 */
static int
convert_text_field_to_sql_c(GetDataInfo * const gdata, const int current_col,
	const char * const neut_str, const OID field_type,
	const SQLSMALLINT fCType, char * const rgbValueBindRow,
	const SQLLEN cbValueMax, const ConnectionClass * const conn,
	SQLLEN * const length_return)
{
	int	result = COPY_OK;
	SQLLEN	len = (-2);
	GetDataClass *pgdc;
	int	copy_len = 0, needbuflen = 0, i;
	const char	*ptr;

	MYLOG(0, "field_type=%u type=%d\n", field_type, fCType);

	switch (field_type)
	{
		case PG_TYPE_FLOAT4:
		case PG_TYPE_FLOAT8:
		case PG_TYPE_NUMERIC:
			set_client_decimal_point((char *) neut_str);
			break;
	}

	if (current_col < 0)
	{
		pgdc = &(gdata->fdata);
		pgdc->data_left = -1;
	}
	else
		pgdc = &gdata->gdata[current_col];
	if (pgdc->data_left < 0)
	{
		if (COPY_OK != (result = setup_getdataclass(&len, &ptr,
				&needbuflen, pgdc, neut_str, field_type,
				fCType, cbValueMax, conn)))
			goto cleanup;
	}
	else
	{
		ptr = pgdc->ttlbuf;
		len = pgdc->ttlbufused;
	}

	MYLOG(0, "DEFAULT: len = " FORMAT_LEN ", ptr = '%.*s'\n", len, (int) len, ptr);

	if (current_col >= 0)
	{
		if (pgdc->data_left > 0)
		{
			ptr += (len - pgdc->data_left);
			len = pgdc->data_left;
			needbuflen = len + (pgdc->ttlbuflen - pgdc->ttlbufused);
		}
		else
			pgdc->data_left = len;
	}

	if (cbValueMax > 0)
	{
		BOOL	already_copied = FALSE;
		int		terminatorlen;

		terminatorlen = get_terminator_len(fCType);
		if (terminatorlen >= cbValueMax)
			copy_len = 0;
		else if (len + terminatorlen > cbValueMax)
			copy_len = get_adjust_len(fCType, cbValueMax - terminatorlen);
		else
			copy_len = len;

		if (!already_copied)
		{
			/* Copy the data */
			if (copy_len > 0)
				memcpy(rgbValueBindRow, ptr, copy_len);
			/* Add null terminator */
			for (i = 0; i < terminatorlen && copy_len + i < cbValueMax; i++)
				rgbValueBindRow[copy_len + i] = '\0';
		}
		/* Adjust data_left for next time */
		if (current_col >= 0)
			pgdc->data_left -= copy_len;
	}

	/*
	 * Finally, check for truncation so that proper status can
	 * be returned
	 */
	if (cbValueMax > 0 && needbuflen > cbValueMax)
		result = COPY_RESULT_TRUNCATED;
	else
	{
		if (pgdc->ttlbuf != NULL)
		{
			free(pgdc->ttlbuf);
			pgdc->ttlbuf = NULL;
		}
	}

#ifdef	UNICODE_SUPPORT
	if (SQL_C_WCHAR == fCType)
		MYLOG(0, "    SQL_C_WCHAR, default: len = " FORMAT_LEN ", cbValueMax = " FORMAT_LEN ", rgbValueBindRow = '%s'\n", len, cbValueMax, rgbValueBindRow);
	else
#endif /* UNICODE_SUPPORT */
	if (SQL_C_BINARY == fCType)
		MYLOG(0, "    SQL_C_BINARY, default: len = " FORMAT_LEN ", cbValueMax = " FORMAT_LEN ", rgbValueBindRow = '%.*s'\n", len, cbValueMax, copy_len, rgbValueBindRow);
	else
		MYLOG(0, "    SQL_C_CHAR, default: len = " FORMAT_LEN ", cbValueMax = " FORMAT_LEN ", rgbValueBindRow = '%s'\n", len, cbValueMax, rgbValueBindRow);

cleanup:
	*length_return = len;

	return result;
}

/*	This is called by SQLGetData() */
int
copy_and_convert_field(StatementClass *stmt,
		OID field_type, int atttypmod,
		void *valuei,
		SQLSMALLINT fCType, int precision,
		PTR rgbValue, SQLLEN cbValueMax,
		SQLLEN *pcbValue, SQLLEN *pIndicator)
{
	CSTR func = "copy_and_convert_field";
	const char *value = valuei;
	ARDFields	*opts = SC_get_ARDF(stmt);
	GetDataInfo	*gdata = SC_get_GDTI(stmt);
	SQLLEN		len = 0;
	SIMPLE_TIME std_time;
#ifdef	HAVE_LOCALTIME_R
	struct tm  tm;
#endif /* HAVE_LOCALTIME_R */
	SQLLEN			pcbValueOffset,
				rgbValueOffset;
	char	   *rgbValueBindRow = NULL;
	SQLLEN		*pcbValueBindRow = NULL, *pIndicatorBindRow = NULL;
	SQLSETPOSIROW		bind_row = stmt->bind_row;
	int			bind_size = opts->bind_size;
	int			result = COPY_OK;
	const ConnectionClass	*conn = SC_get_conn(stmt);
	BOOL	text_bin_handling;
	const char *neut_str = value;
	char		booltemp[3];
	char		midtemp[64];
	GetDataClass *pgdc;

	if (stmt->current_col >= 0)
	{
		if (stmt->current_col >= opts->allocated)
		{
			return SQL_ERROR;
		}
		if (gdata->allocated != opts->allocated)
			extend_getdata_info(gdata, opts->allocated, TRUE);
		pgdc = &gdata->gdata[stmt->current_col];
		if (pgdc->data_left == -2)
			pgdc->data_left = (cbValueMax > 0) ? 0 : -1; /* This seems to be *
						 * needed by ADO ? */
		if (pgdc->data_left == 0)
		{
			if (pgdc->ttlbuf != NULL)
			{
				free(pgdc->ttlbuf);
				pgdc->ttlbuf = NULL;
				pgdc->ttlbuflen = 0;
			}
			pgdc->data_left = -2;		/* needed by ADO ? */
			return COPY_NO_DATA_FOUND;
		}
	}
	/*---------
	 *	rgbValueOffset is *ONLY* for character and binary data.
	 *	pcbValueOffset is for computing any pcbValue location
	 *---------
	 */

	if (bind_size > 0)
		pcbValueOffset = rgbValueOffset = (bind_size * bind_row);
	else
	{
		pcbValueOffset = bind_row * sizeof(SQLLEN);
		rgbValueOffset = bind_row * cbValueMax;
	}
	/*
	 *	The following is applicable in case bind_size > 0
	 *	or the fCType is of variable length.
	 */
	if (rgbValue)
		rgbValueBindRow = (char *) rgbValue + rgbValueOffset;
	if (pcbValue)
		pcbValueBindRow = LENADDR_SHIFT(pcbValue, pcbValueOffset);
	if (pIndicator)
	{
		pIndicatorBindRow = LENADDR_SHIFT(pIndicator, pcbValueOffset);
		*pIndicatorBindRow = 0;
	}

	memset(&std_time, 0, sizeof(SIMPLE_TIME));

	MYLOG(0, "field_type = %d, fctype = %d, value = '%s', cbValueMax=" FORMAT_LEN "\n", field_type, fCType, (value == NULL) ? "<NULL>" : value, cbValueMax);

	if (!value)
	{
MYLOG(0, "null_cvt_date_string=%d\n", conn->connInfo.cvt_null_date_string);
		/* a speicial handling for FOXPRO NULL -> NULL_STRING */
		if (conn->connInfo.cvt_null_date_string > 0 &&
		    (PG_TYPE_DATE == field_type ||
		     PG_TYPE_DATETIME == field_type ||
		     PG_TYPE_TIMESTAMP_NO_TMZONE == field_type) &&
		    (SQL_C_CHAR == fCType ||
#ifdef	UNICODE_SUPPORT
		     SQL_C_WCHAR == fCType ||
#endif	/* UNICODE_SUPPORT */
		     SQL_C_DATE == fCType ||
		     SQL_C_TYPE_DATE == fCType ||
		     SQL_C_DEFAULT == fCType))
		{
			if (pcbValueBindRow)
				*pcbValueBindRow = 0;
			switch (fCType)
			{
				case SQL_C_CHAR:
					if (rgbValueBindRow && cbValueMax > 0)
						*rgbValueBindRow = '\0';
					else
						result = COPY_RESULT_TRUNCATED;
					break;
				case SQL_C_DATE:
				case SQL_C_TYPE_DATE:
				case SQL_C_DEFAULT:
					if (rgbValueBindRow && cbValueMax >= sizeof(DATE_STRUCT))
					{
						memset(rgbValueBindRow, 0, cbValueMax);
						if (pcbValueBindRow)
							*pcbValueBindRow = sizeof(DATE_STRUCT);
					}
					else
						result = COPY_RESULT_TRUNCATED;
					break;
#ifdef	UNICODE_SUPPORT
				case SQL_C_WCHAR:
					if (rgbValueBindRow && cbValueMax >= WCLEN)
						memset(rgbValueBindRow, 0, WCLEN);
					else
						result = COPY_RESULT_TRUNCATED;
					break;
#endif /* UNICODE_SUPPORT */
			}
			return result;
		}
		/*
		 * handle a null just by returning SQL_NULL_DATA in pcbValue, and
		 * doing nothing to the buffer.
		 */
		else if (pIndicator)
		{
			*pIndicatorBindRow = SQL_NULL_DATA;
			return COPY_OK;
		}
		else
		{
			SC_set_error(stmt, STMT_RETURN_NULL_WITHOUT_INDICATOR, "StrLen_or_IndPtr was a null pointer and NULL data was retrieved", func);
			return	SQL_ERROR;
		}
	}

	if (stmt->hdbc->DataSourceToDriver != NULL)
	{
		size_t			length = strlen(value);

		stmt->hdbc->DataSourceToDriver(stmt->hdbc->translation_option,
						SQL_CHAR, valuei, (SDWORD) length,
						valuei, (SDWORD) length, NULL,
						NULL, 0, NULL);
	}

	/*
	 * First convert any specific postgres types into more useable data.
	 *
	 * NOTE: Conversions from PG char/varchar of a date/time/timestamp value
	 * to SQL_C_DATE,SQL_C_TIME, SQL_C_TIMESTAMP not supported
	 */
	switch (field_type)
	{
			/*
			 * $$$ need to add parsing for date/time/timestamp strings in
			 * PG_TYPE_CHAR,VARCHAR $$$
			 */
		case PG_TYPE_DATE:
			sscanf(value, "%4d-%2d-%2d", &std_time.y, &std_time.m, &std_time.d);
			break;

		case PG_TYPE_TIME:
			{

				BOOL	bZone = FALSE;	/* time zone stuff is unreliable */
				int	zone;
				timestamp2stime(value, &std_time, &bZone, &zone);
			}
			break;

		case PG_TYPE_ABSTIME:
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
		case PG_TYPE_TIMESTAMP:
			std_time.fr = 0;
			std_time.infinity = 0;
			if (strnicmp(value, INFINITY_STRING, 8) == 0)
			{
				std_time.infinity = 1;
				std_time.m = 12;
				std_time.d = 31;
				std_time.y = 9999;
				std_time.hh = 23;
				std_time.mm = 59;
				std_time.ss = 59;
			}
			if (strnicmp(value, MINFINITY_STRING, 9) == 0)
			{
				std_time.infinity = -1;
				std_time.m = 1;
				std_time.d = 1;
				// std_time.y = -4713;
				std_time.y = -9999;
				std_time.hh = 0;
				std_time.mm = 0;
				std_time.ss = 0;
			}
			if (strnicmp(value, "invalid", 7) != 0)
			{
				BOOL		bZone = field_type != PG_TYPE_TIMESTAMP_NO_TMZONE;
				int			zone;

				/*
				 * sscanf(value, "%4d-%2d-%2d %2d:%2d:%2d", &std_time.y, &std_time.m,
				 * &std_time.d, &std_time.hh, &std_time.mm, &std_time.ss);
				 */
				bZone = FALSE;	/* time zone stuff is unreliable */
				timestamp2stime(value, &std_time, &bZone, &zone);
MYLOG(DETAIL_LOG_LEVEL, "2stime fr=%d\n", std_time.fr);
			}
			else
			{
				/*
				 * The timestamp is invalid so set something conspicuous,
				 * like the epoch
				 */
				struct tm  *tim;
				time_t	t = 0;
#ifdef	HAVE_LOCALTIME_R
				tim = localtime_r(&t, &tm);
#else
				tim = localtime(&t);
#endif /* HAVE_LOCALTIME_R */
				std_time.m = tim->tm_mon + 1;
				std_time.d = tim->tm_mday;
				std_time.y = tim->tm_year + 1900;
				std_time.hh = tim->tm_hour;
				std_time.mm = tim->tm_min;
				std_time.ss = tim->tm_sec;
			}
			break;

		case PG_TYPE_BOOL:
			{					/* change T/F to 1/0 */
				const ConnInfo *ci = &(conn->connInfo);

				switch (((char *)value)[0])
				{
					case 'f':
					case 'F':
					case 'n':
					case 'N':
					case '0':
						STRCPY_FIXED(booltemp, "0");
						break;
					default:
						if (ci->true_is_minus1)
							STRCPY_FIXED(booltemp, "-1");
						else
							STRCPY_FIXED(booltemp, "1");
				}
				neut_str = booltemp;
			}
			break;

			/* This is for internal use by SQLStatistics() */
		case PG_TYPE_INT2VECTOR:
			if (SQL_C_DEFAULT == fCType)
			{
				int	i, nval, maxc;
				const char *vp;
				/* this is an array of eight integers */
				short	   *short_array = (short *) rgbValueBindRow, shortv;

				maxc = 0;
				if (NULL != short_array)
					maxc = (int) cbValueMax / sizeof(short);
				vp = value;
				nval = 0;
				MYLOG(0, "index=(");
				for (i = 0;; i++)
				{
					if (sscanf(vp, "%hi", &shortv) != 1)
						break;
					MYPRINTF(0, " %hi", shortv);
					nval++;
					if (nval < maxc)
						short_array[i + 1] = shortv;

					/* skip the current token */
					while (IS_NOT_SPACE(*vp))
						vp++;
					/* and skip the space to the next token */
					while ((*vp != '\0') && (isspace(*vp)))
						vp++;
					if (*vp == '\0')
						break;
				}
				MYPRINTF(0, ") nval = %i\n", nval);
				if (maxc > 0)
					short_array[0] = nval;

				/* There is no corresponding fCType for this. */
				len = (nval + 1) * sizeof(short);
				if (pcbValue)
					*pcbValueBindRow = len;

				if (len <= cbValueMax)
					return COPY_OK; /* dont go any further or the data will be
								 * trashed */
				else
					return COPY_RESULT_TRUNCATED;
			}
			break;

			/*
			 * This is a large object OID, which is used to store
			 * LONGVARBINARY objects.
			 */
		case PG_TYPE_LO_UNDEFINED:

			return convert_lo(stmt, value, fCType, rgbValueBindRow, cbValueMax, pcbValueBindRow);

		case 0:
			break;

		default:

			if (field_type == stmt->hdbc->lobj_type	/* hack until permanent type available */
			   || (PG_TYPE_OID == field_type && SQL_C_BINARY == fCType && conn->lo_is_domain)
			   )
				return convert_lo(stmt, value, fCType, rgbValueBindRow, cbValueMax, pcbValueBindRow);
	}

	/* Change default into something useable */
	if (fCType == SQL_C_DEFAULT)
	{
		fCType = pgtype_attr_to_ctype(conn, field_type, atttypmod);
#ifdef	UNICODE_SUPPORT
		if (fCType == SQL_C_WCHAR
		    && CC_default_is_c(conn))
			fCType = SQL_C_CHAR;
#endif

		MYLOG(0, ", SQL_C_DEFAULT: fCType = %d\n", fCType);
	}

	text_bin_handling = FALSE;
	switch (fCType)
	{
		case INTERNAL_ASIS_TYPE:
#ifdef	UNICODE_SUPPORT
		case SQL_C_WCHAR:
#endif /* UNICODE_SUPPORT */
		case SQL_C_CHAR:
			text_bin_handling = TRUE;
			break;
		case SQL_C_BINARY:
			switch (field_type)
			{
				case PG_TYPE_UNKNOWN:
				case PG_TYPE_BPCHAR:
				case PG_TYPE_VARCHAR:
				case PG_TYPE_TEXT:
				case PG_TYPE_XML:
				case PG_TYPE_BPCHARARRAY:
				case PG_TYPE_VARCHARARRAY:
				case PG_TYPE_TEXTARRAY:
				case PG_TYPE_XMLARRAY:
				case PG_TYPE_BYTEA:
					text_bin_handling = TRUE;
					break;
			}
			break;
	}

	if (text_bin_handling)
	{
		BOOL	pre_convert = TRUE;
		int	midsize = sizeof(midtemp);
		int	i;

		/* Special character formatting as required */

		/*
		 * These really should return error if cbValueMax is not big
		 * enough.
		 */
		switch (field_type)
		{
			case PG_TYPE_DATE:
				len = SPRINTF_FIXED(midtemp, "%.4d-%.2d-%.2d", std_time.y, std_time.m, std_time.d);
				break;

			case PG_TYPE_TIME:
				len = SPRINTF_FIXED(midtemp, "%.2d:%.2d:%.2d", std_time.hh, std_time.mm, std_time.ss);
				if (std_time.fr > 0)
				{
					int	wdt;
					int	fr = effective_fraction(std_time.fr, &wdt);

					len = SPRINTFCAT_FIXED(midtemp, ".%0*d", wdt, fr);
				}
				break;

			case PG_TYPE_ABSTIME:
			case PG_TYPE_DATETIME:
			case PG_TYPE_TIMESTAMP_NO_TMZONE:
			case PG_TYPE_TIMESTAMP:
				len = stime2timestamp(&std_time, midtemp, midsize, FALSE,
									  (int) (midsize - 19 - 2) );
				break;

			case PG_TYPE_UUID:
				len = strlen(neut_str);
				for (i = 0; i < len && i < midsize - 2; i++)
					midtemp[i] = toupper((UCHAR) neut_str[i]);
				midtemp[i] = '\0';
				MYLOG(0, "PG_TYPE_UUID: rgbValueBindRow = '%s'\n", rgbValueBindRow);
				break;

				/*
				 * Currently, data is SILENTLY TRUNCATED for BYTEA and
				 * character data types if there is not enough room in
				 * cbValueMax because the driver can't handle multiple
				 * calls to SQLGetData for these, yet.	Most likely, the
				 * buffer passed in will be big enough to handle the
				 * maximum limit of postgres, anyway.
				 *
				 * LongVarBinary types are handled correctly above, observing
				 * truncation and all that stuff since there is
				 * essentially no limit on the large object used to store
				 * those.
				 */
			case PG_TYPE_BYTEA:/* convert binary data to hex strings
								 * (i.e, 255 = "FF") */

			default:
				pre_convert = FALSE;
		}
		if (pre_convert)
			neut_str = midtemp;
		result = convert_text_field_to_sql_c(gdata, stmt->current_col, neut_str, field_type, fCType, rgbValueBindRow, cbValueMax, conn, &len);
	}
	else
	{
		SQLGUID g;

		/*
		 * for SQL_C_CHAR, it's probably ok to leave currency symbols in.
		 * But to convert to numeric types, it is necessary to get rid of
		 * those.
		 */
		if (field_type == PG_TYPE_MONEY)
		{
			if (convert_money(neut_str, midtemp, sizeof(midtemp)))
				neut_str = midtemp;
			else
			{
				MYLOG(0, "couldn't convert money type to %d\n", fCType);
				return COPY_UNSUPPORTED_TYPE;
			}
		}

		switch (fCType)
		{
			case SQL_C_DATE:
			case SQL_C_TYPE_DATE:		/* 91 */
				len = 6;
				{
					DATE_STRUCT *ds;
					struct tm  *tim;

					if (bind_size > 0)
						ds = (DATE_STRUCT *) rgbValueBindRow;
					else
						ds = (DATE_STRUCT *) rgbValue + bind_row;

					/*
					 * Initialize date in case conversion destination
					 * expects date part from this source time data.
					 * A value may be partially set here, so do some
					 * sanity checks on the existing values before
					 * setting them.
					 */
					tim = SC_get_localtime(stmt);
					if (std_time.m == 0)
						std_time.m = tim->tm_mon + 1;
					if (std_time.d == 0)
						std_time.d = tim->tm_mday;
					if (std_time.y == 0)
						std_time.y = tim->tm_year + 1900;
					ds->year = std_time.y;
					ds->month = std_time.m;
					ds->day = std_time.d;
				}
				break;

			case SQL_C_TIME:
			case SQL_C_TYPE_TIME:		/* 92 */
				len = 6;
				{
					TIME_STRUCT *ts;

					if (bind_size > 0)
						ts = (TIME_STRUCT *) rgbValueBindRow;
					else
						ts = (TIME_STRUCT *) rgbValue + bind_row;
					ts->hour = std_time.hh;
					ts->minute = std_time.mm;
					ts->second = std_time.ss;
				}
				break;

			case SQL_C_TIMESTAMP:
			case SQL_C_TYPE_TIMESTAMP:	/* 93 */
				len = 16;
				{
					struct tm  *tim;
					TIMESTAMP_STRUCT *ts;

					if (bind_size > 0)
						ts = (TIMESTAMP_STRUCT *) rgbValueBindRow;
					else
						ts = (TIMESTAMP_STRUCT *) rgbValue + bind_row;

					/*
					 * Initialize date in case conversion destination
					 * expects date part from this source time data.
					 * A value may be partially set here, so do some
					 * sanity checks on the existing values before
					 * setting them.
					 */
					tim = SC_get_localtime(stmt);
					if (std_time.m == 0)
						std_time.m = tim->tm_mon + 1;
					if (std_time.d == 0)
						std_time.d = tim->tm_mday;
					if (std_time.y == 0)
						std_time.y = tim->tm_year + 1900;

					ts->year = std_time.y;
					ts->month = std_time.m;
					ts->day = std_time.d;
					ts->hour = std_time.hh;
					ts->minute = std_time.mm;
					ts->second = std_time.ss;
					ts->fraction = std_time.fr;
				}
				break;

			case SQL_C_BIT:
				len = 1;
				if (bind_size > 0)
					*((UCHAR *) rgbValueBindRow) = atoi(neut_str);
				else
					*((UCHAR *) rgbValue + bind_row) = atoi(neut_str);

				 MYLOG(99, "SQL_C_BIT: bind_row = " FORMAT_POSIROW " val = %d, cb = " FORMAT_LEN ", rgb=%d\n",
					bind_row, atoi(neut_str), cbValueMax, *((UCHAR *)rgbValue));
				break;

			case SQL_C_STINYINT:
			case SQL_C_TINYINT:
				len = 1;
				if (bind_size > 0)
					*((SCHAR *) rgbValueBindRow) = atoi(neut_str);
				else
					*((SCHAR *) rgbValue + bind_row) = atoi(neut_str);
				break;

			case SQL_C_UTINYINT:
				len = 1;
				if (bind_size > 0)
					*((UCHAR *) rgbValueBindRow) = atoi(neut_str);
				else
					*((UCHAR *) rgbValue + bind_row) = atoi(neut_str);
				break;

			case SQL_C_FLOAT:
				set_client_decimal_point((char *) neut_str);
				len = 4;
				if (bind_size > 0)
					*((SFLOAT *) rgbValueBindRow) = (float) get_double_value(neut_str);
				else
					*((SFLOAT *) rgbValue + bind_row) = (float) get_double_value(neut_str);
				break;

			case SQL_C_DOUBLE:
				set_client_decimal_point((char *) neut_str);
				len = 8;
				if (bind_size > 0)
					*((SDOUBLE *) rgbValueBindRow) = get_double_value(neut_str);
				else
					*((SDOUBLE *) rgbValue + bind_row) = get_double_value(neut_str);
				break;

			case SQL_C_NUMERIC:
				{
					SQL_NUMERIC_STRUCT      *ns;
					BOOL	overflowed;

					if (bind_size > 0)
						ns = (SQL_NUMERIC_STRUCT *) rgbValueBindRow;
					else
						ns = (SQL_NUMERIC_STRUCT *) rgbValue + bind_row;

					parse_to_numeric_struct(neut_str, ns, &overflowed);
					if (overflowed)
						result = COPY_RESULT_TRUNCATED;
				}
				break;

			case SQL_C_SSHORT:
			case SQL_C_SHORT:
				len = 2;
				if (bind_size > 0)
					*((SQLSMALLINT *) rgbValueBindRow) = atoi(neut_str);
				else
					*((SQLSMALLINT *) rgbValue + bind_row) = atoi(neut_str);
				break;

			case SQL_C_USHORT:
				len = 2;
				if (bind_size > 0)
					*((SQLUSMALLINT *) rgbValueBindRow) = atoi(neut_str);
				else
					*((SQLUSMALLINT *) rgbValue + bind_row) = atoi(neut_str);
				break;

			case SQL_C_SLONG:
			case SQL_C_LONG:
				len = 4;
				if (bind_size > 0)
					*((SQLINTEGER *) rgbValueBindRow) = atol(neut_str);
				else
					*((SQLINTEGER *) rgbValue + bind_row) = atol(neut_str);
				break;

			case SQL_C_ULONG:
				len = 4;
				if (bind_size > 0)
					*((SQLUINTEGER *) rgbValueBindRow) = ATOI32U(neut_str);
				else
					*((SQLUINTEGER *) rgbValue + bind_row) = ATOI32U(neut_str);
				break;

#ifdef ODBCINT64
			case SQL_C_SBIGINT:
				len = 8;
				if (bind_size > 0)
					*((SQLBIGINT *) rgbValueBindRow) = ATOI64(neut_str);
				else
					*((SQLBIGINT *) rgbValue + bind_row) = ATOI64(neut_str);
				break;

			case SQL_C_UBIGINT:
				len = 8;
				if (bind_size > 0)
					*((SQLUBIGINT *) rgbValueBindRow) = ATOI64U(neut_str);
				else
					*((SQLUBIGINT *) rgbValue + bind_row) = ATOI64U(neut_str);
				break;

#endif /* ODBCINT64 */
			case SQL_C_BINARY:
				/* The following is for SQL_C_VARBOOKMARK */
				if (PG_TYPE_INT4 == field_type)
				{
					UInt4	ival = ATOI32U(neut_str);

MYLOG(DETAIL_LOG_LEVEL, "SQL_C_VARBOOKMARK value=%d\n", ival);
					if (pcbValue)
						*pcbValueBindRow = sizeof(ival);
					if (cbValueMax >= sizeof(ival))
					{
						memcpy(rgbValueBindRow, &ival, sizeof(ival));
						return COPY_OK;
					}
					else
						return COPY_RESULT_TRUNCATED;
				}
				else if (PG_TYPE_UUID == field_type)
				{
					int rtn = char2guid(neut_str, &g);

					if (COPY_OK != rtn)
						return rtn;
					if (pcbValue)
						*pcbValueBindRow = sizeof(g);
					if (cbValueMax >= sizeof(g))
					{
						memcpy(rgbValueBindRow, &g, sizeof(g));
						return COPY_OK;
					}
					else
						return COPY_RESULT_TRUNCATED;
				}
				else
				{
					MYLOG(0, "couldn't convert the type %d to SQL_C_BINARY\n", field_type);
					return COPY_UNSUPPORTED_TYPE;
				}
				break;
			case SQL_C_GUID:

				result = char2guid(neut_str, &g);
				if (COPY_OK != result)
				{
					MYLOG(0, "Could not convert to SQL_C_GUID\n");
					return	COPY_UNSUPPORTED_TYPE;
				}
				len = sizeof(g);
				if (bind_size > 0)
					*((SQLGUID *) rgbValueBindRow) = g;
				else
					*((SQLGUID *) rgbValue + bind_row) = g;
				break;
			case SQL_C_INTERVAL_YEAR:
			case SQL_C_INTERVAL_MONTH:
			case SQL_C_INTERVAL_YEAR_TO_MONTH:
			case SQL_C_INTERVAL_DAY:
			case SQL_C_INTERVAL_HOUR:
			case SQL_C_INTERVAL_DAY_TO_HOUR:
			case SQL_C_INTERVAL_MINUTE:
			case SQL_C_INTERVAL_HOUR_TO_MINUTE:
			case SQL_C_INTERVAL_SECOND:
			case SQL_C_INTERVAL_DAY_TO_SECOND:
			case SQL_C_INTERVAL_HOUR_TO_SECOND:
			case SQL_C_INTERVAL_MINUTE_TO_SECOND:
				interval2istruct(fCType, precision, neut_str, bind_size > 0 ? (SQL_INTERVAL_STRUCT *) rgbValueBindRow : (SQL_INTERVAL_STRUCT *) rgbValue + bind_row);
				break;

			default:
				MYLOG(0, "conversion to the type %d isn't supported\n", fCType);
				return COPY_UNSUPPORTED_TYPE;
		}
	}

	/* store the length of what was copied, if there's a place for it */
	if (pcbValue)
		*pcbValueBindRow = len;

	if (result == COPY_OK && stmt->current_col >= 0)
		gdata->gdata[stmt->current_col].data_left = 0;
	return result;

}


/*--------------------------------------------------------------------
 *	Functions/Macros to get rid of query size limit.
 *
 *	I always used the follwoing macros to convert from
 *	old_statement to new_statement.  Please improve it
 *	if you have a better way.	Hiroshi 2001/05/22
 *--------------------------------------------------------------------
 */

#define	FLGP_USING_CURSOR	(1L << 1)
#define	FLGP_SELECT_INTO		(1L << 2)
#define	FLGP_SELECT_FOR_UPDATE_OR_SHARE	(1L << 3)
#define	FLGP_MULTIPLE_STATEMENT	(1L << 5)
#define	FLGP_SELECT_FOR_READONLY	(1L << 6)
typedef struct _QueryParse {
	const char	*statement;
	int		statement_type;
	size_t		opos;
	ssize_t		from_pos;
	ssize_t		where_pos;
	ssize_t		stmt_len;
	int		in_status;
	char		escape_in_literal, prev_token_end;
	const	char *dollar_tag;
	ssize_t		taglen;
	char		token_curr[64];
	int		token_len;
	size_t		declare_pos;
	UInt4		flags, comment_level;
	encoded_str	encstr;
}	QueryParse;

static void
QP_initialize(QueryParse *q, const StatementClass *stmt)
{
	q->statement = stmt->statement;
	q->statement_type = stmt->statement_type;
	q->opos = 0;
	q->from_pos = -1;
	q->where_pos = -1;
	q->stmt_len = (q->statement) ? strlen(q->statement) : -1;
	q->in_status = 0;
	q->escape_in_literal = '\0';
	q->dollar_tag = NULL;
	q->taglen = -1;
	q->token_curr[0] = '\0';
	q->token_len = 0;
	q->prev_token_end = TRUE;
	q->declare_pos = 0;
	q->flags = 0;
	q->comment_level = 0;
	make_encoded_str(&q->encstr, SC_get_conn(stmt), q->statement);
}

enum {
	QP_IN_IDENT_KEYWORD = 1L	/* identifier or keyword */
	, QP_IN_DQUOTE_IDENTIFIER = (1L << 1) /* "" */
	, QP_IN_LITERAL = (1L << 2)	/* '' */
	, QP_IN_ESCAPE = (1L << 3)	/* \ in literal */
	, QP_IN_DOLLAR_QUOTE = (1L << 4) /* $...$    $...$ */
	, QP_IN_COMMENT_BLOCK = (1L << 5)	 /* slash asterisk */
	, QP_IN_LINE_COMMENT = (1L << 6) /* -- */
};

#define	QP_in_idle_status(qp)	((qp)->in_status == 0)

#define	QP_is_in(qp, status)	(((qp)->in_status & status) != 0)
#define	QP_enter(qp, status)	((qp)->in_status |= status)
#define	QP_exit(qp, status)	((qp)->in_status &= (~status))

/*
 * ResolveOneParam can be work in these four modes:
 *
 * RPM_REPLACE_PARAMS
 *      Replace parameter markers with their values.
 *
 * RPM_FAKE_PARAMS
 *      The query is going to be sent to the server only to be able to
 *      Describe the result set. Parameter markers are replaced with NULL
 *      literals if we don't know their real values yet.
 *
 * RPM_BUILDING_PREPARE_STATEMENT
 *      Building a query suitable for PREPARE statement, or server-side Parse.
 *      Parameter markers are replaced with $n-style parameter markers.
 *
 * RPM_BUILDING_BIND_REQUEST
 *      Building the actual parameter values to send to the server in a Bind
 *      request. Return an unescaped value that will be accepted by the
 *      server's input function.
 */
typedef enum
{
	RPM_REPLACE_PARAMS,
	RPM_FAKE_PARAMS,
	RPM_BUILDING_PREPARE_STATEMENT,
	RPM_BUILDING_BIND_REQUEST
} ResolveParamMode;

#define	FLGB_INACCURATE_RESULT	(1L << 4)
#define	FLGB_CREATE_KEYSET	(1L << 5)
#define	FLGB_KEYSET_DRIVEN	(1L << 6)
#define	FLGB_CONVERT_LF		(1L << 7)
#define	FLGB_DISCARD_OUTPUT	(1L << 8)
#define	FLGB_BINARY_AS_POSSIBLE	(1L << 9)
#define	FLGB_LITERAL_EXTENSION	(1L << 10)
#define	FLGB_HEX_BIN_FORMAT	(1L << 11)
#define	FLGB_PARAM_CAST		(1L << 12)
typedef struct _QueryBuild {
	char   *query_statement;
	size_t	str_alsize;
	size_t	npos;
	SQLLEN	current_row;
	Int2	param_number;
	Int2	dollar_number;
	Int2	num_io_params;
	Int2	num_output_params;
	Int2	num_discard_params;
	Int2	proc_return;
	Int2	brace_level;
	char	parenthesize_the_first;
	APDFields *apdopts;
	IPDFields *ipdopts;
	PutDataInfo *pdata;
	size_t	load_stmt_len;
	size_t	load_from_pos;
	ResolveParamMode param_mode;
	UInt4	flags;
	int	ccsc;
	int	errornumber;
	const char *errormsg;

	ConnectionClass	*conn; /* mainly needed for LO handling */
	StatementClass	*stmt; /* needed to set error info in ENLARGE_.. */
}	QueryBuild;

#define INIT_MIN_ALLOC	4096
static ssize_t
QB_initialize(QueryBuild *qb, size_t size, StatementClass *stmt, ResolveParamMode param_mode)
{
	size_t	newsize = 0;

	qb->param_mode = param_mode;
	qb->flags = 0;
	qb->load_stmt_len = 0;
	qb->load_from_pos = 0;
	qb->stmt = stmt;
	qb->apdopts = NULL;
	qb->ipdopts = NULL;
	qb->pdata = NULL;
	qb->proc_return = 0;
	qb->num_io_params = 0;
	qb->num_output_params = 0;
	qb->num_discard_params = 0;
	qb->brace_level = 0;
	qb->parenthesize_the_first = FALSE;

	/* Copy options from statement */
	qb->apdopts = SC_get_APDF(stmt);
	qb->ipdopts = SC_get_IPDF(stmt);
	qb->pdata = SC_get_PDTI(stmt);
	qb->conn = SC_get_conn(stmt);
	if (stmt->discard_output_params)
		qb->flags |= FLGB_DISCARD_OUTPUT;
	qb->num_io_params = CountParameters(stmt, NULL, NULL, &qb->num_output_params);
	qb->proc_return = stmt->proc_return;
	if (0 != (qb->flags & FLGB_DISCARD_OUTPUT))
		qb->num_discard_params = qb->num_output_params;
	if (qb->num_discard_params < qb->proc_return)
		qb->num_discard_params = qb->proc_return;

	/* Copy options from connection */
	if (qb->conn->connInfo.lf_conversion)
		qb->flags |= FLGB_CONVERT_LF;
	qb->ccsc = qb->conn->ccsc;
	if (CC_get_escape(qb->conn) &&
	    PG_VERSION_GE(qb->conn, 8.1))
		qb->flags |= FLGB_LITERAL_EXTENSION;
	if (PG_VERSION_GE(qb->conn, 9.0))
		qb->flags |= FLGB_HEX_BIN_FORMAT;

	newsize = INIT_MIN_ALLOC;
	while (newsize <= size)
		newsize *= 2;

	if ((qb->query_statement = malloc(newsize)) == NULL)
	{
		qb->str_alsize = 0;
		return -1;
	}
	qb->query_statement[0] = '\0';
	qb->str_alsize = newsize;
	qb->npos = 0;
	qb->current_row = stmt->exec_current_row < 0 ? 0 : stmt->exec_current_row;
	qb->param_number = -1;
	qb->dollar_number = 0;
	qb->errornumber = 0;
	qb->errormsg = NULL;

	return newsize;
}

static int
QB_initialize_copy(QueryBuild *qb_to, const QueryBuild *qb_from, UInt4 size)
{
	memcpy(qb_to, qb_from, sizeof(QueryBuild));

	if ((qb_to->query_statement = malloc(size)) == NULL)
	{
		qb_to->str_alsize = 0;
		return -1;
	}
	qb_to->query_statement[0] = '\0';
	qb_to->str_alsize = size;
	qb_to->npos = 0;

	return size;
}

static void
QB_replace_SC_error(StatementClass *stmt, const QueryBuild *qb, const char *func)
{
	int	number;

	if (0 == qb->errornumber)	return;
	if ((number = SC_get_errornumber(stmt)) > 0) return;
	if (number < 0 && qb->errornumber < 0)	return;
	SC_set_error(stmt, qb->errornumber, qb->errormsg, func);
}

static void
QB_Destructor(QueryBuild *qb)
{
	if (qb->query_statement)
	{
		free(qb->query_statement);
		qb->query_statement = NULL;
		qb->str_alsize = 0;
	}
}

/*
 * New macros (Aceto)
 *--------------------
 */

#define F_OldChar(qp) ((qp)->statement[(qp)->opos])

#define F_OldPtr(qp) ((qp)->statement + (qp)->opos)

#define F_OldNext(qp) (++(qp)->opos)

#define F_OldPrior(qp) (--(qp)->opos)

#define F_OldPos(qp) (qp)->opos

#define F_ExtractOldTo(qp, buf, ch, maxsize) \
do { \
	size_t	c = 0; \
	while ((qp)->statement[qp->opos] != '\0' && (qp)->statement[qp->opos] != ch) \
	{ \
		if (c >= maxsize) \
			break; \
		buf[c++] = (qp)->statement[qp->opos++];	\
	} \
	if ((qp)->statement[qp->opos] == '\0')	\
		{retval = SQL_ERROR; goto cleanup;} \
	buf[c] = '\0'; \
} while (0)

#define F_NewChar(qb) (qb->query_statement[(qb)->npos])

#define F_NewPtr(qb) ((qb)->query_statement + (qb)->npos)

#define F_NewNext(qb) (++(qb)->npos)

#define F_NewPos(qb) ((qb)->npos)


static int
convert_escape(QueryParse *qp, QueryBuild *qb);
static int
inner_process_tokens(QueryParse *qp, QueryBuild *qb);
static int
ResolveOneParam(QueryBuild *qb, QueryParse *qp, BOOL *isnull, BOOL *usebinary,
				Oid *pgType);
static int
processParameters(QueryParse *qp, QueryBuild *qb,
	size_t *output_count, SQLLEN param_pos[][2]);
static size_t
convert_to_pgbinary(const char *in, char *out, size_t len, QueryBuild *qb);
static BOOL convert_special_chars(QueryBuild *qb, const char *si, size_t used);

/*
 * Enlarge qb->query_statement buffer so that it is at least newsize + 1
 * in size.
 */
static ssize_t
enlarge_query_statement(QueryBuild *qb, size_t newsize)
{
	size_t	newalsize = INIT_MIN_ALLOC;
	CSTR func = "enlarge_statement";

	while (newalsize <= newsize)
		newalsize *= 2;
	if (!(qb->query_statement = realloc(qb->query_statement, newalsize)))
	{
		qb->str_alsize = 0;
		if (qb->stmt)
		{
			SC_set_error(qb->stmt, STMT_EXEC_ERROR, "Query buffer allocate error in copy_statement_with_parameters", func);
		}
		else
		{
			qb->errormsg = "Query buffer allocate error in copy_statement_with_parameters";
			qb->errornumber = STMT_EXEC_ERROR;
		}
		return 0;
	}
	qb->str_alsize = newalsize;
	return newalsize;
}

/*----------
 *	Enlarge stmt_with_params if necessary.
 *----------
 */
#define ENLARGE_NEWSTATEMENT(qb, newpos)					\
	do {													\
		if ((newpos) >= (qb)->str_alsize)					\
		{													\
			if (enlarge_query_statement(qb, newpos) <= 0)	\
			{												\
				retval = SQL_ERROR;							\
				goto cleanup;								\
			}												\
		}													\
	} while(0)

/*----------
 *	Terminate the stmt_with_params string with NULL.
 *----------
 */
#define CVT_TERMINATE(qb) 									\
	do {													\
		if (NULL == (qb)->query_statement)					\
		{													\
			retval = SQL_ERROR;								\
			goto cleanup;									\
		}													\
		(qb)->query_statement[(qb)->npos] = '\0';			\
	} while (0)

/*----------
 *	Append a data.
 *----------
 */
#define CVT_APPEND_DATA(qb, s, len)							\
	do {													\
		size_t	newpos = (qb)->npos + len;					\
		ENLARGE_NEWSTATEMENT((qb), newpos);					\
		memcpy(&(qb)->query_statement[(qb)->npos], s, len);	\
		(qb)->npos = newpos;								\
		(qb)->query_statement[newpos] = '\0';				\
	} while (0)

/*----------
 *	Append a string.
 *----------
 */
#define CVT_APPEND_STR(qb, s)								\
	do {													\
		size_t	len = strlen(s);							\
		CVT_APPEND_DATA(qb, s, len);						\
	} while (0)

/*----------
 *	Append a char.
 *----------
 */
#define CVT_APPEND_CHAR(qb, c)								\
	do {													\
		ENLARGE_NEWSTATEMENT(qb, (qb)->npos + 1);			\
		(qb)->query_statement[(qb)->npos++] = c;			\
	} while (0)

/*----------
 *	Append a binary data.
 *	Newly required size may be overestimated currently.
 *----------
 */
#define CVT_APPEND_BINARY(qb, buf, used)					\
	do {													\
		size_t	newlimit;									\
															\
		newlimit = (qb)->npos;								\
		if (qb->flags & FLGB_HEX_BIN_FORMAT)				\
			newlimit += 2 * used + 4;						\
		else												\
			newlimit += 5 * used;							\
		ENLARGE_NEWSTATEMENT(qb, newlimit);					\
		(qb)->npos += convert_to_pgbinary(buf,				\
										  &qb->query_statement[(qb)->npos], \
										  used, qb);		\
	} while (0)

/*----------
 *
 *----------
 */
#define CVT_SPECIAL_CHARS(qb, buf, used)					\
	do {													\
		if (!convert_special_chars(qb, buf, used))			\
		{													\
			retval = SQL_ERROR;								\
			goto cleanup;									\
		}													\
	} while (0)


static RETCODE
QB_start_brace(QueryBuild *qb)
{
	BOOL	replace_by_parenthesis = TRUE;
	RETCODE	retval = SQL_ERROR;

	if (0 == qb->brace_level)
	{
		if (0 == F_NewPos(qb))
		{
			qb->parenthesize_the_first = FALSE;
			replace_by_parenthesis = FALSE;
		}
		else
			qb->parenthesize_the_first = TRUE;
	}
	if (replace_by_parenthesis)
		CVT_APPEND_CHAR(qb, '(');
	qb->brace_level++;
	retval = SQL_SUCCESS;
cleanup:
	return retval;
}

static RETCODE
QB_end_brace(QueryBuild *qb)
{
	BOOL	replace_by_parenthesis = TRUE;
	RETCODE	retval = SQL_ERROR;

	if (qb->brace_level <= 1 &&
	    !qb->parenthesize_the_first)
		replace_by_parenthesis = FALSE;
	if (replace_by_parenthesis)
		CVT_APPEND_CHAR(qb, ')');
	qb->brace_level--;
	retval = SQL_SUCCESS;
cleanup:
	return retval;
}

static RETCODE QB_append_space_to_separate_identifiers(QueryBuild *qb, const QueryParse *qp)
{
	unsigned char	tchar = F_OldChar(qp);
	encoded_str	encstr;
	BOOL		add_space = FALSE;
	RETCODE		retval = SQL_ERROR;

	if (ODBC_ESCAPE_END != tchar)
		return SQL_SUCCESS;
	encoded_str_constr(&encstr, qb->ccsc, F_OldPtr(qp) + 1);
	tchar = encoded_nextchar(&encstr);
	if (MBCS_NON_ASCII(encstr))
		add_space = TRUE;
	else
	{
		if (isalnum(tchar))
			add_space = TRUE;
		else
		{
			switch (tchar)
			{
				case '_':
				case '$':
					add_space = TRUE;
			}
		}
	}
	if (add_space)
		CVT_APPEND_CHAR(qb, ' ');
	retval = SQL_SUCCESS;
cleanup:

	return retval;
}

/*----------
 *	Check if the statement is
 *	SELECT ... INTO table FROM .....
 *	This isn't really a strict check but ...
 *----------
 */
static BOOL
into_table_from(const char *stmt)
{
	if (strnicmp(stmt, "into", 4))
		return FALSE;
	stmt += 4;
	while (isspace((UCHAR) *stmt)) stmt++;
	switch (*stmt)
	{
		case '\0':
		case ',':
		case LITERAL_QUOTE:
		case DOLLAR_QUOTE:
			return FALSE;
		case '-':
		case '/':
			return TRUE;
		case IDENTIFIER_QUOTE:	/* double quoted table name ? */
			do
			{
				do {
					++stmt;
				} while (*stmt != IDENTIFIER_QUOTE && *stmt);
				if (*stmt)
					stmt++;
			}
			while (*stmt == IDENTIFIER_QUOTE);
			break;
		default:
			while (IS_NOT_SPACE(*stmt)) stmt++;
			break;
	}
	if (!*stmt)
		return FALSE;
	while (isspace((UCHAR) *stmt)) stmt++;
	if ('/' == *stmt ||
	    '-' == *stmt)
		return TRUE;
	if (strnicmp(stmt, "from", 4))
		return FALSE;
	return TRUE;
}

/*----------
 *	Check if the statement is
 *	SELECT ... FOR UPDATE .....
 *	This isn't really a strict check but ...
 *----------
 */
static UInt4
table_for_update_or_share(const char *stmt, size_t *endpos)
{
	const char *wstmt = stmt;
	int	advance;
	UInt4	flag = 0, zeroflag = 0;

	while (isspace((UCHAR) *wstmt)) wstmt++;
	if (!*wstmt)
		return 0;
	if (0 == strnicmp(wstmt, "update", advance = 6))
		flag |= FLGP_SELECT_FOR_UPDATE_OR_SHARE;
	else if (0 == strnicmp(wstmt, "share", advance = 5))
		flag |= FLGP_SELECT_FOR_UPDATE_OR_SHARE;
	else if (0 == strnicmp(wstmt, "read", advance = 4))
		flag |= FLGP_SELECT_FOR_READONLY;
	else
	{
		flag |= FLGP_SELECT_FOR_UPDATE_OR_SHARE; /* maybe */
		return flag;
	}
	zeroflag = flag; /* returns flag anyway */
	wstmt += advance;
	if (IS_NOT_SPACE(wstmt[0]))
		return zeroflag;
	else if (0 != (flag & FLGP_SELECT_FOR_READONLY))
	{
		if (IS_NOT_SPACE(wstmt[0]))
			return zeroflag;
		while (isspace((UCHAR) *wstmt)) wstmt++;
		if (!*wstmt)
			return zeroflag;
		if (0 != strnicmp(wstmt, "only", advance = 4))
			return zeroflag;
		wstmt += advance;
	}
	if (IS_NOT_SPACE(wstmt[0]))
		return zeroflag;
	*endpos = wstmt - stmt;
	return flag;
}

/*----------
 *	Check if the statement has OUTER JOIN
 *	This isn't really a strict check but ...
 *----------
 */
static BOOL
check_join(StatementClass *stmt, const char *curptr, size_t curpos)
{
	const char *wstmt;
	ssize_t	stapos, endpos, tokenwd;
	const int	backstep = 4;
	BOOL	outerj = TRUE;

	for (endpos = curpos, wstmt = curptr; endpos >= 0 && isspace((UCHAR) *wstmt); endpos--, wstmt--)
		;
	if (endpos < 0)
		return FALSE;
	for (endpos -= backstep, wstmt -= backstep; endpos >= 0 && isspace((UCHAR) *wstmt); endpos--, wstmt--)
		;
	if (endpos < 0)
		return FALSE;
	for (stapos = endpos; stapos >= 0 && IS_NOT_SPACE(*wstmt); stapos--, wstmt--)
		;
	if (stapos < 0 || 0 == *wstmt)
		return FALSE;
	wstmt++;
	switch (tokenwd = endpos - stapos)
	{
		case 4:
			if (strnicmp(wstmt, "FULL", tokenwd) == 0 ||
			    strnicmp(wstmt, "LEFT", tokenwd) == 0)
				break;
			return FALSE;
		case 5:
			if (strnicmp(wstmt, "OUTER", tokenwd) == 0 ||
			    strnicmp(wstmt, "RIGHT", tokenwd) == 0)
				break;
			if (strnicmp(wstmt, "INNER", tokenwd) == 0 ||
			    strnicmp(wstmt, "CROSS", tokenwd) == 0)
			{
				outerj = FALSE;
				break;
			}
			return FALSE;
		default:
			return FALSE;
	}
	if (stmt)
	{
		if (outerj)
			SC_set_outer_join(stmt);
		else
			SC_set_inner_join(stmt);
	}
	return TRUE;
}

/*----------
 *	Check if the statement is
 *	INSERT INTO ... () VALUES ()
 *	This isn't really a strict check but ...
 *----------
 */
static BOOL
insert_without_target(const char *stmt, size_t *endpos)
{
	const char *wstmt = stmt;

	while (isspace((UCHAR) *wstmt)) wstmt++;
	if (!*wstmt)
		return FALSE;
	if (strnicmp(wstmt, "VALUES", 6))
		return FALSE;
	wstmt += 6;
	if (!wstmt[0] || IS_NOT_SPACE(wstmt[0]))
		return FALSE;
	while (isspace((UCHAR) *wstmt)) wstmt++;
	if (*wstmt != '(' || *(++wstmt) != ')')
		return FALSE;
	wstmt++;
	*endpos = wstmt - stmt;
	return !wstmt[0] || isspace((UCHAR) wstmt[0])
		|| ';' == wstmt[0];
}

static ProcessedStmt *
buildProcessedStmt(const char *srvquery, ssize_t endp, int num_params)
{
	ProcessedStmt *pstmt;
	size_t		qlen;

	qlen = (endp == SQL_NTS) ? strlen(srvquery) : endp;

	pstmt = malloc(sizeof(ProcessedStmt));
	if (!pstmt)
		return NULL;

	pstmt->next = NULL;
	pstmt->query = malloc(qlen + 1);
	if (!pstmt->query)
	{
		free(pstmt);
		return NULL;
	}
	memcpy(pstmt->query, srvquery, qlen);
	pstmt->query[qlen] = '\0';
	pstmt->num_params = num_params;

	return pstmt;
}

/*
 * Process the original SQL query for execution using server-side prepared
 * statements.
 *
 * Split a possible multi-statement query into parts, and replace ?-style
 * parameter markers with $n. The resulting queries are stored in a linked
 * list in stmt->processed_statements.
 *
 * If 'fake_params' is true, we will replace ?-style parameter markers with
 * fake parameter values instead. This is used when a query's result columns
 * have to be described (SQLPrepare+SQLDescribeCol) before executing the
 * query, in UseServerSidePrepare=0 mode.
 */
RETCODE
prepareParametersNoDesc(StatementClass *stmt, BOOL fake_params, BOOL param_cast)
{
	CSTR		func = "process_statements";
	RETCODE		retval;
	ConnectionClass *conn = SC_get_conn(stmt);
	char		plan_name[32];
	po_ind_t	multi;
	const char	*orgquery = NULL, *srvquery = NULL;
	ssize_t		endp1, endp2;
	SQLSMALLINT	num_pa = 0, num_p1, num_p2;
	ProcessedStmt *pstmt;
	ProcessedStmt *last_pstmt;
	QueryParse	query_org, *qp;
	QueryBuild	query_crt, *qb;

MYLOG(DETAIL_LOG_LEVEL, "entering\n");
	qp = &query_org;
	QP_initialize(qp, stmt);
	qb = &query_crt;
	if (QB_initialize(qb, qp->stmt_len, stmt,
					  fake_params ? RPM_FAKE_PARAMS : RPM_BUILDING_PREPARE_STATEMENT) < 0)
	{
		SC_set_errornumber(stmt, STMT_NO_MEMORY_ERROR);
		return SQL_ERROR;
	}
	if (param_cast)
		qb->flags |= FLGB_PARAM_CAST;

	for (qp->opos = 0; qp->opos < qp->stmt_len; qp->opos++)
	{
		retval = inner_process_tokens(qp, qb);
		if (SQL_ERROR == retval)
		{
			QB_replace_SC_error(stmt, qb, func);
			QB_Destructor(qb);
			return retval;
		}
	}
	CVT_TERMINATE(qb);

	retval = SQL_ERROR;
#define	return	DONT_CALL_RETURN_FROM_HERE???
	if (NAMED_PARSE_REQUEST == SC_get_prepare_method(stmt))
		SPRINTF_FIXED(plan_name, "_PLAN%p", stmt);
	else
		plan_name[0] = '\0';

	stmt->current_exec_param = 0;
	multi = stmt->multi_statement;
	orgquery = stmt->statement;
	srvquery = qb->query_statement;

	SC_scanQueryAndCountParams(orgquery, conn, &endp1, &num_p1, &multi, NULL);
	SC_scanQueryAndCountParams(srvquery, conn, &endp2, NULL, NULL, NULL);
	MYLOG(0, "parsed for the first command length=" FORMAT_SSIZE_T "(" FORMAT_SSIZE_T ") num_p=%d\n", endp2, endp1, num_p1);
	pstmt = buildProcessedStmt(srvquery,
							   endp2 < 0 ? SQL_NTS : endp2,
							   fake_params ? 0 : num_p1);
	if (!pstmt)
	{
		SC_set_errornumber(stmt, STMT_NO_MEMORY_ERROR);
		goto cleanup;
	}
	stmt->processed_statements = last_pstmt = pstmt;
	while (multi > 0)
	{
		orgquery += (endp1 + 1);
		srvquery += (endp2 + 1);
		num_pa += num_p1;
		SC_scanQueryAndCountParams(orgquery, conn, &endp1, &num_p1, &multi, NULL);
		SC_scanQueryAndCountParams(srvquery, conn, &endp2, &num_p2, NULL, NULL);
		MYLOG(0, "parsed for the subsequent command length=" FORMAT_SSIZE_T "(" FORMAT_SSIZE_T ") num_p=%d\n", endp2, endp1, num_p1);
		pstmt = buildProcessedStmt(srvquery,
								   endp2 < 0 ? SQL_NTS : endp2,
								   fake_params ? 0 : num_p1);
		if (!pstmt)
		{
			SC_set_errornumber(stmt, STMT_NO_MEMORY_ERROR);
			goto cleanup;
		}
		last_pstmt->next = pstmt;
		last_pstmt = pstmt;
	}

	SC_set_planname(stmt, plan_name);
	SC_set_prepared(stmt, plan_name[0] ? PREPARING_PERMANENTLY : PREPARING_TEMPORARILY);

	retval = SQL_SUCCESS;
cleanup:
#undef	return
	stmt->current_exec_param = -1;
	QB_Destructor(qb);
	return retval;
}

/*
 * Describe the parameters and portal for given query.
 */
static RETCODE
desc_params_and_sync(StatementClass *stmt)
{
	CSTR		func = "desc_params_and_sync";
	RETCODE		retval;
	ConnectionClass *conn = SC_get_conn(stmt);
	QResultClass	*res;
	char	   *plan_name;
	int		func_cs_count = 0;
	SQLSMALLINT	num_pa = 0;
	ProcessedStmt *pstmt;

MYLOG(DETAIL_LOG_LEVEL, "entering\n");

	retval = SQL_ERROR;
#define	return	DONT_CALL_RETURN_FROM_HERE???
	ENTER_INNER_CONN_CS(conn, func_cs_count);

	plan_name = stmt->plan_name ? stmt->plan_name : "";

	pstmt = stmt->processed_statements;

	stmt->current_exec_param = 0;
	res = ParseAndDescribeWithLibpq(stmt, plan_name, pstmt->query, pstmt->num_params, "prepare_and_describe", NULL);
	if (res == NULL)
		goto cleanup;
	// SC_set_Result(stmt, res);
	QR_Destructor(stmt->parsed);
	stmt->parsed = res;
	if (!QR_command_maybe_successful(res))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "Error while preparing parameters", func);
		goto cleanup;
	}
	num_pa = pstmt->num_params;
	for (pstmt = pstmt->next; pstmt; pstmt = pstmt->next)
	{
		if (pstmt->num_params > 0)
		{
			stmt->current_exec_param = num_pa;

			res = ParseAndDescribeWithLibpq(stmt, plan_name, pstmt->query, pstmt->num_params, "prepare_and_describe", NULL);
			if (res == NULL)
				goto cleanup;
			QR_Destructor(res);
			num_pa += pstmt->num_params;
		}
	}
	retval = SQL_SUCCESS;
cleanup:
#undef	return
	CLEANUP_FUNC_CONN_CS(func_cs_count, conn);
	stmt->current_exec_param = -1;
	return retval;
}

/*
 * Process the original SQL query, and and ask the server describe the
 * parameters.
 */
RETCODE	prepareParameters(StatementClass *stmt, BOOL fake_params)
{
	ConnectionClass *conn = SC_get_conn(stmt);

	switch (stmt->prepared)
	{
		case PREPARED_TEMPORARILY:
			if (conn->unnamed_prepared_stmt == stmt)
				return SQL_SUCCESS;
			else
				break;
		case NOT_YET_PREPARED:
		case PREPARING_PERMANENTLY:
		case PREPARING_TEMPORARILY:
			break;
		default:
			return SQL_SUCCESS;
	}

MYLOG(DETAIL_LOG_LEVEL, "calling prepareParameters\n");

	if (prepareParametersNoDesc(stmt, fake_params, PARSE_PARAM_CAST) == SQL_ERROR)
		return SQL_ERROR;
	return desc_params_and_sync(stmt);
}

/*
 *	This function inserts parameters into an SQL statements.
 *	It will also modify a SELECT statement for use with declare/fetch cursors.
 *	This function does a dynamic memory allocation to get rid of query size limit!
 */
int
copy_statement_with_parameters(StatementClass *stmt, BOOL buildPrepareStatement)
{
	CSTR		func = "copy_statement_with_parameters";
	RETCODE		retval;
	QueryParse	query_org, *qp;
	QueryBuild	query_crt, *qb;

	char	   *new_statement;

	ConnectionClass *conn = SC_get_conn(stmt);
	ConnInfo   *ci = &(conn->connInfo);
	const		char *bestitem = NULL;

MYLOG(DETAIL_LOG_LEVEL, "entering prepared=%d\n", stmt->prepared);
	if (!stmt->statement)
	{
		SC_set_error(stmt, STMT_INTERNAL_ERROR, "No statement string", func);
		return SQL_ERROR;
	}

	qp = &query_org;
	QP_initialize(qp, stmt);

	if (stmt->statement_type != STMT_TYPE_SELECT)
	{
		stmt->options.cursor_type = SQL_CURSOR_FORWARD_ONLY;
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
	}
	else if (stmt->options.cursor_type == SQL_CURSOR_FORWARD_ONLY)
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
	else if (stmt->options.scroll_concurrency != SQL_CONCUR_READ_ONLY)
	{
		if (SQL_CURSOR_DYNAMIC == stmt->options.cursor_type &&
		    0 == (ci->updatable_cursors & ALLOW_DYNAMIC_CURSORS))
			stmt->options.cursor_type = SQL_CURSOR_KEYSET_DRIVEN;
		if (SQL_CURSOR_KEYSET_DRIVEN == stmt->options.cursor_type &&
		    0 == (ci->updatable_cursors & ALLOW_KEYSET_DRIVEN_CURSORS))
			stmt->options.cursor_type = SQL_CURSOR_STATIC;
		switch (stmt->options.cursor_type)
		{
			case SQL_CURSOR_DYNAMIC:
			case SQL_CURSOR_KEYSET_DRIVEN:
				if (SC_update_not_ready(stmt))
					parse_statement(stmt, TRUE);
				if (SC_is_updatable(stmt) && stmt->ntab > 0)
				{
					if (bestitem = GET_NAME(stmt->ti[0]->bestitem), NULL == bestitem)
						stmt->options.cursor_type = SQL_CURSOR_STATIC;
				}
				break;
		}
		if (SQL_CURSOR_STATIC == stmt->options.cursor_type)
		{
			if (0 == (ci->updatable_cursors & ALLOW_STATIC_CURSORS))
				stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
			else if (SC_update_not_ready(stmt))
				parse_statement(stmt, TRUE);
		}
		if (SC_parsed_status(stmt) == STMT_PARSE_FATAL)
		{
			stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
			if (stmt->options.cursor_type == SQL_CURSOR_KEYSET_DRIVEN)
				stmt->options.cursor_type = SQL_CURSOR_STATIC;
		}
		else if (!stmt->updatable)
		{
			stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
			stmt->options.cursor_type = SQL_CURSOR_STATIC;
		}
		else
		{
			qp->from_pos = stmt->from_pos;
			qp->where_pos = stmt->where_pos;
		}
MYLOG(DETAIL_LOG_LEVEL, "type=" FORMAT_UINTEGER " concur=" FORMAT_UINTEGER "\n", stmt->options.cursor_type, stmt->options.scroll_concurrency);
	}

	SC_miscinfo_clear(stmt);
	/* If the application hasn't set a cursor name, then generate one */
	if (!SC_cursor_is_valid(stmt))
	{
		char	curname[32];

		SPRINTF_FIXED(curname, "SQL_CUR%p", stmt);
		STRX_TO_NAME(stmt->cursor_name, curname);
	}
	if (stmt->stmt_with_params)
	{
		free(stmt->stmt_with_params);
		stmt->stmt_with_params = NULL;
	}

	SC_no_fetchcursor(stmt);
	qb = &query_crt;
	qb->query_statement = NULL;
	if (PREPARED_PERMANENTLY == stmt->prepared)
	{
		/* already prepared */
		retval = SQL_SUCCESS;
		goto cleanup;
	}

	/*
	 * If it's a simple read-only cursor, we use extended query protocol to
	 * Parse it.
	 */
	if (buildPrepareStatement &&
		 SQL_CONCUR_READ_ONLY == stmt->options.scroll_concurrency)
	{
		/* Nothing to do here. It will be prepared before execution. */
		char		plan_name[32];
		if (NAMED_PARSE_REQUEST == SC_get_prepare_method(stmt))
			SPRINTF_FIXED(plan_name, "_PLAN%p", stmt);
		else
			plan_name[0] = '\0';

		SC_set_planname(stmt, plan_name);
		SC_set_prepared(stmt, plan_name[0] ? PREPARING_PERMANENTLY : PREPARING_TEMPORARILY);

		retval = SQL_SUCCESS;

		goto cleanup;
	}

	/* Otherwise... */
	if (QB_initialize(qb, qp->stmt_len, stmt, RPM_REPLACE_PARAMS) < 0)
	{
		retval = SQL_ERROR;
		goto cleanup;
	}
	if (SIMPLE_PARAM_CAST)
		qb->flags |= FLGB_PARAM_CAST;
	new_statement = qb->query_statement;

	/* For selects, prepend a declare cursor to the statement */
	if (SC_may_use_cursor(stmt) && stmt->external)
	{
		const char *opt_scroll = NULL_STRING, *opt_hold = NULL_STRING;

		if (ci->drivers.use_declarefetch
			 /** && SQL_CONCUR_READ_ONLY == stmt->options.scroll_concurrency **/
			)
		{
			SC_set_fetchcursor(stmt);
			if (SC_is_with_hold(stmt))
				opt_hold = " with hold";
			if (SQL_CURSOR_FORWARD_ONLY != stmt->options.cursor_type)
				opt_scroll = " scroll";
		}
		if (SC_is_fetchcursor(stmt))
		{
			snprintfcat(new_statement, qb->str_alsize, 
				"declare \"%s\"%s cursor%s for ",
				SC_cursor_name(stmt), opt_scroll, opt_hold);
			qb->npos = strlen(new_statement);
			qp->flags |= FLGP_USING_CURSOR;
			qp->declare_pos = qb->npos;
		}
		if (SQL_CONCUR_READ_ONLY != stmt->options.scroll_concurrency)
		{
			qb->flags |= FLGB_CREATE_KEYSET;
			if (SQL_CURSOR_KEYSET_DRIVEN == stmt->options.cursor_type)
				qb->flags |= FLGB_KEYSET_DRIVEN;
		}
	}

	for (qp->opos = 0; qp->opos < qp->stmt_len; qp->opos++)
	{
		retval = inner_process_tokens(qp, qb);
		if (SQL_ERROR == retval)
		{
			QB_replace_SC_error(stmt, qb, func);
			QB_Destructor(qb);
			return retval;
		}
	}
	/* make sure new_statement is always null-terminated */
	CVT_TERMINATE(qb);

	new_statement = qb->query_statement;
	stmt->statement_type = qp->statement_type;
	if (0 == (qp->flags & FLGP_USING_CURSOR))
		SC_no_fetchcursor(stmt);
#ifdef NOT_USED	/* this seems problematic */
	else if (0 == (qp->flags & (FLGP_SELECT_FOR_UPDATE_OR_SHARE | FLGP_SELECT_FOR_READONLY)) &&
		 0 == stmt->multi_statement &&
		 PG_VERSION_GE(conn, 8.3))
	{
		BOOL	semi_colon_found = FALSE;
		const UCHAR *ptr = NULL, semi_colon = ';';
		int	npos;

		if (npos = F_NewPos(qb) - 1, npos >= 0)
			ptr = F_NewPtr(qb) - 1;
		for (; npos >= 0 && isspace(*ptr); npos--, ptr--) ;
		if (npos >= 0 && semi_colon == *ptr)
		{
			qb->npos = npos;
			semi_colon_found = TRUE;
		}
		CVT_APPEND_STR(qb, " for read only");
		if (semi_colon_found)
			CVT_APPEND_CHAR(qb, semi_colon);
		CVT_TERMINATE(qb);
	}
#endif /* NOT_USED */
	if (0 != (qp->flags & FLGP_SELECT_INTO) ||
	    0 != (qp->flags & FLGP_MULTIPLE_STATEMENT))
	{
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
	}
	if (0 != (qp->flags & FLGP_SELECT_FOR_UPDATE_OR_SHARE))
	{
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
	}

	if (conn->DriverToDataSource != NULL)
	{
		size_t			length = strlen(new_statement);

		conn->DriverToDataSource(conn->translation_option,
					SQL_CHAR, new_statement, (SDWORD) length,
					new_statement, (SDWORD) length, NULL,
					NULL, 0, NULL);
	}

	if (!stmt->load_statement && qp->from_pos >= 0)
	{
		size_t	npos = qb->load_stmt_len;

		if (0 == npos)
		{
			npos = qb->npos;
			for (; npos > 0; npos--)
			{
				if (isspace((unsigned char) new_statement[npos - 1]))
					continue;
				if (';' != new_statement[npos - 1])
					break;
			}
			if (0 != (qb->flags & FLGB_KEYSET_DRIVEN))
			{
				qb->npos = npos;
				/* ----------
				 * 1st query is for field information
				 * 2nd query is keyset gathering
				 */
				CVT_APPEND_STR(qb, " where ctid = '(0,0)';select \"ctid");
				if (bestitem)
				{
					CVT_APPEND_STR(qb, "\", \"");
					CVT_APPEND_STR(qb, bestitem);
				}
				CVT_APPEND_STR(qb, "\" from ");
				CVT_APPEND_DATA(qb, qp->statement + qp->from_pos + 5, npos - qp->from_pos - 5);
			}
		}
		npos -= qp->declare_pos;
		stmt->load_statement = malloc(npos + 1);
		if (!stmt->load_statement)
		{
			retval = SQL_ERROR;
			goto cleanup;
		}
		memcpy(stmt->load_statement, qb->query_statement + qp->declare_pos, npos);
		stmt->load_from_pos = qb->load_from_pos - qp->declare_pos;
		stmt->load_statement[npos] = '\0';
	}

	stmt->stmt_with_params = qb->query_statement;
	retval = SQL_SUCCESS;
cleanup:
	return retval;
}

static void
remove_declare_cursor(QueryBuild *qb, QueryParse *qp)
{
	qp->flags &= ~FLGP_USING_CURSOR;
	if (qp->declare_pos <= 0)	return;
	memmove(qb->query_statement, qb->query_statement + qp->declare_pos, qb->npos - qp->declare_pos);
	qb->npos -= qp->declare_pos;
	qp->declare_pos = 0;
}

/*
 * When 'tag' starts with dollar-quoted tag, e.g. "$foo$...", return
 * the length of the tag (e.g. 5, with the previous example). If there
 * is no end-dollar in the string, returns 0. The caller should've checked
 * that the string begins with a dollar.
 */
size_t
findTag(const char *tag, int ccsc)
{
	size_t	taglen = 0;
	encoded_str	encstr;
	UCHAR		tchar;

	encoded_str_constr(&encstr, ccsc, tag + 1);
	for (tchar = encoded_nextchar(&encstr); tchar; tchar = encoded_nextchar(&encstr))
	{
		if (MBCS_NON_ASCII(encstr))
			continue;
		if (DOLLAR_QUOTE == tchar)
		{
			taglen = encstr.pos + 2;
			break;
		}
		if (!isalnum(tchar))
			break;
	}
	return taglen;
}

int
findIdentifier(const UCHAR *str, int ccsc, const UCHAR **next_token)
{
	int	slen = -1;
	encoded_str	encstr;
	UCHAR	tchar;
	BOOL	dquote = FALSE;

	*next_token = NULL;
	encoded_str_constr(&encstr, ccsc, (const char *) str);
	for (tchar = encoded_nextchar(&encstr); tchar; tchar = encoded_nextchar(&encstr))
	{
		if (MBCS_NON_ASCII(encstr))
			continue;
		if (encstr.pos == 0) /* the first character */
		{
			if (dquote = (IDENTIFIER_QUOTE == tchar), dquote)
				continue;
			if (!isalpha(tchar))
			{
				slen = 0;
				if (IS_NOT_SPACE(tchar))
					*next_token = ENCODE_PTR(encstr);
				break;
			}
		}
		if (dquote)
		{
			if (IDENTIFIER_QUOTE == tchar)
			{
				tchar =	encoded_nextchar(&encstr);
				if (IDENTIFIER_QUOTE == tchar)
					continue;
				slen = encstr.pos;
				break;
			}
		}
		else
		{
			if (isalnum(tchar))
				continue;
			switch (tchar)
			{
				case '_':
				case DOLLAR_QUOTE:
					continue;
			}
			slen = encstr.pos;
			if (IS_NOT_SPACE(tchar))
				*next_token = ENCODE_PTR(encstr);
			break;
		}
	}
	if (slen < 0 && !dquote)
		slen = encstr.pos;
	if (NULL == *next_token)
	{
		for (; tchar; tchar = encoded_nextchar(&encstr))
		{
			if (IS_NOT_SPACE((UCHAR) tchar))
			{
				*next_token = ENCODE_PTR(encstr);
				break;
			}
		}
	}
	return slen;
}

static pgNAME lower_or_remove_dquote(pgNAME nm, const UCHAR *src, int srclen, int ccsc)
{
	int	i, outlen;
	char *tc;
	UCHAR	tchar;
	BOOL	idQuote;
	encoded_str	encstr;

	if (nm.name)
		tc = realloc(nm.name, srclen + 1);
	else
		tc = malloc(srclen + 1);
	if (!tc)
	{
		NULL_THE_NAME(nm);
		return nm;
	}
	nm.name = tc;
	idQuote = (src[0] == IDENTIFIER_QUOTE);
	encoded_str_constr(&encstr, ccsc, (const char *) src);
	for (i = 0, tchar = encoded_nextchar(&encstr), outlen = 0; i < srclen; i++, tchar = encoded_nextchar(&encstr))
	{
		if (MBCS_NON_ASCII(encstr))
		{
			tc[outlen++] = tchar;
			continue;
		}
		if (idQuote)
		{
			if (IDENTIFIER_QUOTE == tchar)
			{
				if (0 == i)
					continue;
				if (i == srclen - 1)
					continue;
				i++;
				tchar = encoded_nextchar(&encstr);
			}
			tc[outlen++] = tchar;
		}
		else
		{
			tc[outlen++] = tolower(tchar);
		}
	}
	tc[outlen] = '\0';
	return nm;
}

int
eatTableIdentifiers(const UCHAR *str, int ccsc, pgNAME *table, pgNAME *schema)
{
	int	len;
	const UCHAR *next_token;
	const UCHAR *tstr = str;

	while (isspace(*tstr)) tstr++;

	if ((len = findIdentifier(tstr, ccsc, &next_token)) <= 0)
		return len;	/* table name doesn't exist */
	if (table)
	{
		if (IDENTIFIER_QUOTE == *tstr)
			*table = lower_or_remove_dquote(*table, tstr, len, ccsc);
		else
			STRN_TO_NAME(*table, tstr, len);
	}
	if (!next_token || '.' != *next_token || (int) (next_token - tstr) != len)
		return (int) (next_token - str); /* table only */
	tstr = next_token + 1;
	if ((len = findIdentifier(tstr, ccsc, &next_token)) <= 0)
		return -1;
	if (table)
	{
		if (schema)
			MOVE_NAME(*schema, *table);
		*table = lower_or_remove_dquote(*table, tstr, len, ccsc);
	}
	if (!next_token || '.' != *next_token || (int) (next_token - tstr) != len)
		return (int) (next_token - str); /* schema.table */
	tstr = next_token + 1;
	if ((len = findIdentifier(tstr, ccsc, &next_token)) <= 0)
		return -1;
	if (table)
	{
		if (schema)
			MOVE_NAME(*schema, *table);
		*table = lower_or_remove_dquote(*table, tstr, len, ccsc);
	}
	return (int) (next_token - str); /* catalog.schema.table */
}

static void token_start(QueryParse *qp, char oldchar)
{
	qp->prev_token_end = FALSE;
	qp->token_curr[0] = oldchar;
	qp->token_len = 1;
}
static int token_finish(QueryParse *qp, char oldchar, char *finished_token)
{
	int ret = -1;
	if (!qp->prev_token_end)
	{
		if (oldchar && qp->token_len + 1 < sizeof(qp->token_curr))
			qp->token_curr[qp->token_len++] = oldchar;
		qp->prev_token_end = TRUE;
		qp->token_curr[qp->token_len] = '\0';
		strncpy_null(finished_token, qp->token_curr, sizeof(qp->token_curr));
MYLOG(DETAIL_LOG_LEVEL, "finished token=%s\n", finished_token);
		ret = qp->token_len;
	}
	return ret;
}

static int token_restart(QueryParse *qp, char oldchar, char *finished_token)
{
	int ret = token_finish(qp, 0, finished_token);
	if (IS_NOT_SPACE(oldchar))
		token_start(qp, oldchar);

	return ret;
}

static int token_continue(QueryParse *qp, char oldchar)
{
	if (qp->prev_token_end)
		token_start(qp, oldchar);
	else if (qp->token_len + 1 < sizeof(qp->token_curr))
		qp->token_curr[qp->token_len++] = oldchar;

	return qp->token_len;
}

/*
 *	ParseToken functions
 */
typedef struct {
	QueryParse	*qp;
	int		token_len;
	BOOL		curchar_processed;
	unsigned int	in_status;
	char	finished_token[sizeof(((QueryParse *) NULL)->token_curr)];
} ParseToken;

static void PT_initialize(ParseToken *pt, QueryParse *qp)
{
	pt->qp = qp;
	pt->token_len = -1;
	pt->curchar_processed = FALSE;
	pt->in_status = 0;
	pt->finished_token[0] = '\0';
}

static int PT_token_finish(ParseToken *pt, char oldchar)
{
	int token_len_tmp;

	if (pt->curchar_processed)
		return pt->token_len;
	if ((token_len_tmp = token_finish(pt->qp, oldchar, pt->finished_token)) > 0)
	{
		pt->token_len = token_len_tmp;
		pt->in_status = pt->qp->in_status;
	}
	if (oldchar)
		pt->curchar_processed = TRUE;
	return pt->token_len;
}

static int PT_token_restart(ParseToken *pt, char oldchar)
{
	int token_len_tmp;
	unsigned int	in_status_save;

	if (pt->curchar_processed)
		return pt->token_len;
	in_status_save = pt->qp->in_status;
	if ((token_len_tmp = token_restart(pt->qp, oldchar, pt->finished_token)) > 0)
	{
		pt->token_len = token_len_tmp;
		pt->in_status = in_status_save;
	}
	pt->curchar_processed = TRUE;
	return pt->token_len;
}

static int PT_token_continue(ParseToken *pt, char oldchar)
{
	if (pt->curchar_processed)
		return pt->token_len;
	token_continue(pt->qp, oldchar);
	pt->curchar_processed = TRUE;
	return pt->token_len;
}
#define	PT_TOKEN_IGNORE(pt)	((pt)->curchar_processed = TRUE)

static int
inner_process_tokens(QueryParse *qp, QueryBuild *qb)
{
	CSTR func = "inner_process_tokens";
	BOOL	lf_conv = ((qb->flags & FLGB_CONVERT_LF) != 0);
	const char *bestitem = NULL;

	RETCODE	retval;
	Int4	opos;
	char	   oldchar;
	StatementClass	*stmt = qb->stmt;
	BOOL		isnull;
	BOOL		isbinary;
	Oid			dummy;
	ParseToken	pts, *pt = &pts;

	PT_initialize(pt, qp);

	if (stmt->ntab > 0)
		bestitem = GET_NAME(stmt->ti[0]->bestitem);
	opos = (Int4) qp->opos;
	if (opos < 0)
	{
		qb->errornumber = STMT_SEQUENCE_ERROR;
		qb->errormsg = "Function call for inner_process_tokens sequence error";
		return SQL_ERROR;
	}
	if (qp->from_pos == opos)
	{
		if (0 == (qb->flags & FLGB_CREATE_KEYSET))
		{
			qb->errornumber = STMT_SEQUENCE_ERROR;
			qb->errormsg = "Should come here only when handling updatable cursors";
			return SQL_ERROR;
		}
		CVT_APPEND_STR(qb, ", \"ctid");
		if (bestitem)
		{
			CVT_APPEND_STR(qb, "\", \"");
			CVT_APPEND_STR(qb, bestitem);
		}
		CVT_APPEND_STR(qb, "\" ");
		qb->load_from_pos = qb->npos;
	}
	else if (qp->where_pos == opos)
	{
		qb->load_stmt_len = qb->npos;
		if (0 != (qb->flags & FLGB_KEYSET_DRIVEN))
		{
			CVT_APPEND_STR(qb, "where ctid = '(0,0)';select \"ctid");
			if (bestitem)
			{
				CVT_APPEND_STR(qb, "\", \"");
				CVT_APPEND_STR(qb, bestitem);
			}
			CVT_APPEND_STR(qb, "\" from ");
			CVT_APPEND_DATA(qb, qp->statement + qp->from_pos + 5, qp->where_pos - qp->from_pos - 5);
		}
	}
	oldchar = encoded_byte_check(&qp->encstr, qp->opos);
	if (MBCS_NON_ASCII(qp->encstr))
	{
		if (QP_in_idle_status(qp))
		{
			PT_token_restart(pt, oldchar); /* placed before QP_enter() */
			QP_enter(qp, QP_IN_IDENT_KEYWORD);	/* identifier */
		}
		else if (qp->token_len > 0)
			PT_token_continue(pt, oldchar);
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}

	/*
	 * From here we are guaranteed to handle a 1-byte character.
	 */

	if (QP_is_in(qp, QP_IN_IDENT_KEYWORD))	/* identifier or keyword */
	{
		if (isalnum((UCHAR)oldchar) ||
		    DOLLAR_QUOTE == oldchar ||
		    '_' == oldchar)
		{
			CVT_APPEND_CHAR(qb, oldchar);
			PT_token_continue(pt, oldchar);
			return SQL_SUCCESS;
		}
		PT_token_finish(pt, 0); /* placed before QP_exit() */
		QP_exit(qp, QP_IN_IDENT_KEYWORD);
	}

	if (QP_is_in(qp, QP_IN_ESCAPE))	/* escape in literal check */
	{
		QP_exit(qp, QP_IN_ESCAPE);
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (QP_is_in(qp, QP_IN_DOLLAR_QUOTE)) /* dollar quote check */
	{
		if (oldchar == DOLLAR_QUOTE)
		{
			if (strncmp(F_OldPtr(qp), qp->dollar_tag, qp->taglen) == 0)
			{
				CVT_APPEND_DATA(qb, F_OldPtr(qp), qp->taglen);
				qp->opos += (qp->taglen - 1);
				QP_exit(qp, QP_IN_DOLLAR_QUOTE);
				qp->dollar_tag = NULL;
				qp->taglen = -1;
				return SQL_SUCCESS;
			}
		}
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (QP_is_in(qp, QP_IN_LITERAL)) /* quote check */
	{
		if (oldchar == LITERAL_QUOTE)
		{
			PT_token_finish(pt, oldchar); /* placed before QP_exit() */
			QP_exit(qp, QP_IN_LITERAL);
		}
		else
		{
			PT_token_continue(pt, oldchar);
			if (oldchar == qp->escape_in_literal)
				QP_enter(qp, QP_IN_ESCAPE); /* escape in literal */
		}
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (QP_is_in(qp, QP_IN_DQUOTE_IDENTIFIER)) /* double quote check */
	{
		if (oldchar == IDENTIFIER_QUOTE)
		{
			PT_token_finish(pt, oldchar); /* placed before QP_exit() */
			QP_exit(qp, QP_IN_DQUOTE_IDENTIFIER);
		}
		else
			PT_token_continue(pt, oldchar);
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (QP_is_in(qp, QP_IN_COMMENT_BLOCK)) /* comment_level check */
	{
		if ('/' == oldchar &&
		    '*' == F_OldPtr(qp)[1])
		{
			qp->comment_level++;
			QP_enter(qp, QP_IN_COMMENT_BLOCK);
			CVT_APPEND_CHAR(qb, oldchar);
			F_OldNext(qp);
			oldchar = F_OldChar(qp);
		}
		else if ('*' == oldchar &&
			 '/' == F_OldPtr(qp)[1])
		{
			if (--qp->comment_level <= 0)
				QP_exit(qp, QP_IN_COMMENT_BLOCK);
			CVT_APPEND_CHAR(qb, oldchar);
			F_OldNext(qp);
			oldchar = F_OldChar(qp);
		}
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (QP_is_in(qp, QP_IN_LINE_COMMENT)) /* line comment check */
	{
		if (PG_LINEFEED == oldchar)
			QP_exit(qp, QP_IN_LINE_COMMENT);
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}

	if (!QP_in_idle_status(qp))
	{
		qb->errornumber = STMT_EXEC_ERROR;
		qb->errormsg = "logic error? not in QP_in_idle_status";
		return SQL_ERROR;
	}

	/*
	 * From here we are guaranteed to be in neither a literal_escape,
	 * a LITREAL_QUOTE nor an IDENTIFIER_QUOTE.
	 */
	/* Squeeze carriage-return/linefeed pairs to linefeed only */
	if (lf_conv &&
		 PG_CARRIAGE_RETURN == oldchar &&
		 qp->opos + 1 < qp->stmt_len &&
		 PG_LINEFEED == qp->statement[qp->opos + 1])
		return SQL_SUCCESS;

	/*
	 * Handle literals (date, time, timestamp) and ODBC scalar
	 * functions
	 */
	if (oldchar == ODBC_ESCAPE_START)
	{
		PT_token_finish(pt, 0);
		if (SQL_ERROR == convert_escape(qp, qb))
		{
			if (0 == qb->errornumber)
			{
				qb->errornumber = STMT_EXEC_ERROR;
				qb->errormsg = "ODBC escape convert error";
			}
			MYLOG(0, "convert_escape error\n");
			return SQL_ERROR;
		}
		PT_TOKEN_IGNORE(pt);
		return SQL_SUCCESS;
	}
	/* End of an escape sequence */
	else if (oldchar == ODBC_ESCAPE_END)
	{
		PT_token_finish(pt, 0);
		PT_TOKEN_IGNORE(pt);
		return QB_end_brace(qb);
	}
	else if (oldchar == '@' &&
		 strnicmp(F_OldPtr(qp), "@@identity", 10) == 0)
	{
		ConnectionClass	*conn = SC_get_conn(stmt);
		BOOL		converted = FALSE;
		COL_INFO	*coli;

#ifdef	NOT_USED  /* lastval() isn't always appropriate */
		if (PG_VERSION_GE(conn, 8.1))
		{
			CVT_APPEND_STR(qb, "lastval()");
			converted = TRUE;
		}
		else
#endif /* NOT_USED */
		if (NAME_IS_VALID(conn->tableIns))
		{
			TABLE_INFO	ti, *pti = &ti;

			memset(&ti, 0, sizeof(ti));
			NAME_TO_NAME(ti.schema_name, conn->schemaIns);
			NAME_TO_NAME(ti.table_name, conn->tableIns);
			getCOLIfromTI(func, conn, qb->stmt, 0, &pti);
			coli = ti.col_info;
			NULL_THE_NAME(ti.schema_name);
			NULL_THE_NAME(ti.table_name);
			if (NULL != coli)
			{
				int	i, num_fields = QR_NumResultCols(coli->result);
				const char *auto_increment;

				for (i = 0; i < num_fields; i++)
				{
					auto_increment = (const char *) QR_get_value_backend_text(coli->result, i, COLUMNS_AUTO_INCREMENT);
					if (auto_increment && auto_increment[0] == '1')
					{
						converted = TRUE;
						break;
					}
				}
				if (converted)
				{
				        const char *column_def = (const char *) QR_get_value_backend_text(coli->result, i, COLUMNS_COLUMN_DEF);
					if (NULL != column_def &&
					    strncmp(column_def, "nextval", 7) == 0)
					{
						CVT_APPEND_STR(qb, "curr");
						CVT_APPEND_STR(qb, column_def + 4);
					}
					else
					{
						char relcnv[128];
						const char *column_name = (const char *) QR_get_value_backend_text(coli->result, i, COLUMNS_COLUMN_NAME);

						CVT_APPEND_STR(qb, "currval(pg_get_serial_sequence('");
						if (NAME_IS_VALID(conn->schemaIns))
						{
							CVT_APPEND_STR(qb, identifierEscape((const SQLCHAR *) SAFE_NAME(conn->schemaIns), SQL_NTS, conn, relcnv, sizeof(relcnv), TRUE));
							CVT_APPEND_STR(qb, ".");
						}
						CVT_APPEND_STR(qb, identifierEscape((const SQLCHAR *) SAFE_NAME(conn->tableIns), SQL_NTS, conn, relcnv, sizeof(relcnv), TRUE));
						CVT_APPEND_STR(qb, "','");
						if (NULL != column_name)
							CVT_APPEND_STR(qb, identifierEscape((const SQLCHAR *) column_name, SQL_NTS, conn, relcnv, sizeof(relcnv), FALSE));
						CVT_APPEND_STR(qb, "')::regclass)");
					}
				}
			}
		}
		if (!converted)
			CVT_APPEND_STR(qb, "NULL");
		qp->opos += 10;
		return SQL_SUCCESS;
	}

	/*
	 * Can you have parameter markers inside of quotes?  I dont think
	 * so. All the queries I've seen expect the driver to put quotes
	 * if needed.
	 */
	else if (oldchar != '?')
	{
		if (oldchar == DOLLAR_QUOTE)
		{
			PT_token_finish(pt, 0);
			qp->taglen = findTag(F_OldPtr(qp), qp->encstr.ccsc);
			if (qp->taglen > 0)
			{
				QP_enter(qp, QP_IN_DOLLAR_QUOTE);
				qp->dollar_tag = F_OldPtr(qp);
				CVT_APPEND_DATA(qb, F_OldPtr(qp), qp->taglen);
				qp->opos += (qp->taglen - 1);
				return SQL_SUCCESS;
			}
		}
		else if (oldchar == LITERAL_QUOTE)
		{
			PT_token_restart(pt, oldchar); /* placed before QP_enter() */
			QP_enter(qp, QP_IN_LITERAL);
			qp->escape_in_literal = CC_get_escape(qb->conn);
			if (!qp->escape_in_literal)
			{
				if (LITERAL_EXT == F_OldPtr(qp)[-1])
					qp->escape_in_literal = ESCAPE_IN_LITERAL;
			}
		}
		else if (oldchar == IDENTIFIER_QUOTE)
		{
			PT_token_restart(pt, oldchar); /* placed before QP_enter() */
			QP_enter(qp, QP_IN_DQUOTE_IDENTIFIER);
		}
		else if ('/' == oldchar &&
			 '*' == F_OldPtr(qp)[1])
		{
			qp->comment_level++;
			PT_token_finish(pt, 0); /* comments are excluded */
			QP_enter(qp, QP_IN_COMMENT_BLOCK);
			PT_TOKEN_IGNORE(pt);
		}
		else if ('-' == oldchar &&
			 '-' == F_OldPtr(qp)[1])
		{
			PT_token_finish(pt, 0); /* comments are excluded */
			QP_enter(qp, QP_IN_LINE_COMMENT);
			PT_TOKEN_IGNORE(pt);
		}
		else if (oldchar == ';')
		{
			PT_token_restart(pt, 0);
			/*
			 * can't parse multiple statement using protocol V3.
			 * reset the dollar number here in case it is divided
			 * to parse.
			 */
			qb->dollar_number = 0;
			if (0 != (qp->flags & FLGP_USING_CURSOR))
			{
				const char *vp = &(qp->statement[qp->opos + 1]);

				while (*vp && isspace((unsigned char) *vp))
					vp++;
				if (*vp)	/* multiple statement */
				{
					qp->flags |= FLGP_MULTIPLE_STATEMENT;
					qb->flags &= ~FLGB_KEYSET_DRIVEN;
					remove_declare_cursor(qb, qp);
				}
			}
		}
		else if (isalnum(oldchar))
		{
			PT_token_restart(pt, oldchar); /* placed before QP_enter() */
			QP_enter(qp, QP_IN_IDENT_KEYWORD); /* identifier or keyword */
		}
		else
		{
			/*
			 *	a hack to handle boolean items in VBA
			 *		with MS Access.
			 *	VBA seems to transform the where condition
			 *		a_boolean_item=True
			 *	into
			 *		("a_boolean_item" = 1)
			 *	which causes an ERROR:Operator does not exist boolean = integer .
			 *	So transforms it into
			 *		("a_boolean_item"='1')
			 *	here.
			 */
			if (')' == oldchar &&
			    qb->conn->ms_jet &&
			    1 == qp->token_len &&
			    '1' == qp->token_curr[0] &&
			    8 <= F_OldPos(qp))
			{
				const char *oldptr = F_OldPtr(qp);
				int	oldpos = F_OldPos(qp);

				if (strncmp(oldptr - 5, "\" =", 3) == 0)
				{
					int	i;

					for (i = 6; i < oldpos - 1; i++)
					{
						if (oldptr[-i] == '"')
						{
							if (oldptr[-(i+1)] == '(')
							{
								F_NewPtr(qb)[-4] = '=';
								F_NewPtr(qb)[-3] = '\'';
								F_NewPtr(qb)[-2] = '1';
								F_NewPtr(qb)[-1] = '\'';
							}
							break;
						}
					}
				}
			}
			if (!isalnum((UCHAR) oldchar))
			{
				PT_token_restart(pt, oldchar);
			}
			else
				PT_token_continue(pt, oldchar);
		}

		if (pt->token_len > 0)
			MYLOG(0, "token_len=%d status=%x token=%s\n", pt->token_len, pt->in_status, pt->finished_token);
		if (!pt->curchar_processed)
		{
			MYLOG(0, "Forgot to process ParseToken char=%c status=%u\n", oldchar, qp->in_status);
#ifdef	NOT_USED	/* strict check for debugging */
			qb->errornumber = STMT_EXEC_ERROR;
			qb->errormsg = "Forget to process ParseToken";
			return SQL_ERROR;
#endif /* NOT_USED */
		}
		switch (pt->token_len)
		{
			case 4:
				if (0 != (qp->flags & FLGP_USING_CURSOR) &&
				    into_table_from(&qp->statement[qp->opos - pt->token_len]))
				{
					qp->flags |= FLGP_SELECT_INTO;
					qb->flags &= ~FLGB_KEYSET_DRIVEN;
					qp->statement_type = STMT_TYPE_CREATE;
					remove_declare_cursor(qb, qp);
				}
				else if (stricmp(pt->finished_token, "join") == 0)
					check_join(stmt, F_OldPtr(qp), F_OldPos(qp));
				break;
			case 3:
				if (0 != (qp->flags & FLGP_USING_CURSOR) &&
				    strnicmp(pt->finished_token, "for", 3) == 0)
				{
					UInt4	flg;
					size_t	endpos;

					flg = table_for_update_or_share(F_OldPtr(qp), &endpos);
					if (0 != (FLGP_SELECT_FOR_UPDATE_OR_SHARE & flg))
					{
						qp->flags |= flg;
						remove_declare_cursor(qb, qp);
					}
					else
						qp->flags |= flg;
				}
				break;
			case 2:
			{
				size_t	endpos;

				if (STMT_TYPE_INSERT == qp->statement_type &&
				    strnicmp(pt->finished_token, "()", 2) == 0 &&
				    insert_without_target(F_OldPtr(qp), &endpos))
				{
					qb->npos -= 2;
					CVT_APPEND_STR(qb, "DEFAULT VALUES");
					qp->opos += endpos;
					return SQL_SUCCESS;
				}
				break;
			}
			case 1:
			{
				size_t	endpos;

				if (STMT_TYPE_INSERT == qp->statement_type &&
				    pt->finished_token[0] == '(' &&
				    oldchar == ')' &&
				    insert_without_target(F_OldPtr(qp)+1, &endpos))
				{
					qb->npos --;
					CVT_APPEND_STR(qb, " DEFAULT VALUES");
					qp->opos += endpos;
					return SQL_SUCCESS;
				}
				break;
			}
		}

		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else
		PT_token_restart(pt, oldchar);

	/*
	 * It's a '?' parameter alright
	 */
	retval = ResolveOneParam(qb, qp, &isnull, &isbinary, &dummy);
	if (retval < 0)
		return retval;

	if (SQL_SUCCESS_WITH_INFO == retval) /* means discarding output parameter */
	{
	}
	retval = SQL_SUCCESS;
cleanup:
	return retval;
}

#define	MIN_ALC_SIZE	128

/*
 * Build an array of parameters to pass to libpq's PQexecPrepared
 * function.
 */
BOOL
build_libpq_bind_params(StatementClass *stmt,
						int *nParams,
						OID **paramTypes,
						char ***paramValues,
						int **paramLengths,
						int **paramFormats,
						int *resultFormat)
{
	CSTR func = "build_libpq_bind_params";
	QueryBuild	qb;
	SQLSMALLINT	num_p;
	int			i, num_params;
	ConnectionClass	*conn = SC_get_conn(stmt);
	BOOL		ret = FALSE, discard_output;
	RETCODE		retval;
	const		IPDFields *ipdopts = SC_get_IPDF(stmt);

	*paramTypes = NULL;
	*paramValues = NULL;
	*paramLengths = NULL;
	*paramFormats = NULL;

	num_params = stmt->num_params;
	if (num_params < 0)
	{
		PGAPI_NumParams(stmt, &num_p);
		num_params = num_p;
	}
	if (ipdopts->allocated < num_params)
	{
		char	tmp[100];

		if (0 == ipdopts->allocated)
			STRCPY_FIXED(tmp, "Parameters exist but IPD isn't set. Please call SQLDescribeParam()");
		else
			SPRINTF_FIXED(tmp, "The # of IPD parameters %d < %d the # of parameter markers", ipdopts->allocated, num_params);
		MYLOG(0, "%s\n", tmp);
		SC_set_error(stmt, STMT_COUNT_FIELD_INCORRECT, tmp, func);
		return FALSE;
	}

	if (QB_initialize(&qb, MIN_ALC_SIZE, stmt, RPM_BUILDING_BIND_REQUEST) < 0)
		return FALSE;

	if (num_params > 0)
	{
		*paramTypes = malloc(sizeof(OID) * num_params);
		if (*paramTypes == NULL)
			goto cleanup;
		*paramValues = malloc(sizeof(char *) * num_params);
		if (*paramValues == NULL)
			goto cleanup;
		memset(*paramValues, 0, sizeof(char *) * num_params);
		*paramLengths = malloc(sizeof(int) * num_params);
		if (*paramLengths == NULL)
			goto cleanup;
		*paramFormats = malloc(sizeof(int) * num_params);
		if (*paramFormats == NULL)
			goto cleanup;
	}

	qb.flags |= FLGB_BINARY_AS_POSSIBLE;

	MYLOG(DETAIL_LOG_LEVEL, "num_params=%d proc_return=%d\n", num_params, stmt->proc_return);
	num_p = num_params - qb.num_discard_params;
MYLOG(DETAIL_LOG_LEVEL, "num_p=%d\n", num_p);
	discard_output = (0 != (qb.flags & FLGB_DISCARD_OUTPUT));
	*nParams = 0;
	if (num_p > 0)
	{
		ParameterImplClass	*parameters = ipdopts->parameters;
		int	pno;

		BOOL	isnull;
		BOOL	isbinary;
		char	*val_copy;
		OID	pgType;

		/*
		 * Now build the parameter values.
		 */
		for (i = 0, pno = 0; i < stmt->num_params; i++)
		{
			qb.npos = 0;
			retval = ResolveOneParam(&qb, NULL, &isnull, &isbinary, &pgType);
			if (SQL_ERROR == retval)
			{
				QB_replace_SC_error(stmt, &qb, func);
				ret = FALSE;
				goto cleanup;
			}

			MYLOG(DETAIL_LOG_LEVEL, "%dth parameter type oid is %u\n", i, PIC_dsp_pgtype(conn, parameters[i]));

			if (i < qb.proc_return)
				continue;
			if (SQL_PARAM_OUTPUT == parameters[i].paramType)
			{
				if (discard_output)
					continue;
				(*paramTypes)[pno] = PG_TYPE_VOID;
				(*paramValues)[pno] = NULL;
				(*paramLengths)[pno] = 0;
				(*paramFormats)[pno] = 0;
				pno++;
				continue;
			}
			if (!isnull)
			{
				val_copy = malloc(qb.npos + 1);
				if (!val_copy)
					goto cleanup;
				memcpy(val_copy, qb.query_statement, qb.npos);
				val_copy[qb.npos] = '\0';

				(*paramTypes)[pno] = pgType;
				(*paramValues)[pno] = val_copy;
				if (qb.npos > INT_MAX)
					goto cleanup;
				(*paramLengths)[pno] = (int) qb.npos;
			}
			else
			{
				(*paramTypes)[pno] = pgType;
				(*paramValues)[pno] = NULL;
				(*paramLengths)[pno] = 0;
			}
			if (isbinary)
				MYLOG(0, "%dth parameter is of binary format\n", pno);
			(*paramFormats)[pno] = isbinary ? 1 : 0;

			pno++;
		}
		*nParams = pno;
	}

	/* result format is text */
	*resultFormat = 0;

	ret = TRUE;

cleanup:
	QB_Destructor(&qb);

	return ret;
}


/*
 * With SQL_MAX_NUMERIC_LEN = 16, the highest representable number is
 * 2^128 - 1, which fits in 39 digits.
 */
#define MAX_NUMERIC_DIGITS 39

/*
 * Convert a SQL_NUMERIC_STRUCT into string representation.
 */
static void
ResolveNumericParam(const SQL_NUMERIC_STRUCT *ns, char *chrform)
{
	Int4		i, vlen, len, newlen;
	const UCHAR	*val = (const UCHAR *) ns->val;
	UCHAR		vals[SQL_MAX_NUMERIC_LEN];
	int			lastnonzero;
	UCHAR		calv[MAX_NUMERIC_DIGITS];
	int			precision;

MYLOG(DETAIL_LOG_LEVEL, "C_NUMERIC [prec=%d scale=%d]", ns->precision, ns->scale);

	if (0 == ns->precision)
	{
		if (chrform)
			strncpy_null(chrform, "0", 2);
		return;
	}

	precision = ns->precision;
	if (precision > MAX_NUMERIC_DIGITS)
		precision = MAX_NUMERIC_DIGITS;

	/*
	 * The representation in SQL_NUMERIC_STRUCT is 16 bytes with most
	 * significant byte first. Make a working copy.
	 */
	memcpy(vals, val, SQL_MAX_NUMERIC_LEN);

	vlen = SQL_MAX_NUMERIC_LEN;
	len = 0;
	do
	{
		UInt2		d, r;

		/*
		 * Divide the number by 10, and output the reminder as the next digit.
		 *
		 * Begin from the most-significant byte (last in the array), and at
		 * each step, carry the remainder to the prev byte.
		 */
		r = 0;
		lastnonzero = -1;
		for (i = vlen - 1; i >= 0; i--)
		{
			UInt2	v;

			v = ((UInt2) vals[i]) + (r << 8);
			d = v / 10; r = v % 10;
			vals[i] = (UCHAR) d;

			if (d != 0 && lastnonzero == -1)
				lastnonzero = i;
		}

		/* output the remainder */
		calv[len++] = (UCHAR) r;

		vlen = lastnonzero + 1;
	} while(lastnonzero >= 0 && len < precision);

	/*
	 * calv now contains the digits in reverse order, i.e. least significant
	 * digit is at calv[0]
	 */

MYPRINTF(DETAIL_LOG_LEVEL, " len2=%d", len);

	/* build the final output string. */
	newlen = 0;
	if (0 == ns->sign)
		chrform[newlen++] = '-';

	i = len - 1;
	if (i < ns->scale)
		i = ns->scale;
	for (; i >= ns->scale; i--)
	{
		if (i >= len)
			chrform[newlen++] = '0';
		else
			chrform[newlen++] = calv[i] + '0';
	}
	if (ns->scale > 0)
	{
		chrform[newlen++] = '.';
		for (; i >= 0; i--)
		{
			if (i >= len)
				chrform[newlen++] = '0';
			else
				chrform[newlen++] = calv[i] + '0';
		}
	}
	if (0 == len)
		chrform[newlen++] = '0';
	chrform[newlen] = '\0';
MYLOG(DETAIL_LOG_LEVEL, " convval(2) len=%d %s\n", newlen, chrform);
}

/*
 * Convert a string representation of a numeric into SQL_NUMERIC_STRUCT.
 */
static void
parse_to_numeric_struct(const char *wv, SQL_NUMERIC_STRUCT *ns, BOOL *overflow)
{
	int			i, nlen, dig;
	char		calv[SQL_MAX_NUMERIC_LEN * 3];
	BOOL		dot_exist;

	*overflow = FALSE;

	/* skip leading space */
	while (*wv && isspace((unsigned char) *wv))
		wv++;

	/* sign */
	ns->sign = 1;
	if (*wv == '-')
	{
		ns->sign = 0;
		wv++;
	}
	else if (*wv == '+')
		wv++;

	/* skip leading zeros */
	while (*wv == '0')
		wv++;

	/* read the digits into calv */
	ns->precision = 0;
	ns->scale = 0;
	for (nlen = 0, dot_exist = FALSE;; wv++)
	{
		if (*wv == '.')
		{
			if (dot_exist)
				break;
			dot_exist = TRUE;
		}
		else if (*wv == '\0' || !isdigit((unsigned char) *wv))
			break;
		else
		{
			if (nlen >= sizeof(calv))
			{
				if (dot_exist)
					break;
				else
				{
					ns->scale--;
					*overflow = TRUE;
					continue;
				}
			}
			if (dot_exist)
				ns->scale++;
			calv[nlen++] = *wv;
		}
	}
	ns->precision = nlen;

	/* Convert the decimal digits to binary */
	memset(ns->val, 0, sizeof(ns->val));
	for (dig = 0; dig < nlen; dig++)
	{
		UInt4 carry;

		/* multiply the current value by 10, and add the next digit */
		carry = calv[dig] - '0';
		for (i = 0; i < sizeof(ns->val); i++)
		{
			UInt4		t;

			t = ((UInt4) ns->val[i]) * 10 + carry;
			ns->val[i] = (unsigned char) (t & 0xFF);
			carry = (t >> 8);
		}

		if (carry != 0)
			*overflow = TRUE;
	}
}

static BOOL
parameter_is_with_cast(const QueryParse *qp)
{
	const char *str = F_OldPtr(qp);

	if ('?' != *str)	return FALSE;
	while (isspace(*(++str))) ;
	if (strncmp(str, "::", 2) == 0)
		return TRUE;
	if (strnicmp(str, "as", 2) != 0)
		return FALSE;
	if (isspace(str[2]))
		return TRUE;
	return FALSE;
}

#ifdef	UNICODE_SUPPORT
enum {
	ErrorOutConversionErrors	/* error out conversion errors */
	, ReturnZeroLengthString	/* simply returns zero length strings */
};
static int convert_err_flag =
#ifdef	WIN32
	 ReturnZeroLengthString;
#else
	 ErrorOutConversionErrors;
#endif /* WIN32 */

static BOOL
handle_lu_onvert_error(QueryBuild *qb, int flag, char *buffer, SQLLEN paralen)
{
	int	blen = paralen;

	if (!buffer)
		return FALSE;
	if (get_mylog() > 0 || ReturnZeroLengthString != flag)
	{
		const UCHAR *buf = (UCHAR *) buffer;
		int	i;
		PQExpBufferData pbuf = {0};

		if (SQL_NTS == blen)
			blen = strlen(buffer);
		initPQExpBuffer(&pbuf);
		appendPQExpBuffer(&pbuf, "Could not convert the current data '");
		for (i = 0; i < blen; i++)
		{
			if (buf[i] >= 0x80)
				appendPQExpBuffer(&pbuf, "\\%03o", buf[i]);
			else if ('\\' == buf[i])
				appendPQExpBuffer(&pbuf, "\\\\");
			else
				appendPQExpBuffer(&pbuf, "%c", buf[i]);
		}
		appendPQExpBuffer(&pbuf, "' to wide chars");
		MYLOG(0, "%s\n", pbuf.data);
		if (ReturnZeroLengthString != flag)
		{
			if (qb->stmt)
				SC_set_error(qb->stmt, STMT_EXEC_ERROR, pbuf.data, __FUNCTION__);
			else
				qb->errormsg = "could not convert the current data to wide chars";
		}
		termPQExpBuffer(&pbuf);

	}
	switch (flag)
	{
		case ReturnZeroLengthString:
			if (qb->stmt)
				SC_set_error(qb->stmt, STMT_ERROR_IN_ROW, "conversion error to wide chars occured", __FUNCTION__);
			return TRUE;
		default:
			qb->errornumber = STMT_EXEC_ERROR;
			return FALSE;
	}
}
#endif /* UNICODE_SUPPORT */

/*
 * Resolve one parameter.
 *
 * *isnull is set to TRUE if it was NULL.
 * *isbinary is set to TRUE, if the binary output format was used. (binary
 *   output is only produced if the FLGB_BINARY_AS_POSSIBLE flag is set)
 * *pgType is set to the PostgreSQL type OID that should be used when binding
 * (or 0, to let the server decide)
 */
static int
ResolveOneParam(QueryBuild *qb, QueryParse *qp, BOOL *isnull, BOOL *isbinary,
				OID *pgType)
{
	ConnectionClass *conn = qb->conn;
	const APDFields *apdopts = qb->apdopts;
	const IPDFields *ipdopts = qb->ipdopts;
	PutDataInfo *pdata = qb->pdata;

	int			param_number;
	char		param_string[150],
				tmp[256];
	char		cbuf[PG_NUMERIC_MAX_PRECISION * 2]; /* seems big enough to handle the data in this function */
	OID			param_pgtype;
	SQLSMALLINT	param_ctype, param_sqltype;
	SIMPLE_TIME	st;
	struct tm	*tim;
	SQLLEN		used;
	const char	*send_buf;

	char		*buffer, *allocbuf = NULL, *lastadd = NULL;
	OID			lobj_oid;
	int			lobj_fd;
	SQLULEN		offset = apdopts->param_offset_ptr ? *apdopts->param_offset_ptr : 0;
	size_t		current_row = qb->current_row;
	BOOL		handling_large_object = FALSE, req_bind;
	BOOL		need_quotes = TRUE;
	BOOL		add_parens = FALSE;
	BOOL		negative;
	ParameterInfoClass	*apara;
	ParameterImplClass	*ipara;
	BOOL		outputDiscard,
				valueOutput;
	SDOUBLE		dbv;
	SFLOAT		flv;
	SQL_INTERVAL_STRUCT	*ivstruct;
	const char *ivsign;
	BOOL		final_binary_convert = FALSE;
	RETCODE		retval = SQL_ERROR;

	*isnull = FALSE;
	*isbinary = FALSE;
	*pgType = 0;

	outputDiscard = (0 != (qb->flags & FLGB_DISCARD_OUTPUT));
	valueOutput = (qb->param_mode != RPM_FAKE_PARAMS &&
				   qb->param_mode != RPM_BUILDING_PREPARE_STATEMENT);
	req_bind = (qb->param_mode == RPM_BUILDING_BIND_REQUEST);

	if (qb->proc_return < 0 && qb->stmt)
		qb->proc_return = qb->stmt->proc_return;
	/*
	 * It's a '?' parameter alright
	 */
	param_number = ++qb->param_number;

MYLOG(DETAIL_LOG_LEVEL, "para:%d(%d,%d)\n", param_number, ipdopts->allocated, apdopts->allocated);
	apara = NULL;
	ipara = NULL;
	if (param_number < apdopts->allocated)
		apara = apdopts->parameters + param_number;
	if (param_number < ipdopts->allocated)
		ipara = ipdopts->parameters + param_number;
	if ((!apara || !ipara) && valueOutput)
	{
		MYLOG(0, "The # of (A|I)PD parameters (%d, %d) < %d the # of parameter markers\n", apdopts->allocated, ipdopts->allocated, param_number);
		qb->errormsg = "The # of binded parameters < the # of parameter markers";
		qb->errornumber = STMT_COUNT_FIELD_INCORRECT;
		CVT_TERMINATE(qb);	/* just in case */
		return SQL_ERROR;
	}

MYLOG(DETAIL_LOG_LEVEL, "ipara=%p paramType=%d %d proc_return=%d\n", ipara, ipara ? ipara->paramType : -1, PG_VERSION_LT(conn, 8.1), qb->proc_return);
	if (param_number < qb->proc_return)
	{
		if (ipara && SQL_PARAM_OUTPUT != ipara->paramType)
		{
			qb->errormsg = "The function return value isn't marked as output parameter";
			qb->errornumber = STMT_EXEC_ERROR;
			CVT_TERMINATE(qb);	/* just in case */
			return SQL_ERROR;
		}
		return SQL_SUCCESS;
	}
	if (ipara && SQL_PARAM_OUTPUT == ipara->paramType)
	{
		if (PG_VERSION_LT(conn, 8.1))
		{
			qb->errormsg = "Output parameter isn't available before 8.1 version";
			qb->errornumber = STMT_INTERNAL_ERROR;
			CVT_TERMINATE(qb);	/* just in case */
			return SQL_ERROR;
		}
		if (outputDiscard)
		{
			ssize_t			npos = 0;

			for (npos = qb->npos - 1; npos >= 0 && isspace((unsigned char) qb->query_statement[npos]) ; npos--) ;
			if (npos >= 0)
			{
				switch (qb->query_statement[npos])
				{
					case ',':
						qb->npos = npos;
						qb->query_statement[npos] = '\0';
						break;
					case '(':
						if (!qp)
							break;
						for (npos = qp->opos + 1; isspace((unsigned char) qp->statement[npos]); npos++) ;
						if (qp->statement[npos] == ',')
							qp->opos = npos;
						break;
				}
			}
			return SQL_SUCCESS_WITH_INFO;
		}
	}

	if ((!apara || !ipara) && qb->param_mode == RPM_FAKE_PARAMS)
	{
		CVT_APPEND_STR(qb, "NULL");
		qb->flags |= FLGB_INACCURATE_RESULT;
		return SQL_SUCCESS;
	}
	if (qb->param_mode == RPM_BUILDING_PREPARE_STATEMENT)
	{
		char	pnum[16];

		qb->dollar_number++;
		if (ipara &&
		    SQL_PARAM_OUTPUT != ipara->paramType &&
		    (qb->flags & FLGB_PARAM_CAST) != 0 &&
		    !parameter_is_with_cast(qp))
			SPRINTF_FIXED(pnum, "$%d%s", qb->dollar_number, sqltype_to_pgcast(conn, ipara->SQLType));
		else
			SPRINTF_FIXED(pnum, "$%d", qb->dollar_number);
		CVT_APPEND_STR(qb, pnum);
		return SQL_SUCCESS;
	}
	/*
	 * After this point, we can assume apara and ipara to be set. The only
	 * cases where we allow them to be NULL is when param_mode is
	 * RPM_FAKE_PARAMS or RPM_BUILDING_PREPARE_STATEMENT, and we've now handled
	 * those cases.
	 */

	/* Assign correct buffers based on data at exec param or not */
	if (apara->data_at_exec)
	{
		if (pdata->allocated != apdopts->allocated)
			extend_putdata_info(pdata, apdopts->allocated, TRUE);
		used = pdata->pdata[param_number].EXEC_used ? *pdata->pdata[param_number].EXEC_used : SQL_NTS;
		buffer = pdata->pdata[param_number].EXEC_buffer;
		if (pdata->pdata[param_number].lobj_oid)
			handling_large_object = TRUE;
	}
	else
	{
		UInt4	bind_size = apdopts->param_bind_type;
		UInt4	ctypelen;
		BOOL	bSetUsed = FALSE;

		buffer = apara->buffer + offset;
		if (current_row > 0)
		{
			if (bind_size > 0)
				buffer += (bind_size * current_row);
			else if (ctypelen = ctype_length(apara->CType), ctypelen > 0)
				buffer += current_row * ctypelen;
			else
				buffer += current_row * apara->buflen;
		}
		if (apara->used || apara->indicator)
		{
			SQLULEN	p_offset;

			if (bind_size > 0)
				p_offset = offset + bind_size * current_row;
			else
				p_offset = offset + sizeof(SQLLEN) * current_row;
			if (apara->indicator)
			{
				used = *LENADDR_SHIFT(apara->indicator, p_offset);
				if (SQL_NULL_DATA == used)
					bSetUsed = TRUE;
			}
			if (!bSetUsed && apara->used)
			{
				used = *LENADDR_SHIFT(apara->used, p_offset);
				bSetUsed = TRUE;
			}
		}
		if (!bSetUsed)
			used = SQL_NTS;
	}

	/* Handle DEFAULT_PARAM parameter data. Should be NULL ?
	if (used == SQL_DEFAULT_PARAM)
	{
		return SQL_SUCCESS;
	} */

	param_ctype = apara->CType;
	param_sqltype = ipara->SQLType;
	param_pgtype = PIC_dsp_pgtype(qb->conn, *ipara);

	/* XXX: should we use param_pgtype here instead? */
	*pgType = sqltype_to_bind_pgtype(conn, param_sqltype);

	if (0 == param_sqltype) /* calling SQLSetStmtAttr(.., SQL_ATTR_APP_PARAM_DES, an ARD of another statement) may cause this */
	{
		if (0 != param_pgtype)
		{
			param_sqltype = pgtype_attr_to_concise_type(conn, param_pgtype, PG_ATP_UNSET, PG_ADT_UNSET, PG_UNKNOWNS_UNSET);
			MYLOG(0, "convert from pgtype(%u) to sqltype(%d)\n", param_pgtype, param_sqltype);
		}
	}

	MYLOG(0, "from(fcType)=%d, to(fSqlType)=%d(%u), *pgType=%u\n",
		  param_ctype, param_sqltype, param_pgtype, *pgType);

	/* Handle NULL parameter data */
	if (SQL_PARAM_OUTPUT == ipara->paramType
	    || used == SQL_NULL_DATA
	    || used == SQL_DEFAULT_PARAM)
	{
		*isnull = TRUE;
		if (!req_bind)
			CVT_APPEND_STR(qb, "NULL");
		return SQL_SUCCESS;
	}

	/*
	 * If no buffer, and it's not null, then what the hell is it? Just
	 * leave it alone then.
	 */
	if (!buffer)
	{
		if (qb->param_mode == RPM_FAKE_PARAMS)
		{
			CVT_APPEND_STR(qb, "NULL");
			qb->flags |= FLGB_INACCURATE_RESULT;
			return SQL_SUCCESS;
		}
		else if (!handling_large_object)
		{
			/* shouldn't happen */
			qb->errormsg = "unexpected NULL parameter value";
			qb->errornumber = STMT_EXEC_ERROR;
			return SQL_ERROR;
		}
	}

	/* replace DEFAULT with something we can use */
	if (param_ctype == SQL_C_DEFAULT)
	{
		param_ctype = sqltype_to_default_ctype(conn, param_sqltype);
#ifdef	UNICODE_SUPPORT
		if (param_ctype == SQL_C_WCHAR
		    && CC_default_is_c(conn))
			param_ctype =SQL_C_CHAR;
#endif
	}

	allocbuf = NULL;
	send_buf = NULL;
	param_string[0] = '\0';
	cbuf[0] = '\0';
	memset(&st, 0, sizeof(st));

	ivstruct = (SQL_INTERVAL_STRUCT *) buffer;
	/* Convert input C type to a neutral format */
#ifdef	UNICODE_SUPPORT
	if (get_convtype() > 0) /* coversion between the current locale is available */
	{
		BOOL	wcs_debug = conn->connInfo.wcs_debug;
		BOOL	is_utf8 = (UTF8 == conn->ccsc);
		BOOL	same_encoding = (conn->ccsc == pg_CS_code(conn->locale_encoding));

		switch (param_ctype)
		{
			case SQL_C_CHAR:
				if (!same_encoding || wcs_debug)
				{
					SQLLEN	paralen = used;

					MYLOG(0, "locale param convert\n");
					if ((used = bindpara_msg_to_utf8(buffer, &allocbuf, used)) < 0)
					{
						if (!handle_lu_onvert_error(qb, convert_err_flag, buffer, paralen))
							goto cleanup;
						send_buf = NULL_STRING;
						used = 0;
					}
					else
						send_buf = allocbuf;
				}
				break;
			case SQL_C_WCHAR:
				if (!is_utf8 || (same_encoding && wcs_debug))
				{
					MYLOG(0, "hybrid param convert\n");
					if ((used = bindpara_wchar_to_msg((SQLWCHAR *) buffer, &allocbuf, used)) < 0)
					{
						qb->errormsg = "Could not convert from wide characters to the current locale";
						qb->errornumber = STMT_EXEC_ERROR;
						goto cleanup;
					}
					send_buf = allocbuf;
				}
				break;
		}
	}
#endif /* UNICODE_SUPPORT */
	switch (param_ctype)
	{
		case SQL_C_BINARY:
			send_buf = buffer;
			break;
		case SQL_C_CHAR:
			if (NULL == send_buf)
				send_buf = buffer;
			break;

#ifdef	UNICODE_SUPPORT
		case SQL_C_WCHAR:
MYLOG(0, " C_WCHAR=%d contents=%s(" FORMAT_LEN ")\n", param_ctype, buffer, used);
			if (NULL == send_buf)
			{
				allocbuf = ucs2_to_utf8((SQLWCHAR *) buffer, used > 0 ? used / WCLEN : used, &used, FALSE);
				send_buf = allocbuf;
			}
			break;
#endif /* UNICODE_SUPPORT */

		case SQL_C_DOUBLE:
			dbv = *((SDOUBLE *) buffer);
#ifdef	WIN32
			if (_finite(dbv))
#endif /* WIN32 */
			{
				SPRINTF_FIXED(param_string, "%.*g", PG_DOUBLE_DIGITS, dbv);
				set_server_decimal_point(param_string, SQL_NTS);
			}
#ifdef	WIN32
			else if (_isnan(dbv))
				STRCPY_FIXED(param_string, NAN_STRING);
			else if (dbv < .0)
				STRCPY_FIXED(param_string, MINFINITY_STRING);
			else
				STRCPY_FIXED(param_string, INFINITY_STRING);
#endif /* WIN32 */
			break;

		case SQL_C_FLOAT:
			flv = *((SFLOAT *) buffer);
#ifdef	WIN32
			if (_finite(flv))
#endif /* WIN32 */
			{
				SPRINTF_FIXED(param_string, "%.*g", PG_REAL_DIGITS, flv);
				set_server_decimal_point(param_string, SQL_NTS);
			}
#ifdef	WIN32
			else if (_isnan(flv))
				STRCPY_FIXED(param_string, NAN_STRING);
			else if (flv < .0)
				STRCPY_FIXED(param_string, MINFINITY_STRING);
			else
				STRCPY_FIXED(param_string, INFINITY_STRING);
#endif /* WIN32 */
			break;

		case SQL_C_SLONG:
		case SQL_C_LONG:
			SPRINTF_FIXED(param_string, FORMAT_INTEGER,
					*((SQLINTEGER *) buffer));
			break;

#ifdef ODBCINT64
		case SQL_C_SBIGINT:
		case SQL_BIGINT: /* Is this needed ? */
			SPRINTF_FIXED(param_string, FORMATI64,
					*((SQLBIGINT *) buffer));
			break;

		case SQL_C_UBIGINT:
			SPRINTF_FIXED(param_string, FORMATI64U,
					*((SQLUBIGINT *) buffer));
			break;

#endif /* ODBCINT64 */
		case SQL_C_SSHORT:
		case SQL_C_SHORT:
			ITOA_FIXED(param_string, *((SQLSMALLINT *) buffer));
			break;

		case SQL_C_STINYINT:
		case SQL_C_TINYINT:
			ITOA_FIXED(param_string, *((SCHAR *) buffer));
			break;

		case SQL_C_ULONG:
			SPRINTF_FIXED(param_string, FORMAT_UINTEGER,
					*((SQLUINTEGER *) buffer));
			break;

		case SQL_C_USHORT:
			SPRINTF_FIXED(param_string, "%u",
					*((SQLUSMALLINT *) buffer));
			break;

		case SQL_C_UTINYINT:
			SPRINTF_FIXED(param_string, "%u",
					*((UCHAR *) buffer));
			break;

		case SQL_C_BIT:
			{
				int	i = *((UCHAR *) buffer);

				ITOA_FIXED(param_string, i ? 1 : 0);
				break;
			}

		case SQL_C_DATE:
		case SQL_C_TYPE_DATE:		/* 91 */
			{
				DATE_STRUCT *ds = (DATE_STRUCT *) buffer;

				st.m = ds->month;
				st.d = ds->day;
				st.y = ds->year;

				break;
			}

		case SQL_C_TIME:
		case SQL_C_TYPE_TIME:		/* 92 */
			{
				TIME_STRUCT *ts = (TIME_STRUCT *) buffer;

				st.hh = ts->hour;
				st.mm = ts->minute;
				st.ss = ts->second;
				/*
				 * Initialize date in case conversion destination
				 * expects date part from this source time data.
				 */
				tim = SC_get_localtime(qb->stmt);
				st.m = tim->tm_mon + 1;
				st.d = tim->tm_mday;
				st.y = tim->tm_year + 1900;
				break;
			}

		case SQL_C_TIMESTAMP:
		case SQL_C_TYPE_TIMESTAMP:	/* 93 */
			{
				TIMESTAMP_STRUCT *tss = (TIMESTAMP_STRUCT *) buffer;

				st.m = tss->month;
				st.d = tss->day;
				st.y = tss->year;
				st.hh = tss->hour;
				st.mm = tss->minute;
				st.ss = tss->second;
				st.fr = tss->fraction;

				MYLOG(0, "m=%d,d=%d,y=%d,hh=%d,mm=%d,ss=%d\n", st.m, st.d, st.y, st.hh, st.mm, st.ss);

				break;

			}
		case SQL_C_NUMERIC:
		{
			ResolveNumericParam((SQL_NUMERIC_STRUCT *) buffer, param_string);
			break;
		}
		case SQL_C_INTERVAL_YEAR:
			ivsign = ivstruct->interval_sign ? "-" : "";
			SPRINTF_FIXED(param_string, "%s%u years", ivsign, (unsigned int) ivstruct->intval.year_month.year);
			break;
		case SQL_C_INTERVAL_MONTH:
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
			ivsign = ivstruct->interval_sign ? "-" : "";
			SPRINTF_FIXED(param_string, "%s%u years %s%u mons", ivsign, (unsigned int) ivstruct->intval.year_month.year, ivsign, (unsigned int) ivstruct->intval.year_month.month);
			break;
		case SQL_C_INTERVAL_DAY:
			ivsign = ivstruct->interval_sign ? "-" : "";
			SPRINTF_FIXED(param_string, "%s%u days", ivsign, (unsigned int) ivstruct->intval.day_second.day);
			break;
		case SQL_C_INTERVAL_HOUR:
		case SQL_C_INTERVAL_DAY_TO_HOUR:
			ivsign = ivstruct->interval_sign ? "-" : "";
			SPRINTF_FIXED(param_string, "%s%u days %s%02u:00:00", ivsign, (unsigned int) ivstruct->intval.day_second.day, ivsign, (unsigned int) ivstruct->intval.day_second.hour);
			break;
		case SQL_C_INTERVAL_MINUTE:
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
			ivsign = ivstruct->interval_sign ? "-" : "";
			SPRINTF_FIXED(param_string, "%s%u days %s%02u:%02u:00", ivsign, (unsigned int) ivstruct->intval.day_second.day, ivsign, (unsigned int) ivstruct->intval.day_second.hour, (unsigned int) ivstruct->intval.day_second.minute);
			break;

		case SQL_C_INTERVAL_SECOND:
		case SQL_C_INTERVAL_DAY_TO_SECOND:
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
			ivsign = ivstruct->interval_sign ? "-" : "";
			SPRINTF_FIXED(param_string, "%s%u days %s%02u:%02u:%02u",
					ivsign, (unsigned int) ivstruct->intval.day_second.day,
					ivsign, (unsigned int) ivstruct->intval.day_second.hour,
					(unsigned int) ivstruct->intval.day_second.minute,
					(unsigned int) ivstruct->intval.day_second.second);
			if (ivstruct->intval.day_second.fraction > 0)
			{
				int fraction = ivstruct->intval.day_second.fraction, prec = apara->precision;

				while (fraction % 10 == 0)
				{
					fraction /= 10;
					prec--;
				}
				SPRINTFCAT_FIXED(param_string, ".%0*d", prec, fraction);
			}
			break;
		case SQL_C_GUID:
		{
			/*
			 * SQLGUID.Data1 is an "unsigned long" on some platforms, and
			 * "unsigned int" on others.
			 */
			SQLGUID *g = (SQLGUID *) buffer;
			SPRINTF_FIXED (param_string,
				"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
				(unsigned int) g->Data1,
				g->Data2, g->Data3,
				g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3],
				g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
		}
		break;
		default:
			/* error */
			qb->errormsg = "Unrecognized C_parameter type in copy_statement_with_parameters";
			qb->errornumber = STMT_NOT_IMPLEMENTED_ERROR;
			CVT_TERMINATE(qb);	/* just in case */
			goto cleanup;
	}

	/*
	 * Now that the input data is in a neutral format, convert it to
	 * the desired output format (sqltype)
	 */

	/* Special handling NULL string For FOXPRO */
MYLOG(0, "cvt_null_date_string=%d pgtype=%d send_buf=%p\n", conn->connInfo.cvt_null_date_string, param_pgtype, send_buf);
	if (conn->connInfo.cvt_null_date_string > 0 &&
	    (PG_TYPE_DATE == param_pgtype ||
	     PG_TYPE_DATETIME == param_pgtype ||
	     PG_TYPE_TIMESTAMP_NO_TMZONE == param_pgtype) &&
	    NULL != send_buf &&
	    (
		(SQL_C_CHAR == param_ctype && '\0' == send_buf[0])
#ifdef	UNICODE_SUPPORT
		|| (SQL_C_WCHAR ==param_ctype && '\0' == send_buf[0] && '\0' == send_buf[1])
#endif /* UNICODE_SUPPORT */
	    ))
	{
		*isnull = TRUE;
		if (!req_bind)
			CVT_APPEND_STR(qb, "NULL");
		retval = SQL_SUCCESS;
		goto cleanup;
	}

	/*
	 * We now have the value we want to print in one of these three canonical
	 * formats:
	 *
	 * 1. As a string in 'send_buf', with length indicated by 'used' (can be
	 *    SQL_NTS).
	 * 2. As a null-terminated string in 'param_string'.
	 * 3. Time-related fields in 'st'.
	 */

	/*
	 * For simplicity, fold the param_string representation into 'send_buf'.
	 */
	if (!send_buf && param_string[0])
	{
		send_buf = param_string;
		used = SQL_NTS;
	}

	/*
	 * Do some further processing to create the final string we want to output.
	 * This will use the fields in 'st' to create a string if it's a time/date
	 * value, and do some other conversions.
	 */
	switch (param_sqltype)
	{
		case SQL_CHAR:
		case SQL_VARCHAR:
		case SQL_LONGVARCHAR:
#ifdef	UNICODE_SUPPORT
		case SQL_WCHAR:
		case SQL_WVARCHAR:
		case SQL_WLONGVARCHAR:
#endif /* UNICODE_SUPPORT */
		case SQL_BIT:

			/* Special handling for some column types */
			switch (param_pgtype)
			{
				case PG_TYPE_BOOL:
					/*
					 * consider True is -1 case.
					 *
					 * FIXME: This actually matches anything that begins
					 * with -1, like "-1234" or "-1foobar". Is that
					 * intentional?
					 */
					if (NULL != send_buf && '-' == send_buf[0] && '1' == send_buf[1])
					{
						send_buf = "1";
						used = 1;
					}
					break;
				case PG_TYPE_FLOAT4:
				case PG_TYPE_FLOAT8:
				case PG_TYPE_NUMERIC:
					if (NULL != send_buf)
						set_server_decimal_point((char *) send_buf, used);
					break;
			}
			if (!send_buf)
			{
				/* it was date,time,timestamp -- use m,d,y,hh,mm,ss */
				SPRINTF_FIXED(tmp, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
						st.y, st.m, st.d, st.hh, st.mm, st.ss);
				send_buf = tmp;
				used = SQL_NTS;
			}
			break;

		case SQL_DATE:
		case SQL_TYPE_DATE:	/* 91 */
			if (send_buf)
			{				/* copy char data to time */
				my_strcpy(cbuf, sizeof(cbuf), send_buf, used);
				parse_datetime(cbuf, &st);
			}

			if (st.y < 0)
				SPRINTF_FIXED(tmp, "%.4d-%.2d-%.2d BC", -st.y, st.m, st.d);
			else
				SPRINTF_FIXED(tmp, "%.4d-%.2d-%.2d", st.y, st.m, st.d);
			lastadd = "::date";
			send_buf = tmp;
			used = SQL_NTS;
			break;

		case SQL_TIME:
		case SQL_TYPE_TIME:	/* 92 */
			if (send_buf)
			{				/* copy char data to time */
				my_strcpy(cbuf, sizeof(cbuf), send_buf, used);
				parse_datetime(cbuf, &st);
			}

			if (st.fr > 0)
			{
				int	wdt;
				int	fr = effective_fraction(st.fr, &wdt);
				SPRINTF_FIXED(tmp, "%.2d:%.2d:%.2d.%0*d", st.hh, st.mm, st.ss, wdt, fr);
			}
			else
				SPRINTF_FIXED(tmp, "%.2d:%.2d:%.2d", st.hh, st.mm, st.ss);
			lastadd = "::time";
			send_buf = tmp;
			used = SQL_NTS;
			break;

		case SQL_TIMESTAMP:
		case SQL_TYPE_TIMESTAMP:	/* 93 */
			if (send_buf)
			{
				my_strcpy(cbuf, sizeof(cbuf), send_buf, used);
				parse_datetime(cbuf, &st);
			}

			/*
			 * SPRINTF_FIXED(tmp, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", st.y,
			 * st.m, st.d, st.hh, st.mm, st.ss);
			 */
			/* Time zone stuff is unreliable */
			stime2timestamp(&st, tmp, sizeof(tmp), USE_ZONE, 6);
			lastadd = "::timestamp";
			send_buf = tmp;
			used = SQL_NTS;
			break;

		case SQL_BINARY:
		case SQL_VARBINARY:
		case SQL_LONGVARBINARY:
			switch (param_ctype)
			{
				case SQL_C_BINARY:
					break;
				case SQL_C_CHAR:
#ifdef	UNICODE_SUPPORT
				case SQL_C_WCHAR:
#endif /* UNICODE_SUPPORT */
					switch (used)
					{
						case SQL_NTS:
							used = strlen(send_buf);
							break;
					}
					allocbuf = malloc(used / 2 + 1);
					if (allocbuf)
					{
						pg_hex2bin(send_buf, allocbuf, used);
						send_buf = allocbuf;
						used /= 2;
					}
					break;
				default:
					qb->errormsg = "Could not convert the ctype to binary type";
					qb->errornumber = STMT_EXEC_ERROR;
					goto cleanup;
			}
			if (param_pgtype == PG_TYPE_BYTEA)
			{
				if (0 != (qb->flags & FLGB_BINARY_AS_POSSIBLE))
				{
					MYLOG(0, "sending binary data leng=" FORMAT_LEN "\n", used);
					*isbinary = TRUE;
				}
				else
				{
					/* non-ascii characters should be
					 * converted to octal
					 */
					MYLOG(0, "SQL_VARBINARY: about to call convert_to_pgbinary, used = " FORMAT_LEN "\n", used);
					final_binary_convert = TRUE;
				}
				break;
			}
			if (PG_TYPE_OID == param_pgtype && conn->lo_is_domain)
				;
			else if (param_pgtype != conn->lobj_type)
			{
				qb->errormsg = "Could not convert binary other than LO type";
				qb->errornumber = STMT_EXEC_ERROR;
				goto cleanup;
			}

			if (apara->data_at_exec)
				lobj_oid = pdata->pdata[param_number].lobj_oid;
			else
			{
				BOOL	is_in_trans_at_entry = CC_is_in_trans(conn);
				int		write_result;

				/* begin transaction if needed */
				if (!is_in_trans_at_entry)
				{
					if (!CC_begin(conn))
					{
						qb->errormsg = "Could not begin (in-line) a transaction";
						qb->errornumber = STMT_EXEC_ERROR;
						goto cleanup;
					}
				}

				/* store the oid */
				lobj_oid = odbc_lo_creat(conn, INV_READ | INV_WRITE);
				if (lobj_oid == 0)
				{
					qb->errornumber = STMT_EXEC_ERROR;
					qb->errormsg = "Couldn't create (in-line) large object.";
					goto cleanup;
				}

				/* store the fd */
				lobj_fd = odbc_lo_open(conn, lobj_oid, INV_WRITE);
				if (lobj_fd < 0)
				{
					qb->errornumber = STMT_EXEC_ERROR;
					qb->errormsg = "Couldn't open (in-line) large object for writing.";
					goto cleanup;
				}

				write_result = odbc_lo_write(conn, lobj_fd, buffer, (Int4) used);
				if (write_result < 0)
				{
					qb->errornumber = STMT_EXEC_ERROR;
					qb->errormsg = "Couldn't write to (in-line) large object.";
					goto cleanup;
				}

				odbc_lo_close(conn, lobj_fd);

				/* commit transaction if needed */
				if (!is_in_trans_at_entry)
				{
					if (!CC_commit(conn))
					{
						qb->errormsg = "Could not commit (in-line) a transaction";
						qb->errornumber = STMT_EXEC_ERROR;
						goto cleanup;
					}
				}
			}

			/*
			 * the oid of the large object -- just put that in for the
			 * parameter marker -- the data has already been sent to
			 * the large object
			 */
			SPRINTF_FIXED(param_string, "%u", lobj_oid);
			lastadd = "::lo";
			send_buf = param_string;
			used = SQL_NTS;
			break;

			/*
			 * because of no conversion operator for bool and int4,
			 * SQL_BIT
			 */
			/* must be quoted (0 or 1 is ok to use inside the quotes) */

		case SQL_REAL:
			set_server_decimal_point((char *) send_buf, used);
			lastadd = "::float4";
			break;
		case SQL_FLOAT:
		case SQL_DOUBLE:
			set_server_decimal_point((char *) send_buf, used);
			lastadd = "::float8";
			break;
		case SQL_NUMERIC:
			break;
		/*
		 * If it looks like a valid integer, we can pass it without quotes
		 * and let the server interpret it. Arguably, it would always be
		 * better to explicitly pass it as 'xxx'::integer or 'xxx'::smallint,
		 * but historically we haven't done that, so let's avoid changing the
		 * behaviour.
		 *
		 * If it's a negative number, we have to wrap it in parens. Otherwise
		 * a query like "SELECT 0-?" would turn into "SELECT 0--123".
		 */
		case SQL_INTEGER:
			if (valid_int_literal(send_buf, used, &negative))
			{
				need_quotes = FALSE;
				add_parens = negative;
			}
			else
			{
				/*
				 * Doesn't look like a valid integer. The server will most
				 * likely throw an error, unless it's in some format we don't
				 * recognize but the server does.
				 */
				lastadd = "::int4";
			}
			break;
		case SQL_SMALLINT:
			if (valid_int_literal(send_buf, used, &negative))
			{
				need_quotes = FALSE;
				add_parens = negative;
			}
			else
				lastadd = "::smallint";
			break;
		default:			/* a numeric type or SQL_BIT */
			break;
	}

	if (!send_buf)
	{
		qb->errormsg = "Could not convert parameter ctype to sqltype";
		qb->errornumber = STMT_EXEC_ERROR;
		goto cleanup;
	}
	if (used == SQL_NTS)
		used = strlen(send_buf);

	/*
	 * Ok, we now have the final string representation in 'send_buf', length 'used'.
	 * We're ready to output the final string, with quotes and other
	 * embellishments if necessary.
	 *
	 * In bind-mode, we don't need to do any quoting.
	 */
	if (req_bind)
		CVT_APPEND_DATA(qb, send_buf, used);
	else
	{
		if (add_parens)
			CVT_APPEND_CHAR(qb, '(');

		if (need_quotes)
		{
			if ((qb->flags & FLGB_LITERAL_EXTENSION) != 0)
				CVT_APPEND_CHAR(qb, LITERAL_EXT);
			CVT_APPEND_CHAR(qb, LITERAL_QUOTE);

			if (final_binary_convert)
				CVT_APPEND_BINARY(qb, send_buf, used);
			else
				CVT_SPECIAL_CHARS(qb, send_buf, used);

			CVT_APPEND_CHAR(qb, LITERAL_QUOTE);
		}
		else
			CVT_APPEND_DATA(qb, send_buf, used);

		if (add_parens)
			CVT_APPEND_CHAR(qb, ')');
		if (lastadd && (FLGB_PARAM_CAST & qb->flags) != 0)
			CVT_APPEND_STR(qb, lastadd);
	}

	retval = SQL_SUCCESS;
cleanup:
	if (allocbuf)
		free(allocbuf);
	return retval;
}


static const char *
mapFunction(const char *func, int param_count, const char *keyword)
{
	int			i;
	const char *p1, *p2;

	for (i = 0; (p1 = mapFuncs[i].odbc_name) != NULL; i++)
	{
		if (p1[0] == '%')
		{
			if (p1[1] - '0' == param_count &&
			    !stricmp(p1 + 2, func))
				return mapFuncs[i].pgsql_name;
		}
		else if (!stricmp(p1, func))
			return mapFuncs[i].pgsql_name;
		else if (p2 = strchr(p1, (int) '('), NULL != p2)
		{
			int len = (int) (p2 - mapFuncs[i].odbc_name);

			if (strlen(func) == len &&
			    !strnicmp(p1, func, len) &&
			    !stricmp(p2 + 1, keyword))
				return mapFuncs[i].pgsql_name;
		}
	}

	return NULL;
}

/*
 * processParameters()
 * Process function parameters and work with embedded escapes sequences.
 */
static int
processParameters(QueryParse *qp, QueryBuild *qb,
		size_t *output_count, SQLLEN param_pos[][2])
{
	int retval, innerParenthesis, param_count;
	BOOL stop;

	/* begin with outer '(' */
	innerParenthesis = 0;
	param_count = 0;
	if (NULL != output_count)
		*output_count = 0;
	stop = FALSE;
	for (; F_OldPos(qp) < qp->stmt_len; F_OldNext(qp))
	{
		retval = inner_process_tokens(qp, qb);
		if (retval == SQL_ERROR)
			return retval;
		if (MBCS_NON_ASCII(qp->encstr))
			continue;
		if (!QP_in_idle_status(qp))
			continue;

		switch (F_OldChar(qp))
		{
			case ',':
				if (1 == innerParenthesis)
				{
					param_pos[param_count][1] = F_NewPos(qb) - 2;
					param_count++;
					param_pos[param_count][0] = F_NewPos(qb);
					param_pos[param_count][1] = -1;
				}
				break;
			case '(':
				if (0 == innerParenthesis)
				{
					param_pos[param_count][0] = F_NewPos(qb);
					param_pos[param_count][1] = -1;
				}
				innerParenthesis++;
				break;

			case ')':
				innerParenthesis--;
				if (0 == innerParenthesis)
				{
					param_pos[param_count][1] = F_NewPos(qb) - 2;
					param_count++;
					param_pos[param_count][0] =
					param_pos[param_count][1] = -1;
				}
				if (output_count)
					*output_count = F_NewPos(qb);
				break;

			case ODBC_ESCAPE_END:
				stop = (0 == innerParenthesis);
				break;

		}
		if (stop) /* returns with the last } position */
			break;
	}
	if (param_pos[param_count][0] >= 0)
	{
		MYLOG(0, "closing ) not found %d\n", innerParenthesis);
		qb->errornumber = STMT_EXEC_ERROR;
		qb->errormsg = "processParameters closing ) not found";
		return SQL_ERROR;
	}
	else if (1 == param_count) /* the 1 parameter is really valid ? */
	{
		BOOL	param_exist = FALSE;
		SQLLEN	i;

		for (i = param_pos[0][0]; i <= param_pos[0][1]; i++)
		{
			if (IS_NOT_SPACE(qb->query_statement[i]))
			{
				param_exist = TRUE;
				break;
			}
		}
		if (!param_exist)
		{
			param_pos[0][0] = param_pos[0][1] = -1;
		}
	}

	return SQL_SUCCESS;
}

/*
 * convert_escape()
 * This function doesn't return a pointer to static memory any longer !
 */
static int
convert_escape(QueryParse *qp, QueryBuild *qb)
{
	RETCODE	retval = SQL_SUCCESS;
	char		buf[1024], buf_small[128], key[65];
	UCHAR	ucv;
	UInt4		prtlen;

	QueryBuild	nqb;
	BOOL	nqb_is_valid = FALSE;

	if (F_OldChar(qp) == ODBC_ESCAPE_START) /* skip the first { */
		F_OldNext(qp);
	/* Separate off the key, skipping leading and trailing whitespace */
	while ((ucv = F_OldChar(qp)) != '\0' && isspace(ucv))
		F_OldNext(qp);
	/*
	 * procedure calls
	 */
	/* '?=' to accept return values exists ? */
	if (F_OldChar(qp) == '?')
	{
		qb->param_number++;
		qb->proc_return = 1;
		if (qb->stmt)
			qb->stmt->proc_return = 1;
		while (isspace((UCHAR) qp->statement[++qp->opos]));
		if (F_OldChar(qp) != '=')
		{
			F_OldPrior(qp);
			return SQL_SUCCESS;
		}
		while (isspace((UCHAR) qp->statement[++qp->opos]));
	}

	sscanf(F_OldPtr(qp), "%32s", key);
	while ((ucv = F_OldChar(qp)) != '\0' && (IS_NOT_SPACE(ucv)))
		F_OldNext(qp);
	while ((ucv = F_OldChar(qp)) != '\0' && isspace(ucv))
		F_OldNext(qp);

	/* Avoid the concatenation of the function name with the previous word. Aceto */

	if (stricmp(key, "call") == 0)
	{
		size_t funclen;
		const UCHAR *next_token;

		if (SQL_ERROR == QB_start_brace(qb))
		{
			retval = SQL_ERROR;
			goto cleanup;
		}
		if (qb->num_io_params > 1 ||
		    (0 == qb->proc_return))
			CVT_APPEND_STR(qb, "SELECT * FROM ");
		else
			CVT_APPEND_STR(qb, "SELECT ");
		funclen = findIdentifier((const UCHAR *) F_OldPtr(qp), qb->ccsc, &next_token);
		if (next_token && ODBC_ESCAPE_END == *next_token)
		{
			CVT_APPEND_DATA(qb, F_OldPtr(qp), funclen);
			CVT_APPEND_STR(qb, "()");
			if (SQL_ERROR == QB_end_brace(qb))
			{
				retval = SQL_ERROR;
				goto cleanup;
			}
			/* positioned at } */
			qp->opos += ((const char *) next_token - F_OldPtr(qp));
		}
		else
		{
			/* Continue at inner_process_tokens loop */
			F_OldPrior(qp);
			return SQL_SUCCESS;
		}
	}
	else if (stricmp(key, "d") == 0)
	{
		/* Literal; return the escape part adding type cast */
		F_ExtractOldTo(qp, buf_small, ODBC_ESCAPE_END, sizeof(buf_small));
		prtlen = SPRINTF_FIXED(buf, "%s::date", buf_small);
		CVT_APPEND_DATA(qb, buf, prtlen);
		retval = QB_append_space_to_separate_identifiers(qb, qp);
	}
	else if (stricmp(key, "t") == 0)
	{
		/* Literal; return the escape part adding type cast */
		F_ExtractOldTo(qp, buf_small, ODBC_ESCAPE_END, sizeof(buf_small));
		prtlen = SPRINTF_FIXED(buf, "%s::time", buf_small);
		CVT_APPEND_DATA(qb, buf, prtlen);
		retval = QB_append_space_to_separate_identifiers(qb, qp);
	}
	else if (stricmp(key, "ts") == 0)
	{
		/* Literal; return the escape part adding type cast */
		F_ExtractOldTo(qp, buf_small, ODBC_ESCAPE_END, sizeof(buf_small));
		prtlen = SPRINTF_FIXED(buf, "%s::timestamp", buf_small);
		CVT_APPEND_DATA(qb, buf, prtlen);
		retval = QB_append_space_to_separate_identifiers(qb, qp);
	}
	else if (stricmp(key, "oj") == 0) /* {oj syntax support for 7.1 * servers */
	{
		if (qb->stmt)
			SC_set_outer_join(qb->stmt);
		retval = QB_start_brace(qb);
		/* Continue at inner_process_tokens loop */
		F_OldPrior(qp);
		goto cleanup;
	}
	else if (stricmp(key, "escape") == 0) /* like escape syntax support for 7.1+ servers */
	{
		/* Literal; return the escape part adding type cast */
		F_ExtractOldTo(qp, buf_small, ODBC_ESCAPE_END, sizeof(buf_small));
		prtlen = SPRINTF_FIXED(buf, "%s %s", key, buf_small);
		CVT_APPEND_DATA(qb, buf, prtlen);
		retval = QB_append_space_to_separate_identifiers(qb, qp);
	}
	else if (stricmp(key, "fn") == 0)
	{
		const char *mapExpr;
		int	i, param_count;
		SQLLEN	from, to;
		size_t	param_consumed;
		SQLLEN	param_pos[16][2];
		BOOL	cvt_func = FALSE;

		/* Separate off the func name, skipping leading and trailing whitespace */
		i = 0;
		while ((ucv = F_OldChar(qp)) != '\0' && ucv != '(' &&
			   (IS_NOT_SPACE(ucv)))
		{
			if (i < sizeof(key) - 1)
				key[i++] = ucv;
			F_OldNext(qp);
		}
		key[i] = '\0';
		while ((ucv = F_OldChar(qp)) != '\0' && isspace(ucv))
			F_OldNext(qp);

		/*
		 * We expect left parenthesis here, else return fn body as-is
		 * since it is one of those "function constants".
		 */
		if (F_OldChar(qp) != '(')
		{
			CVT_APPEND_STR(qb, key);
			goto cleanup;
		}

		/*
		 * Process parameter list and inner escape
		 * sequences
		 * Aceto 2002-01-29
		 */

		QB_initialize_copy(&nqb, qb, 1024);
		nqb_is_valid = TRUE;
		if (retval = processParameters(qp, &nqb, &param_consumed, param_pos), retval == SQL_ERROR)
		{
			qb->errornumber = nqb.errornumber;
			qb->errormsg = nqb.errormsg;
			goto cleanup;
		}

		for (param_count = 0;; param_count++)
		{
			if (param_pos[param_count][0] < 0)
				break;
		}
		if (param_count == 1 &&
		    param_pos[0][1] < param_pos[0][0])
			param_count = 0;

		mapExpr = NULL;
		if (stricmp(key, "convert") == 0)
			cvt_func = TRUE;
		else
		{
			char keyword[64] = "";

			if (param_count > 0)
			{
				int i, from, to;
				const char * p;

				for (i = param_pos[0][0], p = nqb.query_statement + i; i <= param_pos[0][1] && isspace(*p); i++, p++)
					;
				from = i;
				for (; i <= param_pos[0][1] && IS_NOT_SPACE(*p); i++, p++)
					;
				to = i - 1;
				if (to >= from)
				{
					int	len = to - from + 1;

					if (len < sizeof(keyword))
					{
						memcpy(keyword, nqb.query_statement + from, len);
						keyword[len] = '\0';
					}
				}
			}
			mapExpr = mapFunction(key, param_count, keyword);
		}
		if (cvt_func)
		{
			if (2 == param_count)
			{
				BOOL add_cast = FALSE, add_quote = FALSE;
				const char *pptr;

				from = param_pos[0][0];
				to = param_pos[0][1];
				for (pptr = nqb.query_statement + from; *pptr && isspace((unsigned char) *pptr); pptr++)
					;
				if (LITERAL_QUOTE == *pptr)
					;
				else if ('-' == *pptr)
					add_quote = TRUE;
				else if (isdigit((unsigned char) *pptr))
					add_quote = TRUE;
				else
					add_cast = TRUE;
				if (add_quote)
					CVT_APPEND_CHAR(qb, LITERAL_QUOTE);
				else if (add_cast)
					CVT_APPEND_CHAR(qb, '(');
				CVT_APPEND_DATA(qb, nqb.query_statement + from, to - from + 1);
				if (add_quote)
					CVT_APPEND_CHAR(qb, LITERAL_QUOTE);
				else if (add_cast)
				{
					const char *cast_form = NULL;
					char	sqltype[32];
					int	typel;

					CVT_APPEND_CHAR(qb, ')');
					from = param_pos[1][0];
					to = param_pos[1][1];
					typel = to - from + 1;
					if (typel < sizeof(sqltype))
					{
						const char *type;

						memcpy(sqltype, nqb.query_statement + from, typel);
						sqltype[typel] = '\0';
MYLOG(0, FORMAT_LEN "-" FORMAT_LEN " SQLtype=%s SQL_BIT=%d\n", to, from, sqltype, SQL_BIT);
						for (type = sqltype; *type && isspace(*type); type++)
							;
						if (strncmp(type, "SQL_", 4) == 0)
						{
							type += 4;
							if (strcmp(type, "INTEGER") == 0)
								cast_form = "int4";
							else if (strcmp(type, "CHAR") == 0)
								cast_form = "varchar";
							else if (strcmp(type, "VARCHAR") == 0)
								cast_form = "varchar";
							else if (strcmp(type, "LONGVARCHAR") == 0)
								cast_form = "text";
							else if (strcmp(type, "WCHAR") == 0)
								cast_form = "varchar";
							else if (strcmp(type, "WVARCHAR") == 0)
								cast_form = "varchar";
							else if (strcmp(type, "WLONGVARCHAR") == 0)
								cast_form = "text";
							else if (strcmp(type, "NUMERIC") == 0)
								cast_form = "numeric";
							else if (strcmp(type, "DOUBLE") == 0)
								cast_form = "float8";
							else if (strcmp(type, "FLOAT") == 0)
								cast_form = "float8";
							else if (strcmp(type, "REAL") == 0)
								cast_form = "float4";
							else if (strcmp(type, "BIGINT") == 0)
								cast_form = "int8";
							else if (strcmp(type, "DECIMAL") == 0)
								cast_form = "numeric";
							else if (strcmp(type, "SMALLINT") == 0)
								cast_form = "int2";
							else if (strcmp(type, "TYPE_DATE") == 0)
								cast_form = "date";
							else if (strcmp(type, "TYPE_TIME") == 0)
								cast_form = "time";
							else if (strcmp(type, "TYPE_TIMESTAMP") == 0)
								cast_form = "timestamp";
							else if (strcmp(type, "BIT") == 0)
								cast_form = "bit";
						}
					}
					if (NULL != cast_form)
					{
						CVT_APPEND_STR(qb, "::");
						CVT_APPEND_STR(qb, cast_form);
					}
				}
			}
			else
			{
				qb->errornumber = STMT_EXEC_ERROR;
				qb->errormsg = "convert param count must be 2";
				retval = SQL_ERROR;
			}
		}
		else if (mapExpr == NULL)
		{
			CVT_APPEND_STR(qb, key);
			CVT_APPEND_DATA(qb, nqb.query_statement, nqb.npos);
		}
		else
		{
			const char *mapptr;
			SQLLEN	paramlen;
			int	pidx;

			for (mapptr = mapExpr; *mapptr; mapptr++)
			{
				if (*mapptr != '$')
				{
					CVT_APPEND_CHAR(qb, *mapptr);
					continue;
				}
				mapptr++;
				if (*mapptr == '*')
				{
					from = 1;
					to = param_consumed - 2;
				}
				else if (isdigit((unsigned char) *mapptr))
				{
					pidx = *mapptr - '0' - 1;
					if (pidx < 0 ||
					    param_pos[pidx][0] < 0)
					{
						qb->errornumber = STMT_EXEC_ERROR;
						qb->errormsg = "param not found";
						MYLOG(0, "%dth param not found for the expression %s\n", pidx + 1, mapExpr);
						retval = SQL_ERROR;
						break;
					}
					from = param_pos[pidx][0];
					to = param_pos[pidx][1];
				}
				else
				{
					qb->errornumber = STMT_EXEC_ERROR;
					qb->errormsg = "internal expression error";
					MYLOG(0, "internal expression error %s\n", mapExpr);
					retval = SQL_ERROR;
					break;
				}
				paramlen = to - from + 1;
				if (paramlen > 0)
					CVT_APPEND_DATA(qb, nqb.query_statement + from, paramlen);
			}
		}
		if (0 == qb->errornumber)
		{
			qb->errornumber = nqb.errornumber;
			qb->errormsg = nqb.errormsg;
		}
		if (SQL_ERROR != retval)
		{
			qb->param_number = nqb.param_number;
			qb->dollar_number = nqb.dollar_number;
			qb->flags = nqb.flags;
		}
	}
	else
	{
		/* Bogus key, leave untranslated */
		retval = SQL_ERROR;
	}

cleanup:
	if (nqb_is_valid)
		QB_Destructor(&nqb);
	return retval;
}

static BOOL
convert_money(const char *s, char *sout, size_t soutmax)
{
	char		in, decp = 0;
	size_t		i = 0,
				out = 0;
	int	num_in = -1, period_in = -1, comma_in = -1;

	for (i = 0; s[i]; i++)
	{
		switch (in = s[i])
		{
			case '.':
				if (period_in < 0)
					period_in = i;
				break;
			case ',':
				if (comma_in < 0)
					comma_in = i;
				break;
			default:
				if ('0' <= in && '9' >= in)
					num_in = i;
				break;
		}
	}
	if (period_in > comma_in)
	{
		if ( period_in >= num_in - 2)
			decp = '.';
	}
	else if (comma_in >= 0 &&
		 comma_in >= num_in - 2)
		decp = ',';
	for (i = 0; s[i] && out + 1 < soutmax; i++)
	{
		switch (in = s[i])
		{
			case '(':
			case '-':
				sout[out++] = '-';
				break;
			default:
				if (in >= '0' && in <= '9')
					sout[out++] = in;
				else if (in == decp)
					sout[out++] = '.';
		}
	}
	sout[out] = '\0';
	return TRUE;
}


/*
 *	This function parses a character string for date/time info and fills in SIMPLE_TIME
 *	It does not zero out SIMPLE_TIME in case it is desired to initialize it with a value
 */
static char
parse_datetime(const char *buf, SIMPLE_TIME *st)
{
	int			y,
				m,
				d,
				hh,
				mm,
				ss;
	int			nf;
	BOOL	bZone;	int	zone;

	y = m = d = hh = mm = ss = 0;
	st->fr = 0;
	st->infinity = 0;

	/*
	 * Handle ODBC time/date/timestamp literals, e.g.
	 * { d '2011-04-22' }
	 * { t '12:34:56' }
	 * { ts '2011-04-22 12:34:56' }
	 */
	if (buf[0] == ODBC_ESCAPE_START)
	{
		while (*(++buf) && *buf != LITERAL_QUOTE);
		if (!(*buf))
			return FALSE;
		buf++;
	}
	bZone = FALSE;
	if (timestamp2stime(buf, st, &bZone, &zone))
		return TRUE;
	if (buf[4] == '-')			/* year first */
		nf = sscanf(buf, "%4d-%2d-%2d %2d:%2d:%2d", &y, &m, &d, &hh, &mm, &ss);
	else
		nf = sscanf(buf, "%2d-%2d-%4d %2d:%2d:%2d", &m, &d, &y, &hh, &mm, &ss);

	if (nf == 5 || nf == 6)
	{
		st->y = y;
		st->m = m;
		st->d = d;
		st->hh = hh;
		st->mm = mm;
		st->ss = ss;

		return TRUE;
	}

	if (buf[4] == '-')			/* year first */
		nf = sscanf(buf, "%4d-%2d-%2d", &y, &m, &d);
	else
		nf = sscanf(buf, "%2d-%2d-%4d", &m, &d, &y);

	if (nf == 3)
	{
		st->y = y;
		st->m = m;
		st->d = d;

		return TRUE;
	}

	nf = sscanf(buf, "%2d:%2d:%2d", &hh, &mm, &ss);
	if (nf == 2 || nf == 3)
	{
		st->hh = hh;
		st->mm = mm;
		st->ss = ss;

		return TRUE;
	}

	return FALSE;
}


/*	Change linefeed to carriage-return/linefeed */
size_t
convert_linefeeds(const char *si, char *dst, size_t max, BOOL convlf, BOOL *changed)
{
	size_t		i = 0,
				out = 0;

	if (max == 0)
		max = 0xffffffff;
	*changed = FALSE;
	for (i = 0; si[i] && out < max - 1; i++)
	{
		if (convlf && si[i] == '\n')
		{
			/* Only add the carriage-return if needed */
			if (i > 0 && PG_CARRIAGE_RETURN == si[i - 1])
			{
				if (dst)
					dst[out++] = si[i];
				else
					out++;
				continue;
			}
			*changed = TRUE;

			if (dst)
			{
				dst[out++] = PG_CARRIAGE_RETURN;
				dst[out++] = '\n';
			}
			else
				out += 2;
		}
		else
		{
			if (dst)
				dst[out++] = si[i];
			else
				out++;
		}
	}
	if (dst)
		dst[out] = '\0';
	return out;
}


/*
 *	Change carriage-return/linefeed to just linefeed
 *	Plus, escape any special characters.
 */
static BOOL
convert_special_chars(QueryBuild *qb, const char *si, size_t used)
{
	size_t		i = 0,
				max;
	char		tchar;
	encoded_str	encstr;
	BOOL	convlf = (0 != (qb->flags & FLGB_CONVERT_LF));
	BOOL	double_special = (qb->param_mode != RPM_BUILDING_BIND_REQUEST);
	int		ccsc = qb->ccsc;
	char		escape_in_literal = CC_get_escape(qb->conn);

	if (used == SQL_NTS)
		max = strlen(si);
	else
		max = used;

	/*
	 * Make sure there's room for the null-terminator, if the input is an
	 * empty string. XXX: I don't think the null-termination is actually
	 * even required, but better safe than sorry.
	 */
	if (!enlarge_query_statement(qb, qb->npos + 1))
		return FALSE;

	encoded_str_constr(&encstr, ccsc, si);
	for (i = 0; i < max && si[i]; i++)
	{
		tchar = encoded_nextchar(&encstr);

		/*
		 * Make sure there is room for three more bytes in the buffer. We
		 * expand quotes to two bytes, plus null-terminate the end.
		 */
		if (qb->npos + 3 >= qb->str_alsize)
		{
			if (!enlarge_query_statement(qb, qb->npos + 3))
				return FALSE;
		}

		if (MBCS_NON_ASCII(encstr))
		{
			qb->query_statement[qb->npos++] = tchar;
			continue;
		}
		if (convlf &&	/* CR/LF -> LF */
		    PG_CARRIAGE_RETURN == tchar &&
		    PG_LINEFEED == si[i + 1])
			continue;
		else if (double_special && /* double special chars ? */
			 (tchar == LITERAL_QUOTE ||
			  tchar == escape_in_literal))
		{
			qb->query_statement[qb->npos++] = tchar;
		}
		qb->query_statement[qb->npos++] = tchar;
	}

	qb->query_statement[qb->npos] = '\0';

	return TRUE;
}

static int
conv_from_octal(const char *s)
{
	ssize_t			i;
	int			y = 0;

	for (i = 1; i <= 3; i++)
		y += (s[i] - '0') << (3 * (3 - i));

	return y;
}


/*	convert octal escapes to bytes */
static size_t
convert_from_pgbinary(const char *value, char *rgbValue, SQLLEN cbValueMax)
{
	size_t		i,
				ilen = strlen(value);
	size_t			o = 0;

	for (i = 0; i < ilen;)
	{
		if (value[i] == BYTEA_ESCAPE_CHAR)
		{
			if (value[i + 1] == BYTEA_ESCAPE_CHAR)
			{
				if (rgbValue)
					rgbValue[o] = value[i];
				o++;
				i += 2;
			}
			else if (value[i + 1] == 'x')
			{
				i += 2;
				if (i < ilen)
				{
					ilen -= i;
					if (rgbValue)
						pg_hex2bin(value + i, rgbValue + o, ilen);
					o += ilen / 2;
				}
				break;
			}
			else
			{
				if (rgbValue)
					rgbValue[o] = conv_from_octal(&value[i]);
				o++;
				i += 4;
			}
		}
		else
		{
			if (rgbValue)
				rgbValue[o] = value[i];
			o++;
			i++;
		}
		/** if (rgbValue)
			MYLOG(0, "i=%d, rgbValue[%d] = %d, %c\n", i, o, rgbValue[o], rgbValue[o]); ***/
	}

	if (rgbValue)
		rgbValue[o] = '\0';		/* extra protection */

	MYLOG(0, "in=" FORMAT_SIZE_T ", out = " FORMAT_SIZE_T "\n", ilen, o);

	return o;
}


static UInt2
conv_to_octal(UCHAR val, char *octal, char escape_ch)
{
	int	i, pos = 0, len;

	if (escape_ch)
		octal[pos++] = escape_ch;
	octal[pos] = BYTEA_ESCAPE_CHAR;
	len = 4 + pos;
	octal[len] = '\0';

	for (i = len - 1; i > pos; i--)
	{
		octal[i] = (val & 7) + '0';
		val >>= 3;
	}

	return (UInt2) len;
}


static char *
conv_to_octal2(UCHAR val, char *octal)
{
	int			i;

	octal[0] = BYTEA_ESCAPE_CHAR;
	octal[4] = '\0';

	for (i = 3; i > 0; i--)
	{
		octal[i] = (val & 7) + '0';
		val >>= 3;
	}

	return octal;
}


/*	convert non-ascii bytes to octal escape sequences */
static size_t
convert_to_pgbinary(const char *in, char *out, size_t len, QueryBuild *qb)
{
	UCHAR	inc;
	size_t			i, o = 0;
	char	escape_in_literal = CC_get_escape(qb->conn);
	BOOL	esc_double = (qb->param_mode != RPM_BUILDING_BIND_REQUEST &&
						  0 != escape_in_literal);

	/* use hex format for 9.0 or later servers */
	if (0 != (qb->flags & FLGB_HEX_BIN_FORMAT))
	{
		if (esc_double)
			out[o++] = escape_in_literal;
		out[o++] = '\\';
		out[o++] = 'x';
		o += pg_bin2hex(in, out + o, len);
		return o;
	}
	for (i = 0; i < len; i++)
	{
		inc = in[i];
		MYLOG(DETAIL_LOG_LEVEL, "in[" FORMAT_SIZE_T "] = %d, %c\n", i, inc, inc);
		if (inc < 128 && (isalnum(inc) || inc == ' '))
			out[o++] = inc;
		else
		{
			if (esc_double)
			{
				o += conv_to_octal(inc, &out[o], escape_in_literal);
			}
			else
			{
				conv_to_octal2(inc, &out[o]);
				o += 4;
			}
		}
	}

	MYLOG(0, "leaving " FORMAT_SIZE_T ", out='%.*s'\n", o, (int) o, out);

	return o;
}


static const char *hextbl = "0123456789ABCDEF";

#define	def_bin2hex(type) \
	(const char *src, type *dst, SQLLEN length) \
{ \
	const char	*src_wk; \
	UCHAR		chr; \
	type		*dst_wk; \
	BOOL		backwards; \
	int		i; \
 \
	backwards = FALSE; \
	if ((char *) dst < src) \
	{ \
		if ((char *) (dst + 2 * (length - 1)) > src + length - 1) \
			return -1; \
	} \
	else if ((char *) dst < src + length) \
		backwards = TRUE; \
	if (backwards) \
	{ \
		for (i = 0, src_wk = src + length - 1, dst_wk = dst + 2 * length - 1; i < length; i++, src_wk--) \
		{ \
			chr = *src_wk; \
			*dst_wk-- = hextbl[chr % 16]; \
			*dst_wk-- = hextbl[chr >> 4]; \
		} \
	} \
	else \
	{ \
		for (i = 0, src_wk = src, dst_wk = dst; i < length; i++, src_wk++) \
		{ \
			chr = *src_wk; \
			*dst_wk++ = hextbl[chr >> 4]; \
			*dst_wk++ = hextbl[chr % 16]; \
		} \
	} \
	dst[2 * length] = '\0'; \
	return 2 * length * sizeof(type); \
}
#ifdef	UNICODE_SUPPORT
static SQLLEN
pg_bin2whex def_bin2hex(SQLWCHAR)
#endif /* UNICODE_SUPPORT */

static SQLLEN
pg_bin2hex def_bin2hex(char)

SQLLEN
pg_hex2bin(const char *src, char *dst, SQLLEN length)
{
	UCHAR		chr;
	const char *src_wk;
	char	   *dst_wk;
	SQLLEN		i;
	int		val;
	BOOL		HByte = TRUE;

	for (i = 0, src_wk = src, dst_wk = dst; i < length; i++, src_wk++)
	{
		chr = *src_wk;
		if (!chr)
			break;
		if (chr >= 'a' && chr <= 'f')
			val = chr - 'a' + 10;
		else if (chr >= 'A' && chr <= 'F')
			val = chr - 'A' + 10;
		else
			val = chr - '0';
		if (HByte)
			*dst_wk = (val << 4);
		else
		{
			*dst_wk += val;
			dst_wk++;
		}
		HByte = !HByte;
	}
	*dst_wk = '\0';
	return length;
}

/*-------
 *	1. get oid (from 'value')
 *	2. open the large object
 *	3. read from the large object (handle multiple GetData)
 *	4. close when read less than requested?  -OR-
 *		lseek/read each time
 *		handle case where application receives truncated and
 *		decides not to continue reading.
 *
 *	CURRENTLY, ONLY LONGVARBINARY is handled, since that is the only
 *	data type currently mapped to a PG_TYPE_LO.  But, if any other types
 *	are desired to map to a large object (PG_TYPE_LO), then that would
 *	need to be handled here.  For example, LONGVARCHAR could possibly be
 *	mapped to PG_TYPE_LO someday, instead of PG_TYPE_TEXT as it is now.
 *-------
 */
static int
convert_lo(StatementClass *stmt, const void *value, SQLSMALLINT fCType, PTR rgbValue,
		   SQLLEN cbValueMax, SQLLEN *pcbValue)
{
	CSTR	func = "convert_lo";
	OID			oid;
	int			result;
	Int8			retval;
	Int8		left64 = -1;
	struct GetBlobDataClass *gdata_blob = NULL;
	ConnectionClass *conn = SC_get_conn(stmt);
	ConnInfo   *ci = &(conn->connInfo);
	GetDataInfo	*gdata_info = SC_get_GDTI(stmt);
	int			factor;

	oid = ATOI32U(value);
	if (0 == oid)
	{
		if (pcbValue)
			*pcbValue = SQL_NULL_DATA;
		return COPY_OK;
	}
	switch (fCType)
	{
		case SQL_C_CHAR:
			factor = 2;
			break;
		case SQL_C_BINARY:
			factor = 1;
			break;
		default:
			SC_set_error(stmt, STMT_EXEC_ERROR, "Could not convert lo to the c-type", func);
			return COPY_GENERAL_ERROR;
	}
	/* If using SQLGetData, then current_col will be set */
	if (stmt->current_col >= 0)
	{
		gdata_blob = &(gdata_info->gdata[stmt->current_col].blob);
		left64 = gdata_blob->data_left64;
	}

	/*
	 * if this is the first call for this column, open the large object
	 * for reading
	 */

	if (!gdata_blob || gdata_blob->data_left64 == -1)
	{
		/* begin transaction if needed */
		if (!CC_is_in_trans(conn))
		{
			if (!CC_begin(conn))
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Could not begin (in-line) a transaction", func);
				return COPY_GENERAL_ERROR;
			}
		}

		stmt->lobj_fd = odbc_lo_open(conn, oid, INV_READ);
		if (stmt->lobj_fd < 0)
		{
			SC_set_error(stmt, STMT_EXEC_ERROR, "Couldnt open large object for reading.", func);
			return COPY_GENERAL_ERROR;
		}

		/* Get the size */
		retval = odbc_lo_lseek64(conn, stmt->lobj_fd, 0L, SEEK_END);
		if (retval >= 0)
		{
			left64 = odbc_lo_tell64(conn, stmt->lobj_fd);
			if (gdata_blob)
				gdata_blob->data_left64 = left64;

			/* return to beginning */
			odbc_lo_lseek64(conn, stmt->lobj_fd, 0L, SEEK_SET);
		}
	}
	else if (left64 == 0)
		return COPY_NO_DATA_FOUND;
	MYLOG(0, "lo data left = " FORMATI64 "\n", left64);

	if (stmt->lobj_fd < 0)
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "Large object FD undefined for multiple read.", func);
		return COPY_GENERAL_ERROR;
	}

	if (0 >= cbValueMax)
		retval = 0;
	else
		retval = (Int8) odbc_lo_read(conn, stmt->lobj_fd, (char *) rgbValue, (Int4) (factor > 1 ? (cbValueMax - 1) / factor : cbValueMax));
	if (retval < 0)
	{
		odbc_lo_close(conn, stmt->lobj_fd);

		/* commit transaction if needed */
		if (!ci->drivers.use_declarefetch && CC_does_autocommit(conn))
		{
			if (!CC_commit(conn))
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Could not commit (in-line) a transaction", func);
				return COPY_GENERAL_ERROR;
			}
		}

		stmt->lobj_fd = -1;

		SC_set_error(stmt, STMT_EXEC_ERROR, "Error reading from large object.", func);
		return COPY_GENERAL_ERROR;
	}

	if (factor > 1)
		pg_bin2hex((char *) rgbValue, (char *) rgbValue, retval);
	if (retval < left64)
		result = COPY_RESULT_TRUNCATED;
	else
		result = COPY_OK;

	if (pcbValue)
	{
		Int8	leftbytes = left64 * factor;
		*pcbValue = left64 < 0 ? SQL_NO_TOTAL : (leftbytes == (SQLLEN) leftbytes ? leftbytes : /* exceeds SQLLEN limit */ SQL_NO_TOTAL);
	}

	if (gdata_blob && gdata_blob->data_left64 > 0)
		gdata_blob->data_left64 -= retval;

	if (!gdata_blob || gdata_blob->data_left64 == 0)
	{
		odbc_lo_close(conn, stmt->lobj_fd);

		/* commit transaction if needed */
		if (!ci->drivers.use_declarefetch && CC_does_autocommit(conn))
		{
			if (!CC_commit(conn))
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Could not commit (in-line) a transaction", func);
				return COPY_GENERAL_ERROR;
			}
		}

		stmt->lobj_fd = -1;		/* prevent further reading */
	}

	return result;
}
