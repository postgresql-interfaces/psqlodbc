/*-------
 * Module:			win_unicode.c
 *
 * Description:		This module contains utf8 <-> ucs2 conversion routines
 *					under WIndows
 *
 *-------
 */

#include "psqlodbc.h"
#ifdef  WIN32
/* gcc <malloc.h> has been replaced by <stdlib.h> */
#include <malloc.h>
#ifdef  _DEBUG
#ifndef _MEMORY_DEBUG_
#include <stdlib.h>
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif /* _MEMORY_DEBUG_ */
#endif /* _DEBUG */
#endif /* WIN32 */
#include <string.h>

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
#define	surrogate_adjust	0x10000

#include <ctype.h>
#ifndef WIN32
#ifdef HAVE_ISWASCII
#include <wctype.h>
#else
#include <wchar.h>
int	iswascii(wchar_t c)
{
	return isascii(wctob(c));
}
#endif  /* HAVE_ISWASCII */
#endif  /* WIN32 */

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
				memcpy(utf8str + len, (char *) &byte2code, sizeof(byte2code));
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
				memcpy(utf8str + len, (char *) &byte4code, sizeof(byte4code));
				len += sizeof(byte4code);
			}
			else
			{
				byte4code = byte3_base |
					    ((byte3_mask1 & *wstr) >> 12) | 
					    ((byte3_mask2 & *wstr) << 2) | 
					    ((byte3_mask3 & *wstr) << 16);
				memcpy(utf8str + len, (char *) &byte4code, 3);
				len += 3;
			}
		} 
		utf8str[len] = '\0';
		if (olen)
			*olen = len;
	}
/*mylog(" olen=%d %s\n", *olen, utf8str ? utf8str : "");*/
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
SQLULEN	utf8_to_ucs2_lf(const char *utf8str, SQLLEN ilen, BOOL lfconv, SQLWCHAR *ucs2str, SQLULEN bufcount)
{
	int	i;
	SQLULEN	ocount, wcode;
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
	for (i = 0, ocount = 0, str = utf8str; i < ilen && *str;)
	{
		/* if (iswascii(*str)) */
		if (isascii(*str))
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
		else if (0xf0 == (*str & 0xf8)) /* 4 byte code */
		{
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
		else
		{
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
	}
	if (ocount < bufcount && ucs2str)
		ucs2str[ocount] = 0;
/*mylog(" ocount=%d\n", ocount);*/
	return ocount;
}

