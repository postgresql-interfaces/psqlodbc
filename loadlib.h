/* File:			loadlib.h
 *
 * Description:		See "loadlib.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __LOADLIB_H__
#define __LOADLIB_H__

#include "connection.h"
#ifndef	DYNAMIC_LOAD
#include <libpq-fe.h>
#include <openssl/ssl.h>
#endif /* DYNAMIC_LOAD */

#include <stdlib.h>
#ifdef  __cplusplus
extern "C" {
#endif
BOOL	LIBPQ_check(void);

#ifdef	__cplusplus
}
#endif
#endif /* __CONNECTION_H__ */

