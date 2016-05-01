/* File:			dlg_specific.h
 *
 * Description:		See "dlg_specific.c"
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __DLG_SPECIFIC_H__
#define __DLG_SPECIFIC_H__

#include "psqlodbc.h"

#ifdef WIN32
#include  <windowsx.h>
#include "resource.h"
#endif

#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */
/*	Unknown data type sizes */
#define UNKNOWNS_AS_MAX				0
#define UNKNOWNS_AS_DONTKNOW			1
#define UNKNOWNS_AS_LONGEST			2

/* ODBC initialization files */
#ifndef WIN32
#define ODBC_INI				".odbc.ini"
#define ODBCINST_INI				"odbcinst.ini"
#else
#define ODBC_INI				"ODBC.INI"
#define ODBCINST_INI				"ODBCINST.INI"
#endif

#define	ODBC_DATASOURCES	"ODBC Data Sources"
#define	INVALID_DRIVER		" @@driver not exist@@ "

#ifdef  UNICODE_SUPPORT
#define INI_DSN				"PostgreSQL35W"
#else
#define INI_DSN				"PostgreSQL30"
#endif /* UNICODE_SUPPORT */

#define INI_KDESC			"Description"	/* Data source
							 * description */
#define INI_SERVER			"Servername"	/* Name of Server
							 * running the Postgres
							 * service */
#define SPEC_SERVER			"server"
#define INI_PORT			"Port"	/* Port on which the
						 * Postmaster is listening */
#define INI_DATABASE			"Database"	/* Database Name */
#define INI_UID				"UID"		/* Default User Name */
#define INI_USERNAME			"Username"	/* Default User Name */
#define INI_PASSWORD			"Password"	/* Default Password */

#define	INI_ABBREVIATE			"CX"
#define INI_DEBUG			"Debug"		/* Debug flag */
#define ABBR_DEBUG			"B2"
#define INI_FETCH			"Fetch"		/* Fetch Max Count */
#define ABBR_FETCH			"A7"
/*
 * "Socket", abbreviated as "A8" was used for socket buffer. Now that we do
 * everything through libpq, it's not used
 */
#define INI_READONLY			"ReadOnly"	/* Database is read only */
#define ABBR_READONLY			"A0"
#define INI_COMMLOG			"CommLog"	/* Communication to
							 * backend logging */
#define ABBR_COMMLOG			"B3"
#define INI_PROTOCOL			"Protocol"	/* Controls rollback-on-error
											 * behavior. Called "Protocol"
											 * for historical reasons */
#define ABBR_PROTOCOL			"A1"
/*	"Optimizer", abbreviated to B4 used to stand for "disable genetic query
 * optimizer". No longer supported, you can use generic ConnSettings instead.
#define INI_OPTIMIZER			"Optimizer"
#define ABBR_OPTIMIZER			"B4"
*/
/* "Ksqo", abbreviated to B5 was used with pre-7.1 server versions for
 * "keyset query optimization". No longer used.
#define INI_KSQO                       "Ksqo"
#define ABBR_KSQO                      "B5"
*/
#define INI_CONNSETTINGS		 "ConnSettings" /* Anything to send to
							 * backend on successful
							 * connection */
#define ABBR_CONNSETTINGS	 "A6"
#define INI_UNIQUEINDEX			"UniqueIndex"	/* Recognize unique
							 * indexes */
#define INI_UNKNOWNSIZES		"UnknownSizes"	/* How to handle unknown
							 * result set sizes */
#define ABBR_UNKNOWNSIZES		"A9"

/* "CancelAsFreeStmt", abbreviated to "C1" was used with ODBC versions older
 * than 3.51. It was a hack that made SQLCancel to imply
 * SQLFreeStmt(SQL_CLOSE). It never had an effect in > 3.51 mode.
#define INI_CANCELASFREESTMT		"CancelAsFreeStmt"
#define ABBR_CANCELASFREESTMT	"C1"
*/
#define INI_USEDECLAREFETCH		"UseDeclareFetch"	/* Use Declare/Fetch
								 * cursors */
#define ABBR_USEDECLAREFETCH		"B6"

/*	More ini stuff */
#define INI_TEXTASLONGVARCHAR		"TextAsLongVarchar"
#define ABBR_TEXTASLONGVARCHAR		"B7"
#define INI_UNKNOWNSASLONGVARCHAR	"UnknownsAsLongVarchar"
#define ABBR_UNKNOWNSASLONGVARCHAR	"B8"
#define INI_BOOLSASCHAR			"BoolsAsChar"
#define ABBR_BOOLSASCHAR		"B9"
#define INI_MAXVARCHARSIZE		"MaxVarcharSize"
#define ABBR_MAXVARCHARSIZE		"B0"
#define INI_MAXLONGVARCHARSIZE		"MaxLongVarcharSize"
#define ABBR_MAXLONGVARCHARSIZE		"B1"

#define INI_FAKEOIDINDEX		"FakeOidIndex"
#define ABBR_FAKEOIDINDEX		"A2"
#define INI_SHOWOIDCOLUMN		"ShowOidColumn"
#define ABBR_SHOWOIDCOLUMN		"A3"
#define INI_ROWVERSIONING		"RowVersioning"
#define ABBR_ROWVERSIONING		"A4"
#define INI_SHOWSYSTEMTABLES		"ShowSystemTables"
#define ABBR_SHOWSYSTEMTABLES		"A5"
#define INI_LIE				"Lie"
#define INI_PARSE			"Parse"
#define ABBR_PARSE			"C0"
#define INI_EXTRASYSTABLEPREFIXES	"ExtraSysTablePrefixes"
#define ABBR_EXTRASYSTABLEPREFIXES	"C2"

#define INI_TRANSLATIONNAME		"TranslationName"
#define INI_TRANSLATIONDLL		"TranslationDLL"
#define INI_TRANSLATIONOPTION		"TranslationOption"
/*
 * "DisallowPremature", abbreviated "C3", used to mean that we should not
 * execute a statement prematurely, before SQLExecute() when e.g.
 * SQLPrepare+SQLDescribeCol is called. We never do that anymore.
 *
#define INI_DISALLOWPREMATURE          "DisallowPremature"
#define ABBR_DISALLOWPREMATURE         "C3"
*/
#define INI_UPDATABLECURSORS		"UpdatableCursors"
#define ABBR_UPDATABLECURSORS		"C4"
#define INI_LFCONVERSION		"LFConversion"
#define ABBR_LFCONVERSION		"C5"
#define INI_TRUEISMINUS1		"TrueIsMinus1"
#define ABBR_TRUEISMINUS1		"C6"
#define INI_INT8AS			"BI"
#define INI_BYTEAASLONGVARBINARY	"ByteaAsLongVarBinary"
#define ABBR_BYTEAASLONGVARBINARY	"C7"
#define INI_USESERVERSIDEPREPARE	"UseServerSidePrepare"
#define ABBR_USESERVERSIDEPREPARE	"C8"
#define INI_LOWERCASEIDENTIFIER		"LowerCaseIdentifier"
#define ABBR_LOWERCASEIDENTIFIER	"C9"
#define INI_SSLMODE			"SSLmode"
#define ABBR_SSLMODE			"CA"
#define INI_EXTRAOPTIONS		"AB"
#define INI_LOGDIR			"Logdir"
#define INI_GSSAUTHUSEGSSAPI		"GssAuthUseGSS"
#define ABBR_GSSAUTHUSEGSSAPI		"D0"
#define INI_KEEPALIVETIME		"KeepaliveTime"
#define ABBR_KEEPALIVETIME		"D1"
#define INI_KEEPALIVEINTERVAL		"KeepaliveInterval"
#define ABBR_KEEPALIVEINTERVAL		"D2"
#define INI_PQOPT			"pqopt"
#define ABBR_PQOPT			"D5"
#define INI_DTCLOG			"Dtclog"
/* "PreferLibpq", abbreviated "D4", used to mean whether to prefer libpq.
 * libpq is now required
#define INI_PREFERLIBPQ			"PreferLibpq"
#define ABBR_PREFERLIBPQ		"D3"
*/
#define ABBR_XAOPT			"D4"

#define	SSLMODE_DISABLE		"disable"
#define	SSLMODE_ALLOW		"allow"
#define	SSLMODE_PREFER		"prefer"
#define	SSLMODE_REQUIRE		"require"
#define	SSLMODE_VERIFY_CA	"verify-ca"
#define	SSLMODE_VERIFY_FULL	"verify-full"
#define	SSLLBYTE_DISABLE	'd'
#define	SSLLBYTE_ALLOW		'a'
#define	SSLLBYTE_PREFER		'p'
#define	SSLLBYTE_REQUIRE	'r'
#define	SSLLBYTE_VERIFY		'v'

#ifdef	_HANDLE_ENLIST_IN_DTC_
#define INI_XAOPT			"XaOpt"
#endif /* _HANDLE_ENLIST_IN_DTC_ */
/* Bit representation for abbreviated connection strings */
#define BIT_LFCONVERSION			(1L)
#define BIT_UPDATABLECURSORS			(1L<<1)
/* #define BIT_DISALLOWPREMATURE                  (1L<<2) */
#define BIT_UNIQUEINDEX				(1L<<3)
#define BIT_UNKNOWN_DONTKNOW			(1L<<6)
#define BIT_UNKNOWN_ASMAX			(1L<<7)
#define BIT_COMMLOG				(1L<<10)
#define BIT_DEBUG				(1L<<11)
#define BIT_PARSE				(1L<<12)
#define BIT_CANCELASFREESTMT			(1L<<13)
#define BIT_USEDECLAREFETCH			(1L<<14)
#define BIT_READONLY				(1L<<15)
#define BIT_TEXTASLONGVARCHAR			(1L<<16)
#define BIT_UNKNOWNSASLONGVARCHAR		(1L<<17)
#define BIT_BOOLSASCHAR				(1L<<18)
#define BIT_ROWVERSIONING			(1L<<19)
#define BIT_SHOWSYSTEMTABLES			(1L<<20)
#define BIT_SHOWOIDCOLUMN			(1L<<21)
#define BIT_FAKEOIDINDEX			(1L<<22)
#define BIT_TRUEISMINUS1			(1L<<23)
#define BIT_BYTEAASLONGVARBINARY		(1L<<24)
#define BIT_USESERVERSIDEPREPARE		(1L<<25)
#define BIT_LOWERCASEIDENTIFIER			(1L<<26)
#define BIT_GSSAUTHUSEGSSAPI			(1L<<27)

#define EFFECTIVE_BIT_COUNT			28

/*	Mask for extra options	*/
#define	BIT_FORCEABBREVCONNSTR			1L
#define	BIT_FAKE_MSS				(1L << 1)
#define	BIT_BDE_ENVIRONMENT			(1L << 2)
#define	BIT_CVT_NULL_DATE			(1L << 3)
#define	BIT_ACCESSIBLE_ONLY			(1L << 4)
#define	BIT_IGNORE_ROUND_TRIP_TIME		(1L << 5)
#define	BIT_DISABLE_KEEPALIVE			(1L << 6)

/*	Connection Defaults */
#define DEFAULT_READONLY			0
#define DEFAULT_PROTOCOL			"7.4"	/* the latest protocol is
												 * the default */
#define DEFAULT_USEDECLAREFETCH			0
#define DEFAULT_TEXTASLONGVARCHAR		1
#define DEFAULT_UNKNOWNSASLONGVARCHAR		0
#define DEFAULT_BOOLSASCHAR			1
#define DEFAULT_UNIQUEINDEX			1		/* dont recognize */
#define DEFAULT_COMMLOG				0		/* dont log */
#define DEFAULT_DEBUG				0
#define DEFAULT_UNKNOWNSIZES			UNKNOWNS_AS_MAX


#define DEFAULT_FAKEOIDINDEX			0
#define DEFAULT_SHOWOIDCOLUMN			0
#define DEFAULT_ROWVERSIONING			0
#define DEFAULT_SHOWSYSTEMTABLES		0		/* dont show system tables */
#define DEFAULT_LIE				0
#define DEFAULT_PARSE				0

#define DEFAULT_CANCELASFREESTMT		0

#define DEFAULT_EXTRASYSTABLEPREFIXES	""

#define DEFAULT_TRUEISMINUS1		0
#define DEFAULT_UPDATABLECURSORS	1
#ifdef	WIN32
#define DEFAULT_LFCONVERSION		1
#else
#define DEFAULT_LFCONVERSION		0
#endif	/* WIN32 */
#define DEFAULT_INT8AS			0
#define DEFAULT_BYTEAASLONGVARBINARY	1
#define DEFAULT_USESERVERSIDEPREPARE	1
#define DEFAULT_LOWERCASEIDENTIFIER	0
#define DEFAULT_SSLMODE			SSLMODE_DISABLE
#define DEFAULT_GSSAUTHUSEGSSAPI	0

#ifdef	_HANDLE_ENLIST_IN_DTC_
#define DEFAULT_XAOPT			1
#endif /* _HANDLE_ENLIST_IN_DTC_ */

/*	for CC_DSN_info */
#define CONN_DONT_OVERWRITE		0
#define CONN_OVERWRITE			1

/*	prototypes */

#ifdef WIN32
void		SetDlgStuff(HWND hdlg, const ConnInfo *ci);
void		GetDlgStuff(HWND hdlg, ConnInfo *ci);

LRESULT CALLBACK driver_optionsProc(HWND hdlg,
				   UINT wMsg,
				   WPARAM wParam,
				   LPARAM lParam);
LRESULT CALLBACK global_optionsProc(HWND hdlg,
				   UINT wMsg,
				   WPARAM wParam,
				   LPARAM lParam);
LRESULT CALLBACK ds_options1Proc(HWND hdlg,
			   UINT wMsg,
			   WPARAM wParam,
			   LPARAM lParam);
LRESULT CALLBACK ds_options2Proc(HWND hdlg,
			   UINT wMsg,
			   WPARAM wParam,
			   LPARAM lParam);
LRESULT CALLBACK ds_options3Proc(HWND hdlg,
			   UINT wMsg,
			   WPARAM wParam,
			   LPARAM lParam);
LRESULT CALLBACK manage_dsnProc(HWND hdlg,
			   UINT wMsg,
			   WPARAM wParam,
			   LPARAM lParam);
#endif   /* WIN32 */

int		write_Ci_Drivers(const char *fileName, const char *sectionName,
		const GLOBAL_VALUES *);
int		writeDriversDefaults(const char *drivername, const GLOBAL_VALUES *);
void		writeDSNinfo(const ConnInfo *ci);
void		getDriversDefaults(const char *drivername, GLOBAL_VALUES *);
void		getDSNinfo(ConnInfo *ci, const char *configDrvrname);
void		makeConnectString(char *connect_string, const ConnInfo *ci, UWORD);
BOOL		get_DSN_or_Driver(ConnInfo *ci, const char *attribute, const char *value);
BOOL		copyConnAttributes(ConnInfo *ci, const char *attribute, const char *value);
int	getDriverNameFromDSN(const char *dsn, char *driver_name, int namelen);
UInt4	getExtraOptions(const ConnInfo *);
BOOL	setExtraOptions(ConnInfo *, const char *str, const char *format);
char	*extract_extra_attribute_setting(const pgNAME setting, const char *attr);
signed char	ci_updatable_cursors_set(ConnInfo *ci);

#ifdef	__cplusplus
}
#endif /* __cplusplus */
#endif /* __DLG_SPECIFIC_H__ */
