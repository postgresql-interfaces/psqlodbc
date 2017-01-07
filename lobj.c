/*--------
 * Module:			lobj.c
 *
 * Description:		This module contains routines related to manipulating
 *					large objects.
 *
 * Classes:			none
 *
 * API functions:	none
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *--------
 */

#include "lobj.h"

#include "connection.h"


OID
odbc_lo_creat(ConnectionClass *conn, int mode)
{
	LO_ARG		argv[1];
	Int4		retval, result_len;

	argv[0].isint = 1;
	argv[0].len = 4;
	argv[0].u.integer = mode;

	if (!CC_send_function(conn, "lo_creat", &retval, &result_len, 1, argv, 1))
		return 0;				/* invalid oid */
	else
		return (OID) retval;
}


int
odbc_lo_open(ConnectionClass *conn, int lobjId, int mode)
{
	int			fd;
	int			result_len;
	LO_ARG		argv[2];

	argv[0].isint = 1;
	argv[0].len = 4;
	argv[0].u.integer = lobjId;

	argv[1].isint = 1;
	argv[1].len = 4;
	argv[1].u.integer = mode;

	if (!CC_send_function(conn, "lo_open", &fd, &result_len, 1, argv, 2))
		return -1;

	if (fd >= 0 && odbc_lo_lseek64(conn, fd, 0L, SEEK_SET) < 0)
		return -1;

	return fd;
}


int
odbc_lo_close(ConnectionClass *conn, int fd)
{
	LO_ARG		argv[1];
	int			retval,
				result_len;

	argv[0].isint = 1;
	argv[0].len = 4;
	argv[0].u.integer = fd;

	if (!CC_send_function(conn, "lo_close", &retval, &result_len, 1, argv, 1))
		return -1;
	else
		return retval;
}


Int4
odbc_lo_read(ConnectionClass *conn, int fd, char *buf, Int4 len)
{
	LO_ARG		argv[2];
	Int4		result_len;

	argv[0].isint = 1;
	argv[0].len = 4;
	argv[0].u.integer = fd;

	argv[1].isint = 1;
	argv[1].len = 4;
	argv[1].u.integer = len;

	if (!CC_send_function(conn, "loread", (int *) buf, &result_len, 0, argv, 2))
		return -1;
	else
		return result_len;
}


Int4
odbc_lo_write(ConnectionClass *conn, int fd, char *buf, Int4 len)
{
	LO_ARG		argv[2];
	Int4		retval,
				result_len;

	if (len <= 0)
		return 0;

	argv[0].isint = 1;
	argv[0].len = 4;
	argv[0].u.integer = fd;

	argv[1].isint = 0;
	argv[1].len = len;
	argv[1].u.ptr = (char *) buf;

	if (!CC_send_function(conn, "lowrite", &retval, &result_len, 1, argv, 2))
		return -1;
	else
		return retval;
}


Int4
odbc_lo_lseek(ConnectionClass *conn, int fd, int offset, Int4 whence)
{
	LO_ARG		argv[3];
	Int4		retval,
				result_len;

	argv[0].isint = 1;
	argv[0].len = 4;
	argv[0].u.integer = fd;

	argv[1].isint = 1;
	argv[1].len = 4;
	argv[1].u.integer = offset;

	argv[2].isint = 1;
	argv[2].len = 4;
	argv[2].u.integer = whence;

	/* We use lo_lseek64 */
	if (!CC_send_function(conn, "lo_lseek", &retval, &result_len, 1, argv, 3))
		return -1;
	else
		return retval;
}


Int4
odbc_lo_tell(ConnectionClass *conn, int fd)
{
	LO_ARG		argv[1];
	Int4		retval, result_len;

	argv[0].isint = 1;
	argv[0].len = 4;
	argv[0].u.integer = fd;

	/* We use lo_tell64 */
	if (!CC_send_function(conn, "lo_tell", &retval, &result_len, 1, argv, 1))
		return -1;
	else
		return retval;
}

Int8
odbc_lo_lseek64(ConnectionClass *conn, int fd, Int8 offset, Int4 whence)
{
	LO_ARG		argv[3];
	Int8		retval;
	Int4		result_len;

	if (PG_VERSION_LT(conn, 9.3))
	{
		Int4	offset32;

		offset32 = (Int4) offset;
		if (offset != (Int8) offset32)
		{
			CC_set_error(conn, CONN_VALUE_OUT_OF_RANGE, "large object lseek64 is unavailable for the server", __FUNCTION__);
			return -1;
		}
		return (Int8) odbc_lo_lseek(conn, fd, offset32, whence);
	}

	argv[0].isint = 1;
	argv[0].len = 4;
	argv[0].u.integer = fd;

	argv[1].isint = 2;
	argv[1].len = sizeof(offset);
	argv[1].u.integer64 = offset;

	argv[2].isint = 1;
	argv[2].len = 4;
	argv[2].u.integer = whence;

	if (!CC_send_function(conn, "lo_lseek64", &retval, &result_len, 2, argv, 3))
		return -1;
	else
		return retval;
}


Int8
odbc_lo_tell64(ConnectionClass *conn, int fd)
{
	LO_ARG		argv[1];
	Int8		retval;
	Int4		result_len;

	if (PG_VERSION_LT(conn, 9.3))
		return (Int8) odbc_lo_tell(conn, fd);

	argv[0].isint = 1;
	argv[0].len = 4;
	argv[0].u.integer = fd;

	if (!CC_send_function(conn, "lo_tell64", &retval, &result_len, 2, argv, 1))
		return -1;
	else
		return retval;
}
