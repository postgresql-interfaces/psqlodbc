/*-------
 * Module:			misc.c
 *
 * Description:		This module contains miscellaneous routines
 *					such as for debugging/logging and string functions.
 *
 * Classes:			n/a
 *
 * API functions:	none
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */

#include "psqlodbc.h"
#include "misc.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifndef WIN32
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <process.h>			/* Byron: is this where Windows keeps def.
								 * of getpid ? */
#endif

/*
 *	returns STRCPY_FAIL, STRCPY_TRUNCATED, or #bytes copied
 *	(not including null term)
 */
ssize_t
my_strcpy(char *dst, ssize_t dst_len, const char *src, ssize_t src_len)
{
	if (dst_len <= 0)
		return STRCPY_FAIL;

	if (src_len == SQL_NULL_DATA)
	{
		dst[0] = '\0';
		return STRCPY_NULL;
	}
	else if (src_len == SQL_NTS)
		src_len = strlen(src);

	if (src_len <= 0)
		return STRCPY_FAIL;
	else
	{
		if (src_len < dst_len)
		{
			memcpy(dst, src, src_len);
			dst[src_len] = '\0';
		}
		else
		{
			memcpy(dst, src, dst_len - 1);
			dst[dst_len - 1] = '\0';	/* truncated */
			return STRCPY_TRUNCATED;
		}
	}

	return strlen(dst);
}


/*
 * strncpy copies up to len characters, and doesn't terminate
 * the destination string if src has len characters or more.
 * instead, I want it to copy up to len-1 characters and always
 * terminate the destination string.
 */
size_t
strncpy_null(char *dst, const char *src, ssize_t len)
{
	int			i;

	if (NULL != dst && len > 0)
	{
		for (i = 0; src[i] && i < len - 1; i++)
			dst[i] = src[i];

		dst[i] = '\0';
	}
	else
		return 0;
	if (src[i])
		return strlen(src);
	return i;
}


/*------
 *	Create a null terminated string (handling the SQL_NTS thing):
 *		1. If buf is supplied, place the string in there
 *		   (assumes enough space) and return buf.
 *		2. If buf is not supplied, malloc space and return this string
 *------
 */
char *
make_string(const SQLCHAR *s, SQLINTEGER len, char *buf, size_t bufsize)
{
	size_t		length;
	char	   *str;

	if (!s || SQL_NULL_DATA == len)
		return NULL;
	if (len >= 0)
		length =len;
	else if (SQL_NTS == len)
		length = strlen((char *) s);
	else
	{
		MYLOG(0, "invalid length=" FORMAT_INTEGER "\n", len);
		return NULL;
	}
	/*
	 * A counted string need not be null terminated, so copy exactly
	 * length bytes and terminate the copy ourselves instead of using
	 * strncpy_null(), which would read s[length] and beyond.
	 */
	if (buf)
	{
		if (bufsize > 0)
		{
			if (length >= bufsize)
				length = bufsize - 1;	/* truncated */
			memcpy(buf, s, length);
			buf[length] = '\0';
		}
		return buf;
	}

MYLOG(DETAIL_LOG_LEVEL, "malloc size=" FORMAT_SIZE_T "\n", length);
	str = malloc(length + 1);
MYLOG(DETAIL_LOG_LEVEL, "str=%p\n", str);
	if (!str)
		return NULL;

	memcpy(str, s, length);
	str[length] = '\0';
	return str;
}

char *
my_trim(char *s)
{
	char *last;

	for (last = s + strlen(s) - 1; last >= s; last--)
	{
		if (*last == ' ')
			*last = '\0';
		else
			break;
	}

	return s;
}

/*
 * snprintfcat is a extension to snprintf
 * It add format to buf at given pos
 */
#ifdef POSIX_SNPRINTF_REQUIRED
static posix_vsnprintf(char *str, size_t size, const char *format, va_list ap);
#define vsnprintf posix_vsnprintf
#endif /* POSIX_SNPRINTF_REQUIRED */

int
snprintfcat(char *buf, size_t size, const char *format, ...)
{
	int len;
	size_t pos = strlen(buf);
	va_list arglist;

	va_start(arglist, format);
	len = vsnprintf(buf + pos, size - pos, format, arglist);
	va_end(arglist);
	return len + pos;
}

/*
 * snprintf_len is a extension to snprintf
 * It returns strlen of buf every time (not -1 when truncated)
 */

size_t
snprintf_len(char *buf, size_t size, const char *format, ...)
{
	ssize_t len;
	va_list arglist;

	va_start(arglist, format);
	if ((len = vsnprintf(buf, size, format, arglist)) < 0)
		len = size;
	va_end(arglist);
	return len;
}

/*
 * Windows doesn't have snprintf(). It has _snprintf() which is similar,
 * but it behaves differently wrt. truncation. This is a compatibility
 * function that uses _snprintf() to provide POSIX snprintf() behavior.
 *
 * Our strategy, if the output doesn't fit, is to create a temporary buffer
 * and call _snprintf() on that. If it still doesn't fit, enlarge the buffer
 * and repeat.
 */
#ifdef POSIX_SNPRINTF_REQUIRED
static int
posix_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	int			len;
	char	   *tmp;
	size_t		newsize;

	len = _vsnprintf(str, size, format, ap);
	if (len < 0)
	{
		if (size == 0)
			newsize = 100;
		else
			newsize = size;
		do
		{
			newsize *= 2;
			tmp = malloc(newsize);
			if (!tmp)
				return -1;
			len = _vsnprintf(tmp, newsize, format, ap);
			if (len >= 0)
				memcpy(str, tmp, size);
			free(tmp);
		} while (len < 0);
	}
	if (len >= size && size > 0)
	{
		/* Ensure the buffer is NULL-terminated */
		str[size - 1] = '\0';
	}
	return len;
}

int
posix_snprintf(char *buf, size_t size, const char *format, ...)
{
	int		len;
	va_list arglist;

	va_start(arglist, format);
	len = posix_vsnprintf(buf, size, format, arglist);
	va_end(arglist);
	return len;
}
#endif /* POSIX_SNPRINTF_REQUIRED */

#ifndef	HAVE_STRLCAT
size_t
strlcat(char *dst, const char *src, size_t size)
{
	size_t ttllen;
	char	*pd = dst;
	const char *ps= src;

	for (ttllen = 0; ttllen < size; ttllen++, pd++)
	{
		if (0 == *pd)
			break;
	}
	if (ttllen >= size - 1)
		return ttllen + strlen(src);
	for (; ttllen < size - 1; ttllen++, pd++, ps++)
	{
		if (0 == (*pd = *ps))
			return ttllen;
	}
	*pd = 0;
	for (; *ps; ttllen++, ps++)
		;
	return ttllen;
}
#endif /* HAVE_STRLCAT */

#ifndef	WIN32
/*
 * Case-insensitive comparisons behind the stricmp()/strnicmp() macros.
 *
 * strcasecmp() and strncasecmp() fold case according to LC_CTYPE, which
 * the driver sets from the environment (see derive_locale_encoding()).
 * In Turkish and Azeri locales 'I' and 'i' are different letters for
 * them, so the SQL keywords and ODBC function names the driver looks up,
 * e.g. "IFNULL" against "ifnull", would not match.  Like PostgreSQL's
 * pg_strcasecmp(), fold ASCII letters independently of the locale and
 * leave only characters with the high bit set to it.
 */
int
pg_strnicmp(const char *s1, const char *s2, size_t n)
{
	for (; n > 0; n--)
	{
		UCHAR	ch1 = (UCHAR) *s1++;
		UCHAR	ch2 = (UCHAR) *s2++;

		if (ch1 != ch2)
		{
			if (ch1 >= 'A' && ch1 <= 'Z')
				ch1 += 'a' - 'A';
			else if (ch1 >= 0x80 && isupper(ch1))
				ch1 = tolower(ch1);

			if (ch2 >= 'A' && ch2 <= 'Z')
				ch2 += 'a' - 'A';
			else if (ch2 >= 0x80 && isupper(ch2))
				ch2 = tolower(ch2);

			if (ch1 != ch2)
				return (int) ch1 - (int) ch2;
		}
		if (ch1 == 0)
			break;
	}
	return 0;
}

int
pg_stricmp(const char *s1, const char *s2)
{
	return pg_strnicmp(s1, s2, (size_t) -1);
}
#endif /* WIN32 */


/*
 * Proprly quote and escape a possibly schema-qualified table name.
 */
/* The length of name in double quotes, with each " in it doubled */
static size_t
quoted_name_length(const char *name)
{
	size_t	len = 2;

	for (; *name != '\0'; name++)
		len += ('"' == *name) ? 2 : 1;
	return len;
}

char *
quote_table(const pgNAME schema, const pgNAME table, char *buf, int buf_size)
{
	const char *ptr;
	size_t		need;
	int			i;

	/*
	 * Each name in double quotes with any " doubled, the '.' between
	 * them and the NUL. A name cut to fit could be another relation or
	 * column, so names that may not fit give the empty string instead:
	 * the statement the driver builds around it then fails with a syntax
	 * error rather than naming another object.
	 */
	need = quoted_name_length(SAFE_NAME(table)) + 1;
	if (NAME_IS_VALID(schema))
		need += quoted_name_length(SAFE_NAME(schema)) + 1;
	if (buf_size <= 0 || need > (size_t) buf_size)
	{
		if (buf_size > 0)
			buf[0] = '\0';
		return buf;
	}

	i = 0;

	if (NAME_IS_VALID(schema))
	{
		buf[i++] = '"';
		for (ptr = SAFE_NAME(schema); *ptr != '\0'; ptr++)
		{
			buf[i++] = *ptr;
			if (*ptr == '"')
				buf[i++] = '"';		/* escape quotes by doubling them */
		}
		buf[i++] = '"';
		buf[i++] = '.';
	}

	buf[i++] = '"';
	for (ptr = SAFE_NAME(table); *ptr != '\0'; ptr++)
	{
		buf[i++] = *ptr;
		if (*ptr == '"')
			buf[i++] = '"'; 		/* escape quotes by doubling them */
	}
	buf[i++] = '"';
	buf[i] = '\0';

	return buf;
}

/*
 * Return TRUE if the value of the libpq connection keyword is a secret, or
 * names a file that can hold one. libpq marks the passwords it knows of in
 * PQconninfoOption.dispchar; this list also serves where a keyword is
 * matched without libpq's description of it. Certificate and CRL paths stay
 * visible for SSL diagnostics.
 */
BOOL
is_sensitive_conninfo_param(const char *keyword)
{
	return is_sensitive_conninfo_keyword(keyword, keyword ? strlen(keyword) : 0);
}

/* is_sensitive_conninfo_param() for a keyword of len bytes */
BOOL
is_sensitive_conninfo_keyword(const char *keyword, size_t len)
{
	static const char * const sensitive_keywords[] = {
		"password",
		"passfile",
		"sslpassword",
		"sslkey",
		"oauth_client_secret",
		NULL
	};
	const char * const *sensitive_keyword;

	if (keyword == NULL)
		return FALSE;
	for (sensitive_keyword = sensitive_keywords; *sensitive_keyword != NULL;
		 sensitive_keyword++)
	{
		if (strlen(*sensitive_keyword) == len &&
			strnicmp(keyword, *sensitive_keyword, len) == 0)
			return TRUE;
	}
	return FALSE;
}

/*
 * Replace with 'x' the secret values in the libpq connection string from p
 * to end, following libpq's rules: a keyword, optional white space, '=',
 * optional white space, and a value that ends at white space or, when it
 * starts with a single quote, at the next unescaped one. A backslash
 * escapes the next character in both kinds of value. What doesn't parse
 * as that is masked from there to end, since libpq may still read part of
 * it as a value.
 */
static void
mask_conninfo_secrets(char *p, const char *end)
{
	while (p < end)
	{
		const char *keyword;
		size_t		keylen;
		BOOL		secret;

		while (p < end && isspace((unsigned char) *p))
			p++;
		if (p >= end)
			break;
		keyword = p;
		while (p < end && '=' != *p && !isspace((unsigned char) *p))
			p++;
		keylen = p - keyword;
		while (p < end && isspace((unsigned char) *p))
			p++;
		if (p >= end || '=' != *p)
		{
			for (; p < end; p++)
				*p = 'x';
			break;
		}
		p++;
		while (p < end && isspace((unsigned char) *p))
			p++;
		secret = is_sensitive_conninfo_keyword(keyword, keylen);
		if (p < end && '\'' == *p)
		{
			for (p++; p < end && '\'' != *p; p++)
			{
				if ('\\' == *p && p + 1 < end)
				{
					if (secret)
						*p = 'x';
					p++;
				}
				if (secret)
					*p = 'x';
			}
			if (p < end)
				p++;			/* the closing quote */
		}
		else
		{
			for (; p < end && !isspace((unsigned char) *p); p++)
			{
				if ('\\' == *p && p + 1 < end)
				{
					if (secret)
						*p = 'x';
					p++;
				}
				if (secret)
					*p = 'x';
			}
		}
	}
}

/*
 * The end of the value of a connection string attribute that starts at
 * value: the '}' that closes it, where "}}" stands for one '}', when it
 * starts with '{', otherwise the next ';' or the end of the string. A
 * braced value without a closing brace ends at the end of the string.
 */
static char *
attribute_value_end(char *value)
{
	char	   *p;

	if ('{' != *value)
	{
		for (p = value; *p && ';' != *p; p++)
			;
		return p;
	}
	for (p = value + 1; *p; p++)
	{
		if ('}' == *p)
		{
			if ('}' == p[1])
				p++;
			else
				return p;
		}
	}
	return p;
}

/*
 * Return a malloc'd copy of a connection string with its secrets masked,
 * so that the debug logs (MyLog and CommLog) can be shared without the
 * database credentials. The string is read by the driver's rules for
 * attributes: "keyword=value" pairs separated by ';', where a value that
 * starts with '{' runs to its closing '}' and "}}" stands for one '}'.
 * The whole value of PWD and Password is masked, and the secrets libpq
 * reads in the value of pqopt (see mask_conninfo_secrets()).
 *
 * The caller is responsible for free()ing the returned string.
 */
char *
hide_password(const char *str)
{
	char	   *outstr, *p;

	if (!str)
		return NULL;
	outstr = strdup(str);
	if (!outstr)
		return NULL;
	for (p = outstr; *p; )
	{
		char	   *keyword = p, *value, *end;
		size_t		keylen;

		while (*p && '=' != *p && ';' != *p)
			p++;
		if ('=' != *p)
		{
			/* an attribute without a value */
			if (*p)
				p++;
			continue;
		}
		keylen = p - keyword;
		value = p + 1;
		end = attribute_value_end(value);
		if ((keylen == 3 && strnicmp(keyword, "PWD", 3) == 0) ||
			(keylen == 8 && strnicmp(keyword, "Password", 8) == 0))
		{
			for (p = value; p < end; p++)
				*p = 'x';
		}
		else if ((keylen == 5 && strnicmp(keyword, "pqopt", 5) == 0) ||
				 (keylen == 2 && strnicmp(keyword, "D5", 2) == 0))
		{
			if ('{' == *value)
				mask_conninfo_secrets(value + 1, end);
			else
				mask_conninfo_secrets(value, end);
		}
		/* past the closing brace, then to the next attribute */
		p = end;
		if ('}' == *p)
			p++;
		while (*p && ';' != *p)
			p++;
		if (*p)
			p++;
	}
	return outstr;
}

/*
 * Return a malloc'd copy of the value of a pqopt attribute, braced or not,
 * with the secrets libpq reads in it masked (see mask_conninfo_secrets()).
 *
 * The caller is responsible for free()ing the returned string.
 */
char *
hide_conninfo_secrets(const char *value)
{
	char	   *outstr;

	if (!value)
		return NULL;
	outstr = strdup(value);
	if (!outstr)
		return NULL;
	if ('{' == outstr[0])
		mask_conninfo_secrets(outstr + 1, attribute_value_end(outstr));
	else
		mask_conninfo_secrets(outstr, outstr + strlen(outstr));
	return outstr;
}
