#ifndef	__MULTIBUYTE_H__
#define	__MULTIBUYTE_H__
/*
 *
 * Multibyte library header ( psqlODBC Only )
 *
 */
#include "psqlodbc.h"
#include "qresult.h"

/* PostgreSQL client encoding */
enum {
	SQL_ASCII 	= 0	/* SQL/ASCII */
	,EUC_JP			/* EUC for Japanese */
	,EUC_CN			/* EUC for Chinese */
	,EUC_KR			/* EUC for Korean */
	,EUC_TW			/* EUC for Taiwan */
	,JOHAB
	,UTF8			/* Unicode UTF-8 */
	,MULE_INTERNAL		/* Mule internal code */
	,LATIN1			/* ISO-8859 Latin 1 */
	,LATIN2			/* ISO-8859 Latin 2 */
	,LATIN3			/* ISO-8859 Latin 3 */
	,LATIN4			/* ISO-8859 Latin 4 */
	,LATIN5			/* ISO-8859 Latin 5 */
	,LATIN6			/* ISO-8859 Latin 6 */
	,LATIN7			/* ISO-8859 Latin 7 */
	,LATIN8			/* ISO-8859 Latin 8 */
	,LATIN9			/* ISO-8859 Latin 9 */
	,LATIN10		/* ISO-8859 Latin 10 */
	,WIN1256		/* Arabic Windows */
	,WIN1258		/* Vietnamese Windows */
	,WIN866			/* Alternativny Variant (MS-DOS CP866) */
	,WIN874			/* Thai Windows */
	,KOI8R			/* KOI8-R/U */
	,WIN1251		/* Cyrillic Windows */
	,WIN1252		/* Western Europe Windows */
	,ISO_8859_5		/* ISO-8859-5 */
	,ISO_8859_6		/* ISO-8859-6 */
	,ISO_8859_7		/* ISO-8859-7 */
	,ISO_8859_8		/* ISO-8859-8 */
	,WIN1250		/* Central Europe Windows */
	,WIN1253		/* Greek Windows */
	,WIN1254		/* Turkish Windows */
	,WIN1255		/* Hebrew Windows */
	,WIN1257		/* Baltic(North Europe) Windows */
	,EUC_JIS_2004		/* EUC for SHIFT-JIS-2004 Japanese */
	,SJIS			/* Shift JIS */
	,BIG5			/* Big5 */
	,GBK			/* GBK */
	,UHC			/* UHC */
	,GB18030		/* GB18030 */
	,SHIFT_JIS_2004		/* SHIFT-JIS-2004 Japanese, JIS X 0213 */
	,OTHER		=	-1
};

#define MAX_CHARACTERSET_NAME	24
#define MAX_CHARACTER_LEN	6

/* OLD Type */
// extern int	multibyte_client_encoding;	/* Multibyte client encoding. */
// extern int	multibyte_status;	/* Multibyte charcter status. */
//
// void		multibyte_init(void);
// unsigned char *check_client_encoding(unsigned char *sql_string);
// int			multibyte_char_check(unsigned char s);
// unsigned char *multibyte_strchr(const unsigned char *string, unsigned int c);

/* New Type */

extern void CC_lookup_characterset(ConnectionClass *self);
extern const char *get_environment_encoding(const ConnectionClass *conn, const char *setenc, const char *svrenc, BOOL bStartup);

extern int pg_CS_stat(int stat,unsigned int charcter,int characterset_code);
extern int pg_CS_code(const UCHAR *stat_string);
extern const UCHAR *pg_CS_name(int code);

typedef struct pg_CS
{
	UCHAR *name;
	int code;
}pg_CS;
extern size_t	pg_mbslen(int ccsc, const UCHAR *string);
extern UCHAR *pg_mbschr(int ccsc, const UCHAR *string, unsigned int character);
extern UCHAR *pg_mbsinc(int ccsc, const UCHAR *current );

/* Old Type Compatible */
typedef struct
{
	int	ccsc;
	const UCHAR *encstr;
	ssize_t	pos;
	int	ccst;
} encoded_str;
#define ENCODE_STATUS(enc)	((enc).ccst)

void encoded_str_constr(encoded_str *encstr, int ccsc, const char *str);
#define make_encoded_str(encstr, conn, str) encoded_str_constr(encstr, conn->ccsc, str)
extern int encoded_nextchar(encoded_str *encstr);
extern ssize_t encoded_position_shift(encoded_str *encstr, size_t shift);
extern int encoded_byte_check(encoded_str *encstr, size_t abspos);
/* #define check_client_encoding(X) pg_CS_name(pg_CS_code(X))
UCHAR *check_client_encoding(const UCHAR *sql_string); */
UCHAR *check_client_encoding(const pgNAME sql_string);
#endif /* __MULTIBUYTE_H__ */
