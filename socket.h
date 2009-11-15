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
#define	WSAAPI
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
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
#ifdef	HAVE_SYS_UN_H
#define HAVE_UNIX_SOCKETS
#endif /* HAVE_SYS_UN_H */
#else

#include <winsock2.h>
#include <ws2tcpip.h>

#if defined(_MSC_VER) && (_MSC_VER < 1300)
/*
 *      The order of the structure elements on Win32 doesn't match the
 *      order specified in the standard, but we have to match it for
 *      IPv6 to work.
 */

#define AI_PASSIVE     0x1  // Socket address will be used in bind() call.
#define AI_CANONNAME   0x2  // Return canonical name in first ai_canonname.
#define AI_NUMERICHOST 0x4  // Nodename must be a numeric address string.

#define NI_NUMERICHOST	1

#define _SS_MAXSIZE 128
#define _SS_ALIGNSIZE (sizeof(__int64))
#define _SS_PAD1SIZE (_SS_ALIGNSIZE - sizeof (short))
#define _SS_PAD2SIZE (_SS_MAXSIZE - (sizeof (short) + _SS_PAD1SIZE + _SS_ALIGNSIZE))

typedef int socklen_t;

struct sockaddr_storage {
	short ss_family;
	char __ss_pad1[_SS_PAD1SIZE];
	__int64 __ss_align;
	char __ss_pad2[_SS_PAD2SIZE];
};
struct addrinfo
{
	int		ai_flags;
	int		ai_family;
	int		ai_socktype;
	int		ai_protocol;
	size_t	ai_addrlen;
	char	*ai_canonname;
	struct sockaddr *ai_addr;
	struct addrinfo *ai_next;
};
#endif /* _MSC_VER */

#define SOCKETFD SOCKET
#define SOCK_ERRNO		(WSAGetLastError())
#define SOCK_ERRNO_SET(e)	WSASetLastError(e)
#ifndef	EWOULDBLOCK
#define	EWOULDBLOCK	WSAEWOULDBLOCK
#endif /* EWOULDBLOCK */
#ifndef	ECONNRESET
#define	ECONNRESET	WSAECONNRESET
#endif /* ECONNRESET */
#ifndef	EINPROGRESS
#define	EINPROGRESS	WSAEINPROGRESS
#endif /* EINPROGRESS */
#endif /* WIN32 */
typedef void (WSAAPI *freeaddrinfo_func) (struct addrinfo *); 
typedef int (WSAAPI *getaddrinfo_func) (const char *, const char *,
#ifndef	__CYGWIN__
	const
#endif
	struct addrinfo *, struct addrinfo **); 
typedef int (WSAAPI *getnameinfo_func) (const struct sockaddr *,
	socklen_t, char *, size_t, char *, size_t, int);

#ifdef	MSG_NOSIGNAL
#define	SEND_FLAG MSG_NOSIGNAL
#define	RECV_FLAG MSG_NOSIGNAL
#elif	defined(MSG_NOSIGPIPE)
#define	SEND_FLAG MSG_NOSIGPIPE
#define	RECV_FLAG MSG_NOSIGPIPE
#else
#define	SEND_FLAG 0
#define	RECV_FLAG 0
#endif /* MSG_NOSIGNAL */

#define SOCKET_ALREADY_CONNECTED		1
#define SOCKET_HOST_NOT_FOUND			2
#define SOCKET_COULD_NOT_CREATE_SOCKET		3
#define SOCKET_COULD_NOT_CONNECT		4
#define SOCKET_READ_ERROR			5
#define SOCKET_WRITE_ERROR			6
#define SOCKET_NULLPOINTER_PARAMETER		7
#define SOCKET_PUT_INT_WRONG_LENGTH		8
#define SOCKET_GET_INT_WRONG_LENGTH		9
#define SOCKET_CLOSED				10
#define SOCKET_READ_TIMEOUT			11
#define SOCKET_WRITE_TIMEOUT			12


struct SocketClass_
{

	int			buffer_size;
	int			buffer_filled_in;
	int			buffer_filled_out;
	int			buffer_read_in;
	UCHAR *buffer_in;
	UCHAR *buffer_out;

	SOCKETFD	socket;
	unsigned int	pversion;
	int		reslen;

	char		*_errormsg_;
	int		errornumber;
	int		sadr_len;
	struct sockaddr_storage sadr_area; /* Used for various connections */
#ifdef	USE_SSPI
	UInt4		sspisvcs;
	void		*ssd;
#endif /* USE_SSPI */
#ifdef	USE_SSL
	/* SSL stuff */
	void		*ssl;		/* libpq ssl */
#endif /* USE_SSL */
#ifndef	NOT_USE_LIBPQ
	void		*pqconn;	/* libpq PGConn */
	BOOL		via_libpq;	/* using libpq library ? */
#endif /* NOT_USE_LIBPQ */

	char		reverse;		/* used to handle Postgres 6.2 protocol
								 * (reverse byte order) */

};

#define SOCK_get_char(self)	(SOCK_get_next_byte(self, FALSE))
#define SOCK_put_char(self, c)	(SOCK_put_next_byte(self, c))


/* error functions */
#define SOCK_get_errcode(self)	(self ? self->errornumber : SOCKET_CLOSED)
#define SOCK_get_errmsg(self)	(self ? self->_errormsg_ : "socket closed")

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
char		SOCK_connect_to(SocketClass *self, unsigned short port, char *hostname, long timeout);
int		SOCK_get_id(SocketClass *self);
void		SOCK_get_n_char(SocketClass *self, char *buffer, Int4 len);
void		SOCK_put_n_char(SocketClass *self, const char *buffer, Int4 len);
BOOL		SOCK_get_string(SocketClass *self, char *buffer, Int4 bufsize);
void		SOCK_put_string(SocketClass *self, const char *string);
int		SOCK_get_int(SocketClass *self, short len);
void		SOCK_put_int(SocketClass *self, int value, short len);
Int4		SOCK_flush_output(SocketClass *self);
UCHAR		SOCK_get_next_byte(SocketClass *self, BOOL peek);
void		SOCK_put_next_byte(SocketClass *self, UCHAR next_byte);
Int4		SOCK_get_response_length(SocketClass *self);
void		SOCK_clear_error(SocketClass *self);
UInt4		SOCK_skip_n_bytes(SocketClass *self, UInt4 skip_length);

#endif /* __SOCKET_H__ */
