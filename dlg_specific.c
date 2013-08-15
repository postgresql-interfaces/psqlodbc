/*-------
 * Module:			dlg_specific.c
 *
 * Description:		This module contains any specific code for handling
 *					dialog boxes such as driver/datasource options.  Both the
 *					ConfigDSN() and the SQLDriverConnect() functions use
 *					functions in this module.  If you were to add a new option
 *					to any dialog box, you would most likely only have to change
 *					things in here rather than in 2 separate places as before.
 *
 * Classes:			none
 *
 * API functions:	none
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */
/* Multibyte support	Eiji Tokuya 2001-03-15 */

#include <ctype.h>
#include "dlg_specific.h"
#include "misc.h"

#include "convert.h"

#include "multibyte.h"
#include "pgapifunc.h"

extern GLOBAL_VALUES globals;

static void encode(const pgNAME, UCHAR *out, int outlen);
static pgNAME decode(const UCHAR *in);
static pgNAME decode_or_remove_braces(const UCHAR *in);

#define	OVR_EXTRA_BITS (BIT_FORCEABBREVCONNSTR | BIT_FAKE_MSS | BIT_BDE_ENVIRONMENT | BIT_CVT_NULL_DATE | BIT_ACCESSIBLE_ONLY)
UInt4	getExtraOptions(const ConnInfo *ci)
{
	UInt4	flag = ci->extra_opts & (~OVR_EXTRA_BITS);

	if (ci->force_abbrev_connstr > 0)
		flag |= BIT_FORCEABBREVCONNSTR;
	if (ci->fake_mss > 0)
		flag |= BIT_FAKE_MSS;
	if (ci->bde_environment > 0)
		flag |= BIT_BDE_ENVIRONMENT;
	if (ci->cvt_null_date_string > 0)
		flag |= BIT_CVT_NULL_DATE;
	if (ci->accessible_only > 0)
		flag |= BIT_ACCESSIBLE_ONLY;
		
	return flag;
}

CSTR	hex_format = "%x";
CSTR	dec_format = "%u";
CSTR	octal_format = "%o";
static UInt4	replaceExtraOptions(ConnInfo *ci, UInt4 flag, BOOL overwrite)
{
	if (overwrite)
		ci->extra_opts = flag;
	else
		ci->extra_opts |= (flag & ~(OVR_EXTRA_BITS));  
	if (overwrite || ci->force_abbrev_connstr < 0)
		ci->force_abbrev_connstr = (0 != (flag & BIT_FORCEABBREVCONNSTR));
	if (overwrite || ci->fake_mss < 0)
		ci->fake_mss = (0 != (flag & BIT_FAKE_MSS));
	if (overwrite || ci->bde_environment < 0)
		ci->bde_environment = (0 != (flag & BIT_BDE_ENVIRONMENT));
	if (overwrite || ci->cvt_null_date_string < 0)
		ci->cvt_null_date_string = (0 != (flag & BIT_CVT_NULL_DATE));
	if (overwrite || ci->accessible_only < 0)
		ci->accessible_only = (0 != (flag & BIT_ACCESSIBLE_ONLY));
		
	return (ci->extra_opts = getExtraOptions(ci));
}
BOOL	setExtraOptions(ConnInfo *ci, const char *optstr, const char *format)
{
	UInt4	flag = 0;

	if (!format)
	{
		if ('0' == *optstr)
		{
			switch (optstr[1])
			{
				case '\0':
					format = dec_format;
					break;
				case 'x':
				case 'X':
					optstr += 2;
					format = hex_format;
					break;
				default:
					format = octal_format;
					break;
			}
		}
		else
			format = dec_format;
	}
		
	if (sscanf(optstr, format, &flag) < 1)
		return FALSE;
	replaceExtraOptions(ci, flag, TRUE);
	return TRUE;
}
UInt4	add_removeExtraOptions(ConnInfo *ci, UInt4 aflag, UInt4 dflag)
{
	ci->extra_opts |= aflag;
	ci->extra_opts &= (~dflag);
	if (0 != (aflag & BIT_FORCEABBREVCONNSTR))
		ci->force_abbrev_connstr = TRUE;
	if (0 != (aflag & BIT_FAKE_MSS))
		ci->fake_mss = TRUE;
	if (0 != (aflag & BIT_BDE_ENVIRONMENT))
		ci->bde_environment = TRUE;
	if (0 != (aflag & BIT_CVT_NULL_DATE))
		ci->cvt_null_date_string = TRUE;
	if (0 != (aflag & BIT_ACCESSIBLE_ONLY))
		ci->accessible_only = TRUE;
	if (0 != (dflag & BIT_FORCEABBREVCONNSTR))
		ci->force_abbrev_connstr = FALSE;
	if (0 != (dflag & BIT_FAKE_MSS))
		ci->fake_mss =FALSE;
	if (0 != (dflag & BIT_CVT_NULL_DATE))
		ci->cvt_null_date_string = FALSE;
	if (0 != (dflag & BIT_ACCESSIBLE_ONLY))
		ci->accessible_only = FALSE;

	return (ci->extra_opts = getExtraOptions(ci));
}

static const char *
abbrev_sslmode(const char *sslmode, char *abbrevmode)
{
	switch (sslmode[0])
	{
		case SSLLBYTE_DISABLE:
		case SSLLBYTE_ALLOW:
		case SSLLBYTE_PREFER:
		case SSLLBYTE_REQUIRE:
			abbrevmode[0] = sslmode[0];
			abbrevmode[1] = '\0';
			break;
		case SSLLBYTE_VERIFY:
			abbrevmode[0] = sslmode[0];
			abbrevmode[2] = '\0';
			switch (sslmode[1])
			{
				case 'f':
				case 'c':
					abbrevmode[1] = sslmode[1];
					break;
				default:
					if (strnicmp(sslmode, "verify_", 7) == 0)
					{
						abbrevmode[1] = sslmode[7];
					}
					else
						strcpy(abbrevmode, sslmode);	
			}
			break;
	} 
	return abbrevmode;
}

void
makeConnectString(char *connect_string, const ConnInfo *ci, UWORD len)
{
	char		got_dsn = (ci->dsn[0] != '\0');
	char		encoded_item[LARGE_REGISTRY_LEN];
	ssize_t		hlen, nlen, olen;
	/*BOOL		abbrev = (len <= 400);*/
	BOOL		abbrev = (len < 1024) || 0 < ci->force_abbrev_connstr;
	UInt4		flag;

inolog("force_abbrev=%d abbrev=%d\n", ci->force_abbrev_connstr, abbrev);
	encode(ci->password, encoded_item, sizeof(encoded_item));
	/* fundamental info */
	nlen = MAX_CONNECT_STRING;
	olen = snprintf(connect_string, nlen, "%s=%s;DATABASE=%s;SERVER=%s;PORT=%s;UID=%s;PWD=%s",
			got_dsn ? "DSN" : "DRIVER",
			got_dsn ? ci->dsn : ci->drivername,
			ci->database,
			ci->server,
			ci->port,
			ci->username,
			encoded_item);
	if (olen < 0 || olen >= nlen)
	{
		connect_string[0] = '\0';
		return;
	}

	encode(ci->conn_settings, encoded_item, sizeof(encoded_item));

	/* extra info */
	hlen = strlen(connect_string);
	nlen = MAX_CONNECT_STRING - hlen;
inolog("hlen=%d", hlen);
	if (!abbrev)
	{
		char	protocol_and[16];
		
		if (ci->rollback_on_error >= 0)
			snprintf(protocol_and, sizeof(protocol_and), "%s-%d", ci->protocol, ci->rollback_on_error);
		else
			strncpy_null(protocol_and, ci->protocol, sizeof(protocol_and));
		olen = snprintf(&connect_string[hlen], nlen, ";"
			INI_SSLMODE "=%s;"
			INI_READONLY "=%s;"
			INI_PROTOCOL "=%s;"
			INI_FAKEOIDINDEX "=%s;"
			INI_SHOWOIDCOLUMN "=%s;"
			INI_ROWVERSIONING "=%s;"
			INI_SHOWSYSTEMTABLES "=%s;"
			INI_CONNSETTINGS "=%s;"
			INI_FETCH "=%d;"
			INI_SOCKET "=%d;"
			INI_UNKNOWNSIZES "=%d;"
			INI_MAXVARCHARSIZE "=%d;"
			INI_MAXLONGVARCHARSIZE "=%d;"
			INI_DEBUG "=%d;"
			INI_COMMLOG "=%d;"
			INI_OPTIMIZER "=%d;"
			INI_KSQO "=%d;"
			INI_USEDECLAREFETCH "=%d;"
			INI_TEXTASLONGVARCHAR "=%d;"
			INI_UNKNOWNSASLONGVARCHAR "=%d;"
			INI_BOOLSASCHAR "=%d;"
			INI_PARSE "=%d;"
			INI_CANCELASFREESTMT "=%d;"
			INI_EXTRASYSTABLEPREFIXES "=%s;"
			INI_LFCONVERSION "=%d;"
			INI_UPDATABLECURSORS "=%d;"
			INI_DISALLOWPREMATURE "=%d;"
			INI_TRUEISMINUS1 "=%d;"
			INI_INT8AS "=%d;"
			INI_BYTEAASLONGVARBINARY "=%d;"
			INI_USESERVERSIDEPREPARE "=%d;"
			INI_LOWERCASEIDENTIFIER "=%d;"
#ifdef	WIN32
			INI_GSSAUTHUSEGSSAPI "=%d;"
#endif /* WIN32 */
#ifdef	_HANDLE_ENLIST_IN_DTC_
			INI_XAOPT "=%d"	/* XAOPT */
#endif /* _HANDLE_ENLIST_IN_DTC_ */
			,ci->sslmode
			,ci->onlyread
			,protocol_and
			,ci->fake_oid_index
			,ci->show_oid_column
			,ci->row_versioning
			,ci->show_system_tables
			,encoded_item
			,ci->drivers.fetch_max
			,ci->drivers.socket_buffersize
			,ci->drivers.unknown_sizes
			,ci->drivers.max_varchar_size
			,ci->drivers.max_longvarchar_size
			,ci->drivers.debug
			,ci->drivers.commlog
			,ci->drivers.disable_optimizer
			,ci->drivers.ksqo
			,ci->drivers.use_declarefetch
			,ci->drivers.text_as_longvarchar
			,ci->drivers.unknowns_as_longvarchar
			,ci->drivers.bools_as_char
			,ci->drivers.parse
			,ci->drivers.cancel_as_freestmt
			,ci->drivers.extra_systable_prefixes
			,ci->lf_conversion
			,ci->allow_keyset
			,ci->disallow_premature
			,ci->true_is_minus1
			,ci->int8_as
			,ci->bytea_as_longvarbinary
			,ci->use_server_side_prepare
			,ci->lower_case_identifier
#ifdef	WIN32
			,ci->gssauth_use_gssapi
#endif /* WIN32 */
#ifdef	_HANDLE_ENLIST_IN_DTC_
			,ci->xa_opt
#endif /* _HANDLE_ENLIST_IN_DTC_ */
				);
	}
	/* Abbreviation is needed ? */
	if (abbrev || olen >= nlen || olen < 0)
	{
		flag = 0;
		if (ci->disallow_premature)
			flag |= BIT_DISALLOWPREMATURE;
		if (ci->allow_keyset)
			flag |= BIT_UPDATABLECURSORS;
		if (ci->lf_conversion)
			flag |= BIT_LFCONVERSION;
		if (ci->drivers.unique_index)
			flag |= BIT_UNIQUEINDEX;
		if (PROTOCOL_74(ci))
			flag |= (BIT_PROTOCOL_64 | BIT_PROTOCOL_63);
		else if (PROTOCOL_64(ci))
			flag |= BIT_PROTOCOL_64;
		else if (PROTOCOL_63(ci))
			flag |= BIT_PROTOCOL_63;
		switch (ci->drivers.unknown_sizes)
		{
			case UNKNOWNS_AS_DONTKNOW:
				flag |= BIT_UNKNOWN_DONTKNOW;
				break;
			case UNKNOWNS_AS_MAX:
				flag |= BIT_UNKNOWN_ASMAX;
				break;
		}
		if (ci->drivers.disable_optimizer)
			flag |= BIT_OPTIMIZER;
		if (ci->drivers.ksqo)
			flag |= BIT_KSQO;
		if (ci->drivers.commlog)
			flag |= BIT_COMMLOG;
		if (ci->drivers.debug)
			flag |= BIT_DEBUG;
		if (ci->drivers.parse)
			flag |= BIT_PARSE;
		if (ci->drivers.cancel_as_freestmt)
			flag |= BIT_CANCELASFREESTMT;
		if (ci->drivers.use_declarefetch)
			flag |= BIT_USEDECLAREFETCH;
		if (ci->onlyread[0] == '1')
			flag |= BIT_READONLY;
		if (ci->drivers.text_as_longvarchar)
			flag |= BIT_TEXTASLONGVARCHAR;
		if (ci->drivers.unknowns_as_longvarchar)
			flag |= BIT_UNKNOWNSASLONGVARCHAR;
		if (ci->drivers.bools_as_char)
			flag |= BIT_BOOLSASCHAR;
		if (ci->row_versioning[0] == '1')
			flag |= BIT_ROWVERSIONING;
		if (ci->show_system_tables[0] == '1')
			flag |= BIT_SHOWSYSTEMTABLES;
		if (ci->show_oid_column[0] == '1')
			flag |= BIT_SHOWOIDCOLUMN;
		if (ci->fake_oid_index[0] == '1')
			flag |= BIT_FAKEOIDINDEX;
		if (ci->true_is_minus1)
			flag |= BIT_TRUEISMINUS1;
		if (ci->bytea_as_longvarbinary)
			flag |= BIT_BYTEAASLONGVARBINARY;
		if (ci->use_server_side_prepare)
			flag |= BIT_USESERVERSIDEPREPARE;
		if (ci->lower_case_identifier)
			flag |= BIT_LOWERCASEIDENTIFIER;
		if (ci->gssauth_use_gssapi)
			flag |= BIT_GSSAUTHUSEGSSAPI;

		if (ci->sslmode[0])
		{
			char	abbrevmode[sizeof(ci->sslmode)];

			olen = snprintf(&connect_string[hlen], nlen, ";"
				ABBR_SSLMODE "=%s", abbrev_sslmode(ci->sslmode, abbrevmode));
		}
		hlen = strlen(connect_string);
		nlen = MAX_CONNECT_STRING - hlen;
		olen = snprintf(&connect_string[hlen], nlen, ";"
				ABBR_CONNSETTINGS "=%s;"
				ABBR_FETCH "=%d;"
				ABBR_SOCKET "=%d;"
				ABBR_MAXVARCHARSIZE "=%d;"
				ABBR_MAXLONGVARCHARSIZE "=%d;"
				INI_INT8AS "=%d;"
				ABBR_EXTRASYSTABLEPREFIXES "=%s;"
				INI_ABBREVIATE "=%02x%x",
				encoded_item,
				ci->drivers.fetch_max,
				ci->drivers.socket_buffersize,
				ci->drivers.max_varchar_size,
				ci->drivers.max_longvarchar_size,
				ci->int8_as,
				ci->drivers.extra_systable_prefixes,
				EFFECTIVE_BIT_COUNT, flag);
		if (olen < nlen && (PROTOCOL_74(ci) || ci->rollback_on_error >= 0))
		{
			hlen = strlen(connect_string);
			nlen = MAX_CONNECT_STRING - hlen;
			/*
			 * The PROTOCOL setting must be placed after CX flag
			 * so that this option can override the CX setting.
			 */
			if (ci->rollback_on_error >= 0)
				olen = snprintf(&connect_string[hlen], nlen, ";"
				ABBR_PROTOCOL "=%s-%d",
				ci->protocol, ci->rollback_on_error);
			else
				olen = snprintf(&connect_string[hlen], nlen, ";"
				ABBR_PROTOCOL "=%s",
				ci->protocol);
		}
	}
	if (olen < nlen)
	{
		flag = getExtraOptions(ci);
		if (0 != flag)
		{
			hlen = strlen(connect_string);
			nlen = MAX_CONNECT_STRING - hlen;
			olen = snprintf(&connect_string[hlen], nlen, ";"
				INI_EXTRAOPTIONS "=%x;",
				flag);
		}
	}
	if (olen < 0 || olen >= nlen) /* failed */
		connect_string[0] = '\0';
}

static void
unfoldCXAttribute(ConnInfo *ci, const char *value)
{
	int	count;
	UInt4	flag;

	if (strlen(value) < 2)
	{
		count = 3;
		sscanf(value, "%x", &flag);
	}
	else
	{
		char	cnt[8];
		memcpy(cnt, value, 2);
		cnt[2] = '\0';
		sscanf(cnt, "%x", &count);
		sscanf(value + 2, "%x", &flag);
	}
	ci->disallow_premature = (char)((flag & BIT_DISALLOWPREMATURE) != 0);
	ci->allow_keyset = (char)((flag & BIT_UPDATABLECURSORS) != 0);
	ci->lf_conversion = (char)((flag & BIT_LFCONVERSION) != 0);
	if (count < 4)
		return;
	ci->drivers.unique_index = (char)((flag & BIT_UNIQUEINDEX) != 0);
	if ((flag & BIT_PROTOCOL_64) != 0)
	{
		if ((flag & BIT_PROTOCOL_63) != 0)
			strcpy(ci->protocol, PG74);
		else
			strcpy(ci->protocol, PG64);
	}
	else if ((flag & BIT_PROTOCOL_63) != 0)
		strcpy(ci->protocol, PG63);
	else
		strcpy(ci->protocol, PG62);
	if ((flag & BIT_UNKNOWN_DONTKNOW) != 0)
		ci->drivers.unknown_sizes = UNKNOWNS_AS_DONTKNOW;
	else if ((flag & BIT_UNKNOWN_ASMAX) != 0)
		ci->drivers.unknown_sizes = UNKNOWNS_AS_MAX;
	else 
		ci->drivers.unknown_sizes = UNKNOWNS_AS_LONGEST;
	ci->drivers.disable_optimizer = (char)((flag & BIT_OPTIMIZER) != 0);
	ci->drivers.ksqo = (char)((flag & BIT_KSQO) != 0);
	ci->drivers.commlog = (char)((flag & BIT_COMMLOG) != 0);
	ci->drivers.debug = (char)((flag & BIT_DEBUG) != 0);
	ci->drivers.parse = (char)((flag & BIT_PARSE) != 0);
	ci->drivers.cancel_as_freestmt = (char)((flag & BIT_CANCELASFREESTMT) != 0);
	ci->drivers.use_declarefetch = (char)((flag & BIT_USEDECLAREFETCH) != 0);
	sprintf(ci->onlyread, "%d", (char)((flag & BIT_READONLY) != 0));
	ci->drivers.text_as_longvarchar = (char)((flag & BIT_TEXTASLONGVARCHAR) !=0);
	ci->drivers.unknowns_as_longvarchar = (char)((flag & BIT_UNKNOWNSASLONGVARCHAR) !=0);
	ci->drivers.bools_as_char = (char)((flag & BIT_BOOLSASCHAR) != 0);
	sprintf(ci->row_versioning, "%d", (char)((flag & BIT_ROWVERSIONING) != 0));
	sprintf(ci->show_system_tables, "%d", (char)((flag & BIT_SHOWSYSTEMTABLES) != 0));
	sprintf(ci->show_oid_column, "%d", (char)((flag & BIT_SHOWOIDCOLUMN) != 0));
	sprintf(ci->fake_oid_index, "%d", (char)((flag & BIT_FAKEOIDINDEX) != 0));
	ci->true_is_minus1 = (char)((flag & BIT_TRUEISMINUS1) != 0);
	ci->bytea_as_longvarbinary = (char)((flag & BIT_BYTEAASLONGVARBINARY) != 0);
	ci->use_server_side_prepare = (char)((flag & BIT_USESERVERSIDEPREPARE) != 0);
	ci->lower_case_identifier = (char)((flag & BIT_LOWERCASEIDENTIFIER) != 0);
	ci->gssauth_use_gssapi = (char)((flag & BIT_GSSAUTHUSEGSSAPI) != 0);
}
BOOL
copyAttributes(ConnInfo *ci, const char *attribute, const char *value)
{
	CSTR	func = "copyAttributes";
	BOOL	found = TRUE;

	if (stricmp(attribute, "DSN") == 0)
		strcpy(ci->dsn, value);

	else if (stricmp(attribute, "driver") == 0)
		strcpy(ci->drivername, value);

	else if (stricmp(attribute, INI_DATABASE) == 0)
		strcpy(ci->database, value);

	else if (stricmp(attribute, INI_SERVER) == 0 || stricmp(attribute, SPEC_SERVER) == 0)
		strcpy(ci->server, value);

	else if (stricmp(attribute, INI_USERNAME) == 0 || stricmp(attribute, INI_UID) == 0)
		strcpy(ci->username, value);

	else if (stricmp(attribute, INI_PASSWORD) == 0 || stricmp(attribute, "pwd") == 0)
		ci->password = decode_or_remove_braces(value);

	else if (stricmp(attribute, INI_PORT) == 0)
		strcpy(ci->port, value);

	else if (stricmp(attribute, INI_READONLY) == 0 || stricmp(attribute, ABBR_READONLY) == 0)
		strcpy(ci->onlyread, value);

	else if (stricmp(attribute, INI_PROTOCOL) == 0 || stricmp(attribute, ABBR_PROTOCOL) == 0)
	{
		char	*ptr;

		ptr = strchr(value, '-');
		if (ptr)
		{
			if ('-' != *value)
			{
				*ptr = '\0';
				strcpy(ci->protocol, value);
			}
			ci->rollback_on_error = atoi(ptr + 1);
			mylog("rollback_on_error=%d\n", ci->rollback_on_error);
		}
		else
			strcpy(ci->protocol, value);
	}

	else if (stricmp(attribute, INI_SHOWOIDCOLUMN) == 0 || stricmp(attribute, ABBR_SHOWOIDCOLUMN) == 0)
		strcpy(ci->show_oid_column, value);

	else if (stricmp(attribute, INI_FAKEOIDINDEX) == 0 || stricmp(attribute, ABBR_FAKEOIDINDEX) == 0)
		strcpy(ci->fake_oid_index, value);

	else if (stricmp(attribute, INI_ROWVERSIONING) == 0 || stricmp(attribute, ABBR_ROWVERSIONING) == 0)
		strcpy(ci->row_versioning, value);

	else if (stricmp(attribute, INI_SHOWSYSTEMTABLES) == 0 || stricmp(attribute, ABBR_SHOWSYSTEMTABLES) == 0)
		strcpy(ci->show_system_tables, value);

	else if (stricmp(attribute, INI_CONNSETTINGS) == 0 || stricmp(attribute, ABBR_CONNSETTINGS) == 0)
	{
		/* We can use the conn_settings directly when they are enclosed with braces */
		if ('{' == *value)
		{
			size_t	len;

			len = strlen(value + 1);
			if (len > 0 && '}' == value[len])
				len--;
			STRN_TO_NAME(ci->conn_settings, value + 1, len);
		}
		else
			ci->conn_settings = decode(value);
	}
	else if (stricmp(attribute, INI_DISALLOWPREMATURE) == 0 || stricmp(attribute, ABBR_DISALLOWPREMATURE) == 0)
		ci->disallow_premature = atoi(value);
	else if (stricmp(attribute, INI_UPDATABLECURSORS) == 0 || stricmp(attribute, ABBR_UPDATABLECURSORS) == 0)
		ci->allow_keyset = atoi(value);
	else if (stricmp(attribute, INI_LFCONVERSION) == 0 || stricmp(attribute, ABBR_LFCONVERSION) == 0)
		ci->lf_conversion = atoi(value);
	else if (stricmp(attribute, INI_TRUEISMINUS1) == 0 || stricmp(attribute, ABBR_TRUEISMINUS1) == 0)
		ci->true_is_minus1 = atoi(value);
	else if (stricmp(attribute, INI_INT8AS) == 0)
		ci->int8_as = atoi(value);
	else if (stricmp(attribute, INI_BYTEAASLONGVARBINARY) == 0 || stricmp(attribute, ABBR_BYTEAASLONGVARBINARY) == 0)
		ci->bytea_as_longvarbinary = atoi(value);
	else if (stricmp(attribute, INI_USESERVERSIDEPREPARE) == 0 || stricmp(attribute, ABBR_USESERVERSIDEPREPARE) == 0)
		ci->use_server_side_prepare = atoi(value);
	else if (stricmp(attribute, INI_LOWERCASEIDENTIFIER) == 0 || stricmp(attribute, ABBR_LOWERCASEIDENTIFIER) == 0)
		ci->lower_case_identifier = atoi(value);
	else if (stricmp(attribute, INI_GSSAUTHUSEGSSAPI) == 0 || stricmp(attribute, ABBR_GSSAUTHUSEGSSAPI) == 0)
		ci->gssauth_use_gssapi = atoi(value);
	else if (stricmp(attribute, INI_SSLMODE) == 0 || stricmp(attribute, ABBR_SSLMODE) == 0)
	{
		switch (value[0])
		{
			case SSLLBYTE_ALLOW:
				strcpy(ci->sslmode, SSLMODE_ALLOW);
				break;
			case SSLLBYTE_PREFER:
				strcpy(ci->sslmode, SSLMODE_PREFER);
				break;
			case SSLLBYTE_REQUIRE:
				strcpy(ci->sslmode, SSLMODE_REQUIRE);
				break;
			case SSLLBYTE_VERIFY:
				switch (value[1])
				{
					case 'f':
						strcpy(ci->sslmode, SSLMODE_VERIFY_FULL);
						break;
					case 'c':
						strcpy(ci->sslmode, SSLMODE_VERIFY_CA);
						break;
					default:
						strcpy(ci->sslmode, value);
				}
				break;
			case SSLLBYTE_DISABLE:
			default:
				strcpy(ci->sslmode, SSLMODE_DISABLE);
				break;
		}
	}
	else if (stricmp(attribute, INI_ABBREVIATE) == 0)
		unfoldCXAttribute(ci, value);
#ifdef	_HANDLE_ENLIST_IN_DTC_
	else if (stricmp(attribute, INI_XAOPT) == 0)
		ci->xa_opt = atoi(value);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
	else if (stricmp(attribute, INI_EXTRAOPTIONS) == 0)
	{
		UInt4	val1 = 0, val2 = 0;
	
		if ('+' == value[0])
		{
			sscanf(value + 1, "%x-%x", &val1, &val2);
			add_removeExtraOptions(ci, val1, val2);
		}
		else if ('-' == value[0])
		{
			sscanf(value + 1, "%x", &val2);
			add_removeExtraOptions(ci, 0, val2);
		}
		else
		{
			setExtraOptions(ci, value, hex_format);
		}
		mylog("force_abbrev=%d bde=%d cvt_null_date=%x\n", ci->force_abbrev_connstr, ci->bde_environment, ci->cvt_null_date_string);
	}
	else
		found = FALSE;

	mylog("%s: DSN='%s',server='%s',dbase='%s',user='%s',passwd='%s',port='%s',onlyread='%s',protocol='%s',conn_settings='%s',disallow_premature=%d)\n", func, ci->dsn, ci->server, ci->database, ci->username, NAME_IS_VALID(ci->password) ? "xxxxx" : "", ci->port, ci->onlyread, ci->protocol, ci->conn_settings, ci->disallow_premature);

	return found;
}

BOOL
copyCommonAttributes(ConnInfo *ci, const char *attribute, const char *value)
{
	CSTR	func = "copyCommonAttributes";
	BOOL	found = TRUE;

	if (stricmp(attribute, INI_FETCH) == 0 || stricmp(attribute, ABBR_FETCH) == 0)
		ci->drivers.fetch_max = atoi(value);
	else if (stricmp(attribute, INI_SOCKET) == 0 || stricmp(attribute, ABBR_SOCKET) == 0)
		ci->drivers.socket_buffersize = atoi(value);
	else if (stricmp(attribute, INI_DEBUG) == 0 || stricmp(attribute, ABBR_DEBUG) == 0)
		ci->drivers.debug = atoi(value);
	else if (stricmp(attribute, INI_COMMLOG) == 0 || stricmp(attribute, ABBR_COMMLOG) == 0)
		ci->drivers.commlog = atoi(value);
	else if (stricmp(attribute, INI_OPTIMIZER) == 0 || stricmp(attribute, ABBR_OPTIMIZER) == 0)
		ci->drivers.disable_optimizer = atoi(value);
	else if (stricmp(attribute, INI_KSQO) == 0 || stricmp(attribute, ABBR_KSQO) == 0)
		ci->drivers.ksqo = atoi(value);

	/*
	 * else if (stricmp(attribute, INI_UNIQUEINDEX) == 0 ||
	 * stricmp(attribute, "UIX") == 0) ci->drivers.unique_index =
	 * atoi(value);
	 */
	else if (stricmp(attribute, INI_UNKNOWNSIZES) == 0 || stricmp(attribute, ABBR_UNKNOWNSIZES) == 0)
		ci->drivers.unknown_sizes = atoi(value);
	else if (stricmp(attribute, INI_LIE) == 0)
		ci->drivers.lie = atoi(value);
	else if (stricmp(attribute, INI_PARSE) == 0 || stricmp(attribute, ABBR_PARSE) == 0)
		ci->drivers.parse = atoi(value);
	else if (stricmp(attribute, INI_CANCELASFREESTMT) == 0 || stricmp(attribute, ABBR_CANCELASFREESTMT) == 0)
		ci->drivers.cancel_as_freestmt = atoi(value);
	else if (stricmp(attribute, INI_USEDECLAREFETCH) == 0 || stricmp(attribute, ABBR_USEDECLAREFETCH) == 0)
		ci->drivers.use_declarefetch = atoi(value);
	else if (stricmp(attribute, INI_MAXVARCHARSIZE) == 0 || stricmp(attribute, ABBR_MAXVARCHARSIZE) == 0)
		ci->drivers.max_varchar_size = atoi(value);
	else if (stricmp(attribute, INI_MAXLONGVARCHARSIZE) == 0 || stricmp(attribute, ABBR_MAXLONGVARCHARSIZE) == 0)
		ci->drivers.max_longvarchar_size = atoi(value);
	else if (stricmp(attribute, INI_TEXTASLONGVARCHAR) == 0 || stricmp(attribute, ABBR_TEXTASLONGVARCHAR) == 0)
		ci->drivers.text_as_longvarchar = atoi(value);
	else if (stricmp(attribute, INI_UNKNOWNSASLONGVARCHAR) == 0 || stricmp(attribute, ABBR_UNKNOWNSASLONGVARCHAR) == 0)
		ci->drivers.unknowns_as_longvarchar = atoi(value);
	else if (stricmp(attribute, INI_BOOLSASCHAR) == 0 || stricmp(attribute, ABBR_BOOLSASCHAR) == 0)
		ci->drivers.bools_as_char = atoi(value);
	else if (stricmp(attribute, INI_EXTRASYSTABLEPREFIXES) == 0 || stricmp(attribute, ABBR_EXTRASYSTABLEPREFIXES) == 0)
		strcpy(ci->drivers.extra_systable_prefixes, value);
	else
		found = FALSE;

	mylog("%s: A7=%d;A8=%d;A9=%d;B0=%d;B1=%d;B2=%d;B3=%d;B4=%d;B5=%d;B6=%d;B7=%d;B8=%d;B9=%d;C0=%d;C1=%d;C2=%s", func,
		  ci->drivers.fetch_max,
		  ci->drivers.socket_buffersize,
		  ci->drivers.unknown_sizes,
		  ci->drivers.max_varchar_size,
		  ci->drivers.max_longvarchar_size,
		  ci->drivers.debug,
		  ci->drivers.commlog,
		  ci->drivers.disable_optimizer,
		  ci->drivers.ksqo,
		  ci->drivers.use_declarefetch,
		  ci->drivers.text_as_longvarchar,
		  ci->drivers.unknowns_as_longvarchar,
		  ci->drivers.bools_as_char,
		  ci->drivers.parse,
		  ci->drivers.cancel_as_freestmt,
		  ci->drivers.extra_systable_prefixes);

	return found;
}


void
getDSNdefaults(ConnInfo *ci)
{
	mylog("calling getDSNdefaults\n");

	if (ci->port[0] == '\0')
		strcpy(ci->port, DEFAULT_PORT);

	if (ci->onlyread[0] == '\0')
		sprintf(ci->onlyread, "%d", globals.onlyread);

	if (ci->protocol[0] == '\0')
		strcpy(ci->protocol, globals.protocol);

	if (ci->fake_oid_index[0] == '\0')
		sprintf(ci->fake_oid_index, "%d", DEFAULT_FAKEOIDINDEX);

	if (ci->show_oid_column[0] == '\0')
		sprintf(ci->show_oid_column, "%d", DEFAULT_SHOWOIDCOLUMN);

	if (ci->show_system_tables[0] == '\0')
		sprintf(ci->show_system_tables, "%d", DEFAULT_SHOWSYSTEMTABLES);

	if (ci->row_versioning[0] == '\0')
		sprintf(ci->row_versioning, "%d", DEFAULT_ROWVERSIONING);

	if (ci->disallow_premature < 0)
		ci->disallow_premature = DEFAULT_DISALLOWPREMATURE;
	if (ci->allow_keyset < 0)
		ci->allow_keyset = DEFAULT_UPDATABLECURSORS;
	if (ci->lf_conversion < 0)
		ci->lf_conversion = DEFAULT_LFCONVERSION;
	if (ci->true_is_minus1 < 0)
		ci->true_is_minus1 = DEFAULT_TRUEISMINUS1;
	if (ci->int8_as < -100)
		ci->int8_as = DEFAULT_INT8AS;
	if (ci->bytea_as_longvarbinary < 0)
		ci->bytea_as_longvarbinary = DEFAULT_BYTEAASLONGVARBINARY;
	if (ci->use_server_side_prepare < 0)
		ci->use_server_side_prepare = DEFAULT_USESERVERSIDEPREPARE;
	if (ci->lower_case_identifier < 0)
		ci->lower_case_identifier = DEFAULT_LOWERCASEIDENTIFIER;
	if (ci->gssauth_use_gssapi < 0)
		ci->gssauth_use_gssapi = DEFAULT_GSSAUTHUSEGSSAPI;
	if (ci->sslmode[0] == '\0')
		strcpy(ci->sslmode, DEFAULT_SSLMODE);
	if (ci->force_abbrev_connstr < 0)
		ci->force_abbrev_connstr = 0;
	if (ci->fake_mss < 0)
		ci->fake_mss = 0;
	if (ci->bde_environment < 0)
		ci->bde_environment = 0;
	if (ci->cvt_null_date_string < 0)
		ci->cvt_null_date_string = 0;
#ifdef	_HANDLE_ENLIST_IN_DTC_
	if (ci->xa_opt < 0)
		ci->xa_opt = DEFAULT_XAOPT;
#endif /* _HANDLE_ENLIST_IN_DTC_ */
}

int
getDriverNameFromDSN(const char *dsn, char *driver_name, int namelen)
{
	return SQLGetPrivateProfileString(ODBC_DATASOURCES, dsn, "", driver_name, namelen, ODBC_INI);
}

int
getLogDir(char *dir, int dirmax)
{
	return SQLGetPrivateProfileString(DBMS_NAME, INI_LOGDIR, "", dir, dirmax, ODBCINST_INI);
}

int
setLogDir(const char *dir)
{
	return SQLWritePrivateProfileString(DBMS_NAME, INI_LOGDIR, dir, ODBCINST_INI);
}

void
getDSNinfo(ConnInfo *ci, char overwrite)
{
	CSTR	func = "getDSNinfo";
	char	   *DSN = ci->dsn;
	char		encoded_item[LARGE_REGISTRY_LEN],
				temp[SMALL_REGISTRY_LEN];

/*
 *	If a driver keyword was present, then dont use a DSN and return.
 *	If DSN is null and no driver, then use the default datasource.
 */
	mylog("%s: DSN=%s overwrite=%d\n", func, DSN, overwrite);
	if (DSN[0] == '\0')
	{
		if (ci->drivername[0] != '\0')
			return;
		else
			strncpy_null(DSN, INI_DSN, sizeof(ci->dsn));
	}

	/* brute-force chop off trailing blanks... */
	while (*(DSN + strlen(DSN) - 1) == ' ')
		*(DSN + strlen(DSN) - 1) = '\0';

	if (ci->drivername[0] == '\0' || overwrite)
	{
		getDriverNameFromDSN(DSN, ci->drivername, sizeof(ci->drivername));
		if (ci->drivername[0] && stricmp(ci->drivername, SAFE_NAME(ci->drivers.drivername)))
			getCommonDefaults(ci->drivername, ODBCINST_INI, ci);
	}

	/* Proceed with getting info for the given DSN. */

	if (ci->desc[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_KDESC, "", ci->desc, sizeof(ci->desc), ODBC_INI);

	if (ci->server[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_SERVER, "", ci->server, sizeof(ci->server), ODBC_INI);

	if (ci->database[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_DATABASE, "", ci->database, sizeof(ci->database), ODBC_INI);

	if (ci->username[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_USERNAME, "", ci->username, sizeof(ci->username), ODBC_INI);

	if (NAME_IS_NULL(ci->password) || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_PASSWORD, "", encoded_item, sizeof(encoded_item), ODBC_INI);
		ci->password = decode(encoded_item);
	}

	if (ci->port[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_PORT, "", ci->port, sizeof(ci->port), ODBC_INI);

	if (ci->onlyread[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_READONLY, "", ci->onlyread, sizeof(ci->onlyread), ODBC_INI);

	if (ci->show_oid_column[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_SHOWOIDCOLUMN, "", ci->show_oid_column, sizeof(ci->show_oid_column), ODBC_INI);

	if (ci->fake_oid_index[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_FAKEOIDINDEX, "", ci->fake_oid_index, sizeof(ci->fake_oid_index), ODBC_INI);

	if (ci->row_versioning[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_ROWVERSIONING, "", ci->row_versioning, sizeof(ci->row_versioning), ODBC_INI);

	if (ci->show_system_tables[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_SHOWSYSTEMTABLES, "", ci->show_system_tables, sizeof(ci->show_system_tables), ODBC_INI);

	if (ci->protocol[0] == '\0' || overwrite)
	{
		char	*ptr;
		SQLGetPrivateProfileString(DSN, INI_PROTOCOL, "", ci->protocol, sizeof(ci->protocol), ODBC_INI);
		if (ptr = strchr(ci->protocol, '-'), NULL != ptr)
		{
			*ptr = '\0';
			if (overwrite || ci->rollback_on_error < 0)
			{
				ci->rollback_on_error = atoi(ptr + 1);
				mylog("rollback_on_error=%d\n", ci->rollback_on_error);
			}
		}
	}

	if (NAME_IS_NULL(ci->conn_settings) || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_CONNSETTINGS, "", encoded_item, sizeof(encoded_item), ODBC_INI);
		ci->conn_settings = decode(encoded_item);
	}

	if (ci->translation_dll[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_TRANSLATIONDLL, "", ci->translation_dll, sizeof(ci->translation_dll), ODBC_INI);

	if (ci->translation_option[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_TRANSLATIONOPTION, "", ci->translation_option, sizeof(ci->translation_option), ODBC_INI);

	if (ci->disallow_premature < 0 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_DISALLOWPREMATURE, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->disallow_premature = atoi(temp);
	}

	if (ci->allow_keyset < 0 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_UPDATABLECURSORS, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->allow_keyset = atoi(temp);
	}

	if (ci->lf_conversion < 0 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_LFCONVERSION, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->lf_conversion = atoi(temp);
	}

	if (ci->true_is_minus1 < 0 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_TRUEISMINUS1, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->true_is_minus1 = atoi(temp);
	}

	if (ci->int8_as < -100 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_INT8AS, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->int8_as = atoi(temp);
	}

	if (ci->bytea_as_longvarbinary < 0 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_BYTEAASLONGVARBINARY, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->bytea_as_longvarbinary = atoi(temp);
	}

	if (ci->use_server_side_prepare < 0 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_USESERVERSIDEPREPARE, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->use_server_side_prepare = atoi(temp);
	}

	if (ci->lower_case_identifier < 0 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_LOWERCASEIDENTIFIER, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->lower_case_identifier = atoi(temp);
	}

	if (ci->gssauth_use_gssapi < 0 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_GSSAUTHUSEGSSAPI, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->gssauth_use_gssapi = atoi(temp);
	}

	if (ci->sslmode[0] == '\0' || overwrite)
		SQLGetPrivateProfileString(DSN, INI_SSLMODE, "", ci->sslmode, sizeof(ci->sslmode), ODBC_INI);

#ifdef	_HANDLE_ENLIST_IN_DTC_
	if (ci->xa_opt < 0 || overwrite)
	{
		SQLGetPrivateProfileString(DSN, INI_XAOPT, "", temp, sizeof(temp), ODBC_INI);
		if (temp[0])
			ci->xa_opt = atoi(temp);
	}
#endif /* _HANDLE_ENLIST_IN_DTC_ */

	/* Force abbrev connstr or bde */
	SQLGetPrivateProfileString(DSN, INI_EXTRAOPTIONS, "",
					temp, sizeof(temp), ODBC_INI);
	if (temp[0])
	{
		UInt4	val = 0;

		sscanf(temp, "%x", &val);
		replaceExtraOptions(ci, val, overwrite);
		mylog("force_abbrev=%d bde=%d cvt_null_date=%d\n", ci->force_abbrev_connstr, ci->bde_environment, ci->cvt_null_date_string);
	}

	/* Allow override of odbcinst.ini parameters here */
	getCommonDefaults(DSN, ODBC_INI, ci);

	qlog("DSN info: DSN='%s',server='%s',port='%s',dbase='%s',user='%s',passwd='%s'\n",
		 DSN,
		 ci->server,
		 ci->port,
		 ci->database,
		 ci->username,
		 NAME_IS_VALID(ci->password) ? "xxxxx" : "");
	qlog("          onlyread='%s',protocol='%s',showoid='%s',fakeoidindex='%s',showsystable='%s'\n",
		 ci->onlyread,
		 ci->protocol,
		 ci->show_oid_column,
		 ci->fake_oid_index,
		 ci->show_system_tables);

	if (get_qlog())
	{
		char	*enc = (char *) check_client_encoding(ci->conn_settings);

		qlog("          conn_settings='%s', conn_encoding='%s'\n", ci->conn_settings,
			NULL != enc ? enc : "(null)");
		if (NULL != enc)
			free(enc);
		qlog("          translation_dll='%s',translation_option='%s'\n",
		 	ci->translation_dll,
		 	ci->translation_option);
	}
}
/*
 *	This function writes any global parameters (that can be manipulated)
 *	to the ODBCINST.INI portion of the registry
 */
int
writeDriverCommoninfo(const char *fileName, const char *sectionName,
			 const GLOBAL_VALUES *comval)
{
	char		tmp[128];
	int		errc = 0;

	if (stricmp(ODBCINST_INI, fileName) == 0 && NULL == sectionName)
		sectionName = DBMS_NAME;
 
	sprintf(tmp, "%d", comval->commlog);
	if (!SQLWritePrivateProfileString(sectionName, INI_COMMLOG, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->debug);
	if (!SQLWritePrivateProfileString(sectionName, INI_DEBUG, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->fetch_max);
	if (!SQLWritePrivateProfileString(sectionName, INI_FETCH, tmp, fileName))
		errc--;

	if (stricmp(ODBCINST_INI, fileName) == 0)
		return errc;

	sprintf(tmp, "%d", comval->fetch_max);
	if (!SQLWritePrivateProfileString(sectionName, INI_FETCH, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->disable_optimizer);
	if (!SQLWritePrivateProfileString(sectionName, INI_OPTIMIZER, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->ksqo);
	if (!SQLWritePrivateProfileString(sectionName, INI_KSQO, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->unique_index);
	if (!SQLWritePrivateProfileString(sectionName, INI_UNIQUEINDEX, tmp, fileName))
		errc--;
	/*
	 * Never update the onlyread from this module.
	 */
	if (stricmp(ODBCINST_INI, fileName) == 0)
	{
		sprintf(tmp, "%d", comval->onlyread);
		SQLWritePrivateProfileString(sectionName, INI_READONLY, tmp,
									 fileName);
	}

	sprintf(tmp, "%d", comval->use_declarefetch);
	if (!SQLWritePrivateProfileString(sectionName, INI_USEDECLAREFETCH, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->unknown_sizes);
	if (!SQLWritePrivateProfileString(sectionName, INI_UNKNOWNSIZES, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->text_as_longvarchar);
	if (!SQLWritePrivateProfileString(sectionName, INI_TEXTASLONGVARCHAR, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->unknowns_as_longvarchar);
	if (!SQLWritePrivateProfileString(sectionName, INI_UNKNOWNSASLONGVARCHAR, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->bools_as_char);
	if (!SQLWritePrivateProfileString(sectionName, INI_BOOLSASCHAR, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->parse);
	if (!SQLWritePrivateProfileString(sectionName, INI_PARSE, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->cancel_as_freestmt);
	if (!SQLWritePrivateProfileString(sectionName, INI_CANCELASFREESTMT, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->max_varchar_size);
	if (!SQLWritePrivateProfileString(sectionName, INI_MAXVARCHARSIZE, tmp, fileName))
		errc--;

	sprintf(tmp, "%d", comval->max_longvarchar_size);
	if (!SQLWritePrivateProfileString(sectionName, INI_MAXLONGVARCHARSIZE, tmp, fileName))
		errc--;

	if (!SQLWritePrivateProfileString(sectionName, INI_EXTRASYSTABLEPREFIXES, comval->extra_systable_prefixes, fileName))
		errc--;

	/*
	 * Never update the conn_setting from this module
	 * SQLWritePrivateProfileString(sectionName, INI_CONNSETTINGS,
	 * comval->conn_settings, fileName);
	 */

	return errc;
}

/*	This is for datasource based options only */
void
writeDSNinfo(const ConnInfo *ci)
{
	const char *DSN = ci->dsn;
	char		encoded_item[LARGE_REGISTRY_LEN],
				temp[SMALL_REGISTRY_LEN];


	SQLWritePrivateProfileString(DSN,
								 INI_KDESC,
								 ci->desc,
								 ODBC_INI);

	SQLWritePrivateProfileString(DSN,
								 INI_DATABASE,
								 ci->database,
								 ODBC_INI);

	SQLWritePrivateProfileString(DSN,
								 INI_SERVER,
								 ci->server,
								 ODBC_INI);

	SQLWritePrivateProfileString(DSN,
								 INI_PORT,
								 ci->port,
								 ODBC_INI);

	SQLWritePrivateProfileString(DSN,
								 INI_USERNAME,
								 ci->username,
								 ODBC_INI);
	SQLWritePrivateProfileString(DSN, INI_UID, ci->username, ODBC_INI);

	encode(ci->password, encoded_item, sizeof(encoded_item));
	SQLWritePrivateProfileString(DSN,
								 INI_PASSWORD,
								 encoded_item,
								 ODBC_INI);

	SQLWritePrivateProfileString(DSN,
								 INI_READONLY,
								 ci->onlyread,
								 ODBC_INI);

	SQLWritePrivateProfileString(DSN,
								 INI_SHOWOIDCOLUMN,
								 ci->show_oid_column,
								 ODBC_INI);

	SQLWritePrivateProfileString(DSN,
								 INI_FAKEOIDINDEX,
								 ci->fake_oid_index,
								 ODBC_INI);

	SQLWritePrivateProfileString(DSN,
								 INI_ROWVERSIONING,
								 ci->row_versioning,
								 ODBC_INI);

	SQLWritePrivateProfileString(DSN,
								 INI_SHOWSYSTEMTABLES,
								 ci->show_system_tables,
								 ODBC_INI);

	if (ci->rollback_on_error >= 0)
		sprintf(temp, "%s-%d", ci->protocol, ci->rollback_on_error);
	else
		strncpy_null(temp, ci->protocol, sizeof(temp));
	SQLWritePrivateProfileString(DSN,
								 INI_PROTOCOL,
								 temp,
								 ODBC_INI);

	encode(ci->conn_settings, encoded_item, sizeof(encoded_item));
	SQLWritePrivateProfileString(DSN,
								 INI_CONNSETTINGS,
								 encoded_item,
								 ODBC_INI);

	sprintf(temp, "%d", ci->disallow_premature);
	SQLWritePrivateProfileString(DSN,
								 INI_DISALLOWPREMATURE,
								 temp,
								 ODBC_INI);
	sprintf(temp, "%d", ci->allow_keyset);
	SQLWritePrivateProfileString(DSN,
								 INI_UPDATABLECURSORS,
								 temp,
								 ODBC_INI);
	sprintf(temp, "%d", ci->lf_conversion);
	SQLWritePrivateProfileString(DSN,
								 INI_LFCONVERSION,
								 temp,
								 ODBC_INI);
	sprintf(temp, "%d", ci->true_is_minus1);
	SQLWritePrivateProfileString(DSN,
								 INI_TRUEISMINUS1,
								 temp,
								 ODBC_INI);
	sprintf(temp, "%d", ci->int8_as);
	SQLWritePrivateProfileString(DSN,
								 INI_INT8AS,
								 temp,
								 ODBC_INI);
	sprintf(temp, "%x", getExtraOptions(ci));
	SQLWritePrivateProfileString(DSN,
							INI_EXTRAOPTIONS,
							 temp,
							 ODBC_INI);
	sprintf(temp, "%d", ci->bytea_as_longvarbinary);
	SQLWritePrivateProfileString(DSN,
								 INI_BYTEAASLONGVARBINARY,
								 temp,
								 ODBC_INI);
	sprintf(temp, "%d", ci->use_server_side_prepare);
	SQLWritePrivateProfileString(DSN,
								 INI_USESERVERSIDEPREPARE,
								 temp,
								 ODBC_INI);
	sprintf(temp, "%d", ci->lower_case_identifier);
	SQLWritePrivateProfileString(DSN,
								 INI_LOWERCASEIDENTIFIER,
								 temp,
								 ODBC_INI);
	sprintf(temp, "%d", ci->gssauth_use_gssapi);
	SQLWritePrivateProfileString(DSN,
								 INI_GSSAUTHUSEGSSAPI,
								 temp,
								 ODBC_INI);
	SQLWritePrivateProfileString(DSN,
								 INI_SSLMODE,
								 ci->sslmode,
								 ODBC_INI);
#ifdef	_HANDLE_ENLIST_IN_DTC_
	sprintf(temp, "%d", ci->xa_opt);
	SQLWritePrivateProfileString(DSN, INI_XAOPT, temp, ODBC_INI);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
}


/*
 *	This function reads the ODBCINST.INI portion of
 *	the registry and gets any driver defaults.
 */
void
getCommonDefaults(const char *section, const char *filename, ConnInfo *ci)
{
	CSTR	func = "getCommonDefaults";
	char		temp[256];
	GLOBAL_VALUES *comval;
	BOOL	inst_position = (stricmp(filename, ODBCINST_INI) == 0);
	const char *drivername = (inst_position ? section : ci->drivername);

	mylog("%s:setting %s position of %p\n", func, filename, ci);
	if (ci)
		comval = &(ci->drivers);
	else
		comval = &globals;
	/* Fetch Count is stored in driver section */
	SQLGetPrivateProfileString(section, INI_FETCH, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
	{
		comval->fetch_max = atoi(temp);
		/* sanity check if using cursors */
		if (comval->fetch_max <= 0)
			comval->fetch_max = FETCH_MAX;
	}
	else if (inst_position)
		comval->fetch_max = FETCH_MAX;

	/* Socket Buffersize is stored in driver section */
	SQLGetPrivateProfileString(section, INI_SOCKET, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->socket_buffersize = atoi(temp);
	else if (inst_position)
		comval->socket_buffersize = SOCK_BUFFER_SIZE;

	/* Debug is stored in the driver section */
	SQLGetPrivateProfileString(section, INI_DEBUG, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->debug = atoi(temp);
	else if (inst_position)
		comval->debug = DEFAULT_DEBUG;

	/* CommLog is stored in the driver section */
	SQLGetPrivateProfileString(section, INI_COMMLOG, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->commlog = atoi(temp);
	else if (inst_position)
		comval->commlog = DEFAULT_COMMLOG;

	if (!ci)
		logs_on_off(0, 0, 0);
	/* Optimizer is stored in the driver section only */
	SQLGetPrivateProfileString(section, INI_OPTIMIZER, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->disable_optimizer = atoi(temp);
	else if (inst_position)
		comval->disable_optimizer = DEFAULT_OPTIMIZER;

	/* KSQO is stored in the driver section only */
	SQLGetPrivateProfileString(section, INI_KSQO, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->ksqo = atoi(temp);
	else if (inst_position)
		comval->ksqo = DEFAULT_KSQO;

	/* Recognize Unique Index is stored in the driver section only */
	SQLGetPrivateProfileString(section, INI_UNIQUEINDEX, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->unique_index = atoi(temp);
	else if (inst_position)
		comval->unique_index = DEFAULT_UNIQUEINDEX;


	/* Unknown Sizes is stored in the driver section only */
	SQLGetPrivateProfileString(section, INI_UNKNOWNSIZES, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->unknown_sizes = atoi(temp);
	else if (inst_position)
		comval->unknown_sizes = DEFAULT_UNKNOWNSIZES;


	/* Lie about supported functions? */
	SQLGetPrivateProfileString(section, INI_LIE, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->lie = atoi(temp);
	else if (inst_position)
		comval->lie = DEFAULT_LIE;

	/* Parse statements */
	SQLGetPrivateProfileString(section, INI_PARSE, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->parse = atoi(temp);
	else if (inst_position)
		comval->parse = DEFAULT_PARSE;

	/* SQLCancel calls SQLFreeStmt in Driver Manager */
	SQLGetPrivateProfileString(section, INI_CANCELASFREESTMT, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->cancel_as_freestmt = atoi(temp);
	else if (inst_position)
		comval->cancel_as_freestmt = DEFAULT_CANCELASFREESTMT;

	/* UseDeclareFetch is stored in the driver section only */
	SQLGetPrivateProfileString(section, INI_USEDECLAREFETCH, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->use_declarefetch = atoi(temp);
	else if (inst_position)
		comval->use_declarefetch = DEFAULT_USEDECLAREFETCH;

	/* Max Varchar Size */
	SQLGetPrivateProfileString(section, INI_MAXVARCHARSIZE, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->max_varchar_size = atoi(temp);
	else if (inst_position)
		comval->max_varchar_size = MAX_VARCHAR_SIZE;

	/* Max TextField Size */
	SQLGetPrivateProfileString(section, INI_MAXLONGVARCHARSIZE, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->max_longvarchar_size = atoi(temp);
	else if (inst_position)
		comval->max_longvarchar_size = TEXT_FIELD_SIZE;

	/* Text As LongVarchar	*/
	SQLGetPrivateProfileString(section, INI_TEXTASLONGVARCHAR, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->text_as_longvarchar = atoi(temp);
	else if (inst_position)
		comval->text_as_longvarchar = DEFAULT_TEXTASLONGVARCHAR;

	/* Unknowns As LongVarchar	*/
	SQLGetPrivateProfileString(section, INI_UNKNOWNSASLONGVARCHAR, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->unknowns_as_longvarchar = atoi(temp);
	else if (inst_position)
		comval->unknowns_as_longvarchar = DEFAULT_UNKNOWNSASLONGVARCHAR;

	/* Bools As Char */
	SQLGetPrivateProfileString(section, INI_BOOLSASCHAR, "",
							   temp, sizeof(temp), filename);
	if (temp[0])
		comval->bools_as_char = atoi(temp);
	else if (inst_position)
		comval->bools_as_char = DEFAULT_BOOLSASCHAR;

	/* Extra Systable prefixes */

	/*
	 * Use @@@ to distinguish between blank extra prefixes and no key
	 * entry
	 */
	SQLGetPrivateProfileString(section, INI_EXTRASYSTABLEPREFIXES, "@@@",
							   temp, sizeof(temp), filename);
	if (strcmp(temp, "@@@"))
		strcpy(comval->extra_systable_prefixes, temp);
	else if (inst_position)
		strcpy(comval->extra_systable_prefixes, DEFAULT_EXTRASYSTABLEPREFIXES);

	mylog("ci=%p globals.extra_systable_prefixes = '%s'\n", ci, comval->extra_systable_prefixes);


	/* Dont allow override of an override! */
	if (inst_position)
	{
		char conn_settings[LARGE_REGISTRY_LEN];

		/*
		 * ConnSettings is stored in the driver section and per datasource
		 * for override
		 */
		SQLGetPrivateProfileString(section, INI_CONNSETTINGS, "",
			conn_settings, sizeof(conn_settings), filename);
		if ('\0' != conn_settings[0])
			STR_TO_NAME(comval->conn_settings, conn_settings);

		/* Default state for future DSN's Readonly attribute */
		SQLGetPrivateProfileString(section, INI_READONLY, "",
								   temp, sizeof(temp), filename);
		if (temp[0])
			comval->onlyread = atoi(temp);
		else
			comval->onlyread = DEFAULT_READONLY;

		/*
		 * Default state for future DSN's protocol attribute This isn't a
		 * real driver option YET.	This is more intended for
		 * customization from the install.
		 */
		SQLGetPrivateProfileString(section, INI_PROTOCOL, "@@@",
								   temp, sizeof(temp), filename);
		if (strcmp(temp, "@@@"))
			strncpy_null(comval->protocol, temp, sizeof(comval->protocol));
		else
			strcpy(comval->protocol, DEFAULT_PROTOCOL);
	}
	STR_TO_NAME(comval->drivername, drivername);
}

static void
encode(const pgNAME in, UCHAR *out, int outlen)
{
	size_t i, ilen, o = 0;
	UCHAR	inc, *ins;

	if (NAME_IS_NULL(in))
	{
		out[0] = '\0';
		return;
	}
	ins = GET_NAME(in);
	ilen = strlen(ins);
	for (i = 0; i < ilen && o < outlen - 1; i++)
	{
		inc = ins[i];
		if (inc == '+')
		{
			if (o + 2 >= outlen)
				break;
			sprintf(&out[o], "%%2B");
			o += 3;
		}
		else if (isspace(inc))
			out[o++] = '+';
		else if (!isalnum(inc))
		{
			if (o + 2 >= outlen)
				break;
			sprintf(&out[o], "%%%02x", inc);
			o += 3;
		}
		else
			out[o++] = inc;
	}
	out[o++] = '\0';
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

static pgNAME
decode(const UCHAR *in)
{
	size_t i, ilen = strlen(in), o = 0;
	UCHAR	inc, *outs;
	pgNAME	out;

	INIT_NAME(out);
	if (0 == ilen)
	{
		return out;
	}
	outs = (char *) malloc(ilen + 1);
	for (i = 0; i < ilen; i++)
	{
		inc = in[i];
		if (inc == '+')
			outs[o++] = ' ';
		else if (inc == '%')
		{
			sprintf(&outs[o++], "%c", conv_from_hex(&in[i]));
			i += 2;
		}
		else
			outs[o++] = inc;
	}
	outs[o++] = '\0';
	STR_TO_NAME(out, outs);
	free(outs);
	return out;
}

/*
 *	Remove braces if the input value is enclosed by braces({}).
 *	Othewise decode the input value. 
 */
static pgNAME
decode_or_remove_braces(const UCHAR *in)
{
	if ('{' == in[0])
	{
		size_t inlen = strlen(in);
		if ('}' == in[inlen - 1]) /* enclosed by braces */
		{
			pgNAME	out;

			INIT_NAME(out);	
			STRN_TO_NAME(out, in + 1, inlen - 2);	
			return out;
		}
	}
	return decode(in);
}

char *extract_attribute_setting(const char *str, const char *attr, BOOL ref_comment)
{
	const UCHAR *cptr, *sptr = NULL;
	UCHAR	*rptr;
	BOOL	allowed_cmd = TRUE, in_quote = FALSE, in_comment = FALSE;
	int	step = 0, skiplen;
	size_t	len = 0, attrlen = strlen(attr);

        for (cptr = (UCHAR *) str; *cptr; cptr++)
        {
		if (in_quote)
		{
			if (LITERAL_QUOTE == *cptr)
			{
				if (4 == step)
				{
					len = cptr - sptr;
					step++;
				}
				in_quote = FALSE;
			}
			continue;
		}
		else if (in_comment)
		{
			if ('*' == *cptr &&
			    '/' == cptr[1])
			{
				if (4 == step)
				{
					len = cptr - sptr;
					step++;
				}
				in_comment = FALSE;
				cptr++;
				continue;
			}
			if (!ref_comment)
				continue;
		}
		else if ('/' == *cptr &&
			 '*' == cptr[1])
		{
			in_comment = TRUE;
			cptr++;
			continue;
		}
		if (';' == *cptr)
		{
			if (4 == step)
			{
				len = cptr - sptr;
			}
			allowed_cmd = TRUE;
			step = 0;
			continue;
		}
		if (!allowed_cmd)
			continue;
		if (isspace(*cptr))
		{
			if (4 == step)
			{
				len =  cptr - sptr;
				step++;
			}
			continue;
		}
		switch (step)
		{
			case 0:
				if (0 != strnicmp(cptr, "set", 3))
				{
					allowed_cmd = FALSE;
					continue;
				}
				step++;
				cptr += 3;
				break;
			case 1:
				if (0 != strnicmp(cptr, attr, attrlen))
				{
					allowed_cmd = FALSE;
					continue;
				}
				step++;
				cptr += (attrlen - 1);
				break;
			case 2:
				skiplen = 0;
				if (0 != strnicmp(cptr, "=", 1))
				{
					skiplen = (int) strlen("to");
					if (0 != strnicmp(cptr, "to", 2))
					{
						allowed_cmd = FALSE;
						continue;
					}
				}
				step++;
				cptr += skiplen;
				break;
			case 3:
				if (LITERAL_QUOTE == *cptr)
				{
					cptr++;
					sptr = cptr;
				}
				else
					sptr = cptr;
				step++;
				break;
		}
	}
	if (!sptr)
		return NULL;
	rptr = malloc(len + 1);
	memcpy(rptr, sptr, len);
	rptr[len] = '\0';
	mylog("extracted a %s '%s' from %s\n", attr, rptr, str);
	return rptr;
}

/*
 *	extract the specified attribute from the comment part.
 *		attribute=[']value[']
 */
char *extract_extra_attribute_setting(const pgNAME setting, const char *attr)
{
	const UCHAR *cptr, *sptr = NULL, *str = GET_NAME(setting);
	UCHAR	*rptr;
	BOOL	allowed_cmd = FALSE, in_quote = FALSE, in_comment = FALSE;
	int	step = 0, step_last = 2;
	size_t	len = 0, attrlen = strlen(attr);

        for (cptr = (UCHAR *) str; *cptr; cptr++)
        {
		if (in_quote)
		{
			if (LITERAL_QUOTE == *cptr)
			{
				if (step_last == step)
				{
					len = cptr - sptr;
					step = 0;
				}
				in_quote = FALSE;
			}
			continue;
		}
		else if (in_comment)
		{
			if ('*' == *cptr &&
			    '/' == cptr[1])
			{
				if (step_last == step)
				{
					len = cptr - sptr;
					step = 0;
				}
				in_comment = FALSE;
				allowed_cmd = FALSE;
				cptr++;
				continue;
			}
		}
		else if ('/' == *cptr &&
			 '*' == cptr[1])
		{
			in_comment = TRUE;
			allowed_cmd = TRUE;
			cptr++;
			continue;
		}
		else
		{
			if (LITERAL_QUOTE == *cptr)
				in_quote = TRUE;
			continue;
		}
		/* now in comment */
		if (';' == *cptr ||
		    isspace(*cptr))
		{
			if (step_last == step)
				len = cptr - sptr;
			allowed_cmd = TRUE;
			step = 0;
			continue;
		}
		if (!allowed_cmd)
			continue;
		switch (step)
		{
			case 0:
				if (0 != strnicmp(cptr, attr, attrlen))
				{
					allowed_cmd = FALSE;
					continue;
				}
				if (cptr[attrlen] != '=')
				{
					allowed_cmd = FALSE;
					continue;
				}
				step++;
				cptr += attrlen;
				break;
			case 1:
				if (LITERAL_QUOTE == *cptr)
				{
					in_quote = TRUE;
					cptr++;
					sptr = cptr;
				}
				else
					sptr = cptr;
				step++;
				break;
		}
	}
	if (!sptr)
		return NULL;
	rptr = malloc(len + 1);
	memcpy(rptr, sptr, len);
	rptr[len] = '\0';
	mylog("extracted a %s '%s' from %s\n", attr, rptr, str);
	return rptr;
}
