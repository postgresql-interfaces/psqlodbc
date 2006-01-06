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

#include <stdlib.h>
#ifdef  __cplusplus
extern "C" {
#endif
#ifdef DYNAMIC_LINK
HMODULE	LIBPQ_load(BOOL checkOnly);
HMODULE GetOpenssl(PQFUNC funcs[], int *count);
#endif
#ifdef	__cplusplus
}
#endif
#endif /* __CONNECTION_H__ */

