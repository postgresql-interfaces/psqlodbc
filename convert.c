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
 * Comments:	   See "notice.txt" for copyright and license information.
 *-------
 */
/* Multibyte support  Eiji Tokuya	2001-03-15	*/

#include "convert.h"

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
#include "pgapifunc.h"

#if defined(UNICODE_SUPPORT) && defined(WIN32)
#define	WIN_UNICODE_SUPPORT
#endif

#ifdef	__CYGWIN__
#  define TIMEZONE_GLOBAL _timezone
#elif	defined(WIN32) || defined(HAVE_INT_TIMEZONE)
#  ifdef __BORLANDC__
#    define timezone _timzone
#    define daylight _daylight
#    define TIMEZONE_GLOBAL _timezone
#  else
#    define TIMEZONE_GLOBAL timezone
#  endif
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
static unsigned int conv_from_octal(const UCHAR *s);
static unsigned int conv_from_hex(const UCHAR *s);
static char *conv_to_octal(UCHAR val, char *octal);
static int pg_bin2hex(UCHAR *src, UCHAR *dst, int length);

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
#define	ATOI32U	atol
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
#define	ATOI64	_atoi64
#define	ATOI64U	_atoi64
#define	FORMATI64	"%I64d"
#define	FORMATI64U	"%I64u"
#elif	defined(HAVE_STRTOLL)
#define	ATOI64(val)	strtoll(val, NULL, 10)
#define	ATOI64U(val)	strtoull(val, NULL, 10)
#define	FORMATI64	"%lld"
#define	FORMATI64U	"%llu"
#else /* HAVE_STRTOLL */
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
	char		rest[64],
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
	if ((scnt = sscanf(str, "%4d-%2d-%2d %2d:%2d:%2d%32s", &st->y, &st->m, &st->d, &st->hh, &st->mm, &st->ss, rest)) < 6)
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
		default:
			return TRUE;
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
stime2timestamp(const SIMPLE_TIME *st, char *str, BOOL bZone, BOOL precision)
{
	char		precstr[16],
				zonestr[16];
	int			i;

	precstr[0] = '\0';
	if (st->infinity > 0)
	{
		strcpy(str, "Infinity");
		return TRUE;
	}
	else if (st->infinity < 0)
	{
		strcpy(str, "-Infinity");
		return TRUE;
	}
	if (precision && st->fr)
	{
		sprintf(precstr, ".%09d", st->fr);
		for (i = 9; i > 0; i--)
		{
			if (precstr[i] != '0')
				break;
			precstr[i] = '\0';
		}
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
	sprintf(str, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d%s%s", st->y, st->m, st->d, st->hh, st->mm, st->ss, precstr, zonestr);
	return TRUE;
}

/*	This is called by SQLFetch() */
int
copy_and_convert_field_bindinfo(StatementClass *stmt, Int4 field_type, void *value, int col)
{
	ARDFields *opts = SC_get_ARDF(stmt);
	BindInfoClass *bic = &(opts->bindings[col]);
	UInt4	offset = opts->row_offset_ptr ? *opts->row_offset_ptr : 0;

	SC_set_current_col(stmt, -1);
	return copy_and_convert_field(stmt, field_type, value, (Int2) bic->returntype, (PTR) (bic->buffer + offset),
							 (SDWORD) bic->buflen, (SDWORD *) (bic->used + (offset >> 2)));
}


/*	This is called by SQLGetData() */
int
copy_and_convert_field(StatementClass *stmt, Int4 field_type, void *value, Int2 fCType,
					   PTR rgbValue, SDWORD cbValueMax, SDWORD *pcbValue)
{
	CSTR func = "copy_and_convert_field";
	ARDFields	*opts = SC_get_ARDF(stmt);
	GetDataInfo	*gdata = SC_get_GDTI(stmt);
	Int4		len = 0,
				copy_len = 0;
	SIMPLE_TIME std_time;
	time_t		stmt_t = SC_get_time(stmt);
	struct tm  *tim;
#ifdef	HAVE_LOCALTIME_R
	struct tm  tm;
#endif /* HAVE_LOCALTIME_R */
	int			pcbValueOffset,
				rgbValueOffset;
	char	   *rgbValueBindRow, *pcbValueBindRow = NULL;
	const char *ptr;
	int			bind_row = stmt->bind_row;
	int			bind_size = opts->bind_size;
	int			result = COPY_OK;
	ConnectionClass		*conn = SC_get_conn(stmt);
#ifdef HAVE_LOCALE_H
	char saved_locale[256];
#endif /* HAVE_LOCALE_H */
	BOOL		changed, true_is_minus1 = FALSE;
	const char *neut_str = value;
	char		midtemp[2][32];
	int			mtemp_cnt = 0;
	GetDataClass *pgdc;
#ifdef	UNICODE_SUPPORT
	BOOL	wchanged =   FALSE;
#endif /* UNICODE_SUPPORT */
#ifdef	WIN_UNICODE_SUPPORT
	SQLWCHAR	*allocbuf = NULL;
	Int4		wstrlen;	
#endif /* WIN_UNICODE_SUPPORT */

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
		pcbValueOffset = bind_row * sizeof(SDWORD);
		rgbValueOffset = bind_row * cbValueMax;
	}
	/*
	 *	The following is applicable in case bind_size > 0
	 *	or the fCType is of variable length. 
	 */
	rgbValueBindRow = (char *) rgbValue + rgbValueOffset;
	if (pcbValue)
		pcbValueBindRow = (char *) pcbValue + pcbValueOffset;

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
		/*
		 * handle a null just by returning SQL_NULL_DATA in pcbValue, and
		 * doing nothing to the buffer.
		 */
		if (pcbValue)
		{
			*((SDWORD *) pcbValueBindRow) = SQL_NULL_DATA;
			return COPY_OK;
		}
		else
		{
			SC_set_error(stmt, STMT_RETURN_NULL_WITHOUT_INDICATOR, "StrLen_or_IndPtr was a null pointer and NULL data was retrieved");	
			SC_log_error(func, "", stmt);
			return	SQL_ERROR;
		}
	}

	if (stmt->hdbc->DataSourceToDriver != NULL)
	{
		int			length = strlen(value);

		stmt->hdbc->DataSourceToDriver(stmt->hdbc->translation_option,
									   SQL_CHAR,
									   value, length,
									   value, length, NULL,
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
			if (strnicmp(value, "infinity", 8) == 0)
			{
				std_time.infinity = 1;
				std_time.m = 12;
				std_time.d = 31;
				std_time.y = 9999;
				std_time.hh = 23;
				std_time.mm = 59;
				std_time.ss = 59;
			}
			if (strnicmp(value, "-infinity", 9) == 0)
			{
				std_time.infinity = -1;
				std_time.m = 0;
				std_time.d = 0;
				std_time.y = 0;
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
						if (true_is_minus1)
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
			{
				int			nval,
							i;
				const char *vp;

				/* this is an array of eight integers */
				short	   *short_array = (short *) rgbValueBindRow;

				len = INDEX_KEYS_STORAGE_COUNT * 2;
				vp = value;
				nval = 0;
				mylog("index=(");
				for (i = 0; i < INDEX_KEYS_STORAGE_COUNT; i++)
				{
					if (sscanf(vp, "%hd", &short_array[i]) != 1)
						break;

					mylog(" %d", short_array[i]);
					nval++;

					/* skip the current token */
					while ((*vp != '\0') && (!isspace((UCHAR) *vp)))
						vp++;
					/* and skip the space to the next token */
					while ((*vp != '\0') && (isspace((UCHAR) *vp)))
						vp++;
					if (*vp == '\0')
						break;
				}
				mylog(") nval = %d\n", nval);

				for (i = nval; i < INDEX_KEYS_STORAGE_COUNT; i++)
					short_array[i] = 0;

#if 0
				sscanf(value, "%hd %hd %hd %hd %hd %hd %hd %hd",
					   &short_array[0],
					   &short_array[1],
					   &short_array[2],
					   &short_array[3],
					   &short_array[4],
					   &short_array[5],
					   &short_array[6],
					   &short_array[7]);
#endif

				/* There is no corresponding fCType for this. */
				if (pcbValue)
					*((SDWORD *) pcbValueBindRow) = len;

				return COPY_OK; /* dont go any further or the data will be
								 * trashed */
			}

			/*
			 * This is a large object OID, which is used to store
			 * LONGVARBINARY objects.
			 */
		case PG_TYPE_LO_UNDEFINED:

			return convert_lo(stmt, value, fCType, rgbValueBindRow, cbValueMax, (SDWORD *) pcbValueBindRow);

		default:

			if (field_type == stmt->hdbc->lobj_type)	/* hack until permanent
														 * type available */
				return convert_lo(stmt, value, fCType, rgbValueBindRow, cbValueMax, (SDWORD *) pcbValueBindRow);
	}

	/* Change default into something useable */
	if (fCType == SQL_C_DEFAULT)
	{
		fCType = pgtype_to_ctype(stmt, field_type);

		mylog("copy_and_convert, SQL_C_DEFAULT: fCType = %d\n", fCType);
	}

#ifdef	UNICODE_SUPPORT
	if (fCType == SQL_C_CHAR || fCType == SQL_C_WCHAR
#else
	if (fCType == SQL_C_CHAR
#endif /* UNICODE_SUPPORT */
	    || fCType == INTERNAL_ASIS_TYPE)
	{
		/* Special character formatting as required */

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
					sprintf(rgbValueBindRow, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
							std_time.y, std_time.m, std_time.d, std_time.hh, std_time.mm, std_time.ss);
				break;

			case PG_TYPE_BOOL:
				len = strlen(neut_str);
				if (cbValueMax > len)
				{
					strcpy(rgbValueBindRow, neut_str);
					mylog("PG_TYPE_BOOL: rgbValueBindRow = '%s'\n", rgbValueBindRow);
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
				if (stmt->current_col < 0)
				{
					pgdc = &(gdata->fdata);
					pgdc->data_left = -1;
				}
				else
					pgdc = &gdata->gdata[stmt->current_col];
				if (pgdc->data_left < 0)
				{
					BOOL lf_conv = conn->connInfo.lf_conversion;
#ifdef	UNICODE_SUPPORT
					if (fCType == SQL_C_WCHAR)
					{
						len = utf8_to_ucs2_lf(neut_str, -1, lf_conv, NULL, 0);
						len *= WCLEN;
						wchanged = changed = TRUE;
					}
					else
#endif /* UNICODE_SUPPORT */
					if (PG_TYPE_BYTEA == field_type)
					{
						len = convert_from_pgbinary(neut_str, NULL, 0);
						len *= 2;
						changed = TRUE;
					}
					else
#ifdef	WIN_UNICODE_SUPPORT
					if (fCType == SQL_C_CHAR)
					{
						wstrlen = utf8_to_ucs2_lf(neut_str, -1, lf_conv, NULL, 0);
						allocbuf = (SQLWCHAR *) malloc(WCLEN * (wstrlen + 1));
						wstrlen = utf8_to_ucs2_lf(neut_str, -1, lf_conv, allocbuf, wstrlen + 1);
						len = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR) allocbuf, wstrlen, NULL, 0, NULL, NULL);
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
					if (changed || len >= cbValueMax)
					{
						if (len >= (int) pgdc->ttlbuflen)
						{
							pgdc->ttlbuf = realloc(pgdc->ttlbuf, len + 1);
							pgdc->ttlbuflen = len + 1;
						}
#ifdef	UNICODE_SUPPORT
						if (fCType == SQL_C_WCHAR)
						{
							utf8_to_ucs2_lf(neut_str, -1, lf_conv, (SQLWCHAR *) pgdc->ttlbuf, len / WCLEN);
						}
						else
#endif /* UNICODE_SUPPORT */
						if (PG_TYPE_BYTEA == field_type)
						{
							len = convert_from_pgbinary(neut_str, pgdc->ttlbuf, pgdc->ttlbuflen);
							pg_bin2hex(pgdc->ttlbuf, pgdc->ttlbuf, len);
							len *= 2; 
						}
						else
#ifdef	WIN_UNICODE_SUPPORT
						if (fCType == SQL_C_CHAR)
						{
							len = WideCharToMultiByte(CP_ACP, 0, allocbuf, wstrlen, pgdc->ttlbuf, pgdc->ttlbuflen, NULL, NULL);
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

				mylog("DEFAULT: len = %d, ptr = '%s'\n", len, ptr);

				if (stmt->current_col >= 0)
				{
					if (pgdc->data_left > 0)
					{
						ptr += len - pgdc->data_left;
						len = pgdc->data_left;
					}
					else
						pgdc->data_left = len;
				}

				if (cbValueMax > 0)
				{
					BOOL	already_copied = FALSE;

					copy_len = (len >= cbValueMax) ? cbValueMax - 1 : len;
#ifdef	UNICODE_SUPPORT
					if (fCType == SQL_C_WCHAR)
					{
						copy_len /= WCLEN;
						copy_len *= WCLEN;
					}
#endif /* UNICODE_SUPPORT */
#ifdef HAVE_LOCALE_H
					switch (field_type)
					{
						case PG_TYPE_FLOAT4:
						case PG_TYPE_FLOAT8:
						case PG_TYPE_NUMERIC:
						{
							struct lconv	*lc;
							char		*new_string;
							int		i, j;

							new_string = malloc(cbValueMax);
							lc = localeconv();
							for (i = 0, j = 0; ptr[i]; i++)
								if (ptr[i] == '.')
								{
									strncpy(&new_string[j], lc->decimal_point, strlen(lc->decimal_point));
									j += strlen(lc->decimal_point);
								}
								else
									new_string[j++] = ptr[i];
							new_string[j] = '\0';
 							strncpy_null(rgbValueBindRow, new_string, copy_len + 1);
							free(new_string);
							already_copied = TRUE;
 							break;
						}
					}
#endif /* HAVE_LOCALE_H */
					if (!already_copied)
					{
						/* Copy the data */
						memcpy(rgbValueBindRow, ptr, copy_len);
						/* Add null terminator */
#ifdef	UNICODE_SUPPORT
						if (fCType == SQL_C_WCHAR)
							memset(rgbValueBindRow + copy_len, 0, WCLEN);
						else
#endif /* UNICODE_SUPPORT */
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
				if (cbValueMax > 0 && len >= cbValueMax)
					result = COPY_RESULT_TRUNCATED;
				else
				{
					if (pgdc->ttlbuf != NULL)
					{
						free(pgdc->ttlbuf);
						pgdc->ttlbuf = NULL;
					}
				}


				mylog("    SQL_C_CHAR, default: len = %d, cbValueMax = %d, rgbValueBindRow = '%s'\n", len, cbValueMax, rgbValueBindRow);
				break;
		}
#ifdef	UNICODE_SUPPORT
		if (SQL_C_WCHAR == fCType && ! wchanged)
		{
			if (cbValueMax > (SDWORD) (WCLEN * len))
			{
				char *str = strdup(rgbValueBindRow);
				UInt4	ucount = utf8_to_ucs2(str, len, (SQLWCHAR *) rgbValueBindRow, cbValueMax / WCLEN);
				if (cbValueMax < (SDWORD) (WCLEN * ucount))
					result = COPY_RESULT_TRUNCATED;
				len = ucount * WCLEN;
				free(str); 
			}
			else
			{
				len *= WCLEN;
				result = COPY_RESULT_TRUNCATED;
			}
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
#ifdef HAVE_LOCALE_H
				strcpy(saved_locale, setlocale(LC_ALL, NULL));
				setlocale(LC_ALL, "C");
#endif /* HAVE_LOCALE_H */
				len = 4;
				if (bind_size > 0)
					*((SFLOAT *) rgbValueBindRow) = (float) atof(neut_str);
				else
					*((SFLOAT *) rgbValue + bind_row) = (float) atof(neut_str);
#ifdef HAVE_LOCALE_H
				setlocale(LC_ALL, saved_locale);
#endif /* HAVE_LOCALE_H */
				break;

			case SQL_C_DOUBLE:
#ifdef HAVE_LOCALE_H
				strcpy(saved_locale, setlocale(LC_ALL, NULL));
				setlocale(LC_ALL, "C");
#endif /* HAVE_LOCALE_H */
				len = 8;
				if (bind_size > 0)
					*((SDOUBLE *) rgbValueBindRow) = atof(neut_str);
				else
					*((SDOUBLE *) rgbValue + bind_row) = atof(neut_str);
#ifdef HAVE_LOCALE_H
				setlocale(LC_ALL, saved_locale);
#endif /* HAVE_LOCALE_H */
				break;

#if (ODBCVER >= 0x0300)
                        case SQL_C_NUMERIC:
#ifdef HAVE_LOCALE_H
			/* strcpy(saved_locale, setlocale(LC_ALL, NULL));
			setlocale(LC_ALL, "C"); not needed currently */ 
#endif /* HAVE_LOCALE_H */
			{
			SQL_NUMERIC_STRUCT      *ns;
			int	i, nlen, bit, hval, tv, dig, sta, olen;
			char	calv[SQL_MAX_NUMERIC_LEN * 3];
			const char *wv;
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
					else
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
#ifdef HAVE_LOCALE_H
			/* setlocale(LC_ALL, saved_locale); */
#endif /* HAVE_LOCALE_H */
			break;
#endif /* ODBCVER */

			case SQL_C_SSHORT:
			case SQL_C_SHORT:
				len = 2;
				if (bind_size > 0)
					*((SWORD *) rgbValueBindRow) = atoi(neut_str);
				else
					*((SWORD *) rgbValue + bind_row) = atoi(neut_str);
				break;

			case SQL_C_USHORT:
				len = 2;
				if (bind_size > 0)
					*((UWORD *) rgbValueBindRow) = atoi(neut_str);
				else
					*((UWORD *) rgbValue + bind_row) = atoi(neut_str);
				break;

			case SQL_C_SLONG:
			case SQL_C_LONG:
				len = 4;
				if (bind_size > 0)
					*((SDWORD *) rgbValueBindRow) = atol(neut_str);
				else
					*((SDWORD *) rgbValue + bind_row) = atol(neut_str);
				break;

			case SQL_C_ULONG:
				len = 4;
				if (bind_size > 0)
					*((UDWORD *) rgbValueBindRow) = ATOI32U(neut_str);
				else
					*((UDWORD *) rgbValue + bind_row) = ATOI32U(neut_str);
				break;

#if (ODBCVER >= 0x0300) && defined(ODBCINT64)
			case SQL_C_SBIGINT:
			case SQL_BIGINT: /* Is this needed ? */
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
				    PG_TYPE_BPCHAR == field_type)
				{
					int	len = SQL_NULL_DATA;

					if (neut_str)
						len = strlen(neut_str);
					if (pcbValue)
						*((SDWORD *) pcbValueBindRow) = len;
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
						*((SDWORD *) pcbValueBindRow) = sizeof(ival);
					if (cbValueMax >= sizeof(ival))
					{
						memcpy(rgbValueBindRow, &ival, sizeof(ival));
						return COPY_OK;
					}
					else	
						return COPY_RESULT_TRUNCATED;
				}
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

			default:
				qlog("conversion to the type %d isn't supported\n", fCType);
				return COPY_UNSUPPORTED_TYPE;
		}
	}

	/* store the length of what was copied, if there's a place for it */
	if (pcbValue)
		*((SDWORD *) pcbValueBindRow) = len;

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
#define	FLGP_CURSOR_CHECK_OK	(1L << 1)
#define	FLGP_SELECT_INTO		(1L << 2)
#define	FLGP_SELECT_FOR_UPDATE	(1L << 3)
#define	FLGP_BUILDING_PREPARE_STATEMENT	(1L << 4)
typedef struct _QueryParse {
	const char	*statement;
	int		statement_type;
	UInt4		opos;
	int		from_pos;
	int		where_pos;
	UInt4		stmt_len;
	BOOL		in_quote, in_dquote, in_escape;
	char		token_save[64];
	int		token_len;
	BOOL		prev_token_end;
	BOOL		proc_no_param;
	unsigned	int declare_pos;
	UInt4		flags;
	encoded_str	encstr;
}	QueryParse;

static int
QP_initialize(QueryParse *q, const StatementClass *stmt)
{
	q->statement = stmt->execute_statement ? stmt->execute_statement : stmt->statement;
	q->statement_type = stmt->statement_type;
	q->opos = 0;
	q->from_pos = -1;
	q->where_pos = -1;
	q->stmt_len = (q->statement) ? strlen(q->statement) : -1;
	q->in_quote = q->in_dquote = q->in_escape = FALSE;
	q->token_save[0] = '\0';
	q->token_len = 0;
	q->prev_token_end = TRUE;
	q->proc_no_param = TRUE;
	q->declare_pos = 0;
	q->flags = 0;
	make_encoded_str(&q->encstr, SC_get_conn(stmt), q->statement);

	return q->stmt_len;
}

#define	FLGB_PRE_EXECUTING	1L
#define	FLGB_INACCURATE_RESULT	(1L << 1)
#define	FLGB_CREATE_KEYSET	(1L << 2)
#define	FLGB_KEYSET_DRIVEN	(1L << 3)
#define	FLGB_BUILDING_PREPARE_STATEMENT	(1L << 4)
typedef struct _QueryBuild {
	char	*query_statement;
	UInt4	str_size_limit;
	UInt4	str_alsize;
	UInt4	npos;
	int	current_row;
	int	param_number;
	APDFields *apdopts;
	IPDFields *ipdopts;
	PutDataInfo *pdata;
	UInt4	load_stmt_len;
	UInt4	flags;
	BOOL	lf_conv;
	int	ccsc;
	int	errornumber;
	const char *errormsg;

	ConnectionClass	*conn; /* mainly needed for LO handling */
	StatementClass	*stmt; /* needed to set error info in ENLARGE_.. */ 
}	QueryBuild;

#define INIT_MIN_ALLOC	4096
static int
QB_initialize(QueryBuild *qb, UInt4 size, StatementClass *stmt, ConnectionClass *conn)
{
	UInt4	newsize = 0;

	qb->flags = 0;
	qb->load_stmt_len = 0;
	qb->stmt = stmt;
	qb->apdopts = NULL;
	qb->ipdopts = NULL;
	qb->pdata = NULL;
	if (conn)
		qb->conn = conn;
	else if (stmt)
	{
		qb->apdopts = SC_get_APDF(stmt);
		qb->ipdopts = SC_get_IPDF(stmt);
		qb->pdata = SC_get_PDTI(stmt);
		qb->conn = SC_get_conn(stmt);
		if (stmt->pre_executing)
			qb->flags |= FLGB_PRE_EXECUTING;
	}
	else
	{
		qb->conn = NULL;
		return -1;
	}
	qb->lf_conv = qb->conn->connInfo.lf_conversion;
	qb->ccsc = qb->conn->ccsc;
		
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
	unsigned int	c = 0; \
	while (qp->statement[qp->opos] != '\0' && qp->statement[qp->opos] != ch) \
	{ \
	    buf[c++] = qp->statement[qp->opos++]; \
		if (c >= maxsize) \
			break; \
	} \
	if (qp->statement[qp->opos] == '\0') \
		return SQL_ERROR; \
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
ResolveOneParam(QueryBuild *qb);
static int
processParameters(QueryParse *qp, QueryBuild *qb,
UInt4 *output_count, Int4 param_pos[][2]);

static int
enlarge_query_statement(QueryBuild *qb, unsigned int newsize)
{
	unsigned int newalsize = INIT_MIN_ALLOC;
	CSTR func = "enlarge_statement";

	if (qb->str_size_limit > 0 && qb->str_size_limit < (int) newsize)
	{
		free(qb->query_statement);
		qb->query_statement = NULL;
		qb->str_alsize = 0;
		if (qb->stmt)
		{
			
			SC_set_error(qb->stmt, STMT_EXEC_ERROR, "Query buffer overflow in copy_statement_with_parameters");
			SC_log_error(func, "", qb->stmt);
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
			SC_set_error(qb->stmt, STMT_EXEC_ERROR, "Query buffer allocate error in copy_statement_with_parameters");
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
			return SQL_ERROR; \
	}

/*----------
 *	Terminate the stmt_with_params string with NULL.
 *----------
 */
#define CVT_TERMINATE(qb) \
do { \
	qb->query_statement[qb->npos] = '\0'; \
} while (0)

/*----------
 *	Append a data.
 *----------
 */
#define CVT_APPEND_DATA(qb, s, len) \
do { \
	unsigned int	newpos = qb->npos + len; \
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
	unsigned int len = strlen(s); \
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
 *	Newly reqeuired size may be overestimated currently.
 *----------
 */
#define CVT_APPEND_BINARY(qb, buf, used) \
do { \
	unsigned int	newlimit = qb->npos + 5 * used; \
	ENLARGE_NEWSTATEMENT(qb, newlimit); \
	qb->npos += convert_to_pgbinary(buf, &qb->query_statement[qb->npos], used); \
} while (0)

/*----------
 *
 *----------
 */
#define CVT_SPECIAL_CHARS(qb, buf, used) \
do { \
	int cnvlen = convert_special_chars(buf, NULL, used, qb->lf_conv, qb->ccsc); \
	unsigned int	newlimit = qb->npos + cnvlen; \
\
	ENLARGE_NEWSTATEMENT(qb, newlimit); \
	convert_special_chars(buf, &qb->query_statement[qb->npos], used, qb->lf_conv, qb->ccsc); \
	qb->npos += cnvlen; \
} while (0)

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
		case '\'':
			return FALSE;
		case '\"':				/* double quoted table name ? */
			do
			{
				do
					while (*(++stmt) != '\"' && *stmt);
				while (*stmt && *(++stmt) == '\"');
				while (*stmt && !isspace((UCHAR) *stmt) && *stmt != '\"')
					stmt++;
			}
			while (*stmt == '\"');
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
static BOOL
table_for_update(const char *stmt, int *endpos)
{
	const char *wstmt = stmt;

	while (isspace((UCHAR) *(++wstmt)));
	if (!*wstmt)
		return FALSE;
	if (strnicmp(wstmt, "update", 6))
		return FALSE;
	wstmt += 6;
	*endpos = wstmt - stmt;
	return !wstmt[0] || isspace((UCHAR) wstmt[0]);
}

/*----------
 *	Check if the statement is
 *	INSERT INTO ... () VALUES ()
 *	This isn't really a strict check but ...
 *----------
 */
static BOOL
insert_without_target(const char *stmt, int *endpos)
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

static	int
Prepare_and_convert(StatementClass *stmt, QueryParse *qp, QueryBuild *qb)
{
	CSTR func = "Prepare_and_convert";
	char	*new_statement, *exe_statement = NULL;
	int	retval;

	if (QB_initialize(qb, qp->stmt_len, stmt, NULL) < 0)
		return SQL_ERROR;
	if (!stmt->prepared) /*  not yet prepared */
	{
		int	i, elen;
		SWORD	marker_count;
		const IPDFields *ipdopts = qb->ipdopts;

		new_statement = qb->query_statement;
		qb->flags = FLGB_BUILDING_PREPARE_STATEMENT;
		sprintf(new_statement, "PREPARE \"_PLAN%0x\"", stmt);
		qb->npos = strlen(new_statement);
		if (SQL_SUCCESS != PGAPI_NumParams(stmt, &marker_count))
		{
			QB_Destructor(qb);
			return SQL_ERROR;
		}
		if (marker_count > 0)
		{
			CVT_APPEND_CHAR(qb, '(');
			for (i = 0; i < marker_count; i++)
			{
				if (i > 0)
					CVT_APPEND_STR(qb, ", ");
				CVT_APPEND_STR(qb, pgtype_to_name(stmt, ipdopts->parameters[i].PGType));
			}
			CVT_APPEND_CHAR(qb, ')');
		}
		CVT_APPEND_STR(qb, " as ");
		for (qp->opos = 0; qp->opos < qp->stmt_len; qp->opos++)
		{
			retval = inner_process_tokens(qp, qb);
			if (SQL_ERROR == retval)
			{
				if (0 == SC_get_errornumber(stmt))
				{
					SC_set_error(stmt, qb->errornumber, qb->errormsg);
				}
				SC_log_error(func, "", stmt);
				QB_Destructor(qb);
				return retval;
			}
		}
		CVT_APPEND_CHAR(qb, ';');
		/* build the execute statement */
		exe_statement = malloc(30 + 2 * marker_count);
		sprintf(exe_statement, "EXECUTE \"_PLAN%0x\"", stmt);
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
		stmt->execute_statement = exe_statement;
		QP_initialize(qp, stmt);
	}
	qb->flags = 0;
	qb->param_number = -1;
	for (qp->opos = 0; qp->opos < qp->stmt_len; qp->opos++)
	{
		retval = inner_process_tokens(qp, qb);
		if (SQL_ERROR == retval)
		{
			if (0 == SC_get_errornumber(stmt))
			{
				SC_set_error(stmt, qb->errornumber, qb->errormsg);
			}
			SC_log_error(func, "", stmt);
			if (exe_statement)
			{
				free(exe_statement);
				stmt->execute_statement = NULL;
			}
			QB_Destructor(qb);
			return retval;
		}
	}
	/* make sure new_statement is always null-terminated */
	CVT_TERMINATE(qb);

	if (exe_statement)
		SC_set_prepare_before_exec(stmt);
	stmt->stmt_with_params = qb->query_statement;
	return SQL_SUCCESS;
}

#define		my_strchr(conn, s1,c1) pg_mbschr(conn->ccsc, s1,c1)

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

	BOOL	begin_first = FALSE, prepare_dummy_cursor = FALSE;
	ConnectionClass *conn = SC_get_conn(stmt);
	ConnInfo   *ci = &(conn->connInfo);
	int		current_row;

	if (!stmt->statement)
	{
		SC_log_error(func, "No statement string", stmt);
		return SQL_ERROR;
	}

	current_row = stmt->exec_current_row < 0 ? 0 : stmt->exec_current_row;
	qp = &query_org;
	QP_initialize(qp, stmt);

#ifdef	DRIVER_CURSOR_IMPLEMENT
	if (stmt->statement_type != STMT_TYPE_SELECT)
	{
		stmt->options.cursor_type = SQL_CURSOR_FORWARD_ONLY;
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
	}
	else if (stmt->options.cursor_type == SQL_CURSOR_FORWARD_ONLY)
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
	else if (stmt->options.scroll_concurrency != SQL_CONCUR_READ_ONLY)
	{
		if (stmt->parse_status == STMT_PARSE_NONE)
			parse_statement(stmt);
		if (stmt->parse_status == STMT_PARSE_FATAL)
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
	}
#else
	stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
	if (stmt->options.cursor_type == SQL_CURSOR_KEYSET_DRIVEN)
		stmt->options.cursor_type = SQL_CURSOR_STATIC;
#endif   /* DRIVER_CURSOR_IMPLEMENT */

	stmt->miscinfo = 0;
	/* If the application hasn't set a cursor name, then generate one */
	if (stmt->cursor_name[0] == '\0')
		sprintf(stmt->cursor_name, "SQL_CUR%p", stmt);
	if (stmt->stmt_with_params)
	{
		free(stmt->stmt_with_params);
		stmt->stmt_with_params = NULL;
	}

	SC_no_fetchcursor(stmt);
	SC_no_pre_executable(stmt);
	if (stmt->statement_type == STMT_TYPE_SELECT)
		SC_set_pre_executable(stmt);
	qb = &query_crt;
	if (stmt->prepared || (buildPrepareStatement && stmt->options.scroll_concurrency == SQL_CONCUR_READ_ONLY))
	{
		return Prepare_and_convert(stmt, qp, qb);
	}

	if (ci->disallow_premature)
		prepare_dummy_cursor = stmt->pre_executing;
	if (prepare_dummy_cursor)
		qp->flags |= FLGP_PREPARE_DUMMY_CURSOR;
	if (QB_initialize(qb, qp->stmt_len, stmt, NULL) < 0)
		return SQL_ERROR;
	new_statement = qb->query_statement;

	/* For selects, prepend a declare cursor to the statement */
	if (stmt->statement_type == STMT_TYPE_SELECT)
	{
		if (prepare_dummy_cursor || ci->drivers.use_declarefetch)
		{
			if (prepare_dummy_cursor)
			{
				if (!CC_is_in_trans(conn) && PG_VERSION_GE(conn, 7.1))
				{
					strcpy(new_statement, "BEGIN;");
					begin_first = TRUE;
				}
			}
			else if (ci->drivers.use_declarefetch)
				SC_set_fetchcursor(stmt);
			sprintf(new_statement, "%sdeclare %s cursor for ",
					new_statement, stmt->cursor_name);
			qb->npos = strlen(new_statement);
			qp->flags |= FLGP_CURSOR_CHECK_OK;
			qp->declare_pos = qb->npos;
		}
		else if (SQL_CONCUR_READ_ONLY != stmt->options.scroll_concurrency)
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
			if (0 == SC_get_errornumber(stmt))
			{
				SC_set_error(stmt, qb->errornumber, qb->errormsg);
			}
			SC_log_error(func, "", stmt);
			QB_Destructor(qb);
			return retval;
		}
	}
	/* make sure new_statement is always null-terminated */
	CVT_TERMINATE(qb);

	new_statement = qb->query_statement;
	stmt->statement_type = qp->statement_type;
	stmt->inaccurate_result = (0 != (qb->flags & FLGB_INACCURATE_RESULT));
	if (0 != (qp->flags & FLGP_SELECT_INTO))
	{
		SC_no_pre_executable(stmt);
		SC_no_fetchcursor(stmt);
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
	}
	if (0 != (qp->flags & FLGP_SELECT_FOR_UPDATE))
	{
		SC_no_fetchcursor(stmt);
		stmt->options.scroll_concurrency = SQL_CONCUR_READ_ONLY;
	}

	if (conn->DriverToDataSource != NULL)
	{
		int			length = strlen(new_statement);

		conn->DriverToDataSource(conn->translation_option,
								 SQL_CHAR,
								 new_statement, length,
								 new_statement, length, NULL,
								 NULL, 0, NULL);
	}

#ifdef	DRIVER_CURSOR_IMPLEMENT
	if (!stmt->load_statement && qp->from_pos >= 0)
	{
		UInt4	npos = qb->load_stmt_len;

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
				CVT_APPEND_STR(qb, " where ctid = '(0,0)';select ctid, oid from ");
				CVT_APPEND_DATA(qb, qp->statement + qp->from_pos + 5, npos - qp->from_pos - 5);
			}
		}
		stmt->load_statement = malloc(npos + 1);
		memcpy(stmt->load_statement, qb->query_statement, npos);
		stmt->load_statement[npos] = '\0';
	}
#endif   /* DRIVER_CURSOR_IMPLEMENT */
	if (prepare_dummy_cursor && SC_is_pre_executable(stmt))
	{
		char		fetchstr[128];

		sprintf(fetchstr, ";fetch backward in %s;close %s;",
				stmt->cursor_name, stmt->cursor_name);
		if (begin_first && CC_is_in_autocommit(conn))
			strcat(fetchstr, "COMMIT;");
		CVT_APPEND_STR(qb, fetchstr);
		stmt->inaccurate_result = TRUE;
	}

	stmt->stmt_with_params = qb->query_statement;
	return SQL_SUCCESS;
}

static int
inner_process_tokens(QueryParse *qp, QueryBuild *qb)
{
	CSTR func = "inner_process_tokens";
	BOOL	lf_conv = qb->lf_conv;

	RETCODE	retval;
	char	   oldchar;

	if (qp->from_pos == (Int4) qp->opos)
	{
		CVT_APPEND_STR(qb, ", CTID, OID ");
	}
	else if (qp->where_pos == (Int4) qp->opos)
	{
		qb->load_stmt_len = qb->npos;
		if (0 != (qb->flags & FLGB_KEYSET_DRIVEN))
		{
			CVT_APPEND_STR(qb, "where ctid = '(0,0)';select CTID, OID from ");
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
	else if (qp->in_quote || qp->in_dquote) /* quote/double quote check */
	{
		if (oldchar == '\\')
			qp->in_escape = TRUE;
		else if (oldchar == '\'' && qp->in_quote)
			qp->in_quote = FALSE;
		else if (oldchar == '\"' && qp->in_dquote)
			qp->in_dquote = FALSE;
		CVT_APPEND_CHAR(qb, oldchar);
		return SQL_SUCCESS;
	}

	/*
	 * From here we are guranteed to be in neither an escape, a quote
	 * nor a double quote.
	 */
	/* Squeeze carriage-return/linefeed pairs to linefeed only */
	else if (lf_conv && oldchar == '\r' && qp->opos + 1 < qp->stmt_len &&
			qp->statement[qp->opos + 1] == '\n')
		return SQL_SUCCESS;

	/*
	 * Handle literals (date, time, timestamp) and ODBC scalar
	 * functions
	 */
	else if (oldchar == '{')
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
		if (isalnum(F_OldPtr(qp)[1]))
			CVT_APPEND_CHAR(qb, ' ');
		return SQL_SUCCESS;
	}
	/* End of an escape sequence */
	else if (oldchar == '}')
	{
		if (qp->statement_type == STMT_TYPE_PROCCALL)
		{
			if (qp->proc_no_param)
				CVT_APPEND_STR(qb, "()");
		}
		else if (!isspace(F_OldPtr(qp)[1]))
			CVT_APPEND_CHAR(qb, ' ');
		return SQL_SUCCESS;
	}

	/*
	 * Can you have parameter markers inside of quotes?  I dont think
	 * so. All the queries I've seen expect the driver to put quotes
	 * if needed.
	 */
	else if (oldchar != '?')
	{
		if (oldchar == '\'')
			qp->in_quote = TRUE;
		else if (oldchar == '\\')
			qp->in_escape = TRUE;
		else if (oldchar == '\"')
			qp->in_dquote = TRUE;
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
						if (0 != (qp->flags & FLGP_CURSOR_CHECK_OK) &&
							into_table_from(&qp->statement[qp->opos - qp->token_len]))
						{
							qp->flags |= FLGP_SELECT_INTO;
							qp->flags &= ~FLGP_CURSOR_CHECK_OK;
							qb->flags &= ~FLGB_KEYSET_DRIVEN;
							qp->statement_type = STMT_TYPE_CREATE;
							memmove(qb->query_statement, qb->query_statement + qp->declare_pos, qb->npos - qp->declare_pos);
							qb->npos -= qp->declare_pos;
						}
					}
					else if (qp->token_len == 3)
					{
						int			endpos;

						if (0 != (qp->flags & FLGP_CURSOR_CHECK_OK) &&
							strnicmp(qp->token_save, "for", 3) == 0 &&
							table_for_update(&qp->statement[qp->opos], &endpos))
						{
							qp->flags |= FLGP_SELECT_FOR_UPDATE;
							qp->flags &= ~FLGP_CURSOR_CHECK_OK;
							if (qp->flags & FLGP_PREPARE_DUMMY_CURSOR)
							{
								qb->npos -= 4;
								qp->opos += endpos;
							}
							else
							{
								memmove(qb->query_statement, qb->query_statement + qp->declare_pos, qb->npos - qp->declare_pos);
								qb->npos -= qp->declare_pos;
							}
						}
					}
					else if (qp->token_len == 2)
					{
						int	endpos;

						if (STMT_TYPE_INSERT == qp->statement_type &&
							strnicmp(qp->token_save, "()", 2) == 0 &&
							insert_without_target(&qp->statement[qp->opos], &endpos))
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
	if (retval = ResolveOneParam(qb), retval < 0)
		return retval;

	return SQL_SUCCESS;
}

#if (ODBCVER >= 0x0300)
static BOOL
ResolveNumericParam(const SQL_NUMERIC_STRUCT *ns, char *chrform)
{
	static const int prec[] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25, 29, 32, 34, 37, 39};
	Int4	i, j, k, ival, vlen, len, newlen;
	UCHAR		calv[40];
	const UCHAR	*val = (const UCHAR *) ns->val;
	BOOL	next_figure;

	if (0 == ns->precision)
	{
		strcpy(chrform, "0");
		return TRUE;
	}
	else if (ns->precision < prec[sizeof(Int4)])
	{
		for (i = 0, ival = 0; i < sizeof(Int4) && prec[i] <= ns->precision; i++)
		{
			ival += (val[i] << (8 * i)); /* ns->val is little endian */
		}
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
				o1val *= -1;
			sprintf(chrform, "%d.%0.*d", o1val, ns->scale, o2val);
		}
		return TRUE;
	}

	for (i = 0; i < SQL_MAX_NUMERIC_LEN && prec[i] <= ns->precision; i++)
		;
	vlen = i;
	len = 0;
	memset(calv, 0, sizeof(calv));
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
	return TRUE;
}
#endif /* ODBCVER */

/*
 *
 */
static int
ResolveOneParam(QueryBuild *qb)
{
	CSTR func = "ResolveOneParam";

	ConnectionClass *conn = qb->conn;
	ConnInfo   *ci = &(conn->connInfo);
	const APDFields *apdopts = qb->apdopts;
	const IPDFields *ipdopts = qb->ipdopts;
	PutDataInfo *pdata = qb->pdata;

	int		param_number;
	char		param_string[128], tmp[256],
			cbuf[PG_NUMERIC_MAX_PRECISION * 2]; /* seems big enough to handle the data in this function */
	Int4		param_pgtype;
	Int2		param_ctype, param_sqltype;
	SIMPLE_TIME	st;
	time_t		t;
	struct tm	*tim;
#ifdef	HAVE_LOCALTIME_R
	struct tm	tm;
#endif /* HAVE_LOCALTIME_R */
	SDWORD		used;
	char		*buffer, *buf, *allocbuf;
	Oid		lobj_oid;
	int		lobj_fd, retval;
	UInt4	offset = apdopts->param_offset_ptr ? *apdopts->param_offset_ptr : 0;
	UInt4	current_row = qb->current_row;
	BOOL	handling_large_object = FALSE;

	/*
	 * Its a '?' parameter alright
	 */
	param_number = ++qb->param_number;

	if (param_number >= apdopts->allocated)
	{
		if (0 != (qb->flags & FLGB_PRE_EXECUTING))
		{
			CVT_APPEND_STR(qb, "NULL");
			qb->flags |= FLGB_INACCURATE_RESULT;
			return SQL_SUCCESS;
		}
		else
		{
			qb->errormsg = "The # of binded parameters < the # of parameter markers";
			qb->errornumber = STMT_COUNT_FIELD_INCORRECT;
			CVT_TERMINATE(qb);	/* just in case */
			return SQL_ERROR;
		}
	}
	if (SQL_PARAM_OUTPUT == ipdopts->parameters[param_number].paramType)
	{
		qb->errormsg = "Output parameter isn't available";
		qb->errornumber = STMT_NOT_IMPLEMENTED_ERROR;
		CVT_TERMINATE(qb);	/* just in case */
		return SQL_ERROR;
	}

	if (0 != (qb->flags & FLGB_BUILDING_PREPARE_STATEMENT))
	{
		char	pnum[16];

		sprintf(pnum, "$%d", param_number + 1);
		CVT_APPEND_STR(qb, pnum); 
		return SQL_SUCCESS;
	} 
	/* Assign correct buffers based on data at exec param or not */
	if (apdopts->parameters[param_number].data_at_exec)
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

		buffer = apdopts->parameters[param_number].buffer + offset;
		if (current_row > 0)
		{
			if (bind_size > 0)
				buffer += (bind_size * current_row);
			else if (ctypelen = ctype_length(apdopts->parameters[param_number].CType), ctypelen > 0)
				buffer += current_row * ctypelen;
			else 
				buffer += current_row * apdopts->parameters[param_number].buflen;
		}
		if (apdopts->parameters[param_number].used)
		{
			UInt4	p_offset = offset;
			if (bind_size > 0)
				p_offset = offset + bind_size * current_row;
			else
				p_offset = offset + sizeof(SDWORD) * current_row;
			used = *(SDWORD *)((char *)apdopts->parameters[param_number].used + p_offset);
		}
		else
			used = SQL_NTS;
	}	

	/* Handle NULL parameter data */
	if (used == SQL_NULL_DATA)
	{
		CVT_APPEND_STR(qb, "NULL");
		return SQL_SUCCESS;
	}
	/* Handle DEFAULT_PARAM parameter data */
	if (used == SQL_DEFAULT_PARAM)
	{
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

	param_ctype = apdopts->parameters[param_number].CType;
	param_sqltype = ipdopts->parameters[param_number].SQLType;
	param_pgtype = ipdopts->parameters[param_number].PGType;

	mylog("%s: from(fcType)=%d, to(fSqlType)=%d\n", func,
				param_ctype, param_sqltype);

	/* replace DEFAULT with something we can use */
	if (param_ctype == SQL_C_DEFAULT)
		param_ctype = sqltype_to_default_ctype(conn, param_sqltype);

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

	/* Convert input C type to a neutral format */
	switch (param_ctype)
	{
		case SQL_C_BINARY:
			buf = buffer;
			break;
		case SQL_C_CHAR:
#ifdef	WIN_UNICODE_SUPPORT
			switch (param_sqltype)
			{
				case SQL_WCHAR:
				case SQL_WVARCHAR:
				case SQL_WLONGVARCHAR:
					if (SQL_NTS == used)
						used = strlen(buffer);
					allocbuf = malloc(WCLEN * (used + 1));
					used = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buffer,
						used, (LPWSTR) allocbuf, used + 1);
					buf = ucs2_to_utf8((SQLWCHAR *) allocbuf, used, &used, FALSE);
					free(allocbuf);
					allocbuf = buf;
					break;
				default:
					buf = buffer;
			}
#else
			buf = buffer;
#endif /* WIN_UNICODE_SUPPORT */
			break;

#ifdef	UNICODE_SUPPORT
		case SQL_C_WCHAR:
			buf = allocbuf = ucs2_to_utf8((SQLWCHAR *) buffer, used / WCLEN, &used, FALSE);
			used *= WCLEN;
			break;
#endif /* UNICODE_SUPPORT */

		case SQL_C_DOUBLE:
			sprintf(param_string, "%.15g",
						*((SDOUBLE *) buffer));
			break;

		case SQL_C_FLOAT:
			sprintf(param_string, "%.6g",
					*((SFLOAT *) buffer));
			break;

		case SQL_C_SLONG:
		case SQL_C_LONG:
			sprintf(param_string, "%ld",
					*((SDWORD *) buffer));
			break;

#if (ODBCVER >= 0x0300) && defined(ODBCINT64)
		case SQL_C_SBIGINT:
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
					*((SWORD *) buffer));
			break;

		case SQL_C_STINYINT:
		case SQL_C_TINYINT:
			sprintf(param_string, "%d",
					*((SCHAR *) buffer));
			break;

		case SQL_C_ULONG:
			sprintf(param_string, "%lu",
					*((UDWORD *) buffer));
			break;

		case SQL_C_USHORT:
			sprintf(param_string, "%u",
					*((UWORD *) buffer));
			break;

		case SQL_C_UTINYINT:
			sprintf(param_string, "%u",
					*((UCHAR *) buffer));
			break;

		case SQL_C_BIT:
			{
				int			i = *((UCHAR *) buffer);

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
#endif
		default:
			/* error */
			qb->errormsg = "Unrecognized C_parameter type in copy_statement_with_parameters";
			qb->errornumber = STMT_NOT_IMPLEMENTED_ERROR;
			CVT_TERMINATE(qb);	/* just in case */
			return SQL_ERROR;
	}

	/*
	 * Now that the input data is in a neutral format, convert it to
	 * the desired output format (sqltype)
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

			CVT_APPEND_CHAR(qb, '\'');	/* Open Quote */

			/* it was a SQL_C_CHAR */
			if (buf)
				CVT_SPECIAL_CHARS(qb, buf, used);

			/* it was a numeric type */
			else if (param_string[0] != '\0')
				CVT_APPEND_STR(qb, param_string);

			/* it was date,time,timestamp -- use m,d,y,hh,mm,ss */
			else
			{
				sprintf(tmp, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
						st.y, st.m, st.d, st.hh, st.mm, st.ss);

				CVT_APPEND_STR(qb, tmp);
			}

			CVT_APPEND_CHAR(qb, '\'');	/* Close Quote */

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

			sprintf(tmp, "'%.4d-%.2d-%.2d'::date", st.y, st.m, st.d);

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

			sprintf(tmp, "'%.2d:%.2d:%.2d'::time", st.hh, st.mm, st.ss);

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
			 * sprintf(tmp, "'%.4d-%.2d-%.2d %.2d:%.2d:%.2d'", st.y,
			 * st.m, st.d, st.hh, st.mm, st.ss);
			 */
			tmp[0] = '\'';
			/* Time zone stuff is unreliable */
			stime2timestamp(&st, tmp + 1, USE_ZONE, PG_VERSION_GE(conn, 7.2));
			strcat(tmp, "'::timestamp");

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
					return SQL_ERROR;
			}
			if (param_pgtype == PG_TYPE_BYTEA)
			{
				/* non-ascii characters should be
				 * converted to octal
				 */
				CVT_APPEND_CHAR(qb, '\'');	/* Open Quote */

				mylog("SQL_VARBINARY: about to call convert_to_pgbinary, used = %d\n", used);

				CVT_APPEND_BINARY(qb, buf, used);

				CVT_APPEND_CHAR(qb, '\'');	/* Close Quote */

				break;
			}
			if (param_pgtype != conn->lobj_type)
			{
				qb->errormsg = "Could not convert binary other than LO type";
				qb->errornumber = STMT_EXEC_ERROR;
				return SQL_ERROR;
			}

			if (apdopts->parameters[param_number].data_at_exec)
				lobj_oid = pdata->pdata[param_number].lobj_oid;
			else
			{
				/* begin transaction if needed */
				if (!CC_is_in_trans(conn))
				{
					if (!CC_begin(conn))
					{
						qb->errormsg = "Could not begin (in-line) a transaction";
						qb->errornumber = STMT_EXEC_ERROR;
						return SQL_ERROR;
					}
				}

				/* store the oid */
				lobj_oid = lo_creat(conn, INV_READ | INV_WRITE);
				if (lobj_oid == 0)
				{
					qb->errornumber = STMT_EXEC_ERROR;
					qb->errormsg = "Couldnt create (in-line) large object.";
					return SQL_ERROR;
				}

				/* store the fd */
				lobj_fd = lo_open(conn, lobj_oid, INV_WRITE);
				if (lobj_fd < 0)
				{
					qb->errornumber = STMT_EXEC_ERROR;
					qb->errormsg = "Couldnt open (in-line) large object for writing.";
					return SQL_ERROR;
				}

				retval = lo_write(conn, lobj_fd, buffer, used);

				lo_close(conn, lobj_fd);

				/* commit transaction if needed */
				if (!ci->drivers.use_declarefetch && CC_is_in_autocommit(conn))
				{
					if (!CC_commit(conn))
					{
						qb->errormsg = "Could not commit (in-line) a transaction";
						qb->errornumber = STMT_EXEC_ERROR;
						return SQL_ERROR;
					}
				}
			}

			/*
			 * the oid of the large object -- just put that in for the
			 * parameter marker -- the data has already been sent to
			 * the large object
			 */
			sprintf(param_string, "'%d'::lo", lobj_oid);
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
			sprintf(tmp, "'%s'::float4", param_string);
			CVT_APPEND_STR(qb, tmp);
			break;
		case SQL_FLOAT:
		case SQL_DOUBLE:
			if (buf)
				my_strcpy(param_string, sizeof(param_string), buf, used);
			sprintf(tmp, "'%s'::float8", param_string);
			CVT_APPEND_STR(qb, tmp);
			break;
		case SQL_NUMERIC:
			if (buf)
			{
				cbuf[0] = '\'';
				my_strcpy(cbuf + 1, sizeof(cbuf) - 3, buf, used);	/* 3 = 1('\'') +
																	* strlen("'")
																	* + 1('\0') */
				strcat(cbuf, "'");
			}
			else
				sprintf(cbuf, "'%s'", param_string);
			CVT_APPEND_STR(qb, cbuf);
			break;
		default:			/* a numeric type or SQL_BIT */
			if (param_sqltype == SQL_BIT)
				CVT_APPEND_CHAR(qb, '\'');		/* Open Quote */

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

			if (param_sqltype == SQL_BIT)
				CVT_APPEND_CHAR(qb, '\'');		/* Close Quote */

			break;
	}
	if (allocbuf)
		free(allocbuf);
	return SQL_SUCCESS;
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
		UInt4 *output_count, Int4 param_pos[][2])
{
	CSTR func = "processParameters";
	int retval, innerParenthesis, param_count;
	BOOL stop;

	/* begin with outer '(' */
	innerParenthesis = 0;
	param_count = 0;
	stop = FALSE;
	for (; F_OldPos(qp) < qp->stmt_len; F_OldNext(qp))
	{
		retval = inner_process_tokens(qp, qb);
		if (retval == SQL_ERROR)
			return retval;
		if (ENCODE_STATUS(qp->encstr) != 0)
			continue;
		if (qp->in_dquote || qp->in_quote || qp->in_escape)
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

			case '}':
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
		int	i;

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
	RETCODE	retval = SQL_SUCCESS;
	char		buf[1024], buf_small[128], key[65];
	UCHAR	ucv;
	UInt4		prtlen;
 
	if (F_OldChar(qp) == '{') /* skip the first { */
		F_OldNext(qp);
	/* Separate off the key, skipping leading and trailing whitespace */
	while ((ucv = F_OldChar(qp)) != '\0' && isspace(ucv))
		F_OldNext(qp);
	/*
	 * procedure calls
	 */
	if (qp->statement_type == STMT_TYPE_PROCCALL)
	{
		int	lit_call_len = 4;
		ConnectionClass *conn = qb->conn;

		/* '?=' to accept return values exists ? */
		if (F_OldChar(qp) == '?')
		{
			qb->param_number++;
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
		CVT_APPEND_STR(qb, "SELECT ");
		if (my_strchr(conn, F_OldPtr(qp), '('))
			qp->proc_no_param = FALSE;
		return SQL_SUCCESS;
	}

	sscanf(F_OldPtr(qp), "%32s", key);
	while ((ucv = F_OldChar(qp)) != '\0' && (!isspace(ucv)))
		F_OldNext(qp);
	while ((ucv = F_OldChar(qp)) != '\0' && isspace(ucv))
		F_OldNext(qp);
    
	/* Avoid the concatenation of the function name with the previous word. Aceto */

	if (F_NewPos(qb) > 0 && isalnum(F_NewPtr(qb)[-1]))
		CVT_APPEND_CHAR(qb, ' ');
	
	if (stricmp(key, "d") == 0)
	{
		/* Literal; return the escape part adding type cast */
		F_ExtractOldTo(qp, buf_small, '}', sizeof(buf_small));
		if (PG_VERSION_LT(qb->conn, 7.3))
			prtlen = snprintf(buf, sizeof(buf), "%s ", buf_small);
		else
			prtlen = snprintf(buf, sizeof(buf), "%s::date ", buf_small);
		CVT_APPEND_DATA(qb, buf, prtlen);
	}
	else if (stricmp(key, "t") == 0)
	{
		/* Literal; return the escape part adding type cast */
		F_ExtractOldTo(qp, buf_small, '}', sizeof(buf_small));
		prtlen = snprintf(buf, sizeof(buf), "%s::time", buf_small);
		CVT_APPEND_DATA(qb, buf, prtlen);
	}
	else if (stricmp(key, "ts") == 0)
	{
		/* Literal; return the escape part adding type cast */
		F_ExtractOldTo(qp, buf_small, '}', sizeof(buf_small));
		if (PG_VERSION_LT(qb->conn, 7.1))
			prtlen = snprintf(buf, sizeof(buf), "%s::datetime", buf_small);
		else
			prtlen = snprintf(buf, sizeof(buf), "%s::timestamp", buf_small);
		CVT_APPEND_DATA(qb, buf, prtlen);
	}
	else if (stricmp(key, "oj") == 0) /* {oj syntax support for 7.1 * servers */
	{
		F_OldPrior(qp);
		return SQL_SUCCESS; /* Continue at inner_process_tokens loop */
	}
	else if (stricmp(key, "fn") == 0)
	{
		QueryBuild	nqb;
		const char *mapExpr;
		int	i, param_count;
		UInt4	param_consumed;
		Int4	param_pos[16][2];

		/* Separate off the func name, skipping leading and trailing whitespace */
		i = 0;
		while ((ucv = F_OldChar(qp)) != '\0' && ucv != '(' &&
			   (!isspace(ucv)))
		{
			if (i < sizeof(key)-1)
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
			return SQL_SUCCESS;
		}

		/*
		 * Process parameter list and inner escape
		 * sequences
		 * Aceto 2002-01-29
		 */

		QB_initialize_copy(&nqb, qb, 1024);
		if (retval = processParameters(qp, &nqb, &param_consumed, param_pos), retval == SQL_ERROR)
		{
			qb->errornumber = nqb.errornumber;
			qb->errormsg = nqb.errormsg;
			QB_Destructor(&nqb);
			return retval;
		}

		for (param_count = 0;; param_count++)
		{
			if (param_pos[param_count][0] < 0)
				break;
		}
		if (param_count == 1 &&
		    param_pos[0][1] < param_pos[0][0])
			param_count = 0;

		mapExpr = mapFunction(key, param_count);
		if (mapExpr == NULL)
		{
			CVT_APPEND_STR(qb, key);
			CVT_APPEND_DATA(qb, nqb.query_statement, nqb.npos);
		}
		else
		{
			const char *mapptr;
			int	from, to, pidx, paramlen;

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
					CVT_APPEND_DATA(qb, nqb.query_statement+ from, paramlen);
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
		QB_Destructor(&nqb);
	}
	else
	{
		/* Bogus key, leave untranslated */
		return SQL_ERROR;
	}
 
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
	if (buf[0] == '{')
	{
		while (*(++buf) && *buf != '\'');
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
int
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
			if (i > 0 && si[i - 1] == '\r')
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
				dst[out++] = '\r';
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
int
convert_special_chars(const char *si, char *dst, int used, BOOL convlf, int ccsc)
{
	size_t		i = 0,
				out = 0,
				max;
	char	   *p = NULL;
	encoded_str	encstr;

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
		encoded_nextchar(&encstr);
		if (ENCODE_STATUS(encstr) != 0)
		{
			if (p)
				p[out] = si[i];
			out++;
			continue;
		}
		if (convlf && si[i] == '\r' && si[i + 1] == '\n')
			continue;
		else if (si[i] == '\'' || si[i] == '\\')
		{
			if (p)
				p[out++] = '\\';
			else
				out++;
		}
		if (p)
			p[out++] = si[i];
		else
			out++;
	}
	if (p)
		p[out] = '\0';
	return out;
}


/*	!!! Need to implement this function !!!  */
int
convert_pgbinary_to_char(const char *value, char *rgbValue, int cbValueMax)
{
	mylog("convert_pgbinary_to_char: value = '%s'\n", value);

	strncpy_null(rgbValue, value, cbValueMax);
	return 0;
}


static unsigned int
conv_from_octal(const UCHAR *s)
{
	int			i,
				y = 0;

	for (i = 1; i <= 3; i++)
		y += (s[i] - '0') << (3 * (3 - i));

	return y;

}


static unsigned int
conv_from_hex(const UCHAR *s)
{
	int			i,
				y = 0,
				val;

	for (i = 1; i <= 2; i++)
	{
		if (s[i] >= 'a' && s[i] <= 'f')
			val = s[i] - 'a' + 10;
		else if (s[i] >= 'A' && s[i] <= 'F')
			val = s[i] - 'A' + 10;
		else
			val = s[i] - '0';

		y += val << (4 * (2 - i));
	}

	return y;
}


/*	convert octal escapes to bytes */
int
convert_from_pgbinary(const UCHAR *value, UCHAR *rgbValue, int cbValueMax)
{
	size_t		i,
				ilen = strlen(value);
	int			o = 0;


	for (i = 0; i < ilen;)
	{
		if (value[i] == '\\')
		{
			if (value[i + 1] == '\\')
			{
				if (rgbValue)
					rgbValue[o] = value[i];
				i += 2;
			}
			else
			{
				if (rgbValue)
					rgbValue[o] = conv_from_octal(&value[i]);
				i += 4;
			}
		}
		else
		{
			if (rgbValue)
				rgbValue[o] = value[i];
			i++;
		}
		/** if (rgbValue)
			mylog("convert_from_pgbinary: i=%d, rgbValue[%d] = %d, %c\n", i, o, rgbValue[o], rgbValue[o]); ***/
		o++;
	}

	if (rgbValue)
		rgbValue[o] = '\0';		/* extra protection */

	mylog("convert_from_pgbinary: in=%d, out = %d\n", ilen, o);

	return o;
}


static char *
conv_to_octal(UCHAR val, char *octal)
{
	int			i;

	octal[0] = '\\';
	octal[1] = '\\';
	octal[5] = '\0';

	for (i = 4; i > 1; i--)
	{
		octal[i] = (val & 7) + '0';
		val >>= 3;
	}

	return octal;
}


/*	convert non-ascii bytes to octal escape sequences */
int
convert_to_pgbinary(const UCHAR *in, char *out, int len)
{
	int			i,
				o = 0;

	for (i = 0; i < len; i++)
	{
		mylog("convert_to_pgbinary: in[%d] = %d, %c\n", i, in[i], in[i]);
		if (isalnum(in[i]) || in[i] == ' ')
			out[o++] = in[i];
		else
		{
			conv_to_octal(in[i], &out[o]);
			o += 5;
		}
	}

	mylog("convert_to_pgbinary: returning %d, out='%.*s'\n", o, o, out);

	return o;
}


void
encode(const char *in, char *out)
{
	unsigned int i,
				ilen = strlen(in),
				o = 0;

	for (i = 0; i < ilen; i++)
	{
		if (in[i] == '+')
		{
			sprintf(&out[o], "%%2B");
			o += 3;
		}
		else if (isspace((UCHAR) in[i]))
			out[o++] = '+';
		else if (!isalnum((UCHAR) in[i]))
		{
			sprintf(&out[o], "%%%02x", (UCHAR) in[i]);
			o += 3;
		}
		else
			out[o++] = in[i];
	}
	out[o++] = '\0';
}


void
decode(const char *in, char *out)
{
	unsigned int i,
				ilen = strlen(in),
				o = 0;

	for (i = 0; i < ilen; i++)
	{
		if (in[i] == '+')
			out[o++] = ' ';
		else if (in[i] == '%')
		{
			sprintf(&out[o++], "%c", conv_from_hex(&in[i]));
			i += 2;
		}
		else
			out[o++] = in[i];
	}
	out[o++] = '\0';
}

static const char *hextbl = "0123456789ABCDEF";
static int
pg_bin2hex(UCHAR *src, UCHAR *dst, int length)
{
	UCHAR		chr,
			   *src_wk,
			   *dst_wk;
	BOOL		backwards;
	int			i;

	backwards = FALSE;
	if (dst < src)
	{
		if (dst + length > src + 1)
			return -1;
	}
	else if (dst < src + length)
		backwards = TRUE;
	if (backwards)
	{
		for (i = 0, src_wk = src + length - 1, dst_wk = dst + 2 * length - 1; i < length; i++, src_wk--)
		{
			chr = *src_wk;
			*dst_wk-- = hextbl[chr % 16];
			*dst_wk-- = hextbl[chr >> 4];
		}
	}
	else
	{
		for (i = 0, src_wk = src, dst_wk = dst; i < length; i++, src_wk++)
		{
			chr = *src_wk;
			*dst_wk++ = hextbl[chr >> 4];
			*dst_wk++ = hextbl[chr % 16];
		}
	}
	dst[2 * length] = '\0';
	return length;
}

int
pg_hex2bin(const UCHAR *src, UCHAR *dst, int length)
{
	UCHAR		chr;
	const UCHAR	*src_wk;
	UCHAR		*dst_wk;
	int		i, val;
	BOOL		HByte = TRUE;

	for (i = 0, src_wk = src, dst_wk = dst; i < length; i++, src_wk++)
	{
		chr = *src_wk;
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
convert_lo(StatementClass *stmt, const void *value, Int2 fCType, PTR rgbValue,
		   SDWORD cbValueMax, SDWORD *pcbValue)
{
	Oid			oid;
	int			retval,
				result,
				left = -1;
	GetDataClass *gdata = NULL;
	ConnectionClass *conn = SC_get_conn(stmt);
	ConnInfo   *ci = &(conn->connInfo);
	GetDataInfo	*gdata_info = SC_get_GDTI(stmt);
	int			factor;

	switch (fCType)
	{
		case SQL_C_CHAR:
			factor = 2;
			break;
		case SQL_C_BINARY:
			factor = 1;
			break;
		default:
			SC_set_error(stmt, STMT_EXEC_ERROR, "Could not convert lo to the c-type");
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
				SC_set_error(stmt, STMT_EXEC_ERROR, "Could not begin (in-line) a transaction");
				return COPY_GENERAL_ERROR;
			}
		}

		oid = ATOI32U(value);
		stmt->lobj_fd = lo_open(conn, oid, INV_READ);
		if (stmt->lobj_fd < 0)
		{
			SC_set_error(stmt, STMT_EXEC_ERROR, "Couldnt open large object for reading.");
			return COPY_GENERAL_ERROR;
		}

		/* Get the size */
		retval = lo_lseek(conn, stmt->lobj_fd, 0L, SEEK_END);
		if (retval >= 0)
		{
			left = lo_tell(conn, stmt->lobj_fd);
			if (gdata)
				gdata->data_left = left;

			/* return to beginning */
			lo_lseek(conn, stmt->lobj_fd, 0L, SEEK_SET);
		}
	}
	mylog("lo data left = %d\n", left);

	if (left == 0)
		return COPY_NO_DATA_FOUND;

	if (stmt->lobj_fd < 0)
	{
		SC_set_error(stmt, STMT_EXEC_ERROR, "Large object FD undefined for multiple read.");
		return COPY_GENERAL_ERROR;
	}

	retval = lo_read(conn, stmt->lobj_fd, (char *) rgbValue, factor > 1 ? (cbValueMax - 1) / factor : cbValueMax);
	if (retval < 0)
	{
		lo_close(conn, stmt->lobj_fd);

		/* commit transaction if needed */
		if (!ci->drivers.use_declarefetch && CC_is_in_autocommit(conn))
		{
			if (!CC_commit(conn))
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Could not commit (in-line) a transaction");
				return COPY_GENERAL_ERROR;
			}
		}

		stmt->lobj_fd = -1;

		SC_set_error(stmt, STMT_EXEC_ERROR, "Error reading from large object.");
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
		lo_close(conn, stmt->lobj_fd);

		/* commit transaction if needed */
		if (!ci->drivers.use_declarefetch && CC_is_in_autocommit(conn))
		{
			if (!CC_commit(conn))
			{
				SC_set_error(stmt, STMT_EXEC_ERROR, "Could not commit (in-line) a transaction");
				return COPY_GENERAL_ERROR;
			}
		}

		stmt->lobj_fd = -1;		/* prevent further reading */
	}

	return result;
}
