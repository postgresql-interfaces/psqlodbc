/*-------
 * Module:			win_unicode.c
 *
 * Description:		This module contains utf8 <-> ucs2 conversion routines
 *					under WIndows
 *
 *-------
 */

#include "psqlodbc.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define	byte3check	0xfffff800
#define	byte2_base	0x80c0
#define	byte2_mask1	0x07c0
#define	byte2_mask2	0x003f
#define	byte3_base	0x8080e0
#define	byte3_mask1	0xf000
#define	byte3_mask2	0x0fc0
#define	byte3_mask3	0x003f


#define	surrog_check	0xfc00
#define	surrog1_bits	0xd800
#define	surrog2_bits	0xdc00
#define	byte4_base	0x808080f0
#define	byte4_sr1_mask1	0x0700
#define	byte4_sr1_mask2	0x00fc
#define	byte4_sr1_mask3	0x0003
#define	byte4_sr2_mask1	0x03c0
#define	byte4_sr2_mask2	0x003f
#define	surrogate_adjust	(0x10000 >> 10)

static int little_endian = -1;

SQLULEN	ucs2strlen(const SQLWCHAR *ucs2str)
{
	SQLULEN	len;
	for (len = 0; ucs2str[len]; len++)
		;
	return len;
}
char *ucs2_to_utf8(const SQLWCHAR *ucs2str, SQLLEN ilen, SQLLEN *olen, BOOL lower_identifier)
{
	char *	utf8str;
/*mylog("ucs2_to_utf8 %p ilen=%d ", ucs2str, ilen);*/

	if (!ucs2str)
	{
		*olen = SQL_NULL_DATA;
		return NULL;
	}
	if (little_endian < 0)
	{
		int	crt = 1;
		little_endian = (0 != ((char *) &crt)[0]);
	}
	if (SQL_NTS == ilen)
		ilen = ucs2strlen(ucs2str);
/*mylog(" newlen=%d", ilen);*/
	utf8str = (char *) malloc(ilen * 4 + 1);
	if (utf8str)
	{
		int	i, len = 0;
		UInt2	byte2code;
		Int4	byte4code, surrd1, surrd2;
		const SQLWCHAR	*wstr;

		for (i = 0, wstr = ucs2str; i < ilen; i++, wstr++)
		{
			if (!*wstr)
				break;
			else if (0 == (*wstr & 0xffffff80)) /* ASCII */
			{
				if (lower_identifier)
					utf8str[len++] = (char) tolower(*wstr);
				else
					utf8str[len++] = (char) *wstr;
			}
			else if ((*wstr & byte3check) == 0)
			{
				byte2code = byte2_base |
					    ((byte2_mask1 & *wstr) >> 6) |
					    ((byte2_mask2 & *wstr) << 8);
				if (little_endian)
					memcpy(utf8str + len, (char *) &byte2code, sizeof(byte2code));
				else
				{
					utf8str[len] = ((char *) &byte2code)[1];
					utf8str[len + 1] = ((char *) &byte2code)[0];
				}
				len += sizeof(byte2code);
			}
			/* surrogate pair check for non ucs-2 code */
			else if (surrog1_bits == (*wstr & surrog_check))
			{
				surrd1 = (*wstr & ~surrog_check) + surrogate_adjust;
				wstr++;
				i++;
				surrd2 = (*wstr & ~surrog_check);
				byte4code = byte4_base |
					   ((byte4_sr1_mask1 & surrd1) >> 8) |
					   ((byte4_sr1_mask2 & surrd1) << 6) |
					   ((byte4_sr1_mask3 & surrd1) << 20) |
					   ((byte4_sr2_mask1 & surrd2) << 10) |
					   ((byte4_sr2_mask2 & surrd2) << 24);
				if (little_endian)
					memcpy(utf8str + len, (char *) &byte4code, sizeof(byte4code));
				else
				{
					utf8str[len] = ((char *) &byte4code)[3];
					utf8str[len + 1] = ((char *) &byte4code)[2];
					utf8str[len + 2] = ((char *) &byte4code)[1];
					utf8str[len + 3] = ((char *) &byte4code)[0];
				}
				len += sizeof(byte4code);
			}
			else
			{
				byte4code = byte3_base |
					    ((byte3_mask1 & *wstr) >> 12) |
					    ((byte3_mask2 & *wstr) << 2) |
					    ((byte3_mask3 & *wstr) << 16);
				if (little_endian)
					memcpy(utf8str + len, (char *) &byte4code, 3);
				else
				{
					utf8str[len] = ((char *) &byte4code)[3];
					utf8str[len + 1] = ((char *) &byte4code)[2];
					utf8str[len + 2] = ((char *) &byte4code)[1];
				}
				len += 3;
			}
		}
		utf8str[len] = '\0';
		if (olen)
			*olen = len;
	}
mylog(" %s:olen=%d utf8str=%s\n", __FUNCTION__, *olen, utf8str ? utf8str : "");
	return utf8str;
}

#define	byte3_m1	0x0f
#define	byte3_m2	0x3f
#define	byte3_m3	0x3f
#define	byte2_m1	0x1f
#define	byte2_m2	0x3f
#define	byte4_m1	0x07
#define	byte4_m2	0x3f
#define	byte4_m31	0x30
#define	byte4_m32	0x0f
#define	byte4_m4	0x3f

/*
 * Convert a string from UTF-8 encoding to UCS-2.
 *
 * utf8str		- input string in UTF-8
 * ilen			- length of input string in bytes  (or SQL_NTS)
 * lfconv		- TRUE if line feeds (LF) should be converted to CR + LF
 * ucs2str		- output buffer
 * bufcount		- size of output buffer
 * errcheck		- if TRUE, check for invalidly encoded input characters
 *
 * Returns the number of SQLWCHARs copied to output buffer. If the output
 * buffer is too small, the output is truncated. The output string is
 * NULL-terminated, except when the output is truncated.
 */
SQLULEN
utf8_to_ucs2_lf(const char *utf8str, SQLLEN ilen, BOOL lfconv,
				SQLWCHAR *ucs2str, SQLULEN bufcount, BOOL errcheck)
{
	int			i;
	SQLULEN		rtn, ocount, wcode;
	const UCHAR *str;

/*mylog("utf8_to_ucs2 ilen=%d bufcount=%d", ilen, bufcount);*/
	if (!utf8str)
		return 0;
/*mylog(" string=%s\n", utf8str);*/

	if (!bufcount)
		ucs2str = NULL;
	else if (!ucs2str)
		bufcount = 0;
	if (ilen < 0)
		ilen = strlen(utf8str);
	for (i = 0, ocount = 0, str = (SQLCHAR *) utf8str; i < ilen && *str;)
	{
		if ((*str & 0x80) == 0)
		{
			if (lfconv && PG_LINEFEED == *str &&
			    (i == 0 || PG_CARRIAGE_RETURN != str[-1]))
			{
				if (ocount < bufcount)
					ucs2str[ocount] = PG_CARRIAGE_RETURN;
				ocount++;
			}
			if (ocount < bufcount)
				ucs2str[ocount] = *str;
			ocount++;
			i++;
			str++;
		}
		else if (0xf8 == (*str & 0xf8)) /* more than 5 byte code */
		{
			ocount = (SQLULEN) -1;
			goto cleanup;
		}
		else if (0xf0 == (*str & 0xf8)) /* 4 byte code */
		{
			if (errcheck)
			{
				if (i + 4 > ilen ||
				    0 == (str[1] & 0x80) ||
				    0 == (str[2] & 0x80) ||
				    0 == (str[3] & 0x80))
				{
					ocount = (SQLULEN) -1;
					goto cleanup;
				}
			}
			if (ocount < bufcount)
			{
				wcode = (surrog1_bits |
					((((UInt4) *str) & byte4_m1) << 8) |
					((((UInt4) str[1]) & byte4_m2) << 2) |
					((((UInt4) str[2]) & byte4_m31) >> 4))
					- surrogate_adjust;
				ucs2str[ocount] = (SQLWCHAR) wcode;
			}
			ocount++;
			if (ocount < bufcount)
			{
				wcode = surrog2_bits |
					((((UInt4) str[2]) & byte4_m32) << 6) |
					(((UInt4) str[3]) & byte4_m4);
				ucs2str[ocount] = (SQLWCHAR) wcode;
			}
			ocount++;
			i += 4;
			str += 4;
		}
		else if (0xe0 == (*str & 0xf0)) /* 3 byte code */
		{
			if (errcheck)
			{
				if (i + 3 > ilen ||
				    0 == (str[1] & 0x80) ||
				    0 == (str[2] & 0x80))
				{
					ocount = (SQLULEN) -1;
					goto cleanup;
				}
			}
			if (ocount < bufcount)
			{
				wcode = ((((UInt4) *str) & byte3_m1) << 12) |
					((((UInt4) str[1]) & byte3_m2) << 6) |
					(((UInt4) str[2]) & byte3_m3);
				ucs2str[ocount] = (SQLWCHAR) wcode;
			}
			ocount++;
			i += 3;
			str += 3;
		}
		else if (0xc0 == (*str & 0xe0)) /* 2 byte code */
		{
			if (errcheck)
			{
				if (i + 2 > ilen ||
				    0 == (str[1] & 0x80))
				{
					ocount = (SQLULEN) -1;
					goto cleanup;
				}
			}
			if (ocount < bufcount)
			{
				wcode = ((((UInt4) *str) & byte2_m1) << 6) |
					(((UInt4) str[1]) & byte2_m2);
				ucs2str[ocount] = (SQLWCHAR) wcode;
			}
			ocount++;
			i += 2;
			str += 2;
		}
		else
		{
			ocount = (SQLULEN) -1;
			goto cleanup;
		}
	}
cleanup:
	rtn = ocount;
	if (ocount == (SQLULEN) -1)
	{
		if (!errcheck)
			rtn = 0;
		ocount = 0;
	}
	if (ocount < bufcount && ucs2str)
		ucs2str[ocount] = 0;
/*mylog(" ocount=%d\n", ocount);*/
	return rtn;
}

#ifdef	NOT_USED
/* UCS4 => utf8 */
#define	byte4check	0xffff0000
#define	byte4_check	0x10000
#define	byte4_mask1	0x1c0000
#define	byte4_mask2	0x3f000
#define	byte4_mask3	0x0fc0
#define	byte4_mask4	0x003f

#define	byte4_m3	0x3f

SQLULEN	ucs4strlen(const UInt4 *ucs4str)
{
	SQLULEN	len;
	for (len = 0; ucs4str[len]; len++)
		;
	return len;
}

char *ucs4_to_utf8(const UInt4 *ucs4str, SQLLEN ilen, SQLLEN *olen, BOOL lower_identifier)
{
	char *	utf8str;
/*mylog(" %s:%p ilen=%d ", __FUNCTION__, ucs4str, ilen);*/

	if (!ucs4str)
	{
		*olen = SQL_NULL_DATA;
		return NULL;
	}
	if (little_endian < 0)
	{
		int	crt = 1;
		little_endian = (0 != ((char *) &crt)[0]);
	}
	if (SQL_NTS == ilen)
		ilen = ucs4strlen(ucs4str);
/*mylog(" newlen=%d", ilen);*/
	utf8str = (char *) malloc(ilen * 4 + 1);
	if (utf8str)
	{
		int	i, len = 0;
		UInt2	byte2code;
		Int4	byte4code;
		const UInt4 *wstr;

		for (i = 0, wstr = ucs4str; i < ilen; i++, wstr++)
		{
			if (!*wstr)
				break;
			else if (0 == (*wstr & 0xffffff80)) /* ASCII */
			{
				if (lower_identifier)
					utf8str[len++] = (char) tolower(*wstr);
				else
					utf8str[len++] = (char) *wstr;
			}
			else if ((*wstr & byte3check) == 0)
			{
				byte2code = byte2_base |
					    ((byte2_mask1 & *wstr) >> 6) |
					    ((byte2_mask2 & *wstr) << 8);
				if (little_endian)
					memcpy(utf8str + len, (char *) &byte2code, sizeof(byte2code));
				else
				{
					utf8str[len] = ((char *) &byte2code)[1];
					utf8str[len + 1] = ((char *) &byte2code)[0];
				}
				len += sizeof(byte2code);
			}
			else if ((*wstr & byte4check) == 0)
			{
				byte4code = byte3_base |
					    ((byte3_mask1 & *wstr) >> 12) |
					    ((byte3_mask2 & *wstr) << 2) |
					    ((byte3_mask3 & *wstr) << 16);
				if (little_endian)
					memcpy(utf8str + len, (char *) &byte4code, 3);
				else
				{
					utf8str[len] = ((char *) &byte4code)[3];
					utf8str[len + 1] = ((char *) &byte4code)[2];
					utf8str[len + 2] = ((char *) &byte4code)[1];
				}
				len += 3;
			}
			else
			{
				byte4code = byte4_base |
					   ((byte4_mask1 & *wstr) >> 18) |
					   ((byte4_mask2 & *wstr) >> 4) |
					   ((byte4_mask3 & *wstr) << 2) |
					   ((byte4_mask4 & *wstr) << 16);
				if (little_endian)
					memcpy(utf8str + len, (char *) &byte4code, sizeof(byte4code));
				else
				{
					utf8str[len] = ((char *) &byte4code)[3];
					utf8str[len + 1] = ((char *) &byte4code)[2];
					utf8str[len + 2] = ((char *) &byte4code)[1];
					utf8str[len + 3] = ((char *) &byte4code)[0];
				}
				len += sizeof(byte4code);
			}
		}
		utf8str[len] = '\0';
		if (olen)
			*olen = len;
	}
mylog(" %s:olen=%d %s\n", __FUNCTION__, *olen, utf8str ? utf8str : "");
	return utf8str;
}

/*
 * Convert a string from UTF-8 encoding to UTF-32.
 *
 * utf8str		- input string in UTF-8
 * ilen			- length of input string in bytes  (or SQL_NTS)
 * lfconv		- TRUE if line feeds (LF) should be converted to CR + LF
 * ucs4str		- output buffer
 * bufcount		- size of output buffer
 * errcheck		- if TRUE, check for invalidly encoded input characters
 *
 * Returns the number of UInt4s copied to output buffer. If the output
 * buffer is too small, the output is truncated. The output string is
 * NULL-terminated, except when the output is truncated.
 */
SQLULEN
utf8_to_ucs4_lf(const char *utf8str, SQLLEN ilen, BOOL lfconv,
				UInt4 *ucs4str, SQLULEN bufcount, BOOL errcheck)
{
	int			i;
	SQLULEN		rtn, ocount, wcode;
	const UCHAR *str;

/*mylog(" %s:ilen=%d bufcount=%d", __FUNCTION__. ilen, bufcount);*/
	if (!utf8str)
		return 0;
/*mylog(" string=%s\n", utf8str);*/

	if (!bufcount)
		ucs4str = NULL;
	else if (!ucs4str)
		bufcount = 0;
	if (ilen < 0)
		ilen = strlen(utf8str);
	for (i = 0, ocount = 0, str = (SQLCHAR *) utf8str; i < ilen && *str;)
	{
		if ((*str & 0x80) == 0)
		{
			if (lfconv && PG_LINEFEED == *str &&
			    (i == 0 || PG_CARRIAGE_RETURN != str[-1]))
			{
				if (ocount < bufcount)
					ucs4str[ocount] = PG_CARRIAGE_RETURN;
				ocount++;
			}
			if (ocount < bufcount)
				ucs4str[ocount] = *str;
			ocount++;
			i++;
			str++;
		}
		else if (0xf8 == (*str & 0xf8)) /* more than 5 byte code */
		{
			ocount = (SQLULEN) -1;
			goto cleanup;
		}
		else if (0xf0 == (*str & 0xf8)) /* 4 byte code */
		{
			if (errcheck)
			{
				if (i + 4 > ilen ||
				    0 == (str[1] & 0x80) ||
				    0 == (str[2] & 0x80) ||
				    0 == (str[3] & 0x80))
				{
					ocount = (SQLULEN) -1;
					goto cleanup;
				}
			}
			if (ocount < bufcount)
			{
				wcode = (((((UInt4) *str) & byte4_m1) << 18) |
					((((UInt4) str[1]) & byte4_m2) << 12) |
					((((UInt4) str[2]) & byte4_m3) << 6)) |
					(((UInt4) str[3]) & byte4_m4);
				ucs4str[ocount] = wcode;
			}
			ocount++;
			i += 4;
			str += 4;
		}
		else if (0xe0 == (*str & 0xf0)) /* 3 byte code */
		{
			if (errcheck)
			{
				if (i + 3 > ilen ||
				    0 == (str[1] & 0x80) ||
				    0 == (str[2] & 0x80))
				{
					ocount = (SQLULEN) -1;
					goto cleanup;
				}
			}
			if (ocount < bufcount)
			{
				wcode = ((((UInt4) *str) & byte3_m1) << 12) |
					((((UInt4) str[1]) & byte3_m2) << 6) |
					(((UInt4) str[2]) & byte3_m3);
				ucs4str[ocount] = wcode;
			}
			ocount++;
			i += 3;
			str += 3;
		}
		else if (0xc0 == (*str & 0xe0)) /* 2 byte code */
		{
			if (errcheck)
			{
				if (i + 2 > ilen ||
				    0 == (str[1] & 0x80))
				{
					ocount = (SQLULEN) -1;
					goto cleanup;
				}
			}
			if (ocount < bufcount)
			{
				wcode = ((((UInt4) *str) & byte2_m1) << 6) |
					(((UInt4) str[1]) & byte2_m2);
				ucs4str[ocount] = (SQLWCHAR) wcode;
			}
			ocount++;
			i += 2;
			str += 2;
		}
		else
		{
			ocount = (SQLULEN) -1;
			goto cleanup;
		}
	}
cleanup:
	rtn = ocount;
	if (ocount == (SQLULEN) -1)
	{
		if (!errcheck)
			rtn = 0;
		ocount = 0;
	}
	if (ocount < bufcount && ucs4str)
		ucs4str[ocount] = 0;
/*mylog(" ocount=%d\n", ocount);*/
	return rtn;
}
#endif /* NOT_USED */

static	int	wcstype = -1;

int get_wcstype(void)
{
	wchar_t	*wdt;
	int sizeof_w = sizeof(wchar_t);
	const UCHAR *cdt;

	if (wcstype >= 0)
		return wcstype;
	wdt = L"a";
	cdt = (UCHAR *) wdt;
	switch (sizeof_w)
	{
		case 2:
			if ('a'  == cdt[0] &&
			    '\0' == cdt[1] &&
			    '\0' == cdt[2] &&
			    '\0' == cdt[3])
			{
				mylog(" %s:UTF-16LE detected\n", __FUNCTION__);
				wcstype = WCSTYPE_UTF16_LE;
			}
			break;
		case 4:
			if ('a'  == cdt[0] &&
			    '\0' == cdt[1] &&
			    '\0' == cdt[2] &&
			    '\0' == cdt[3] &&
			    '\0' == cdt[4] &&
			    '\0' == cdt[5] &&
			    '\0' == cdt[6] &&
			    '\0' == cdt[7])
			{
				mylog(" %s:UTF32-LE detected\n", __FUNCTION__);
				wcstype = WCSTYPE_UTF32_LE;
			}
			break;
	}
	if (wcstype < 0)
		wcstype = WCSTYPE_UNKNOWN;	/* unknown */
	return wcstype;
}

/*
 *	Firstly this function must be called with outmsg parameter is NULL
 */
int msgtowstr(const char *inmsg, int inlen, wchar_t *outmsg, int buflen)
{
	int	outlen = 0;

mylog(" %s:inmsg=%p inlen=%d buflen=%d\n", __FUNCTION__, inmsg, inlen, buflen);
#ifdef	WIN32
	outlen = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
								 inmsg, inlen, outmsg, buflen);
#else
#ifdef	HAVE_MBSTOWCS
	outlen = mbstowcs((wchar_t *) outmsg, inmsg, inlen);
mylog(" mbstowcs inlen=%d buflen=%d outlen=%d\n", inlen, buflen, outlen);
#endif /* HAVE_MBSTOWCS */
#endif /* WIN32 */
	if (outmsg && outlen < buflen)
		outmsg[outlen] = 0;
mylog(" %s:out=%dchars\n", __FUNCTION__, outlen);

	return outlen;
}

int wstrtomsg(const wchar_t *wstr, int wstrlen, char *outmsg, int buflen)
{
	int	outlen = 0;

mylog(" %s:wstr=%p wstrlen=%d buflen=%d\n", __FUNCTION__, wstr, wstrlen, buflen);
#ifdef	WIN32
	outlen = WideCharToMultiByte(CP_ACP, 0, wstr, (int) wstrlen, outmsg, buflen, NULL, NULL);
#else
#ifdef	HAVE_MBSTOWCS
	outlen = wcstombs(outmsg, wstr, buflen);
mylog(" wcstombs wstrlen=%d buflen=%d outlen=%d\n", wstrlen, buflen, outlen);
#endif /* HAVE_MBSTOWCS */
#endif /* WIN32 */
	if (outmsg && outlen < buflen)
		outmsg[outlen] = 0;

	return outlen;
}
