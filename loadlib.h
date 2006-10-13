/* File:			loadlib.h
 *
 * Description:		See "loadlib.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __LOADLIB_H__
#define __LOADLIB_H__

#include "psqlodbc.h"

#include <stdlib.h>
#ifdef  __cplusplus
extern "C" {
#endif
BOOL	LIBPQ_check(void);
void	*CALL_PQconnectdb(const char *conninfo, BOOL *);
/* void	UnloadDelayLoadedDLLs(BOOL); */
void	CleanupDelayLoadedDLLs(void);

#ifdef	__cplusplus
}
#endif
#endif /* __LOADLIB_H__ */

