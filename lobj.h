/* File:			lobj.h
 *
 * Description:		See "lobj.c"
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __LOBJ_H__
#define __LOBJ_H__


#include "psqlodbc.h"

struct lo_arg
{
	int			isint;
	int			len;
	union
	{
		int			integer;
		char	   *ptr;
		Int8	integer64;
	}			u;
};

#define INV_WRITE					0x00020000
#define INV_READ					0x00040000

OID		odbc_lo_creat(ConnectionClass *conn, int mode);
int		odbc_lo_open(ConnectionClass *conn, int lobjId, int mode);
int		odbc_lo_close(ConnectionClass *conn, int fd);
Int4		odbc_lo_read(ConnectionClass *conn, int fd, char *buf, Int4 len);
Int4		odbc_lo_write(ConnectionClass *conn, int fd, char *buf, Int4 len);
Int4		odbc_lo_lseek(ConnectionClass *conn, int fd, int offset, Int4 len);
Int4		odbc_lo_tell(ConnectionClass *conn, int fd);

Int8		odbc_lo_lseek64(ConnectionClass *conn, int fd, Int8 offset, Int4 len);
Int8		odbc_lo_tell64(ConnectionClass *conn, int fd);
#endif
