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
#include "statement.h"
#include "qresult.h"
#include "bind.h"
#include "pgtypes.h"
#include "lobj.h"
#include "connection.h"
#include "catfunc.h"
#include "pgapifunc.h"

#if defined(UNICODE_SUPPORT) && defined(WIN32)
#define	WIN_UNICODE_SUPPORT
#endif

CSTR	NAN_STRING = "NaN";
CSTR	INFINITY_STRING = "Infinity";
CSTR	MINFINITY_STRING = "-Infinity";

#ifdef	__CYGWIN__
#define TIMEZONE_GLOBAL _timezone
#elif	defined(WIN32) || defined(HAVE_INT_TIMEZONE)
#define TIMEZONE_GLOBAL timezone
#endif

/*
 *	How to map ODBC scalar functions {fn func(args)} to Postgres.
 *	This is just a simple substitution.  List augmented from:
 *	http://www.merant.com/datadirect/download/docs/odbc16/Odbcref/rappc.htm
 *	- thomas 2000-04-03
 */
char	   *mapFuncs[][2] = {
/*	{ "ASCII",		 "ascii"	  }, built_in */
	{"CHAR", "chr($*)" },
	{"CONCAT", "textcat($*)" },
/*	{ "DIFFERENCE", "difference" }, how to ? */
	{"INSERT", "substring($1 from 1 for $2 - 1) || $4 || substring($1 from $2 + $3)" },
	{"LCASE", "lower($*)" },
	{"LEFT", "ltrunc($*)" },
	{"%2LOCATE", "strpos($2,  $1)" },	/* 2 parameters */
	{"%3LOCATE", "strpos(substring($2 from $3), $1) + $3 - 1" },	/* 3 parameters */
	{"LENGTH", "char_length($*)"},
/*	{ "LTRIM",		 "ltrim"	  }, built_in */
	{"RIGHT", "rtrunc($*)" },
	{"SPACE", "repeat('' '', $1)" },
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
/*	{ "ATAN2",		 "atan2"	  }, bui;t_in */
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
	{"POWER", "pow($*)" },
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

/*	{ "DATABASE",	 "database"   }, */
	{"IFNULL", "coalesce($*)" },
	{"USER", "cast(current_user as text)" },
	{0, 0}
};

static const char *mapFunction(const char *func, int param_count);
static int conv_from_octal(const UCHAR *s);
static SQLLEN pg_bin2hex(const UCHAR *src, UCHAR *dst, SQLLEN length);
#ifdef	UNICODE_SUPPORT
static SQLLEN pg_bin2whex(const UCHAR *src, SQLWCHAR *dst, SQLLEN length);
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
#define	FORMATI64	"%I64d"
#define	FORMATI64U	"%I64u"
#elif	(SIZEOF_LONG == 8)
#define	ATOI64(val)	strtol(val, NULL, 10)
#define	ATOI64U(val)	strtoul(val, NULL, 10)
#define	FORMATI64	"%ld"
#define	FORMATI64U	"%lu"
#else
#define	FORMATI64	"%lld"
#define	FORMATI64U	"%llu"
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
	if ((scnt = sscanf(str, "%4d-%2d-%2d %2d:%2d:%2d%31s %15s", &st->y, &st->m, &st->d, &st->hh, &st->mm, &st->ss, rest, bc)) < 6)
		return FALSE;
	else if (scnt == 6)
		return TRUE;
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
	if (!tzname[0] || !tzname[0][0])
	{
		*bZone = FALSE;
		return TRUE;
	}
	timediff = TIMEZONE_GLOBAL + (*zone) * 3600;
	if (!daylight && timediff == 0)		/* the same timezone */
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

static BOOL
stime2timestamp(const SIMPLE_TIME *st, char *str, BOOL bZone, int precision)
{
	char		precstr[16],
				zonestr[16];
	int			i;

	precstr[0] = '\0';
	if (st->infinity > 0)
	{
		strcpy(str, INFINITY_STRING);
		return TRUE;
	}
	else if (st->infinity < 0)
	{
		strcpy(str, MINFINITY_STRING);
		return TRUE;
	}
	if (precision > 0 && st->fr)
	{
		sprintf(precstr, ".%09d", st->fr);
		if (precision < 9)
			precstr[precision + 1] = '\0';
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
	if (bZone && tzname[0] && tzname[0][0] && st->y >= 1970)
	{
		long		zoneint;
		struct tm	tm;
		time_t		time0;

		zoneint = TIMEZONE_GLOBAL;
		if (daylight && st->y >= 1900)
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
			sprintf(zonestr, "-%02d", (int) zoneint / 3600);
		else
			sprintf(zonestr, "+%02d", -(int) zoneint / 3600);
	}
#endif /* TIMEZONE_GLOBAL */
	if (st->y < 0)
		sprintf(str, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d%s%s BC", -st->y, st->m, st->d, st->hh, st->mm, st->ss, precstr, zonestr);
	else
		sprintf(str, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d%s%s", st->y, st->m, st->d, st->hh, st->mm, st->ss, precstr, zonestr);
	return TRUE;
}

#if (ODBCVER >= 0x0300)
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
	if (strnicmp(lit1, "day", 3) != 0)
		return FALSE;
	sign = days < 0 ? SQL_TRUE : SQL_FALSE;
	switch (scnt)
	{
		case 5:
		case 6:
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
	sign = hours < 0 ? SQL_TRUE : SQL_FALSE;
	switch (scnt)
	{
		case 3:
		case 4:
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
#endif /* ODBCVER */


#ifdef	HAVE_LOCALE_H
static char *current_locale = NULL;
static char *current_decimal_point = NULL;
static void current_numeric_locale()
{
	char *loc = setlocale(LC_NUMERIC, NULL);
	if (NULL == current_locale || 0 != stricmp(loc, current_locale))
	{
		struct lconv	*lc = localeconv();

		if (NULL != current_locale)
			free(current_locale);
		current_locale = strdup(loc);
		if (NULL != current_decimal_point)
			free(current_decimal_point);
		current_decimal_point = strdup(lc->decimal_point);
	}
}

static void set_server_decimal_point(char *num)
{
	char *str;

	current_numeric_locale();
	if ('.' == *current_decimal_point)
		return; 
	for (str = num; '\0' != *str; str++)
	{
		if (*str == *current_decimal_point)
		{
			*str = '.';
			break;
		}
	}
}

static void set_client_decimal_point(char *num)
{
	char *str;

	current_numeric_locale();
	if ('.' == *current_decimal_point)
		return;
	for (str = num; '\0' != *str; str++)
	{
		if (*str == '.')
		{
			*str = *current_decimal_point;
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

#if (ODBCVER >= 0x0350)
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
#endif /* ODBCVER */

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
	SQLLEN		len = 0,
				copy_len = 0, needbuflen = 0;
	SIMPLE_TIME std_time;
	time_t		stmt_t = SC_get_time(stmt);
	struct tm  *tim;
#ifdef	HAVE_LOCALTIME_R
	struct tm  tm;
#endif /* HAVE_LOCALTIME_R */
	SQLLEN			pcbValueOffset,
				rgbValueOffset;
	char	   *rgbValueBindRow = NULL;
	SQLLEN		*pcbValueBindRow = NULL, *pIndicatorBindRow = NULL;
	const char *ptr;
	SQLSETPOSIROW		bind_row = stmt->bind_row;
	int			bind_size = opts->bind_size;
	int			result = COPY_OK;
	const ConnectionClass	*conn = SC_get_conn(stmt);
	BOOL		changed;
	BOOL	text_handling, localize_needed;
	const char *neut_str = value;
	char		midtemp[2][32];
	int			mtemp_cnt = 0;
	GetDataClass *pgdc;
#ifdef	UNICODE_SUPPORT
	BOOL	wconverted =   FALSE;
#endif /* UNICODE_SUPPORT */
#ifdef	WIN_UNICODE_SUPPORT
	SQLWCHAR	*allocbuf = NULL;
	ssize_t		wstrlen;	
#endif /* WIN_UNICODE_SUPPORT */
#if (ODBCVER >= 0x0350)
	SQLGUID g;
#endif

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

	/* Initialize current date */
#ifdef	HAVE_LOCALTIME_R
	tim = localtime_r(&stmt_t, &tm);
#else
	tim = localtime(&stmt_t);
#endif /* HAVE_LOCALTIME_R */
	std_time.m = tim->tm_mon + 1;
	std_time.d = tim->tm_mday;
	std_time.y = tim->tm_year + 1900;

	mylog("copy_and_convert: field_type = %d, fctype = %d, value = '%s', cbValueMax=%d\n", field_type, fCType, (value == NULL) ? "<NULL>" : value, cbValueMax);

	if (!value)
	{
mylog("null_cvt_date_string=%d\n", conn->connInfo.cvt_null_date_string);
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
#if (ODBCVER >= 0x0300)
		     SQL_C_TYPE_DATE == fCType ||
#endif /* ODBCVER */
		     SQL_C_DEFAULT == fCType))
		{
			if (pcbValueBindRow)
				*pcbValueBindRow = 0;
			switch (fCType)
			{
				case SQL_C_CHAR:
					if (rgbValueBindRow && cbValueMax > 0)
						rgbValueBindRow = '\0';
					else
						result = COPY_RESULT_TRUNCATED;
					break;
				case SQL_C_DATE:
#if (ODBCVER >= 0x0300)
				case SQL_C_TYPE_DATE:
#endif /* ODBCVER */
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
			sscanf(value, "%2d:%2d:%2d", &std_time.hh, &std_time.mm, &std_time.ss);
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
				BOOL		bZone = (field_type != PG_TYPE_TIMESTAMP_NO_TMZONE && PG_VERSION_GE(conn, 7.2));
				int			zone;

				/*
				 * sscanf(value, "%4d-%2d-%2d %2d:%2d:%2d", &std_time.y, &std_time.m,
				 * &std_time.d, &std_time.hh, &std_time.mm, &std_time.ss);
				 */
				bZone = FALSE;	/* time zone stuff is unreliable */
				timestamp2stime(value, &std_time, &bZone, &zone);
inolog("2stime fr=%d\n", std_time.fr);
			}
			else
			{
				/*
				 * The timestamp is invalid so set something conspicuous,
				 * like the epoch
				 */
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
				char	   *s;
				const ConnInfo *ci = &(conn->connInfo);

				s = midtemp[mtemp_cnt];
				switch (((char *)value)[0])
				{
					case 'f':
					case 'F':
					case 'n':
					case 'N':
					case '0':
						strcpy(s, "0");
						break;
					default:
						if (ci->true_is_minus1)
							strcpy(s, "-1");
						else
							strcpy(s, "1");
				}
				neut_str = midtemp[mtemp_cnt];
				mtemp_cnt++;
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
				mylog("index=(");
				for (i = 0;; i++)
				{
					if (sscanf(vp, "%hi", &shortv) != 1)
						break;
					mylog(" %hi", shortv);
					if (0 == shortv && PG_VERSION_LT(conn, 7.2))
						break;
					nval++;
					if (nval < maxc)
						short_array[i + 1] = shortv;

					/* skip the current token */
					while ((*vp != '\0') && (!isspace((UCHAR) *vp)))
						vp++;
					/* and skip the space to the next token */
					while ((*vp != '\0') && (isspace((UCHAR) *vp)))
						vp++;
					if (*vp == '\0')
						break;
				}
				mylog(") nval = %i\n", nval);
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
		if (fCType == SQL_C_WCHAR
		    && CC_default_is_c(conn))
			fCType = SQL_C_CHAR;

		mylog("copy_and_convert, SQL_C_DEFAULT: fCType = %d\n", fCType);
	}

	text_handling = localize_needed = FALSE;
	switch (fCType)
	{
		case INTERNAL_ASIS_TYPE:
#ifdef	UNICODE_SUPPORT
	    	case SQL_C_WCHAR:
#endif /* UNICODE_SUPPORT */
	    	case SQL_C_CHAR:
			text_handling = TRUE;
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
					text_handling = TRUE;
					break;
			}
			break;
	}
	if (text_handling)
	{
#ifdef	WIN_UNICODE_SUPPORT
		if (SQL_C_CHAR == fCType
		    || SQL_C_BINARY == fCType)
			localize_needed = TRUE;
#endif /* WIN_UNICODE_SUPPORT */
	}

	if (text_handling)
	{
		/* Special character formatting as required */

		BOOL	hex_bin_format = FALSE;
		/*
		 * These really should return error if cbValueMax is not big
		 * enough.
		 */
		switch (field_type)
		{
			case PG_TYPE_DATE:
				len = 10;
				if (cbValueMax > len)
					sprintf(rgbValueBindRow, "%.4d-%.2d-%.2d", std_time.y, std_time.m, std_time.d);
				break;

			case PG_TYPE_TIME:
				len = 8;
				if (cbValueMax > len)
					sprintf(rgbValueBindRow, "%.2d:%.2d:%.2d", std_time.hh, std_time.mm, std_time.ss);
				break;

			case PG_TYPE_ABSTIME:
			case PG_TYPE_DATETIME:
			case PG_TYPE_TIMESTAMP_NO_TMZONE:
			case PG_TYPE_TIMESTAMP:
				len = 19;
				if (cbValueMax > len)
				{
					/* sprintf(rgbValueBindRow, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
						std_time.y, std_time.m, std_time.d, std_time.hh, std_time.mm, std_time.ss); */
					stime2timestamp(&std_time, rgbValueBindRow, FALSE,
									PG_VERSION_GE(conn, 7.2) ? (int) cbValueMax - len - 2 : 0);
					len = strlen(rgbValueBindRow);
				}
				break;

			case PG_TYPE_BOOL:
				len = strlen(neut_str);
				if (cbValueMax > len)
				{
					strcpy(rgbValueBindRow, neut_str);
					mylog("PG_TYPE_BOOL: rgbValueBindRow = '%s'\n", rgbValueBindRow);
				}
				break;

			case PG_TYPE_UUID:
				len = strlen(neut_str);
				if (cbValueMax > len)
				{
					int i;

					for (i = 0; i < len; i++)
						rgbValueBindRow[i] = toupper((UCHAR) neut_str[i]);
					rgbValueBindRow[i] = '\0';
					mylog("PG_TYPE_UUID: rgbValueBindRow = '%s'\n", rgbValueBindRow);
				}
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
				switch (field_type)
				{
					case PG_TYPE_FLOAT4:
					case PG_TYPE_FLOAT8:
					case PG_TYPE_NUMERIC:
						set_client_decimal_point((char *) neut_str);
						break;
					case PG_TYPE_BYTEA:
						if (0 == strnicmp(neut_str, "\\x", 2))
						{
							hex_bin_format = TRUE;
							neut_str += 2;
						}
						break;
				}

				if (stmt->current_col < 0)
				{
					pgdc = &(gdata->fdata);
					pgdc->data_left = -1;
				}
				else
					pgdc = &gdata->gdata[stmt->current_col];
#ifdef	UNICODE_SUPPORT
				if (fCType == SQL_C_WCHAR)
					wconverted = TRUE;
#endif /* UNICODE_SUPPORT */
				if (pgdc->data_left < 0)
				{
					BOOL lf_conv = conn->connInfo.lf_conversion;
					if (PG_TYPE_BYTEA == field_type)
					{
						if (hex_bin_format)
							len = strlen(neut_str);
						else
						{
							len = convert_from_pgbinary(neut_str, NULL, 0);
							len *= 2;
						}
						changed = TRUE;
#ifdef	UNICODE_SUPPORT
						if (fCType == SQL_C_WCHAR)
							len *= WCLEN;
#endif /* UNICODE_SUPPORT */
					}
					else
#ifdef	UNICODE_SUPPORT
					if (fCType == SQL_C_WCHAR)
					{
						len = utf8_to_ucs2_lf(neut_str, SQL_NTS, lf_conv, NULL, 0);
						len *= WCLEN;
						changed = TRUE;
					}
					else
#endif /* UNICODE_SUPPORT */
#ifdef	WIN_UNICODE_SUPPORT
					if (localize_needed)
					{
						wstrlen = utf8_to_ucs2_lf(neut_str, SQL_NTS, lf_conv, NULL, 0);
						allocbuf = (SQLWCHAR *) malloc(WCLEN * (wstrlen + 1));
						wstrlen = utf8_to_ucs2_lf(neut_str, SQL_NTS, lf_conv, allocbuf, wstrlen + 1);
						len = wstrtomsg(NULL, (const LPWSTR) allocbuf, (int) wstrlen, NULL, 0);
						changed = TRUE;
					}
					else
#endif /* WIN_UNICODE_SUPPORT */
						/* convert linefeeds to carriage-return/linefeed */
						len = convert_linefeeds(neut_str, NULL, 0, lf_conv, &changed);
					if (cbValueMax == 0)		/* just returns length
												 * info */
					{
						result = COPY_RESULT_TRUNCATED;
#ifdef	WIN_UNICODE_SUPPORT
						if (allocbuf)
							free(allocbuf);
#endif /* WIN_UNICODE_SUPPORT */
						break;
					}
					if (!pgdc->ttlbuf)
						pgdc->ttlbuflen = 0;
					needbuflen = len;
					switch (fCType)
					{
#ifdef	UNICODE_SUPPORT
						case SQL_C_WCHAR:
							needbuflen += WCLEN;
							break;
#endif /* UNICODE_SUPPORT */
						case SQL_C_BINARY:
							break;
						default:
							needbuflen++;
					}
					if (changed || needbuflen > cbValueMax)
					{
						if (needbuflen > (SQLLEN) pgdc->ttlbuflen)
						{
							pgdc->ttlbuf = realloc(pgdc->ttlbuf, needbuflen);
							pgdc->ttlbuflen = needbuflen;
						}
#ifdef	UNICODE_SUPPORT
						if (fCType == SQL_C_WCHAR)
						{
							if (PG_TYPE_BYTEA == field_type && !hex_bin_format)
							{
								len = convert_from_pgbinary(neut_str, pgdc->ttlbuf, pgdc->ttlbuflen);
								len = pg_bin2whex(pgdc->ttlbuf, (SQLWCHAR *) pgdc->ttlbuf, len);
							}
							else
								utf8_to_ucs2_lf(neut_str, SQL_NTS, lf_conv, (SQLWCHAR *) pgdc->ttlbuf, len / WCLEN);
						}
						else
#endif /* UNICODE_SUPPORT */
						if (PG_TYPE_BYTEA == field_type)
						{
							if (hex_bin_format)
							{
								len = strlen(neut_str);
								strncpy_null(pgdc->ttlbuf, neut_str, pgdc->ttlbuflen);
							}
							else
							{
								len = convert_from_pgbinary(neut_str, pgdc->ttlbuf, pgdc->ttlbuflen);
								len = pg_bin2hex(pgdc->ttlbuf, pgdc->ttlbuf, len);
							} 
						}
						else
#ifdef	WIN_UNICODE_SUPPORT
						if (localize_needed)
						{
							len = wstrtomsg(NULL, allocbuf, (int) wstrlen, pgdc->ttlbuf, (int) pgdc->ttlbuflen);
							free(allocbuf);
							allocbuf = NULL;
						}
						else
#endif /* WIN_UNICODE_SUPPORT */
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
				}
				else
				{
					ptr = pgdc->ttlbuf;
					len = pgdc->ttlbufused;
				}

				mylog("DEFAULT: len = %d, ptr = '%.*s'\n", len, len, ptr);

				if (stmt->current_col >= 0)
				{
					if (pgdc->data_left > 0)
					{
						ptr += len - pgdc->data_left;
						len = pgdc->data_left;
						needbuflen = len + (pgdc->ttlbuflen - pgdc->ttlbufused);
					}
					else
						pgdc->data_left = len;
				}

				if (cbValueMax > 0)
				{
					BOOL	already_copied = FALSE;

					if (fCType == SQL_C_BINARY)
						copy_len = (len > cbValueMax) ? cbValueMax : len;
					else
						copy_len = (len >= cbValueMax) ? (cbValueMax - 1) : len;
#ifdef	UNICODE_SUPPORT
					if (fCType == SQL_C_WCHAR)
					{
						copy_len /= WCLEN;
						copy_len *= WCLEN;
					}
#endif /* UNICODE_SUPPORT */
					if (!already_copied)
					{
						/* Copy the data */
						memcpy(rgbValueBindRow, ptr, copy_len);
						/* Add null terminator */
#ifdef	UNICODE_SUPPORT
						if (fCType == SQL_C_WCHAR)
						{
							if (copy_len + WCLEN <= cbValueMax)
								memset(rgbValueBindRow + copy_len, 0, WCLEN);
						}
						else
#endif /* UNICODE_SUPPORT */
						if (copy_len < cbValueMax)
							rgbValueBindRow[copy_len] = '\0';
					}
					/* Adjust data_left for next time */
					if (stmt->current_col >= 0)
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


				if (SQL_C_WCHAR == fCType)
					mylog("    SQL_C_WCHAR, default: len = %d, cbValueMax = %d, rgbValueBindRow = '%s'\n", len, cbValueMax, rgbValueBindRow);
				else if (SQL_C_BINARY == fCType)
					mylog("    SQL_C_BINARY, default: len = %d, cbValueMax = %d, rgbValueBindRow = '%.*s'\n", len, cbValueMax, copy_len, rgbValueBindRow);
				else
					mylog("    SQL_C_CHAR, default: len = %d, cbValueMax = %d, rgbValueBindRow = '%s'\n", len, cbValueMax, rgbValueBindRow);
				break;
		}
#ifdef	UNICODE_SUPPORT
		if (SQL_C_WCHAR == fCType && ! wconverted)
		{
			char *str = strdup(rgbValueBindRow);
			SQLLEN	ucount = utf8_to_ucs2(str, len, (SQLWCHAR *) rgbValueBindRow, cbValueMax / WCLEN);
			if (cbValueMax < WCLEN * ucount)
				result = COPY_RESULT_TRUNCATED;
			len = ucount * WCLEN;
			free(str); 
		}
#endif /* UNICODE_SUPPORT */

	}
	else
	{
		/*
		 * for SQL_C_CHAR, it's probably ok to leave currency symbols in.
		 * But to convert to numeric types, it is necessary to get rid of
		 * those.
		 */
		if (field_type == PG_TYPE_MONEY)
		{
			if (convert_money(neut_str, midtemp[mtemp_cnt], sizeof(midtemp[0])))
			{
				neut_str = midtemp[mtemp_cnt];
				mtemp_cnt++;
			}
			else
			{
				qlog("couldn't convert money type to %d\n", fCType);
				return COPY_UNSUPPORTED_TYPE;
			}
		}

		switch (fCType)
		{
			case SQL_C_DATE:
#if (ODBCVER >= 0x0300)
			case SQL_C_TYPE_DATE:		/* 91 */
#endif
				len = 6;
				{
					DATE_STRUCT *ds;

					if (bind_size > 0)
						ds = (DATE_STRUCT *) rgbValueBindRow;
					else
						ds = (DATE_STRUCT *) rgbValue + bind_row;
					ds->year = std_time.y;
					ds->month = std_time.m;
					ds->day = std_time.d;
				}
				break;

			case SQL_C_TIME:
#if (ODBCVER >= 0x0300)
			case SQL_C_TYPE_TIME:		/* 92 */
#endif
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
#if (ODBCVER >= 0x0300)
			case SQL_C_TYPE_TIMESTAMP:	/* 93 */
#endif
				len = 16;
				{
					TIMESTAMP_STRUCT *ts;

					if (bind_size > 0)
						ts = (TIMESTAMP_STRUCT *) rgbValueBindRow;
					else
						ts = (TIMESTAMP_STRUCT *) rgbValue + bind_row;
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

				/*
				 * mylog("SQL_C_BIT: bind_row = %d val = %d, cb = %d,
				 * rgb=%d\n", bind_row, atoi(neut_str), cbValueMax,
				 * *((UCHAR *)rgbValue));
				 */
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

#if (ODBCVER >= 0x0300)
                        case SQL_C_NUMERIC:
			{
			SQL_NUMERIC_STRUCT      *ns;
			int	i, nlen, bit, hval, tv, dig, sta, olen;
			char	calv[SQL_MAX_NUMERIC_LEN * 3];
			const UCHAR *wv;
			BOOL	dot_exist;

			len = sizeof(SQL_NUMERIC_STRUCT);
			if (bind_size > 0)
				ns = (SQL_NUMERIC_STRUCT *) rgbValueBindRow;
			else
				ns = (SQL_NUMERIC_STRUCT *) rgbValue + bind_row;
			for (wv = neut_str; *wv && isspace(*wv); wv++)
				;
			ns->sign = 1;
			if (*wv == '-')
			{
				ns->sign = 0;
				wv++;
			}
			else if (*wv == '+')
				wv++;
			while (*wv == '0') wv++;
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
				else if (!isdigit(*wv))
						break;
				else
				{
					if (dot_exist)
						ns->scale++;
					ns->precision++;
					calv[nlen++] = *wv;
				}
			}
			memset(ns->val, 0, sizeof(ns->val));
			for (hval = 0, bit = 1L, sta = 0, olen = 0; sta < nlen;)
			{
				for (dig = 0, i = sta; i < nlen; i++)
				{
					tv = dig * 10 + calv[i] - '0';
					dig = tv % 2;
					calv[i] = tv / 2 + '0';
					if (i == sta && tv < 2)
						sta++;
				}
				if (dig > 0)
					hval |= bit;
				bit <<= 1;
				if (bit >= (1L << 8))
				{
					ns->val[olen++] = hval;
					hval = 0;
					bit = 1L;
					if (olen >= SQL_MAX_NUMERIC_LEN - 1)
					{
						ns->scale = sta - ns->precision;
						break;
					}
				} 
			}
			if (hval && olen < SQL_MAX_NUMERIC_LEN - 1)
				ns->val[olen++] = hval;
			}
			break;
#endif /* ODBCVER */

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

#if (ODBCVER >= 0x0300) && defined(ODBCINT64)
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
				if (PG_TYPE_UNKNOWN == field_type ||
				    PG_TYPE_TEXT == field_type ||
				    PG_TYPE_VARCHAR == field_type ||
				    PG_TYPE_BPCHAR == field_type ||
				    PG_TYPE_TEXTARRAY == field_type ||
				    PG_TYPE_VARCHARARRAY == field_type ||
				    PG_TYPE_BPCHARARRAY == field_type)
				{
					ssize_t	len = SQL_NULL_DATA;

					if (neut_str)
						len = strlen(neut_str);
					if (pcbValue)
						*pcbValueBindRow = len;
					if (len > 0 && cbValueMax > 0)
					{
						memcpy(rgbValueBindRow, neut_str, len < cbValueMax ? len : cbValueMax);
						if (cbValueMax >= len + 1)
							rgbValueBindRow[len] = '\0';
					}
					if (cbValueMax >= len)
						return COPY_OK;
					else
						return COPY_RESULT_TRUNCATED;
				}
				/* The following is for SQL_C_VARBOOKMARK */
				else if (PG_TYPE_INT4 == field_type)
				{
					UInt4	ival = ATOI32U(neut_str);

inolog("SQL_C_VARBOOKMARK value=%d\n", ival);
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
#if (ODBCVER >= 0x0350)
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
#endif /* ODBCVER */
				else if (PG_TYPE_BYTEA != field_type)
				{
					mylog("couldn't convert the type %d to SQL_C_BINARY\n", field_type);
					qlog("couldn't convert the type %d to SQL_C_BINARY\n", field_type);
					return COPY_UNSUPPORTED_TYPE;
				}
				/* truncate if necessary */
				/* convert octal escapes to bytes */

				if (stmt->current_col < 0)
				{
					pgdc = &(gdata->fdata);
					pgdc->data_left = -1;
				}
				else
					pgdc = &gdata->gdata[stmt->current_col];
				if (!pgdc->ttlbuf)
					pgdc->ttlbuflen = 0;
				if (pgdc->data_left < 0)
				{
					if (cbValueMax <= 0)
					{
						len = convert_from_pgbinary(neut_str, NULL, 0);
						result = COPY_RESULT_TRUNCATED;
						break;
					}
					if (len = strlen(neut_str), len >= (int) pgdc->ttlbuflen)
					{
						pgdc->ttlbuf = realloc(pgdc->ttlbuf, len + 1);
						pgdc->ttlbuflen = len + 1;
					}
					len = convert_from_pgbinary(neut_str, pgdc->ttlbuf, pgdc->ttlbuflen);
					pgdc->ttlbufused = len;
				}
				else
					len = pgdc->ttlbufused;
				ptr = pgdc->ttlbuf;

				if (stmt->current_col >= 0)
				{
					/*
					 * Second (or more) call to SQLGetData so move the
					 * pointer
					 */
					if (pgdc->data_left > 0)
					{
						ptr += len - pgdc->data_left;
						len = pgdc->data_left;
					}

					/* First call to SQLGetData so initialize data_left */
					else
						pgdc->data_left = len;

				}

				if (cbValueMax > 0)
				{
					copy_len = (len > cbValueMax) ? cbValueMax : len;

					/* Copy the data */
					memcpy(rgbValueBindRow, ptr, copy_len);

					/* Adjust data_left for next time */
					if (stmt->current_col >= 0)
						pgdc->data_left -= copy_len;
				}

				/*
				 * Finally, check for truncation so that proper status can
				 * be returned
				 */
				if (len > cbValueMax)
					result = COPY_RESULT_TRUNCATED;
				else if (pgdc->ttlbuf)
				{
					free(pgdc->ttlbuf);
					pgdc->ttlbuf = NULL;
				}
				mylog("SQL_C_BINARY: len = %d, copy_len = %d\n", len, copy_len);
				break;
#if (ODBCVER >= 0x0350)
			case SQL_C_GUID:

				result = char2guid(neut_str, &g);
				if (COPY_OK != result)
				{
					mylog("Could not convert to SQL_C_GUID");	
					return	COPY_UNSUPPORTED_TYPE;
				}
				len = sizeof(g);
				if (bind_size > 0)
					*((SQLGUID *) rgbValueBindRow) = g;
				else
					*((SQLGUID *) rgbValue + bind_row) = g;
				break;
#endif /* ODBCVER */
#if (ODBCVER >= 0x0300)
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
#endif /* ODBCVER */

			default:
				qlog("conversion to the type %d isn't supported\n", fCType);
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

#define	FLGP_PREPARE_DUMMY_CURSOR	1L
#define	FLGP_USING_CURSOR	(1L << 1)
#define	FLGP_SELECT_INTO		(1L << 2)
#define	FLGP_SELECT_FOR_UPDATE_OR_SHARE	(1L << 3)
#define	FLGP_BUILDING_PREPARE_STATEMENT	(1L << 4)
#define	FLGP_MULTIPLE_STATEMENT	(1L << 5)
#define	FLGP_SELECT_FOR_READONLY	(1L << 6)
typedef struct _QueryParse {
	const UCHAR	*statement;
	int		statement_type;
	size_t		opos;
	Int4		from_pos;	/* PG comm length restriction */
	Int4		where_pos;	/* PG comm length restriction */
	ssize_t		stmt_len;
	char		in_literal, in_identifier, in_escape, in_dollar_quote;
	const	char *dollar_tag;
	ssize_t		taglen;
	char		token_save[64];
	int		token_len;
	char		prev_token_end, in_line_comment;
	size_t		declare_pos;
	UInt4		flags, comment_level;
	encoded_str	encstr;
}	QueryParse;

static void
QP_initialize(QueryParse *q, const StatementClass *stmt)
{
	q->statement = stmt->execute_statement ? stmt->execute_statement : stmt->statement;
	q->statement_type = stmt->statement_type;
	q->opos = 0;
	q->from_pos = -1;
	q->where_pos = -1;
	q->stmt_len = (q->statement) ? strlen(q->statement) : -1;
	q->in_literal = q->in_identifier = q->in_escape = q->in_dollar_quote = FALSE;
	q->dollar_tag = NULL;
	q->taglen = -1;
	q->token_save[0] = '\0';
	q->token_len = 0;
	q->prev_token_end = TRUE;
	q->in_line_comment = FALSE;
	q->declare_pos = 0;
	q->flags = 0;
	q->comment_level = 0;
	make_encoded_str(&q->encstr, SC_get_conn(stmt), q->statement);
}

#define	FLGB_PRE_EXECUTING	1L
#define	FLGB_BUILDING_PREPARE_STATEMENT	(1L << 1)
#define	FLGB_BUILDING_BIND_REQUEST	(1L << 2)
#define	FLGB_EXECUTE_PREPARED		(1L << 3)

#define	FLGB_INACCURATE_RESULT	(1L << 4)
#define	FLGB_CREATE_KEYSET	(1L << 5)
#define	FLGB_KEYSET_DRIVEN	(1L << 6)
#define	FLGB_CONVERT_LF		(1L << 7)
#define	FLGB_DISCARD_OUTPUT	(1L << 8)
#define	FLGB_BINARY_AS_POSSIBLE	(1L << 9)
#define	FLGB_LITERAL_EXTENSION	(1L << 10)
#define	FLGB_HEX_BIN_FORMAT	(1L << 11)
typedef struct _QueryBuild {
	UCHAR	*query_statement;
	size_t	str_size_limit;
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
	UInt4	flags;
	int	ccsc;
	int	errornumber;
	const char *errormsg;

	ConnectionClass	*conn; /* mainly needed for LO handling */
	StatementClass	*stmt; /* needed to set error info in ENLARGE_.. */ 
}	QueryBuild;

#define INIT_MIN_ALLOC	4096
static ssize_t
QB_initialize(QueryBuild *qb, size_t size, StatementClass *stmt, ConnectionClass *conn)
{
	size_t	newsize = 0;

	qb->flags = 0;
	qb->load_stmt_len = 0;
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
	if (conn)
		qb->conn = conn;
	else if (stmt)
	{
		Int2	dummy;

		qb->apdopts = SC_get_APDF(stmt);
		qb->ipdopts = SC_get_IPDF(stmt);
		qb->pdata = SC_get_PDTI(stmt);
		qb->conn = SC_get_conn(stmt);
		if (stmt->pre_executing)
			qb->flags |= FLGB_PRE_EXECUTING;
		if (stmt->discard_output_params)
			qb->flags |= FLGB_DISCARD_OUTPUT;
		qb->num_io_params = CountParameters(stmt, NULL, &dummy, &qb->num_output_params);
		qb->proc_return = stmt->proc_return;
		if (0 != (qb->flags & FLGB_DISCARD_OUTPUT))
			qb->num_discard_params = qb->num_output_params;
		if (qb->num_discard_params < qb->proc_return)
			qb->num_discard_params = qb->proc_return;
	}
	else
	{
		qb->conn = NULL;
		return -1;
	}
	if (qb->conn->connInfo.lf_conversion)
		qb->flags |= FLGB_CONVERT_LF;
	qb->ccsc = qb->conn->ccsc;
	if (CC_get_escape(qb->conn) &&
	    PG_VERSION_GE(qb->conn, 8.1))
		qb->flags |= FLGB_LITERAL_EXTENSION;
	if (PG_VERSION_GE(qb->conn, 9.0))
		qb->flags |= FLGB_HEX_BIN_FORMAT;
		
	if (stmt)
		qb->str_size_limit = stmt->stmt_size_limit;
	else
		qb->str_size_limit = -1;
	if (qb->str_size_limit > 0)
	{
		if (size > qb->str_size_limit)
			return -1;
		newsize = qb->str_size_limit;
	}
	else 
	{
		newsize = INIT_MIN_ALLOC;
		while (newsize <= size)
			newsize *= 2;
	}
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

	if (qb_to->str_size_limit > 0)
	{
		if (size > qb_to->str_size_limit)
			return -1;
	}
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

#define F_OldChar(qp) \
qp->statement[qp->opos]

#define F_OldPtr(qp) \
(qp->statement + qp->opos)

#define F_OldNext(qp) \
(++qp->opos)

#define F_OldPrior(qp) \
(--qp->opos)

#define F_OldPos(qp) \
qp->opos

#define F_ExtractOldTo(qp, buf, ch, maxsize) \
do { \
	size_t	c = 0; \
	while (qp->statement[qp->opos] != '\0' && qp->statement[qp->opos] != ch) \
	{ \
		if (c >= maxsize) \
			break; \
		buf[c++] = qp->statement[qp->opos++]; \
	} \
	if (qp->statement[qp->opos] == '\0') \
		{retval = SQL_ERROR; goto cleanup;} \
	buf[c] = '\0'; \
} while (0)

#define F_NewChar(qb) \
qb->query_statement[qb->npos]

#define F_NewPtr(qb) \
(qb->query_statement + qb->npos)

#define F_NewNext(qb) \
(++qb->npos)

#define F_NewPos(qb) \
(qb->npos)


static int
convert_escape(QueryParse *qp, QueryBuild *qb);
static int
inner_process_tokens(QueryParse *qp, QueryBuild *qb);
static int
ResolveOneParam(QueryBuild *qb, QueryParse *qp);
static int
processParameters(QueryParse *qp, QueryBuild *qb,
	size_t *output_count, SQLLEN param_pos[][2]);
static size_t
convert_to_pgbinary(const UCHAR *in, char *out, size_t len, QueryBuild *qb);

static ssize_t
enlarge_query_statement(QueryBuild *qb, size_t newsize)
{
	size_t	newalsize = INIT_MIN_ALLOC;
	CSTR func = "enlarge_statement";

	if (qb->str_size_limit > 0 && qb->str_size_limit < (int) newsize)
	{
		free(qb->query_statement);
		qb->query_statement = NULL;
		qb->str_alsize = 0;
		if (qb->stmt)
		{
			
			SC_set_error(qb->stmt, STMT_EXEC_ERROR, "Query buffer overflow in copy_statement_with_parameters", func);
		}
		else
		{
			qb->errormsg = "Query buffer overflow in copy_statement_with_parameters";
			qb->errornumber = STMT_EXEC_ERROR;
		}
		return -1;
	}
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
#define ENLARGE_NEWSTATEMENT(qb, newpos) \
	if (newpos >= qb->str_alsize) \
	{ \
		if (enlarge_query_statement(qb, newpos) <= 0) \
			{retval = SQL_ERROR; goto cleanup;} \
	}

/*----------
 *	Terminate the stmt_with_params string with NULL.
 *----------
 */
#define CVT_TERMINATE(qb) \
do { \
	if (NULL == qb->query_statement) {retval = SQL_ERROR; goto cleanup;} \
	qb->query_statement[qb->npos] = '\0'; \
} while (0)

/*----------
 *	Append a data.
 *----------
 */
#define CVT_APPEND_DATA(qb, s, len) \
do { \
	size_t	newpos = qb->npos + len; \
	ENLARGE_NEWSTATEMENT(qb, newpos) \
	memcpy(&qb->query_statement[qb->npos], s, len); \
	qb->npos = newpos; \
	qb->query_statement[newpos] = '\0'; \
} while (0)

/*----------
 *	Append a string.
 *----------
 */
#define CVT_APPEND_STR(qb, s) \
do { \
	size_t	len = strlen(s); \
	CVT_APPEND_DATA(qb, s, len); \
} while (0)

/*----------
 *	Append a char.
 *----------
 */
#define CVT_APPEND_CHAR(qb, c) \
do { \
	ENLARGE_NEWSTATEMENT(qb, qb->npos + 1); \
	qb->query_statement[qb->npos++] = c; \
} while (0)

/*----------
 *	Append a binary data.
 *	Newly required size may be overestimated currently.
 *----------
 */
#define CVT_APPEND_BINARY(qb, buf, used) \
do { \
	size_t	newlimit = qb->npos + ((qb->flags & FLGB_HEX_BIN_FORMAT) ? 2 * used + 4 : 5 * used); \
	ENLARGE_NEWSTATEMENT(qb, newlimit); \
	qb->npos += convert_to_pgbinary(buf, &qb->query_statement[qb->npos], used, qb); \
} while (0)

/*----------
 *
 *----------
 */
#define CVT_SPECIAL_CHARS(qb, buf, used) \
do { \
	size_t cnvlen = convert_special_chars(buf, NULL, used, qb->flags, qb->ccsc, CC_get_escape(qb->conn)); \
	size_t	newlimit = qb->npos + cnvlen; \
\
	ENLARGE_NEWSTATEMENT(qb, newlimit); \
	convert_special_chars(buf, &qb->query_statement[qb->npos], used, qb->flags, qb->ccsc, CC_get_escape(qb->conn)); \
	qb->npos += cnvlen; \
} while (0)

#ifdef NOT_USED
#define CVT_TEXT_FIELD(qb, buf, used) \
do { \
	char	escape_ch = CC_get_escape(qb->conn); \
	int flags = ((0 != qb->flags & FLGB_CONVERT_LF) ? CONVERT_CRLF_TO_LF : 0) | ((0 != qb->flags & FLGB_BUILDING_BIND_REQUEST) ? 0 : DOUBLE_LITERAL_QUOTE | (escape_ch ? DOUBLE_LITERAL_IN_ESCAPE : 0)); \
	int cnvlen = (flags & (DOUBLE_LITERAL_QUOTE | DOUBLE_LITERAL_IN_ESCAPE)) != 0 ? used * 2 : used; \
	if (used > 0 && qb->npos + cnvlen >= qb->str_alsize) \
	{ \
		cnvlen = convert_text_field(buf, NULL, used, qb->ccsc, escape_ch, &flags); \
		size_t	newlimit = qb->npos + cnvlen; \
\
		ENLARGE_NEWSTATEMENT(qb, newlimit); \
	} \
	cnvlen = convert_text_field(buf, &qb->query_statement[qb->npos], used, qb->ccsc, escape_ch, &flags); \
	qb->npos += cnvlen; \
} while (0)
#endif /* NOT_USED */

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
	if (ENCODE_STATUS(encstr) != 0)
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
	if (!isspace((UCHAR) *stmt))
		return FALSE;
	while (isspace((UCHAR) *(++stmt)));
	switch (*stmt)
	{
		case '\0':
		case ',':
		case LITERAL_QUOTE:
			return FALSE;
		case IDENTIFIER_QUOTE:	/* double quoted table name ? */
			do
			{
				do
					while (*(++stmt) != IDENTIFIER_QUOTE && *stmt);
				while (*stmt && *(++stmt) == IDENTIFIER_QUOTE);
				while (*stmt && !isspace((UCHAR) *stmt) && *stmt != IDENTIFIER_QUOTE)
					stmt++;
			}
			while (*stmt == IDENTIFIER_QUOTE);
			break;
		default:
			while (!isspace((UCHAR) *(++stmt)));
			break;
	}
	if (!*stmt)
		return FALSE;
	while (isspace((UCHAR) *(++stmt)));
	if (strnicmp(stmt, "from", 4))
		return FALSE;
	return isspace((UCHAR) stmt[4]);
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
	UInt4	flag = 0;

	while (isspace((UCHAR) *(++wstmt)));
	if (!*wstmt)
		return 0;
	if (0 == strnicmp(wstmt, "update", advance = 6))
		flag |= FLGP_SELECT_FOR_UPDATE_OR_SHARE;
	else if (0 == strnicmp(wstmt, "share", advance = 5))
		flag |= FLGP_SELECT_FOR_UPDATE_OR_SHARE;
	else if (0 == strnicmp(wstmt, "read", advance = 4))
		flag |= FLGP_SELECT_FOR_READONLY;
	else
		return 0;
	wstmt += advance;
	if (0 != wstmt[0] && !isspace((UCHAR) wstmt[0]))
		return 0;
	else if (0 != (flag & FLGP_SELECT_FOR_READONLY))
	{
		if (!isspace((UCHAR) wstmt[0]))
			return 0;
		while (isspace((UCHAR) *(++wstmt)));
		if (!*wstmt)
			return 0;
		if (0 != strnicmp(wstmt, "only", advance = 4))
			return 0;
		wstmt += advance;
	}
	if (0 != wstmt[0] && !isspace((UCHAR) wstmt[0]))
		return 0;
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
	for (stapos = endpos; stapos >= 0 && !isspace((UCHAR) *wstmt); stapos--, wstmt--)
		;
	if (stapos < 0)
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

	while (isspace((UCHAR) *(++wstmt)));
	if (!*wstmt)
		return FALSE;
	if (strnicmp(wstmt, "VALUES", 6))
		return FALSE;
	wstmt += 6;
	if (!wstmt[0] || !isspace((UCHAR) wstmt[0]))
		return FALSE;
	while (isspace((UCHAR) *(++wstmt)));
	if (*wstmt != '(' || *(++wstmt) != ')')
		return FALSE;
	wstmt++;
	*endpos = wstmt - stmt;
	return !wstmt[0] || isspace((UCHAR) wstmt[0])
		|| ';' == wstmt[0];
}

static
RETCODE	prep_params(StatementClass *stmt, QueryParse *qp, QueryBuild *qb, BOOL sync);
static	int
Prepare_and_convert(StatementClass *stmt, QueryParse *qp, QueryBuild *qb)
{
	CSTR func = "Prepare_and_convert";
	char	*exe_statement = NULL;
	RETCODE	retval;
	ConnectionClass	*conn = SC_get_conn(stmt);
	ConnInfo	*ci = &(conn->connInfo);
	BOOL	discardOutput, outpara;

	if (PROTOCOL_74(ci))
	{
		switch (stmt->prepared)
		{
			case NOT_YET_PREPARED:
			case ONCE_DESCRIBED:
				break;
			default:
				return SQL_SUCCESS;
		}
	}
	if (QB_initialize(qb, qp->stmt_len, stmt, NULL) < 0)
		return SQL_ERROR;
	if (PROTOCOL_74(ci))
		return prep_params(stmt, qp, qb, FALSE);
	discardOutput = (0 != (qb->flags & FLGB_DISCARD_OUTPUT));
	if (NOT_YET_PREPARED == stmt->prepared) /*  not yet prepared */
	{
		ssize_t	elen;
		int	i, oc;
		SQLSMALLINT	marker_count;
		const IPDFields *ipdopts = qb->ipdopts;
		char	plan_name[32];

		qb->flags |= FLGB_BUILDING_PREPARE_STATEMENT;
		sprintf(plan_name, "_PLAN%p", stmt);
#ifdef NOT_USED
		new_statement = qb->query_statement;
		sprintf(new_statement, "PREPARE \"%s\"", plan_name);
		qb->npos = strlen(new_statement);
#endif /* NOT_USED */
		CVT_APPEND_STR(qb, "PREPARE \"");
		CVT_APPEND_STR(qb, plan_name);
		CVT_APPEND_CHAR(qb, IDENTIFIER_QUOTE);
		marker_count = stmt->num_params - qb->num_discard_params;
		if (!ipdopts || ipdopts->allocated < marker_count)
		{
			SC_set_error(stmt, STMT_COUNT_FIELD_INCORRECT,
				"The # of binded parameters < the # of parameter markers", func);
			retval = SQL_ERROR;
			goto cleanup;
		}
		if (marker_count > 0)
		{
			CVT_APPEND_CHAR(qb, '(');
			for (i = qb->proc_return, oc = 0; i < stmt->num_params; i++)
			{
				outpara = FALSE;
				if (i < ipdopts->allocated &&
				    SQL_PARAM_OUTPUT == ipdopts->parameters[i].paramType)
				{
					outpara = TRUE;
					if (discardOutput)
						continue;
				}
				if (oc > 0)
					CVT_APPEND_STR(qb, ", ");
				if (outpara)
					CVT_APPEND_STR(qb, "void");
				else
					CVT_APPEND_STR(qb, pgtype_to_name(stmt, PIC_dsp_pgtype(conn, ipdopts->parameters[i]), -1, FALSE));
				oc++;
			}
			CVT_APPEND_CHAR(qb, ')');
		}
		CVT_APPEND_STR(qb, " as ");
		for (qp->opos = 0; qp->opos < qp->stmt_len; qp->opos++)
		{
			retval = inner_process_tokens(qp, qb);
			if (SQL_ERROR == retval)
				goto cleanup;
		}
		CVT_APPEND_CHAR(qb, ';');
		/* build the execute statement */
		exe_statement = malloc(30 + 2 * marker_count);
		sprintf(exe_statement, "EXECUTE \"%s\"", plan_name);
		if (marker_count > 0)
		{
			elen = strlen(exe_statement);
			exe_statement[elen++] = '(';
			for (i = 0; i < marker_count; i++)
			{
				if (i > 0)
					exe_statement[elen++] = ',';
				exe_statement[elen++]  = '?';
			}
			exe_statement[elen++] = ')';
			exe_statement[elen] = '\0';
		}
inolog("exe_statement=%s\n", exe_statement);
		stmt->execute_statement = exe_statement;
		QP_initialize(qp, stmt);
		SC_set_planname(stmt, plan_name);
	}
	qb->flags &= FLGB_DISCARD_OUTPUT;
	qb->flags |= FLGB_EXECUTE_PREPARED;
	qb->param_number = -1;
	for (qp->opos = 0; qp->opos < qp->stmt_len; qp->opos++)
	{
		retval = inner_process_tokens(qp, qb);
		if (SQL_ERROR == retval)
			goto cleanup;
	}
	/* make sure new_statement is always null-terminated */
	CVT_TERMINATE(qb);
	retval = SQL_SUCCESS;

cleanup:
	if (SQL_SUCCEEDED(retval))
	{
		if (exe_statement)
			SC_set_concat_prepare_exec(stmt);
		stmt->stmt_with_params = qb->query_statement;
		qb->query_statement = NULL;
	}
	else
	{
		QB_replace_SC_error(stmt, qb, func);
		if (exe_statement)
		{
			free(exe_statement);
			stmt->execute_statement = NULL;
		}
		QB_Destructor(qb);
	}
	return retval;
}

#define		my_strchr(conn, s1,c1) pg_mbschr(conn->ccsc, s1,c1)

static
RETCODE	prep_params(StatementClass *stmt, QueryParse *qp, QueryBuild *qb, BOOL sync)
{
	CSTR		func = "prep_params";
	RETCODE		retval;
	BOOL		ret, once_descr;
	ConnectionClass *conn = SC_get_conn(stmt);
	QResultClass	*res, *dest_res = NULL;
	char		plan_name[32];
	po_ind_t	multi;
	int		func_cs_count = 0;
	const char	*orgquery = NULL, *srvquery = NULL;
	Int4		endp1, endp2;
	SQLSMALLINT	num_pa = 0, num_p1, num_p2;

inolog("prep_params\n");
	once_descr = (ONCE_DESCRIBED == stmt->prepared);
	qb->flags |= FLGB_BUILDING_PREPARE_STATEMENT;
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
	ENTER_INNER_CONN_CS(conn, func_cs_count);
	if (NAMED_PARSE_REQUEST == SC_get_prepare_method(stmt))
		sprintf(plan_name, "_PLAN%p", stmt);
	else
		strcpy(plan_name, NULL_STRING);

	stmt->current_exec_param = 0;
	multi = stmt->multi_statement;
	if (multi > 0)
	{
		orgquery = stmt->statement;
		SC_scanQueryAndCountParams(orgquery, conn, &endp1, &num_p1, NULL, NULL);
		srvquery = qb->query_statement;
		SC_scanQueryAndCountParams(srvquery, conn, &endp2, NULL, NULL, NULL);
		mylog("%s:SendParseRequest for the first command length=%d(%d) num_p=%d\n", func, endp2, endp1, num_p1);
		ret = SendParseRequest(stmt, plan_name, srvquery, endp2, num_p1);
	}
	else
		ret = SendParseRequest(stmt, plan_name, qb->query_statement, SQL_NTS, -1);
	if (!ret)
		goto cleanup;
	if (!once_descr && (!SendDescribeRequest(stmt, plan_name, TRUE)))
		goto cleanup;
	SC_set_planname(stmt, plan_name);
	SC_set_prepared(stmt, plan_name[0] ? PREPARING_PERMANENTLY : PREPARING_TEMPORARILY);
	if (!sync)
	{
		retval = SQL_SUCCESS;
		goto cleanup;
	}
	if (!(res = SendSyncAndReceive(stmt, NULL, "prepare_and_describe")))
	{
		SC_set_error(stmt, STMT_NO_RESPONSE, "commnication error while preapreand_describe", func);
		CC_on_abort(conn, CONN_DEAD);
		goto cleanup;
	}
	if (once_descr)
		dest_res = res;
	else
		SC_set_Result(stmt, res);
	if (!QR_command_maybe_successful(res))
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "Error while preparing parameters", func);
		goto cleanup;
	}
	if (stmt->multi_statement <= 0)
	{
		retval = SQL_SUCCESS;
		goto cleanup;
	}
	while (multi > 0)
	{
		orgquery += (endp1 + 1);
		srvquery += (endp2 + 1);
		num_pa += num_p1;
		SC_scanQueryAndCountParams(orgquery, conn, &endp1, &num_p1, &multi, NULL);
		SC_scanQueryAndCountParams(srvquery, conn, &endp2, &num_p2, NULL, NULL);
		mylog("%s:SendParseRequest for the subsequent command length=%d(%d) num_p=%d\n", func, endp2, endp1, num_p1);
		if (num_p2 > 0)
		{
			stmt->current_exec_param = num_pa;
			ret = SendParseRequest(stmt, plan_name, srvquery, endp2 < 0 ? SQL_NTS : endp2, num_p1);
			if (!ret)	goto cleanup;
			if (!once_descr && !SendDescribeRequest(stmt, plan_name, TRUE))
				goto cleanup;
			if (!(res = SendSyncAndReceive(stmt, NULL, "prepare_and_describe")))
			{
				SC_set_error(stmt, STMT_NO_RESPONSE, "commnication error while preapreand_describe", func);
				CC_on_abort(conn, CONN_DEAD);
				goto cleanup;
			}
			QR_Destructor(res);
		}
	}
	retval = SQL_SUCCESS;
cleanup:
#undef	return
	if (dest_res)
		QR_Destructor(dest_res);
	CLEANUP_FUNC_CONN_CS(func_cs_count, conn);
	stmt->current_exec_param = -1;
	QB_Destructor(qb);
	return retval;
}

RETCODE	prepareParameters(StatementClass *stmt)
{
	switch (stmt->prepared)
	{
		QueryParse	query_org, *qp;
		QueryBuild	query_crt, *qb;
		case NOT_YET_PREPARED:
		case ONCE_DESCRIBED:

inolog("prepareParameters\n");
			qp = &query_org;
			QP_initialize(qp, stmt);
			qb = &query_crt;
			if (QB_initialize(qb, qp->stmt_len, stmt, NULL) < 0)
				return SQL_ERROR;
			return prep_params(stmt, qp, qb, TRUE);
	}
	return SQL_SUCCESS;
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

	UCHAR		*new_statement;

	BOOL	begin_first = FALSE, prepare_dummy_cursor = FALSE, bPrepConv;
	ConnectionClass *conn = SC_get_conn(stmt);
	ConnInfo   *ci = &(conn->connInfo);
	const		char *bestitem = NULL;

inolog("%s: enter prepared=%d\n", func, stmt->prepared);
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
				if (SC_is_updatable(stmt) &&
			    	    stmt->ntab > 0)
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
inolog("type=%d concur=%d\n", stmt->options.cursor_type, stmt->options.scroll_concurrency);
	}

	SC_miscinfo_clear(stmt);
	/* If the application hasn't set a cursor name, then generate one */
	if (!SC_cursor_is_valid(stmt))
	{
		char	curname[32];

		sprintf(curname, "SQL_CUR%p", stmt);
		STRX_TO_NAME(stmt->cursor_name, curname);
	}
	if (stmt->stmt_with_params)
	{
		free(stmt->stmt_with_params);
		stmt->stmt_with_params = NULL;
	}

	SC_no_fetchcursor(stmt);
	SC_no_pre_executable(stmt);
	if (SC_may_use_cursor(stmt))
		SC_set_pre_executable(stmt);
	qb = &query_crt;
	qb->query_statement = NULL;
	bPrepConv = FALSE;
	if (PREPARED_PERMANENTLY == stmt->prepared)
		bPrepConv = TRUE;
	else if (buildPrepareStatement &&
		 SQL_CONCUR_READ_ONLY == stmt->options.scroll_concurrency)
		bPrepConv = TRUE;
	if (bPrepConv)
	{
		retval = Prepare_and_convert(stmt, qp, qb);
		goto cleanup;
	}
	SC_forget_unnamed(stmt);
	buildPrepareStatement = FALSE;

	if (ci->disallow_premature)
		prepare_dummy_cursor = stmt->pre_executing;
	if (prepare_dummy_cursor)
		qp->flags |= FLGP_PREPARE_DUMMY_CURSOR;
	if (QB_initialize(qb, qp->stmt_len, stmt, NULL) < 0)
	{
		retval = SQL_ERROR;
		goto cleanup;
	}
	new_statement = qb->query_statement;

	/* For selects, prepend a declare cursor to the statement */
	if (SC_may_use_cursor(stmt) && !stmt->internal)
	{
		const char *opt_scroll = NULL_STRING, *opt_hold = NULL_STRING;

		if (prepare_dummy_cursor)
		{
			SC_set_fetchcursor(stmt);
			if (PG_VERSION_GE(conn, 7.4))
					opt_scroll = " scroll";
			if (!CC_is_in_trans(conn) && PG_VERSION_GE(conn, 7.1))
			{
				strcpy(new_statement, "BEGIN;");
				begin_first = TRUE;
			}
		}
		else if (ci->drivers.use_declarefetch
			 /** && SQL_CONCUR_READ_ONLY == stmt->options.scroll_concurrency **/
			)
		{
			SC_set_fetchcursor(stmt);
			if (SC_is_with_hold(stmt))
				opt_hold = " with hold";
			if (PG_VERSION_GE(conn, 7.4))
			{
				if (SQL_CURSOR_FORWARD_ONLY != stmt->options.cursor_type)
					opt_scroll = " scroll";
			}
		}
		if (SC_is_fetchcursor(stmt))
		{
			sprintf(new_statement, "%sdeclare \"%s\"%s cursor%s for ",
				new_statement, SC_cursor_name(stmt), opt_scroll, opt_hold);
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
	stmt->inaccurate_result = (0 != (qb->flags & FLGB_INACCURATE_RESULT));
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
		SC_no_pre_executable(stmt);
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
				if (isspace(new_statement[npos - 1]))
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
		memcpy(stmt->load_statement, qb->query_statement + qp->declare_pos, npos);
		stmt->load_statement[npos] = '\0';
	}
	if (prepare_dummy_cursor && SC_is_pre_executable(stmt))
	{
		char		fetchstr[128];

		sprintf(fetchstr, ";fetch backward in \"%s\";close \"%s\";",
				SC_cursor_name(stmt), SC_cursor_name(stmt));
		if (begin_first && CC_is_in_autocommit(conn))
			strcat(fetchstr, "COMMIT;");
		CVT_APPEND_STR(qb, fetchstr);
		stmt->inaccurate_result = TRUE;
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

Int4 findTag(const char *tag, char dollar_quote, int ccsc)
{
	Int4	taglen = 0;
	encoded_str	encstr;
	unsigned char	tchar;
	const char	*sptr;

	encoded_str_constr(&encstr, ccsc, tag + 1);
	for (sptr = tag + 1; *sptr; sptr++)
	{
		tchar = encoded_nextchar(&encstr);
		if (ENCODE_STATUS(encstr) != 0)
			continue;
		if (dollar_quote == tchar)
		{
			taglen = sptr - tag + 1;
			break;
		}
		if (isspace(tchar))
			break;
	}
	return taglen;
}

static
Int4 findIdentifier(const UCHAR *str, int ccsc, const UCHAR **nextdel)
{
	Int4	strlen = 0;
	encoded_str	encstr;
	unsigned char	tchar;
	const UCHAR	*sptr;
	BOOL	dquote = FALSE;

	*nextdel = NULL;
	encoded_str_constr(&encstr, ccsc, str);
	for (sptr = str; *sptr; sptr++)
	{
		tchar = encoded_nextchar(&encstr);
		if (ENCODE_STATUS(encstr) != 0)
			continue;
		if (sptr == str) /* the first character */
		{
			if (dquote = (IDENTIFIER_QUOTE == tchar), dquote)
				continue;
			if (!isalpha(tchar))
			{
				strlen = 0;
				if (!isspace(tchar))
					*nextdel = sptr;
				break;
			}
		}
		if (dquote)
		{
			if (IDENTIFIER_QUOTE == tchar)
			{
				if (IDENTIFIER_QUOTE == sptr[1])
				{
					encoded_nextchar(&encstr);
					sptr++;
					continue;
				}
				strlen = sptr - str + 1;
				sptr++;
				break;
			}
		}
		else
		{
			if (isalnum(tchar))
				continue;
			if (isspace(tchar))
			{
				strlen = sptr - str;
				break;
			}
			switch (tchar)
			{
				case '_':
				case '$':
					continue;
			}
			strlen = sptr - str;
			*nextdel = sptr;
			break;
		}
	}
	if (NULL == *nextdel)
	{
		for (; *sptr; sptr++)
		{
			if (!isspace((UCHAR) *sptr))
			{
				*nextdel = sptr;
				break;
			}
		}
	}
	return strlen;
}

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
	char	literal_quote = LITERAL_QUOTE, dollar_quote = DOLLAR_QUOTE, escape_in_literal = '\0';

	if (stmt && stmt->ntab > 0)
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
			qb->errormsg = "Should come here only when hamdling updatable cursors";
			return SQL_ERROR;
		}
		CVT_APPEND_STR(qb, ", \"ctid");
		if (bestitem)
		{
			CVT_APPEND_STR(qb, "\", \"");
			CVT_APPEND_STR(qb, bestitem);
		}
		CVT_APPEND_STR(qb, "\" ");
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
	if (ENCODE_STATUS(qp->encstr) != 0)
	{
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}

	/*
	 * From here we are guaranteed to handle a 1-byte character.
	 */
	if (qp->in_escape)			/* escape check */
	{
		qp->in_escape = FALSE;
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (qp->in_dollar_quote) /* dollar quote check */
	{
		if (oldchar == dollar_quote)
		{
			if (strncmp(F_OldPtr(qp), qp->dollar_tag, qp->taglen) == 0)
			{
				CVT_APPEND_DATA(qb, F_OldPtr(qp), qp->taglen);
				qp->opos += (qp->taglen - 1);
				qp->in_dollar_quote = FALSE;
				qp->in_literal = FALSE;
				qp->dollar_tag = NULL;
				qp->taglen = -1;
				return SQL_SUCCESS;
			}
		}
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (qp->in_literal) /* quote check */
	{
		if (oldchar == escape_in_literal)
			qp->in_escape = TRUE;
		else if (oldchar == literal_quote)
			qp->in_literal = FALSE;
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (qp->in_identifier) /* double quote check */
	{
		if (oldchar == IDENTIFIER_QUOTE)
			qp->in_identifier = FALSE;
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (qp->comment_level > 0) /* comment_level check */
	{
		if ('/' == oldchar &&
		    '*' == F_OldPtr(qp)[1])
		{
			qp->comment_level++;
			CVT_APPEND_CHAR(qb, oldchar);
			F_OldNext(qp);
			oldchar = F_OldChar(qp);
		}
		else if ('*' == oldchar &&
			 '/' == F_OldPtr(qp)[1])
		{
			qp->comment_level--;
			CVT_APPEND_CHAR(qb, oldchar);
			F_OldNext(qp);
			oldchar = F_OldChar(qp);
		}
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}
	else if (qp->in_line_comment) /* line comment check */
	{
		if (PG_LINEFEED == oldchar)
			qp->in_line_comment = FALSE;
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}

	/*
	 * From here we are guranteed to be in neither a literal_escape,
	 * a literal_quote nor an idetifier_quote.
	 */
	/* Squeeze carriage-return/linefeed pairs to linefeed only */
	else if (lf_conv &&
		 PG_CARRIAGE_RETURN == oldchar &&
		 qp->opos + 1 < qp->stmt_len &&
		 PG_LINEFEED == qp->statement[qp->opos + 1])
		return SQL_SUCCESS;

	/*
	 * Handle literals (date, time, timestamp) and ODBC scalar
	 * functions
	 */
	else if (oldchar == ODBC_ESCAPE_START)
	{
		if (SQL_ERROR == convert_escape(qp, qb))
		{
			if (0 == qb->errornumber)
			{
				qb->errornumber = STMT_EXEC_ERROR;
				qb->errormsg = "ODBC escape convert error";
			}
			mylog("%s convert_escape error\n", func);
			return SQL_ERROR;
		}
		return SQL_SUCCESS;
	}
	/* End of an escape sequence */
	else if (oldchar == ODBC_ESCAPE_END)
	{
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

				for (i = 0; i < num_fields; i++)
				{
        				if (*((char *)QR_get_value_backend_text(coli->result, i, COLUMNS_AUTO_INCREMENT)) == '1')
					{
						CVT_APPEND_STR(qb, "curr");
						CVT_APPEND_STR(qb, (char *)QR_get_value_backend_text(coli->result, i, COLUMNS_COLUMN_DEF) + 4);
						converted = TRUE;
						break;
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
		if (oldchar == dollar_quote)
		{
			qp->taglen = findTag(F_OldPtr(qp), dollar_quote, qp->encstr.ccsc);
			if (qp->taglen > 0)
			{
				qp->in_literal = TRUE;
				qp->in_dollar_quote = TRUE;
				qp->dollar_tag = F_OldPtr(qp);
				CVT_APPEND_DATA(qb, F_OldPtr(qp), qp->taglen);
				qp->opos += (qp->taglen - 1);
				return SQL_SUCCESS;
			}
		}
		else if (oldchar == literal_quote)
		{
			if (!qp->in_identifier)
			{
				qp->in_literal = TRUE;
				escape_in_literal = CC_get_escape(qb->conn);
				if (!escape_in_literal)
				{
					if (LITERAL_EXT == F_OldPtr(qp)[-1])
						escape_in_literal = ESCAPE_IN_LITERAL;
				}
			}
		}
		else if (oldchar == IDENTIFIER_QUOTE)
		{
			if (!qp->in_literal)
				qp->in_identifier = TRUE;
		}
		else if ('/' == oldchar &&
			 '*' == F_OldPtr(qp)[1])
		{
			qp->comment_level++;
		}
		else if ('-' == oldchar &&
			 '-' == F_OldPtr(qp)[1])
		{
			qp->in_line_comment = TRUE;
		}
		else if (oldchar == ';')
		{
			/*
			 * can't parse multiple statement using protocol V3.
			 * reset the dollar number here in case it is divided
			 * to parse.
			 */
			qb->dollar_number = 0;
			if (0 != (qp->flags & FLGP_USING_CURSOR))
			{
				const UCHAR *vp = &(qp->statement[qp->opos + 1]);

				while (*vp && isspace(*vp))
					vp++;
				if (*vp)	/* multiple statement */
				{
					qp->flags |= FLGP_MULTIPLE_STATEMENT;
					qb->flags &= ~FLGB_KEYSET_DRIVEN;
					remove_declare_cursor(qb, qp);
				}
			}
		}
		else
		{
			if (isspace((UCHAR) oldchar))
			{
				if (!qp->prev_token_end)
				{
					qp->prev_token_end = TRUE;
					qp->token_save[qp->token_len] = '\0';
					if (qp->token_len == 4)
					{
						if (0 != (qp->flags & FLGP_USING_CURSOR) &&
							into_table_from(&qp->statement[qp->opos - qp->token_len]))
						{
							qp->flags |= FLGP_SELECT_INTO;
							qb->flags &= ~FLGB_KEYSET_DRIVEN;
							qp->statement_type = STMT_TYPE_CREATE;
							remove_declare_cursor(qb, qp);
						}
						else if (stricmp(qp->token_save, "join") == 0)
						{
							if (stmt)
								check_join(stmt, F_OldPtr(qp), F_OldPos(qp));
						}
					}
					else if (qp->token_len == 3)
					{

						if (0 != (qp->flags & FLGP_USING_CURSOR) &&
							strnicmp(qp->token_save, "for", 3) == 0)
						{
							UInt4	flg;
							size_t	endpos;

							flg = table_for_update_or_share(F_OldPtr(qp), &endpos);
							if (0 != (FLGP_SELECT_FOR_UPDATE_OR_SHARE & flg))
							{
								if (qp->flags & FLGP_PREPARE_DUMMY_CURSOR)
								{
									qb->npos -= 4;
									qp->opos += endpos;
								}
								else
								{
									qp->flags |= flg;
									remove_declare_cursor(qb, qp);
								}
							}
							else
								qp->flags |= flg;
						}
					}
					else if (qp->token_len == 2)
					{
						size_t	endpos;

						if (STMT_TYPE_INSERT == qp->statement_type &&
							strnicmp(qp->token_save, "()", 2) == 0 &&
							insert_without_target(F_OldPtr(qp), &endpos))
						{
							qb->npos -= 2;
							CVT_APPEND_STR(qb, "DEFAULT VALUES");
							qp->opos += endpos;
							return SQL_SUCCESS;
						}
					}
				}
			}
			else if (qp->prev_token_end)
			{
				qp->prev_token_end = FALSE;
				qp->token_save[0] = oldchar;
				qp->token_len = 1;
			}
			else if (qp->token_len + 1 < sizeof(qp->token_save))
				qp->token_save[qp->token_len++] = oldchar;
		}
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}

	/*
	 * Its a '?' parameter alright
	 */
	if (retval = ResolveOneParam(qb, qp), retval < 0)
		return retval;

	if (SQL_SUCCESS_WITH_INFO == retval) /* means discarding output parameter */
	{
	}
	retval = SQL_SUCCESS;
cleanup:
	return retval;
}

#define	MIN_ALC_SIZE	128
BOOL	BuildBindRequest(StatementClass *stmt, const char *plan_name)
{
	CSTR func = "BuildBindRequest";
	QueryBuild	qb;
	size_t		leng, plen;
	UInt4		netleng;
	SQLSMALLINT	num_p;
	Int2		netnum_p;
	int		i, num_params;
	char		*bindreq;
	ConnectionClass	*conn = SC_get_conn(stmt);
	BOOL		ret = TRUE, sockerr = FALSE, discard_output;
	RETCODE		retval;
	const		IPDFields *ipdopts = SC_get_IPDF(stmt);

	num_params = stmt->num_params;
	if (num_params < 0)
	{
		PGAPI_NumParams(stmt, &num_p);
		num_params = num_p;
	}
	if (ipdopts->allocated < num_params)
	{
		SC_set_error(stmt, STMT_COUNT_FIELD_INCORRECT, "The # of binded parameters < the # of parameter markers", func);
		return FALSE;
	}
        plen = strlen(plan_name);
	netleng = sizeof(netleng)	/* length fields */
		  + 2 * (plen + 1)	/* portal name/plan name */
		  + sizeof(Int2) * (num_params + 1) /* parameter types (max) */
		  + sizeof(Int2)	/* result format */
		  + 1;
        if (QB_initialize(&qb, netleng > MIN_ALC_SIZE ? netleng : MIN_ALC_SIZE, stmt, NULL) < 0)
	{
                return FALSE;
	}
        qb.flags |= FLGB_BUILDING_BIND_REQUEST;
        qb.flags |= FLGB_BINARY_AS_POSSIBLE;
        bindreq = qb.query_statement;
        leng = sizeof(netleng);
        memcpy(bindreq + leng, plan_name, plen + 1); /* portal name */
        leng += (plen + 1);
        memcpy(bindreq + leng, plan_name, plen + 1); /* prepared plan name */
        leng += (plen + 1);
inolog("num_params=%d proc_return=%d\n", num_params, stmt->proc_return);
        num_p = num_params - qb.num_discard_params;
inolog("num_p=%d\n", num_p);
	discard_output = (0 != (qb.flags & FLGB_DISCARD_OUTPUT));
        netnum_p = htons(num_p);	/* Network byte order */
	if (0 != (qb.flags & FLGB_BINARY_AS_POSSIBLE))
	{
		int	j;
		ParameterImplClass	*parameters = ipdopts->parameters;
		Int2	net_one = 1;
 
		net_one = htons(net_one);
        	memcpy(bindreq + leng, &netnum_p, sizeof(netnum_p)); /* number of parameter format */
        	leng += sizeof(Int2);
		if (num_p > 0)
        		memset(bindreq + leng, 0, sizeof(Int2) * num_p);  /* initialize by text format */
		for (i = stmt->proc_return, j = 0; i < num_params; i++)
		{
inolog("%dth parameter type oid is %u\n", i, PIC_dsp_pgtype(conn, parameters[i]));
			if (discard_output &&
			    SQL_PARAM_OUTPUT == parameters[i].paramType)
				continue;
			if (PG_TYPE_BYTEA == PIC_dsp_pgtype(conn, parameters[i]))
			{
				mylog("%dth parameter is of binary format\n", j);
				memcpy(bindreq + leng + sizeof(Int2) * j,
        			&net_one, sizeof(net_one));  /* binary */
			}
			j++; 
		}
		leng += sizeof(Int2) * num_p;
	}
	else
	{
        	memset(bindreq + leng, 0, sizeof(Int2));  /* text format */
        	leng += sizeof(Int2);
	}
        memcpy(bindreq + leng, &netnum_p, sizeof(netnum_p)); /* number of params */
        leng += sizeof(Int2); /* must be 2 */
        qb.npos = leng;
        for (i = 0; i < stmt->num_params; i++)
	{
                retval = ResolveOneParam(&qb, NULL);
		if (SQL_ERROR == retval)
		{
			QB_replace_SC_error(stmt, &qb, func);
			ret = FALSE;
			goto cleanup;
		}
	}

        leng = qb.npos;
        memset(qb.query_statement + leng, 0, sizeof(Int2)); /* result format is text */
        leng += sizeof(Int2);
inolog("bind leng=%d\n", leng);
        netleng = htonl((UInt4) leng);	/* Network byte order */
        memcpy(qb.query_statement, &netleng, sizeof(netleng));
	if (CC_is_in_trans(conn) && !SC_accessed_db(stmt))
	{
		if (SQL_ERROR == SetStatementSvp(stmt))
		{
			SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal savepoint error in SendBindRequest", func);
			ret = FALSE;
			goto cleanup;
		}
	}

        SOCK_put_char(conn->sock, 'B'); /* Bind Message */
	if (SOCK_get_errcode(conn->sock) != 0)
	{
		sockerr = TRUE;
		goto cleanup;
	}
        SOCK_put_n_char(conn->sock, qb.query_statement, leng);
	if (SOCK_get_errcode(conn->sock) != 0)
		sockerr = TRUE;
cleanup:
	QB_Destructor(&qb);

	if (sockerr)
	{
		CC_set_error(conn, CONNECTION_COULD_NOT_SEND, "Could not send D Request to backend", func);
		CC_on_abort(conn, CONN_DEAD);
		ret = FALSE;
	}
	return ret;
}

#if (ODBCVER >= 0x0300)
static BOOL
ResolveNumericParam(const SQL_NUMERIC_STRUCT *ns, char *chrform)
{
	static const int prec[] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25, 27, 29, 32, 34, 37, 39};
	Int4	i, j, k, ival, vlen, len, newlen;
	UCHAR		calv[40];
	const UCHAR	*val = (const UCHAR *) ns->val;
	BOOL	next_figure;

inolog("C_NUMERIC [prec=%d scale=%d]", ns->precision, ns->scale);
	if (0 == ns->precision)
	{
		strcpy(chrform, "0");
		return TRUE;
	}
	else if (ns->precision < prec[sizeof(Int4)])
	{
		for (i = 0, ival = 0; i < sizeof(Int4) && prec[i] <= ns->precision; i++)
		{
inolog("(%d)", val[i]);
			ival += (val[i] << (8 * i)); /* ns->val is little endian */
		}
inolog(" ival=%d,%d", ival, (val[3] << 24) | (val[2] << 16) | (val[1] << 8) | val[0]);
		if (0 == ns->scale)
		{
			if (0 == ns->sign)
				ival *= -1;
			sprintf(chrform, "%d", ival);
		}
		else if (ns->scale > 0)
		{
			Int4	i, div, o1val, o2val;

			for (i = 0, div = 1; i < ns->scale; i++)
				div *= 10;
			o1val = ival / div;
			o2val = ival % div;
			if (0 == ns->sign)
				sprintf(chrform, "-%d.%0*d", o1val, ns->scale, o2val);
			else
				sprintf(chrform, "%d.%0*d", o1val, ns->scale, o2val);
		}
inolog(" convval=%s\n", chrform);
		return TRUE;
	}

	for (i = 0; i < SQL_MAX_NUMERIC_LEN && prec[i] <= ns->precision; i++)
		;
	vlen = i;
	len = 0;
	memset(calv, 0, sizeof(calv));
inolog(" len1=%d", vlen);
	for (i = vlen - 1; i >= 0; i--)
	{
		for (j = len - 1; j >= 0; j--)
		{
			if (!calv[j])
				continue;
			ival = (((Int4)calv[j]) << 8);
			calv[j] = (ival % 10);
			ival /= 10;
			calv[j + 1] += (ival % 10);
			ival /= 10;
			calv[j + 2] += (ival % 10);
			ival /= 10;
			calv[j + 3] += ival;
			for (k = j;; k++)
			{
				next_figure = FALSE;
				if (calv[k] > 0)
				{
					if (k >= len)
						len = k + 1;
					while (calv[k] > 9)
					{
						calv[k + 1]++;
						calv[k] -= 10;
						next_figure = TRUE;
					}
				}
				if (k >= j + 3 && !next_figure)
					break;
			}
		}
		ival = val[i];
		if (!ival)
			continue;
		calv[0] += (ival % 10);
		ival /= 10;
		calv[1] += (ival % 10);
		ival /= 10;
		calv[2] += ival;
		for (j = 0;; j++)
		{
			next_figure = FALSE;
			if (calv[j] > 0)
			{
				if (j >= len)
					len = j + 1;
				while (calv[j] > 9)
				{
					calv[j + 1]++;
					calv[j] -= 10;
					next_figure = TRUE;
				}
			}
			if (j >= 2 && !next_figure)
				break;
		}
	}
inolog(" len2=%d", len);
	newlen = 0;
	if (0 == ns->sign)
		chrform[newlen++] = '-';
	if (i = len - 1, i < ns->scale)
		i = ns->scale;
	for (; i >= ns->scale; i--)
		chrform[newlen++] = calv[i] + '0';
	if (ns->scale > 0)
	{
		chrform[newlen++] = '.';
		for (; i >= 0; i--)
			chrform[newlen++] = calv[i] + '0';
	}
	if (0 == len)
		chrform[newlen++] = '0';
	chrform[newlen] = '\0';
inolog(" convval(2) len=%d %s\n", newlen, chrform);
	return TRUE;
}
#endif /* ODBCVER */

/*
 *
 */
static int
ResolveOneParam(QueryBuild *qb, QueryParse *qp)
{
	CSTR func = "ResolveOneParam";

	ConnectionClass *conn = qb->conn;
	const APDFields *apdopts = qb->apdopts;
	const IPDFields *ipdopts = qb->ipdopts;
	PutDataInfo *pdata = qb->pdata;

	int		param_number;
	char		param_string[128], tmp[256],
			cbuf[PG_NUMERIC_MAX_PRECISION * 2]; /* seems big enough to handle the data in this function */
	OID		param_pgtype;
	SQLSMALLINT	param_ctype, param_sqltype;
	SIMPLE_TIME	st;
	time_t		t;
	struct tm	*tim;
#ifdef	HAVE_LOCALTIME_R
	struct tm	tm;
#endif /* HAVE_LOCALTIME_R */
	SQLLEN		used;
	char		*buffer, *buf, *allocbuf = NULL, *lastadd = NULL;
	OID		lobj_oid;
	int		lobj_fd;
	SQLULEN	offset = apdopts->param_offset_ptr ? *apdopts->param_offset_ptr : 0;
	size_t	current_row = qb->current_row;
	int	npos = 0;
	BOOL	handling_large_object = FALSE, req_bind, add_quote = FALSE;
	ParameterInfoClass	*apara;
	ParameterImplClass	*ipara;
	BOOL outputDiscard, valueOutput;
	SDOUBLE	dbv;
	SFLOAT	flv;
#if (ODBCVER >= 0x0300)
	SQL_INTERVAL_STRUCT	*ivstruct;
	const char		*ivsign;
#endif /* ODBCVER */
	RETCODE	retval = SQL_ERROR;

	outputDiscard = (0 != (qb->flags & FLGB_DISCARD_OUTPUT));
	valueOutput = (0 == (qb->flags & (FLGB_PRE_EXECUTING | FLGB_BUILDING_PREPARE_STATEMENT)));

	if (qb->proc_return < 0 && qb->stmt)
		qb->proc_return = qb->stmt->proc_return;
	/*
	 * Its a '?' parameter alright
	 */
	param_number = ++qb->param_number;

inolog("resolveOneParam %d(%d,%d)\n", param_number, ipdopts->allocated, apdopts->allocated);
	apara = NULL;
	ipara = NULL;
	if (param_number < apdopts->allocated)
		apara = apdopts->parameters + param_number;
	if (param_number < ipdopts->allocated)
		ipara = ipdopts->parameters + param_number;
	if ((!apara || !ipara) && valueOutput)
	{
mylog("!!! The # of binded parameters (%d, %d) < the # of parameter markers %d\n", apdopts->allocated, ipdopts->allocated, param_number);
		qb->errormsg = "The # of binded parameters < the # of parameter markers";
		qb->errornumber = STMT_COUNT_FIELD_INCORRECT;
		CVT_TERMINATE(qb);	/* just in case */
		return SQL_ERROR;
	}
	if (0 != (qb->flags & FLGB_EXECUTE_PREPARED)
	    && (outputDiscard || param_number < qb->proc_return)
			)
	{
		while (ipara && SQL_PARAM_OUTPUT == ipara->paramType)
		{
			apara = NULL;
			ipara = NULL;
			param_number = ++qb->param_number;
			if (param_number < apdopts->allocated)
				apara = apdopts->parameters + param_number;
			if (param_number < ipdopts->allocated)
				ipara = ipdopts->parameters + param_number;
		}
	}
 
inolog("ipara=%p paramType=%d %d proc_return=%d\n", ipara, ipara ? ipara->paramType : -1, PG_VERSION_LT(conn, 8.1), qb->proc_return);
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
	if (ipara &&
	    SQL_PARAM_OUTPUT == ipara->paramType)
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
			for (npos = qb->npos - 1; npos >= 0 && isspace(qb->query_statement[npos]) ; npos--) ;
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
						for (npos = qp->opos + 1; isspace(qp->statement[npos]); npos++) ;
						if (qp->statement[npos] == ',')
							qp->opos = npos;
						break;
				}
			}
			return SQL_SUCCESS_WITH_INFO;
		}
	}

	if (!apara || !ipara)
	{
		if (0 != (qb->flags & FLGB_PRE_EXECUTING))
		{
			CVT_APPEND_STR(qb, "NULL");
			qb->flags |= FLGB_INACCURATE_RESULT;
			return SQL_SUCCESS;
		}
	}
	if (0 != (qb->flags & FLGB_BUILDING_PREPARE_STATEMENT))
	{
		char	pnum[16];

#ifdef	NOT_USED /* !! named parameter is unavailable !! */
		if (ipara && SAFE_NAME(ipara->paramName)[0])
		{
			CVT_APPEND_CHAR(qb, '"'); 
			CVT_APPEND_STR(qb, SAFE_NAME(ipara->paramName)); 
			CVT_APPEND_CHAR(qb, '"'); 
			CVT_APPEND_STR(qb, " = "); 
		}
#endif /* NOT_USED */
		qb->dollar_number++;
		sprintf(pnum, "$%d", qb->dollar_number);
		CVT_APPEND_STR(qb, pnum); 
		return SQL_SUCCESS;
	}
 
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
			SQLULEN	p_offset = offset;

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

	req_bind = (0 != (FLGB_BUILDING_BIND_REQUEST & qb->flags));
	/* Handle DEFAULT_PARAM parameter data. Should be NULL ?
	if (used == SQL_DEFAULT_PARAM)
	{
		return SQL_SUCCESS;
	} */
	if (req_bind)
	{
		npos = qb->npos;
		ENLARGE_NEWSTATEMENT(qb, npos + 4);
		qb->npos += 4;
	}
	/* Handle NULL parameter data */
	if ((ipara && SQL_PARAM_OUTPUT == ipara->paramType)
	    || used == SQL_NULL_DATA
	    || used == SQL_DEFAULT_PARAM)
	{
		if (req_bind)
		{
			DWORD	len = htonl(-1);
			memcpy(qb->query_statement + npos, &len, sizeof(len));
		}
		else
			CVT_APPEND_STR(qb, "NULL");
		return SQL_SUCCESS;
	}

	/*
	 * If no buffer, and it's not null, then what the hell is it? Just
	 * leave it alone then.
	 */
	if (!buffer)
	{
		if (0 != (qb->flags & FLGB_PRE_EXECUTING))
		{
			CVT_APPEND_STR(qb, "NULL");
			qb->flags |= FLGB_INACCURATE_RESULT;
			return SQL_SUCCESS;
		}
		else if (!handling_large_object)
		{
			CVT_APPEND_CHAR(qb, '?');
			return SQL_SUCCESS;
		}
	}

	param_ctype = apara->CType;
	param_sqltype = ipara->SQLType;
	param_pgtype = PIC_dsp_pgtype(qb->conn, *ipara);

	mylog("%s: from(fcType)=%d, to(fSqlType)=%d(%u)\n", func,
				param_ctype, param_sqltype, param_pgtype);

	/* replace DEFAULT with something we can use */
	if (param_ctype == SQL_C_DEFAULT)
	{
		param_ctype = sqltype_to_default_ctype(conn, param_sqltype);
		if (param_ctype == SQL_C_WCHAR
		    && CC_default_is_c(conn))
			param_ctype =SQL_C_CHAR;
	}

	allocbuf = buf = NULL;
	param_string[0] = '\0';
	cbuf[0] = '\0';
	memset(&st, 0, sizeof(st));
	t = SC_get_time(qb->stmt);
#ifdef	HAVE_LOCALTIME_R
	tim = localtime_r(&t, &tm);
#else
	tim = localtime(&t);
#endif /* HAVE_LOCALTIME_R */
	st.m = tim->tm_mon + 1;
	st.d = tim->tm_mday;
	st.y = tim->tm_year + 1900;

#if (ODBCVER >= 0x0300)
	ivstruct = (SQL_INTERVAL_STRUCT *) buffer;
#endif /* ODBCVER */
	/* Convert input C type to a neutral format */
	switch (param_ctype)
	{
		case SQL_C_BINARY:
			buf = buffer;
			break;
		case SQL_C_CHAR:
#ifdef	WIN_UNICODE_SUPPORT
			if (SQL_NTS == used)
				used = strlen(buffer);
			allocbuf = malloc(WCLEN * (used + 1));
			used = msgtowstr(NULL, buffer, (int) used, (LPWSTR) allocbuf, (int) (used + 1));
			buf = ucs2_to_utf8((SQLWCHAR *) allocbuf, used, &used, FALSE);
			free(allocbuf);
			allocbuf = buf;
#else
			buf = buffer;
#endif /* WIN_UNICODE_SUPPORT */
			break;

#ifdef	UNICODE_SUPPORT
		case SQL_C_WCHAR:
mylog("C_WCHAR=%s(%d)\n", buffer, used);
			buf = allocbuf = ucs2_to_utf8((SQLWCHAR *) buffer, used > 0 ? used / WCLEN : used, &used, FALSE);
			/* used *= WCLEN; */
			break;
#endif /* UNICODE_SUPPORT */

		case SQL_C_DOUBLE:
			dbv = *((SDOUBLE *) buffer);
#ifdef	WIN32
			if (_finite(dbv))
#endif /* WIN32 */
			{
				sprintf(param_string, "%.15g", dbv);
				set_server_decimal_point(param_string);
			}
#ifdef	WIN32
			else if (_isnan(dbv))
				strcpy(param_string, NAN_STRING);
			else if (dbv < .0)
				strcpy(param_string, MINFINITY_STRING);
			else 
				strcpy(param_string, INFINITY_STRING);
#endif /* WIN32 */
			break;

		case SQL_C_FLOAT:
			flv = *((SFLOAT *) buffer);
#ifdef	WIN32
			if (_finite(flv))
#endif /* WIN32 */
			{
				sprintf(param_string, "%.6g", flv);
				set_server_decimal_point(param_string);
			}
#ifdef	WIN32
			else if (_isnan(flv))
				strcpy(param_string, NAN_STRING);
			else if (flv < .0)
				strcpy(param_string, MINFINITY_STRING);
			else 
				strcpy(param_string, INFINITY_STRING);
#endif /* WIN32 */
			break;

		case SQL_C_SLONG:
		case SQL_C_LONG:
			sprintf(param_string, FORMAT_INTEGER,
					*((SQLINTEGER *) buffer));
			break;

#if (ODBCVER >= 0x0300) && defined(ODBCINT64)
		case SQL_C_SBIGINT:
		case SQL_BIGINT: /* Is this needed ? */
			sprintf(param_string, FORMATI64,
					*((SQLBIGINT *) buffer));
			break;

		case SQL_C_UBIGINT:
			sprintf(param_string, FORMATI64U,
					*((SQLUBIGINT *) buffer));
			break;

#endif /* ODBCINT64 */
		case SQL_C_SSHORT:
		case SQL_C_SHORT:
			sprintf(param_string, "%d",
					*((SQLSMALLINT *) buffer));
			break;

		case SQL_C_STINYINT:
		case SQL_C_TINYINT:
			sprintf(param_string, "%d",
					*((SCHAR *) buffer));
			break;

		case SQL_C_ULONG:
			sprintf(param_string, FORMAT_UINTEGER,
					*((SQLUINTEGER *) buffer));
			break;

		case SQL_C_USHORT:
			sprintf(param_string, "%u",
					*((SQLUSMALLINT *) buffer));
			break;

		case SQL_C_UTINYINT:
			sprintf(param_string, "%u",
					*((UCHAR *) buffer));
			break;

		case SQL_C_BIT:
			{
				int	i = *((UCHAR *) buffer);

				sprintf(param_string, "%d", i ? 1 : 0);
				break;
			}

		case SQL_C_DATE:
#if (ODBCVER >= 0x0300)
		case SQL_C_TYPE_DATE:		/* 91 */
#endif
			{
				DATE_STRUCT *ds = (DATE_STRUCT *) buffer;

				st.m = ds->month;
				st.d = ds->day;
				st.y = ds->year;

				break;
			}

		case SQL_C_TIME:
#if (ODBCVER >= 0x0300)
		case SQL_C_TYPE_TIME:		/* 92 */
#endif
			{
				TIME_STRUCT *ts = (TIME_STRUCT *) buffer;

				st.hh = ts->hour;
				st.mm = ts->minute;
				st.ss = ts->second;

				break;
			}

		case SQL_C_TIMESTAMP:
#if (ODBCVER >= 0x0300)
		case SQL_C_TYPE_TIMESTAMP:	/* 93 */
#endif
			{
				TIMESTAMP_STRUCT *tss = (TIMESTAMP_STRUCT *) buffer;

				st.m = tss->month;
				st.d = tss->day;
				st.y = tss->year;
				st.hh = tss->hour;
				st.mm = tss->minute;
				st.ss = tss->second;
				st.fr = tss->fraction;

				mylog("m=%d,d=%d,y=%d,hh=%d,mm=%d,ss=%d\n", st.m, st.d, st.y, st.hh, st.mm, st.ss);

				break;

			}
#if (ODBCVER >= 0x0300)
		case SQL_C_NUMERIC:
			if (ResolveNumericParam((SQL_NUMERIC_STRUCT *) buffer, param_string))
				break;
		case SQL_C_INTERVAL_YEAR:
			ivsign = ivstruct->interval_sign ? "-" : "";
			sprintf(param_string, "%s%d years", ivsign, ivstruct->intval.year_month.year);
			break;
		case SQL_C_INTERVAL_MONTH:
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
			ivsign = ivstruct->interval_sign ? "-" : "";
			sprintf(param_string, "%s%d years %s%d mons", ivsign, ivstruct->intval.year_month.year, ivsign, ivstruct->intval.year_month.month);
			break;
		case SQL_C_INTERVAL_DAY:
			ivsign = ivstruct->interval_sign ? "-" : "";
			sprintf(param_string, "%s%d days", ivsign, ivstruct->intval.day_second.day);
			break;
		case SQL_C_INTERVAL_HOUR:
		case SQL_C_INTERVAL_DAY_TO_HOUR:
			ivsign = ivstruct->interval_sign ? "-" : "";
			sprintf(param_string, "%s%d days %s%02d:00:00", ivsign, ivstruct->intval.day_second.day, ivsign, ivstruct->intval.day_second.hour);
			break;
		case SQL_C_INTERVAL_MINUTE:
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
			ivsign = ivstruct->interval_sign ? "-" : "";
			sprintf(param_string, "%s%d days %s%02d:%02d:00", ivsign, ivstruct->intval.day_second.day, ivsign, ivstruct->intval.day_second.hour, ivstruct->intval.day_second.minute);
			break;

		case SQL_C_INTERVAL_SECOND:
		case SQL_C_INTERVAL_DAY_TO_SECOND:
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
			ivsign = ivstruct->interval_sign ? "-" : "";
			sprintf(param_string, "%s%d days %s%02d:%02d:%02d",
				ivsign, ivstruct->intval.day_second.day,
				ivsign, ivstruct->intval.day_second.hour,
				ivstruct->intval.day_second.minute,
				ivstruct->intval.day_second.second);
			if (ivstruct->intval.day_second.fraction > 0)
			{
				int fraction = ivstruct->intval.day_second.fraction, prec = apara->precision;
				
				while (fraction % 10 == 0)
				{
					fraction /= 10;
					prec--;
				}
				sprintf(&param_string[strlen(param_string)], ".%0*d", prec, fraction);
			}
			break;
#endif
#if (ODBCVER >= 0x0350)
		case SQL_C_GUID:
		{
			SQLGUID *g = (SQLGUID *) buffer;
			snprintf (param_string, sizeof(param_string),
				"%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
				g->Data1,
				g->Data2, g->Data3,
				g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3],
				g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
		}
		break;
#endif
		default:
			/* error */
			qb->errormsg = "Unrecognized C_parameter type in copy_statement_with_parameters";
			qb->errornumber = STMT_NOT_IMPLEMENTED_ERROR;
			CVT_TERMINATE(qb);	/* just in case */
			retval = SQL_ERROR;
			goto cleanup;
	}

	/*
	 * Now that the input data is in a neutral format, convert it to
	 * the desired output format (sqltype)
	 */

	/* Special handling NULL string For FOXPRO */
mylog("cvt_null_date_string=%d pgtype=%d buf=%p\n", conn->connInfo.cvt_null_date_string, param_pgtype, buf);
	if (conn->connInfo.cvt_null_date_string > 0 &&
	    (PG_TYPE_DATE == param_pgtype ||
	     PG_TYPE_DATETIME == param_pgtype ||
	     PG_TYPE_TIMESTAMP_NO_TMZONE == param_pgtype) &&
	    NULL != buf &&
	    (
		(SQL_C_CHAR == param_ctype && '\0' == buf[0])
#ifdef	UNICODE_SUPPORT
		|| (SQL_C_WCHAR ==param_ctype && '\0' == buf[0] && '\0' == buf[1])
#endif /* UNICODE_SUPPORT */
	    ))
	{
		if (req_bind)
		{
			DWORD	len = htonl(-1);
			memcpy(qb->query_statement + npos, &len, sizeof(len));
		}
		else
			CVT_APPEND_STR(qb, "NULL");
		retval = SQL_SUCCESS;
		goto cleanup;
	}
	if (!req_bind)
	{
		switch (param_sqltype)
		{
			case SQL_INTEGER:
			case SQL_SMALLINT:
				break;
			case SQL_CHAR:
			case SQL_VARCHAR:
			case SQL_LONGVARCHAR:
			case SQL_BINARY:
			case SQL_VARBINARY:
			case SQL_LONGVARBINARY:
#ifdef	UNICODE_SUPPORT
			case SQL_WCHAR:
			case SQL_WVARCHAR:
			case SQL_WLONGVARCHAR:
#endif /* UNICODE_SUPPORT */
mylog("buf=%p flag=%d\n", buf, qb->flags);
				if (buf && (qb->flags & FLGB_LITERAL_EXTENSION) != 0)
				{
					CVT_APPEND_CHAR(qb, LITERAL_EXT);
				}
			default:
				CVT_APPEND_CHAR(qb, LITERAL_QUOTE);
				add_quote = TRUE;
				break;
		}
	}
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

			switch (param_pgtype)
			{
				case PG_TYPE_BOOL:
					/* consider True is -1 case */
					if (NULL != buf)
					{
						if ('-' == buf[0] &&
						    '1' == buf[1])
							strcpy(buf, "1");
					}
					else if ('-' == param_string[0] &&
						 '1' == param_string[1])
						strcpy(param_string, "1");
					break;
				case PG_TYPE_FLOAT4:
				case PG_TYPE_FLOAT8:
				case PG_TYPE_NUMERIC:
					if (NULL != buf)
						set_server_decimal_point(buf);
					else
						set_server_decimal_point(param_string);
					break;
			}
			/* it was a SQL_C_CHAR */
			if (buf)
				CVT_SPECIAL_CHARS(qb, buf, used);
			/* it was a numeric type */
			else if (param_string[0] != '\0')
				CVT_APPEND_STR(qb, param_string);

			/* it was date,time,timestamp -- use m,d,y,hh,mm,ss */
			else
			{
				snprintf(tmp, sizeof(tmp), "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
						st.y, st.m, st.d, st.hh, st.mm, st.ss);

				CVT_APPEND_STR(qb, tmp);
			}
			break;

		case SQL_DATE:
#if (ODBCVER >= 0x0300)
		case SQL_TYPE_DATE:	/* 91 */
#endif
			if (buf)
			{				/* copy char data to time */
				my_strcpy(cbuf, sizeof(cbuf), buf, used);
				parse_datetime(cbuf, &st);
			}

			if (st.y < 0)
				sprintf(tmp, "%.4d-%.2d-%.2d BC", -st.y, st.m, st.d);
			else
				sprintf(tmp, "%.4d-%.2d-%.2d", st.y, st.m, st.d);
			lastadd = "::date";
			CVT_APPEND_STR(qb, tmp);
			break;

		case SQL_TIME:
#if (ODBCVER >= 0x0300)
		case SQL_TYPE_TIME:	/* 92 */
#endif
			if (buf)
			{				/* copy char data to time */
				my_strcpy(cbuf, sizeof(cbuf), buf, used);
				parse_datetime(cbuf, &st);
			}

			sprintf(tmp, "%.2d:%.2d:%.2d", st.hh, st.mm, st.ss);
			lastadd = "::time";
			CVT_APPEND_STR(qb, tmp);
			break;

		case SQL_TIMESTAMP:
#if (ODBCVER >= 0x0300)
		case SQL_TYPE_TIMESTAMP:	/* 93 */
#endif

			if (buf)
			{
				my_strcpy(cbuf, sizeof(cbuf), buf, used);
				parse_datetime(cbuf, &st);
			}

			/*
			 * sprintf(tmp, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", st.y,
			 * st.m, st.d, st.hh, st.mm, st.ss);
			 */
			/* Time zone stuff is unreliable */
			stime2timestamp(&st, tmp, USE_ZONE, PG_VERSION_GE(conn, 7.2) ? 6 : 0);
			lastadd = "::timestamp";
			CVT_APPEND_STR(qb, tmp);

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
							used = strlen(buf);
							break;
					}
					allocbuf = malloc(used / 2 + 1);
					if (allocbuf)
					{
						pg_hex2bin(buf, allocbuf, used);
						buf = allocbuf;
						used /= 2;
					}
					break;
				default:
					qb->errormsg = "Could not convert the ctype to binary type";
					qb->errornumber = STMT_EXEC_ERROR;
					retval = SQL_ERROR;
					goto cleanup;
			}
			if (param_pgtype == PG_TYPE_BYTEA)
			{
				if (0 != (qb->flags & FLGB_BINARY_AS_POSSIBLE))
				{
					mylog("sending binary data leng=%d\n", used);
					CVT_APPEND_DATA(qb, buf, used);
				}
				else
				{
					/* non-ascii characters should be
				 	 * converted to octal
				 	 */
					mylog("SQL_VARBINARY: about to call convert_to_pgbinary, used = %d\n", used);

					CVT_APPEND_BINARY(qb, buf, used);
				}
				break;
			}
			if (PG_TYPE_OID == param_pgtype && conn->lo_is_domain)
				;
			else if (param_pgtype != conn->lobj_type)
			{
				qb->errormsg = "Could not convert binary other than LO type";
				qb->errornumber = STMT_EXEC_ERROR;
				retval = SQL_ERROR;
				goto cleanup;
			}

			if (apara->data_at_exec)
				lobj_oid = pdata->pdata[param_number].lobj_oid;
			else
			{
				BOOL	is_in_trans_at_entry = CC_is_in_trans(conn);

				/* begin transaction if needed */
				if (!is_in_trans_at_entry)
				{
					if (!CC_begin(conn))
					{
						qb->errormsg = "Could not begin (in-line) a transaction";
						qb->errornumber = STMT_EXEC_ERROR;
						retval = SQL_ERROR;
						goto cleanup;
					}
				}

				/* store the oid */
				lobj_oid = odbc_lo_creat(conn, INV_READ | INV_WRITE);
				if (lobj_oid == 0)
				{
					qb->errornumber = STMT_EXEC_ERROR;
					qb->errormsg = "Couldnt create (in-line) large object.";
					retval = SQL_ERROR;
					goto cleanup;
				}

				/* store the fd */
				lobj_fd = odbc_lo_open(conn, lobj_oid, INV_WRITE);
				if (lobj_fd < 0)
				{
					qb->errornumber = STMT_EXEC_ERROR;
					qb->errormsg = "Couldnt open (in-line) large object for writing.";
					retval = SQL_ERROR;
					goto cleanup;
				}

				retval = odbc_lo_write(conn, lobj_fd, buffer, (Int4) used);

				odbc_lo_close(conn, lobj_fd);

				/* commit transaction if needed */
				if (!is_in_trans_at_entry)
				{
					if (!CC_commit(conn))
					{
						qb->errormsg = "Could not commit (in-line) a transaction";
						qb->errornumber = STMT_EXEC_ERROR;
						retval = SQL_ERROR;
						goto cleanup;
					}
				}
			}

			/*
			 * the oid of the large object -- just put that in for the
			 * parameter marker -- the data has already been sent to
			 * the large object
			 */
			sprintf(param_string, "%u", lobj_oid);
			lastadd = "::lo";
			CVT_APPEND_STR(qb, param_string);

			break;

			/*
			 * because of no conversion operator for bool and int4,
			 * SQL_BIT
			 */
			/* must be quoted (0 or 1 is ok to use inside the quotes) */

		case SQL_REAL:
			if (buf)
				my_strcpy(param_string, sizeof(param_string), buf, used);
			set_server_decimal_point(param_string);
			lastadd = "::float4";
			CVT_APPEND_STR(qb, param_string);
			break;
		case SQL_FLOAT:
		case SQL_DOUBLE:
			if (buf)
				my_strcpy(param_string, sizeof(param_string), buf, used);
			set_server_decimal_point(param_string);
			lastadd = "::float8";
			CVT_APPEND_STR(qb, param_string);
			break;
		case SQL_NUMERIC:
			if (buf)
			{
				my_strcpy(cbuf, sizeof(cbuf), buf, used);
			}
			else
				sprintf(cbuf, "%s", param_string);
			CVT_APPEND_STR(qb, cbuf);
			break;
		default:			/* a numeric type or SQL_BIT */

			if (buf)
			{
				switch (used)
				{
					case SQL_NULL_DATA:
						break;
					case SQL_NTS:
						CVT_APPEND_STR(qb, buf);
						break;
					default:
						CVT_APPEND_DATA(qb, buf, used);
				}
			}
			else
				CVT_APPEND_STR(qb, param_string);

			break;
	}
	if (add_quote)
	{
		CVT_APPEND_CHAR(qb, LITERAL_QUOTE);
		if (lastadd)
			CVT_APPEND_STR(qb, lastadd);
	}
	if (req_bind)
	{
		UInt4	slen = htonl((UInt4) (qb->npos - npos - 4));

		memcpy(qb->query_statement + npos, &slen, sizeof(slen));
	}
	retval = SQL_SUCCESS;
cleanup:
	if (allocbuf)
		free(allocbuf);
	return retval;
}


static const char *
mapFunction(const char *func, int param_count)
{
	int			i;

	for (i = 0; mapFuncs[i][0]; i++)
	{
		if (mapFuncs[i][0][0] == '%')
		{
			if (mapFuncs[i][0][1] - '0' == param_count &&
			    !stricmp(mapFuncs[i][0] + 2, func))
				return mapFuncs[i][1];
		}
		else if (!stricmp(mapFuncs[i][0], func))
			return mapFuncs[i][1];
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
	CSTR func = "processParameters";
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
		if (ENCODE_STATUS(qp->encstr) != 0)
			continue;
		if (qp->in_identifier || qp->in_literal || qp->in_escape)
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
		mylog("%s closing ) not found %d\n", func, innerParenthesis);
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
			if (!isspace(qb->query_statement[i]))
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
	CSTR func = "convert_escape";
	ConnectionClass *conn = qb->conn;
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
	/**
	if (qp->statement_type == STMT_TYPE_PROCCALL)
	{
		int	lit_call_len = 4;

		// '?=' to accept return values exists ?
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
		if (strnicmp(F_OldPtr(qp), "call", lit_call_len) ||
			!isspace((UCHAR) F_OldPtr(qp)[lit_call_len]))
		{
			F_OldPrior(qp);
			return SQL_SUCCESS;
		}
		qp->opos += lit_call_len;
		if (qb->num_io_params > 1 ||
		    (0 == qb->proc_return &&
		     PG_VERSION_GE(conn, 7.3)))
			CVT_APPEND_STR(qb, "SELECT * FROM");
		else
			CVT_APPEND_STR(qb, "SELECT");
		if (my_strchr(conn, F_OldPtr(qp), '('))
			qp->proc_no_param = FALSE;
		return SQL_SUCCESS;
	}
	**/

	sscanf(F_OldPtr(qp), "%32s", key);
	while ((ucv = F_OldChar(qp)) != '\0' && (!isspace(ucv)))
		F_OldNext(qp);
	while ((ucv = F_OldChar(qp)) != '\0' && isspace(ucv))
		F_OldNext(qp);
    
	/* Avoid the concatenation of the function name with the previous word. Aceto */

	
	if (stricmp(key, "call") == 0)
	{
		Int4 funclen;
		const UCHAR *nextdel;

		if (SQL_ERROR == QB_start_brace(qb))
		{
			retval = SQL_ERROR;
			goto cleanup;
		}
		if (qb->num_io_params > 1 ||
		    (0 == qb->proc_return &&
		     PG_VERSION_GE(conn, 7.3)))
			CVT_APPEND_STR(qb, "SELECT * FROM ");
		else
			CVT_APPEND_STR(qb, "SELECT ");
		funclen = findIdentifier(F_OldPtr(qp), qb->ccsc, &nextdel);
		if (nextdel && ODBC_ESCAPE_END == *nextdel)
		{
			CVT_APPEND_DATA(qb, F_OldPtr(qp), funclen);
			CVT_APPEND_STR(qb, "()");
			if (SQL_ERROR == QB_end_brace(qb))
			{
				retval = SQL_ERROR;
				goto cleanup;
			}
			/* positioned at } */
			qp->opos += (nextdel - F_OldPtr(qp));
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
		if (PG_VERSION_LT(conn, 7.3))
			prtlen = snprintf(buf, sizeof(buf), "%s", buf_small);
		else
			prtlen = snprintf(buf, sizeof(buf), "%s::date", buf_small);
		CVT_APPEND_DATA(qb, buf, prtlen);
		retval = QB_append_space_to_separate_identifiers(qb, qp);
	}
	else if (stricmp(key, "t") == 0)
	{
		/* Literal; return the escape part adding type cast */
		F_ExtractOldTo(qp, buf_small, ODBC_ESCAPE_END, sizeof(buf_small));
		prtlen = snprintf(buf, sizeof(buf), "%s::time", buf_small);
		CVT_APPEND_DATA(qb, buf, prtlen);
		retval = QB_append_space_to_separate_identifiers(qb, qp);
	}
	else if (stricmp(key, "ts") == 0)
	{
		/* Literal; return the escape part adding type cast */
		F_ExtractOldTo(qp, buf_small, ODBC_ESCAPE_END, sizeof(buf_small));
		if (PG_VERSION_LT(conn, 7.1))
			prtlen = snprintf(buf, sizeof(buf), "%s::datetime", buf_small);
		else
			prtlen = snprintf(buf, sizeof(buf), "%s::timestamp", buf_small);
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
		prtlen = snprintf(buf, sizeof(buf), "%s %s", key, buf_small);
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
			   (!isspace(ucv)))
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
			mapExpr = mapFunction(key, param_count);
		if (cvt_func)
		{
			if (2 == param_count)
			{
				BOOL add_cast = FALSE, add_quote = FALSE;
				const UCHAR *pptr;

				from = param_pos[0][0];
				to = param_pos[0][1];
				for (pptr = nqb.query_statement + from; *pptr && isspace(*pptr); pptr++)
					;
				if (LITERAL_QUOTE == *pptr)
					;
				else if ('-' == *pptr)
					add_quote = TRUE;
				else if (isdigit(*pptr))
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

					CVT_APPEND_CHAR(qb, ')');
					from = param_pos[1][0];
					to = param_pos[1][1];
					if (to < from + 9)
					{
						char	num[10];
						memcpy(num, nqb.query_statement + from, to - from + 1);
						num[to - from + 1] = '\0';
mylog("%d-%d num=%s SQL_BIT=%d\n", to, from, num, SQL_BIT);
						switch (atoi(num))
						{
							case SQL_BIT:
								cast_form = "boolean";
								break;
							case SQL_INTEGER:
								cast_form = "int4";
								break;
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
			const UCHAR *mapptr;
			SQLLEN	paramlen;
			int	pidx;

			for (prtlen = 0, mapptr = mapExpr; *mapptr; mapptr++)
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
				else if (isdigit(*mapptr))
				{
					pidx = *mapptr - '0' - 1;
					if (pidx < 0 ||
					    param_pos[pidx][0] < 0)
					{
						qb->errornumber = STMT_EXEC_ERROR;
						qb->errormsg = "param not found";
						qlog("%s %dth param not found for the expression %s\n", pidx + 1, mapExpr);
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
					qlog("%s internal expression error %s\n", func, mapExpr);
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

BOOL
convert_money(const char *s, char *sout, size_t soutmax)
{
	size_t		i = 0,
				out = 0;

	for (i = 0; s[i]; i++)
	{
		if (s[i] == '$' || s[i] == ',' || s[i] == ')')
			;					/* skip these characters */
		else
		{
			if (out + 1 >= soutmax)
				return FALSE;	/* sout is too short */
			if (s[i] == '(')
				sout[out++] = '-';
			else
				sout[out++] = s[i];
		}
	}
	sout[out] = '\0';
	return TRUE;
}


/*
 *	This function parses a character string for date/time info and fills in SIMPLE_TIME
 *	It does not zero out SIMPLE_TIME in case it is desired to initialize it with a value
 */
char
parse_datetime(const char *buf, SIMPLE_TIME *st)
{
	int			y,
				m,
				d,
				hh,
				mm,
				ss;
	int			nf;

	y = m = d = hh = mm = ss = 0;
	st->fr = 0;
	st->infinity = 0;

	/* escape sequence ? */
	if (buf[0] == ODBC_ESCAPE_START)
	{
		while (*(++buf) && *buf != LITERAL_QUOTE);
		if (!(*buf))
			return FALSE;
		buf++;
	}
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
size_t
convert_special_chars(const char *si, char *dst, SQLLEN used, UInt4 flags, int ccsc, int escape_in_literal)
{
	size_t		i = 0,
				out = 0,
				max;
	char	   *p = NULL, literal_quote = LITERAL_QUOTE, tchar;
	encoded_str	encstr;
	BOOL	convlf = (0 != (flags & FLGB_CONVERT_LF)),
		double_special = (0 == (flags & FLGB_BUILDING_BIND_REQUEST));

	if (used == SQL_NTS)
		max = strlen(si);
	else
		max = used;
	if (dst)
	{
		p = dst;
		p[0] = '\0';
	}
	encoded_str_constr(&encstr, ccsc, si);

	for (i = 0; i < max && si[i]; i++)
	{
		tchar = encoded_nextchar(&encstr);
		if (ENCODE_STATUS(encstr) != 0)
		{
			if (p)
				p[out] = tchar;
			out++;
			continue;
		}
		if (convlf &&	/* CR/LF -> LF */
		    PG_CARRIAGE_RETURN == tchar &&
		    PG_LINEFEED == si[i + 1])
			continue;
		else if (double_special && /* double special chars ? */
			 (tchar == literal_quote ||
			  tchar == escape_in_literal))
		{
			if (p)
				p[out++] = tchar;
			else
				out++;
		}
		if (p)
			p[out++] = tchar;
		else
			out++;
	}
	if (p)
		p[out] = '\0';
	return out;
}

#ifdef NOT_USED
#define	CVT_CRLF_TO_LF			1L
#define	DOUBLE_LITERAL_QUOTE		(1L << 1)
#define	DOUBLE_ESCAPE_IN_LITERAL	(1L << 2)
static int
convert_text_field(const char *si, char *dst, int used, int ccsc, int escape_in_literal, UInt4 *flags)
{
	size_t		i = 0, out = 0, max;
	UInt4	iflags = *flags;
	char	*p = NULL, literal_quote = LITERAL_QUOTE, tchar;
	encoded_str	encstr;
	BOOL	convlf = (0 != (iflags & CVT_CRLF_TO_LF)),
		double_literal_quote = (0 != (iflags & DOUBLE_LITERAL_QUOTE)),
		double_escape_in_literal = (0 != (iflags & DOUBLE_ESCAPE_IN_LITERAL));

	if (SQL_NTS == used)
		max = strlen(si);
	else
		max = used;
	if (0 == iflags)
	{
		if (dst)
			strncpy_null(dst, si, max + 1);
		else
			return max;
	}
	if (dst)
	{
		p = dst;
		p[0] = '\0';
	}
	encoded_str_constr(&encstr, ccsc, si);

	*flags = 0;
	for (i = 0; i < max && si[i]; i++)
	{
		tchar = encoded_nextchar(&encstr);
		if (ENCODE_STATUS(encstr) != 0)
		{
			if (p)
				p[out] = tchar;
			out++;
			continue;
		}
		if (convlf &&	/* CR/LF -> LF */
		    PG_CARRIAGE_RETURN == tchar &&
		    PG_LINEFEED == si[i + 1])
		{
			*flags |= CVT_CRLF_TO_LF;
			continue;
		}
		else if (double_literal_quote && /* double literal quote ? */
			 tchar == literal_quote)
		{
			if (p)
				p[out] = tchar;
			out++;
			*flags |= DOUBLE_LITERAL_QUOTE;
		}
		else if (double_escape_in_literal && /* double escape ? */
			 tchar == escape_in_literal)
		{
			if (p)
				p[out] = tchar;
			out++;
			*flags |= DOUBLE_ESCAPE_IN_LITERAL;
		}
		if (p)
			p[out] = tchar;
		out++;
	}
	if (p)
		p[out] = '\0';
	return out;
}
#endif /* NOT_USED */


/*	!!! Need to implement this function !!!  */
int
convert_pgbinary_to_char(const char *value, char *rgbValue, ssize_t cbValueMax)
{
	mylog("convert_pgbinary_to_char: value = '%s'\n", value);

	strncpy_null(rgbValue, value, cbValueMax);
	return 0;
}


static int
conv_from_octal(const UCHAR *s)
{
	ssize_t			i;
	int			y = 0;

	for (i = 1; i <= 3; i++)
		y += (s[i] - '0') << (3 * (3 - i));

	return y;
}


/*	convert octal escapes to bytes */
size_t
convert_from_pgbinary(const UCHAR *value, UCHAR *rgbValue, SQLLEN cbValueMax)
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
			mylog("convert_from_pgbinary: i=%d, rgbValue[%d] = %d, %c\n", i, o, rgbValue[o], rgbValue[o]); ***/
	}

	if (rgbValue)
		rgbValue[o] = '\0';		/* extra protection */

	mylog("convert_from_pgbinary: in=%d, out = %d\n", ilen, o);

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
convert_to_pgbinary(const UCHAR *in, char *out, size_t len, QueryBuild *qb)
{
	CSTR	func = "convert_to_pgbinary";
	UCHAR	inc;
	size_t			i, o = 0;
	char	escape_in_literal = CC_get_escape(qb->conn);
	BOOL	esc_double = (0 == (qb->flags & FLGB_BUILDING_BIND_REQUEST) && 0 != escape_in_literal);

	/* use hex format for 9.0 or later servers */
	if (0 != (qb->flags & FLGB_HEX_BIN_FORMAT))
	{
		if (esc_double)
			out[o++] = escape_in_literal;
		out[o++] = '\\';
		out[o++] = 'x';
		o += pg_bin2hex(in, (UCHAR *) out + o, len);
		return o;
	}
	for (i = 0; i < len; i++)
	{
		inc = in[i];
		inolog("%s: in[%d] = %d, %c\n", func, i, inc, inc);
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

	mylog("%s: returning %d, out='%.*s'\n", func, o, o, out);

	return o;
}


static const char *hextbl = "0123456789ABCDEF";

#define	def_bin2hex(type) \
	(const UCHAR *src, type *dst, SQLLEN length) \
{ \
	const UCHAR	*src_wk; \
	UCHAR		chr; \
	type		*dst_wk; \
	BOOL		backwards; \
	int		i; \
 \
	backwards = FALSE; \
	if ((UCHAR *)dst < src) \
	{ \
		if ((UCHAR *) (dst + 2 * (length - 1)) > src + length - 1) \
			return -1; \
	} \
	else if ((UCHAR *) dst < src + length) \
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
pg_bin2hex def_bin2hex(UCHAR)

SQLLEN
pg_hex2bin(const UCHAR *src, UCHAR *dst, SQLLEN length)
{
	UCHAR		chr;
	const UCHAR	*src_wk;
	UCHAR		*dst_wk;
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
int
convert_lo(StatementClass *stmt, const void *value, SQLSMALLINT fCType, PTR rgbValue,
		   SQLLEN cbValueMax, SQLLEN *pcbValue)
{
	CSTR	func = "convert_lo";
	OID			oid;
	int			retval,
				result;
	SQLLEN	left = -1;
	GetDataClass *gdata = NULL;
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
		gdata = &gdata_info->gdata[stmt->current_col];
		left = gdata->data_left;
	}

	/*
	 * if this is the first call for this column, open the large object
	 * for reading
	 */

	if (!gdata || gdata->data_left == -1)
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
		retval = odbc_lo_lseek(conn, stmt->lobj_fd, 0L, SEEK_END);
		if (retval >= 0)
		{
			left = odbc_lo_tell(conn, stmt->lobj_fd);
			if (gdata)
				gdata->data_left = left;

			/* return to beginning */
			odbc_lo_lseek(conn, stmt->lobj_fd, 0L, SEEK_SET);
		}
	}
	else if (left == 0)
		return COPY_NO_DATA_FOUND;
	mylog("lo data left = %d\n", left);

	if (stmt->lobj_fd < 0)
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "Large object FD undefined for multiple read.", func);
		return COPY_GENERAL_ERROR;
	}

	if (0 >= cbValueMax)
		retval = 0;
	else
		retval = odbc_lo_read(conn, stmt->lobj_fd, (char *) rgbValue, (Int4) (factor > 1 ? (cbValueMax - 1) / factor : cbValueMax));
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
	if (retval < left)
		result = COPY_RESULT_TRUNCATED;
	else
		result = COPY_OK;

	if (pcbValue)
		*pcbValue = left < 0 ? SQL_NO_TOTAL : left * factor;

	if (gdata && gdata->data_left > 0)
		gdata->data_left -= retval;

	if (!gdata || gdata->data_left == 0)
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
