/* File:			misc.h
 *
 * Description:		See "misc.c"
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __MISC_H__
#define __MISC_H__

#include <stdio.h>
#ifndef  WIN32
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void		remove_newlines(char *string);
char	   *strncpy_null(char *dst, const char *src, ssize_t len);
#ifndef	HAVE_STRLCPY
size_t		strlcat(char *, const char *, size_t);
#endif /* HAVE_STRLCPY */
char	   *my_trim(char *string);
char	   *make_string(const char *s, ssize_t len, char *buf, size_t bufsize);
SQLCHAR	*make_lstring_ifneeded(ConnectionClass *, const SQLCHAR *s, ssize_t len, BOOL);
char	   *schema_strcat(char *buf, const char *fmt, const char *s, ssize_t len,
		const char *, int, ConnectionClass *conn);
char	   *schema_strcat1(char *buf, const char *fmt, const char *s1,
				const char *s, ssize_t len,
				const char *, int, ConnectionClass *conn);
int	   snprintf_add(char *buf, size_t size, const char *format, ...);
size_t	   snprintf_len(char *buf, size_t size, const char *format, ...);
/* #define	GET_SCHEMA_NAME(nspname) 	(stricmp(nspname, "public") ? nspname : "") */
char *quote_table(const pgNAME schema, pgNAME table);

#define	GET_SCHEMA_NAME(nspname) 	(nspname)

/* defines for return value of my_strcpy */
#define STRCPY_SUCCESS		1
#define STRCPY_FAIL			0
#define STRCPY_TRUNCATED	(-1)
#define STRCPY_NULL			(-2)

ssize_t			my_strcpy(char *dst, ssize_t dst_len, const char *src, ssize_t src_len);

#ifdef __cplusplus
}
#endif

#endif /* __MISC_H__ */
