/* File:			socket.h
 *
 * Description:		See "socket.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "psqlodbc.h"
#include <errno.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define closesocket(xxx) close(xxx)
#define SOCKETFD int

#ifndef		  INADDR_NONE
#ifndef _IN_ADDR_T
#define _IN_ADDR_T
typedef unsigned int in_addr_t;
#endif /* _IN_ADDR_T */
#define INADDR_NONE ((in_addr_t)-1)
#endif /* _IN_ADDR_NONE */

#define SOCK_ERRNO	errno
#define SOCK_ERRNO_SET(e)	(errno = e)
#if defined HAVE_SYS_UN_H && !defined HAVE_UNIX_SOCKETS
#define HAVE_UNIX_SOCKETS
#endif /* HAVE_SYS_UN_H */
#else
#include <winsock.h>
#define SOCKETFD SOCKET
#define SOCK_ERRNO	(WSAGetLastError())
#define SOCK_ERRNO_SET(e)	WSASetLastError(e)
#endif /* WIN32 */

#define SOCKET_ALREADY_CONNECTED			1
#define SOCKET_HOST_NOT_FOUND				2
#define SOCKET_COULD_NOT_CREATE_SOCKET		3
#define SOCKET_COULD_NOT_CONNECT			4
#define SOCKET_READ_ERROR					5
#define SOCKET_WRITE_ERROR					6
#define SOCKET_NULLPOINTER_PARAMETER		7
#define SOCKET_PUT_INT_WRONG_LENGTH			8
#define SOCKET_GET_INT_WRONG_LENGTH			9
#define SOCKET_CLOSED						10


struct SocketClass_
{

	int			buffer_size;
	int			buffer_filled_in;
	int			buffer_filled_out;
	int			buffer_read_in;
	UCHAR *buffer_in;
	UCHAR *buffer_out;

	SOCKETFD	socket;

	char	   *errormsg;
	int			errornumber;
	struct sockaddr	*sadr; /* Used for handling connections for cancel */
	int		sadr_len;
	struct sockaddr_in sadr_in; /* Used for INET connections */

	char		reverse;		/* used to handle Postgres 6.2 protocol
								 * (reverse byte order) */

};

#define SOCK_get_char(self)		(SOCK_get_next_byte(self))
#define SOCK_put_char(self, c)	(SOCK_put_next_byte(self, c))


/* error functions */
#define SOCK_get_errcode(self)	(self ? self->errornumber : SOCKET_CLOSED)
#define SOCK_get_errmsg(self)	(self ? self->errormsg : "socket closed")

/*
 *	code taken from postgres libpq et al.
 */
#ifndef WIN32
#define DEFAULT_PGSOCKET_DIR	"/tmp"
#define UNIXSOCK_PATH(sun, port, defpath) \
	snprintf((sun)->sun_path, sizeof((sun)->sun_path), "%s/.s.PGSQL.%d", \
		((defpath) && *(defpath) != '\0') ? (defpath) : \
			DEFAULT_PGSOCKET_DIR, \
			(port))

/*
 *	We do this because sun_len is in BSD's struct, while others don't.
 *	We never actually set BSD's sun_len, and I can't think of a
 *	platform-safe way of doing it, but the code still works. bjm
 */
#ifndef	offsetof
#define offsetof(type, field)	((long) &((type *)0)->field)
#endif /* offsetof */
#if defined(SUN_LEN)
#define UNIXSOCK_LEN(sun) SUN_LEN(sun)
#else
#define UNIXSOCK_LEN(sun) \
	(strlen((sun)->sun_path) + offsetof(struct sockaddr_un, sun_path))
#endif /* SUN_LEN */
#endif /* WIN32 */
/*
 *	END code taken from postgres libpq et al.
 */


/* Socket prototypes */
SocketClass *SOCK_Constructor(const ConnectionClass *conn);
void		SOCK_Destructor(SocketClass *self);
char		SOCK_connect_to(SocketClass *self, unsigned short port,
				char *hostname
#ifdef HAVE_UNIX_SOCKETS
				, char *uds
#endif
				);
void		SOCK_get_n_char(SocketClass *self, char *buffer, int len);
void		SOCK_put_n_char(SocketClass *self, char *buffer, int len);
BOOL		SOCK_get_string(SocketClass *self, char *buffer, int bufsize);
void		SOCK_put_string(SocketClass *self, char *string);
int			SOCK_get_int(SocketClass *self, short len);
void		SOCK_put_int(SocketClass *self, int value, short len);
void		SOCK_flush_output(SocketClass *self);
UCHAR		SOCK_get_next_byte(SocketClass *self);
void		SOCK_put_next_byte(SocketClass *self, UCHAR next_byte);
void		SOCK_clear_error(SocketClass *self);

#endif /* __SOCKET_H__ */
