/*-------
 * Module:			socket.c
 *
 * Description:		This module contains functions for low level socket
 *					operations (connecting/reading/writing to the backend)
 *
 * Classes:			SocketClass (Functions prefix: "SOCK_")
 *
 * API functions:	none
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#include "psqlodbc.h"

#ifdef	USE_SSPI
#include "sspisvcs.h"
#endif /* USE_SSPI */
#ifndef	NOT_USE_LIBPQ
#include <libpq-fe.h>
#ifdef USE_SSL
#include <openssl/ssl.h>
#endif /* USE_SSL */
#endif /* NOT_USE_LIBPQ */
#include "socket.h"
#include "loadlib.h"

#include "connection.h"

#ifdef WIN32
#include <time.h>
#else
#include <stdlib.h>
#include <string.h>				/* for memset */
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH__SYS_TIME */
#endif /* WIN32 */

extern GLOBAL_VALUES globals;

static void SOCK_set_error(SocketClass *s, int _no, const char *_msg)
{
	int	gerrno = SOCK_ERRNO;
	s->errornumber = _no;
	if (NULL != s->_errormsg_)
		free(s->_errormsg_);
	if (NULL != _msg)
		s->_errormsg_ = strdup(_msg);
	else
		s->_errormsg_ = NULL;
	mylog("(%d)%s ERRNO=%d\n", _no, _msg, gerrno);
}

void
SOCK_clear_error(SocketClass *self)
{
	self->errornumber = 0;
	if (NULL != self->_errormsg_)
		free(self->_errormsg_);
	self->_errormsg_ = NULL;
}


SocketClass *
SOCK_Constructor(const ConnectionClass *conn)
{
	SocketClass *rv;

	rv = (SocketClass *) malloc(sizeof(SocketClass));

	if (rv != NULL)
	{
		rv->socket = (SOCKETFD) -1;
#ifdef	USE_SSPI
		rv->ssd = NULL;
		rv->sspisvcs = 0;
#endif /* USE_SSPI */
#ifndef	NOT_USE_LIBPQ
		rv->via_libpq = FALSE;
#ifdef USE_SSL
		rv->ssl = NULL;
#endif
		rv->pqconn = NULL;
#endif /* NOT_USE_LIBPQ */
		rv->pversion = 0;
		rv->reslen = 0;
		rv->buffer_filled_in = 0;
		rv->buffer_filled_out = 0;
		rv->buffer_read_in = 0;

		if (conn)
			rv->buffer_size = conn->connInfo.drivers.socket_buffersize;
		else
			rv->buffer_size = globals.socket_buffersize;
		rv->buffer_in = (UCHAR *) malloc(rv->buffer_size);
		if (!rv->buffer_in)
		{
			free(rv);
			return NULL;
		}

		rv->buffer_out = (UCHAR *) malloc(rv->buffer_size);
		if (!rv->buffer_out)
		{
			free(rv->buffer_in);
			free(rv);
			return NULL;
		}
		rv->_errormsg_ = NULL;
		rv->errornumber = 0;
		rv->reverse = FALSE;
	}
	return rv;
}

void
SOCK_Destructor(SocketClass *self)
{
	mylog("SOCK_Destructor\n");
	if (!self)
		return;
#ifndef	NOT_USE_LIBPQ
	if (self->pqconn)
	{
		if (self->via_libpq)
		{
			PQfinish(self->pqconn);
			/* UnloadDelayLoadedDLLs(NULL != self->ssl); */
		}
		self->via_libpq = FALSE;
		self->pqconn = NULL;
#ifdef USE_SSL
		self->ssl = NULL;
#endif
	}
	else
#endif /* NOT_USE_LIBPQ */
	{
		if (self->socket != (SOCKETFD) -1)
		{
			SOCK_put_char(self, 'X');
			if (PG_PROTOCOL_74 == self->pversion)
				SOCK_put_int(self, 4, 4);
			SOCK_flush_output(self);
			closesocket(self->socket);
		}
#ifdef	USE_SSPI
		if (self->ssd)
		{
			ReleaseSvcSpecData(self);
			free(self->ssd);
			self->ssd = NULL;
		}
		self->sspisvcs = 0;
#endif /* USE_SSPI */
	}

	if (self->buffer_in)
		free(self->buffer_in);

	if (self->buffer_out)
		free(self->buffer_out);
	if (self->_errormsg_)
		free(self->_errormsg_);

	free(self);
}

#if defined(_MSC_VER) && (_MSC_VER < 1300)
static freeaddrinfo_func freeaddrinfo_ptr = NULL;
static getaddrinfo_func getaddrinfo_ptr = NULL;
static getnameinfo_func getnameinfo_ptr = NULL;
static	HMODULE ws2_hnd = NULL;
#else
static freeaddrinfo_func freeaddrinfo_ptr = freeaddrinfo;
static getaddrinfo_func getaddrinfo_ptr = getaddrinfo;
#endif /* _MSC_VER */

static BOOL format_sockerr(char *errmsg, size_t buflen, int errnum, const char *cmd, const char *host, int portno)
{
	BOOL ret = FALSE;

#ifdef	WIN32
	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL,
		errnum, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
		errmsg, (DWORD)buflen, NULL))
		ret = TRUE;
#else
#if defined(POSIX_MULTITHREAD_SUPPORT) && defined(HAVE_STRERROR_R)
#ifdef	STRERROR_R_CHAR_P
	const char *pchar;

	pchar = (const char *) strerror_r(errnum, errmsg, buflen);
	if (NULL != pchar)
	{
		if (pchar != errmsg)
			strncpy_null(errmsg, pchar, buflen);
		ret = TRUE;
	}
#else
	if (0 == strerror_r(errnum, errmsg, buflen))
		ret = TRUE;
#endif /* STRERROR_R_CHAR_P */
#else
	strncpy_null(errmsg, strerror(errnum), buflen);
	ret = TRUE;
#endif /* POSIX_MULTITHREAD_SUPPORT */
#endif /* WIN32 */
	if (ret)
	{
		size_t	tlen = strlen(errmsg);
		errmsg += tlen;
		buflen -= tlen;
		snprintf(errmsg, buflen, " [%s:%d]", host, portno);
	}
	else
		snprintf(errmsg, buflen, "%s failed for [%s:%d] ", cmd, host, portno);
	return ret;
}
 
char
SOCK_connect_to(SocketClass *self, unsigned short port, char *hostname, long timeout)
{
	struct addrinfo	rest, *addrs = NULL, *curadr = NULL;
	int	family = 0; 
	char	retval = 0;
	int	gerrno;

	if (self->socket != (SOCKETFD) -1)
	{
		SOCK_set_error(self, SOCKET_ALREADY_CONNECTED, "Socket is already connected");
		return 0;
	}

#if defined(_MSC_VER) && (_MSC_VER < 1300)
	if (ws2_hnd == NULL)
		ws2_hnd = GetModuleHandle("ws2_32.dll");
	if (freeaddrinfo_ptr == NULL)
		freeaddrinfo_ptr = (freeaddrinfo_func)GetProcAddress(ws2_hnd, "freeaddrinfo"); 
	if (getaddrinfo_ptr == NULL)
		getaddrinfo_ptr = (getaddrinfo_func)GetProcAddress(ws2_hnd, "getaddrinfo"); 
	if (getnameinfo_ptr == NULL)
		getnameinfo_ptr = (getnameinfo_func)GetProcAddress(ws2_hnd, "getnameinfo"); 
#endif
	/*
	 * Hostname lookup.
	 */
	if (hostname && hostname[0]
#ifndef	WIN32
	    && '/' != hostname[0]
#endif /* WIN32 */
	   )
	{
		char	portstr[16];
		int	ret;

		memset(&rest, 0, sizeof(rest));
		rest.ai_socktype = SOCK_STREAM;
		rest.ai_family = AF_UNSPEC;
		snprintf(portstr, sizeof(portstr), "%d", port);
		if (inet_addr(hostname) != INADDR_NONE)
			rest.ai_flags |= AI_NUMERICHOST;	
		ret = getaddrinfo_ptr(hostname, portstr, &rest, &addrs);
		if (ret || !addrs)
		{
			SOCK_set_error(self, SOCKET_HOST_NOT_FOUND, "Could not resolve hostname.");
			if (addrs)
				freeaddrinfo_ptr(addrs);
			return 0;
		}
		curadr = addrs;
	}
	else
#ifdef	HAVE_UNIX_SOCKETS
	{
		struct sockaddr_un *un = (struct sockaddr_un *) &(self->sadr_area);
		family = un->sun_family = AF_UNIX;
		/* passing NULL or '' means pg default "/tmp" */
		UNIXSOCK_PATH(un, port, hostname);
		self->sadr_len = UNIXSOCK_LEN(un);
	}
#else
	{
		SOCK_set_error(self, SOCKET_HOST_NOT_FOUND, "Hostname isn't specified.");
		return 0;
	}
#endif /* HAVE_UNIX_SOCKETS */

retry:
	if (curadr)
		family = curadr->ai_family;
	self->socket = socket(family, SOCK_STREAM, 0);
	if (self->socket == (SOCKETFD) -1)
	{
		SOCK_set_error(self, SOCKET_COULD_NOT_CREATE_SOCKET, "Could not create Socket.");
		goto cleanup;
	}
#ifdef	TCP_NODELAY
	if (family != AF_UNIX)
	{
		int i;
		socklen_t	len;

		i = 1;
		len = sizeof(i);
		if (setsockopt(self->socket, IPPROTO_TCP, TCP_NODELAY, (char *) &i, len) < 0)
		{
			SOCK_set_error(self, SOCKET_COULD_NOT_CONNECT, "Could not set socket to NODELAY.");
			goto cleanup;
		}
	}
#endif /* TCP_NODELAY */
#ifdef	WIN32
	{
		long	ioctlsocket_ret = 1;

		/* Returns non-0 on failure, while fcntl() returns -1 on failure */
		ioctlsocket(self->socket, FIONBIO, &ioctlsocket_ret);
	}
#else
        fcntl(self->socket, F_SETFL, O_NONBLOCK);
#endif

	if (curadr)
	{
		struct sockaddr *in = (struct sockaddr *) &(self->sadr_area);
		memset((char *) in, 0, sizeof(self->sadr_area));
		memcpy(in, curadr->ai_addr, curadr->ai_addrlen);
		self->sadr_len = (int) curadr->ai_addrlen;
	}
	if (connect(self->socket, (struct sockaddr *) &(self->sadr_area), self->sadr_len) < 0)
	{
		int	ret, optval;
		fd_set	fds, except_fds;
		struct	timeval	tm;
		socklen_t	optlen = sizeof(optval);
		time_t	t_now, t_finish = 0;
		BOOL	tm_exp = FALSE;

		gerrno = SOCK_ERRNO;
		switch (gerrno)
		{
			case 0:
			case EINPROGRESS:
			case EINTR:
#ifdef EAGAIN
			case EAGAIN:
#endif /* EAGAIN */
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
			case EWOULDBLOCK:
#endif /* EWOULDBLOCK */
		    		break;
			default:
				SOCK_set_error(self, SOCKET_COULD_NOT_CONNECT, "Could not connect to remote socket immedaitely");
				goto cleanup;
		}
		if (timeout > 0)
		{
			t_now = time(NULL);
			t_finish = t_now + timeout;
			tm.tv_sec = timeout;
			tm.tv_usec = 0;
		}
		do {
			FD_ZERO(&fds);
			FD_ZERO(&except_fds);
			FD_SET(self->socket, &fds);
			FD_SET(self->socket, &except_fds);
			ret = select((int) self->socket + 1, NULL, &fds, &except_fds, timeout > 0 ? &tm : NULL);
			gerrno = SOCK_ERRNO;
			if (0 < ret)
				break;
			else if (0 == ret)
				tm_exp = TRUE;
			else if (EINTR != gerrno)
				break;
			else if (timeout > 0)
			{
				if (t_now = time(NULL), t_now >= t_finish)
					tm_exp = TRUE;
				else
				{
					tm.tv_sec = (long) (t_finish - t_now);
					tm.tv_usec = 0;
				}
			}
		} while (!tm_exp);
		if (tm_exp)
		{
			SOCK_set_error(self, SOCKET_COULD_NOT_CONNECT, "Could not connect .. timeout occured.");
			goto cleanup;
		}
		else if (0 > ret)
		{
			SOCK_set_error(self, SOCKET_COULD_NOT_CONNECT, "Could not connect .. select error occured.");
			mylog("select error ret=%d ERROR=%d\n", ret, gerrno);
			goto cleanup;
		}
		if (getsockopt(self->socket, SOL_SOCKET, SO_ERROR,
				(char *) &optval, &optlen) == -1)
		{
			SOCK_set_error(self, SOCKET_COULD_NOT_CONNECT, "Could not connect .. getsockopt error.");
		}
		else if (optval != 0)
		{
			char	errmsg[256], host[64];

			host[0] = '\0';
#if defined(_MSC_VER) && (_MSC_VER < 1300)
			getnameinfo_ptr
#else
			getnameinfo
#endif
				((struct sockaddr *) &(self->sadr_area),
					self->sadr_len, host, sizeof(host),
					NULL, 0, NI_NUMERICHOST);
			/* snprintf(errmsg, sizeof(errmsg), "connect getsockopt val %d addr=%s\n", optval, host); */
			format_sockerr(errmsg, sizeof(errmsg), optval, "connect", host, port);
			mylog(errmsg);
			SOCK_set_error(self, SOCKET_COULD_NOT_CONNECT, errmsg);
		}
		else
			retval = 1;
	}
	else
		retval = 1;

cleanup:
	if (0 == retval)
	{
		if (self->socket >= 0)
		{
			closesocket(self->socket);
			self->socket = (SOCKETFD) -1;
		}
		if (curadr && curadr->ai_next)
		{
			curadr = curadr->ai_next;
			goto retry;
		}
	}
	else
		SOCK_set_error(self, 0, NULL);
	
	if (addrs)
		freeaddrinfo_ptr(addrs);
	return retval;
}


/*
 *	To handle EWOULDBLOCK etc (mainly for libpq non-blocking connection).
 */
#define	MAX_RETRY_COUNT	30

static int SOCK_wait_for_ready(SocketClass *sock, BOOL output, int retry_count)
{
	int	ret, gerrno;
	fd_set	fds, except_fds;
	struct	timeval	tm;
	BOOL	no_timeout = TRUE;

	if (0 == retry_count)
		no_timeout = FALSE;
	else if (0  > retry_count)
		no_timeout = TRUE;
#ifdef	USE_SSL
	else if (sock && NULL == sock->ssl)
		no_timeout = TRUE;
#endif /* USE_SSL */
	do {
		FD_ZERO(&fds);
		FD_ZERO(&except_fds);
		FD_SET(sock->socket, &fds);
		FD_SET(sock->socket, &except_fds);
		if (!no_timeout)
		{
			tm.tv_sec = retry_count;
			tm.tv_usec = 0;
		}
		ret = select((int) sock->socket + 1, output ? NULL : &fds, output ? &fds : NULL, &except_fds, no_timeout ? NULL : &tm);
		gerrno = SOCK_ERRNO;
	} while (ret < 0 && EINTR == gerrno);
	if (retry_count < 0)
		retry_count *= -1;
	if (0 == ret && retry_count > MAX_RETRY_COUNT)
	{
		ret = -1;
		if (sock)
			SOCK_set_error(sock, output ? SOCKET_WRITE_TIMEOUT : SOCKET_READ_TIMEOUT, "SOCK_wait_for_ready timeout");
	}
	return ret;
}

static int SOCK_SSPI_recv(SocketClass *self, void *buffer, int len)
{
#ifdef	USE_SSPI
	if (self->sspisvcs && self->ssd)
		return SSPI_recv(self, (char *) buffer, len);
	else
#endif /* USE_SSPI */
	return recv(self->socket, (char *) buffer, len, RECV_FLAG);
}

static int SOCK_SSPI_send(SocketClass *self, const void *buffer, int len)
{
#ifdef	USE_SSPI
	if (self->sspisvcs && self->ssd)
		return SSPI_send(self, buffer, len);
	else
#endif /* USE_SSPI */
	return send(self->socket, (char *) buffer, len, SEND_FLAG);
}

#ifdef USE_SSL
/*
 *	The stuff for SSL.
 */
/* included in  <openssl/ssl.h>
#define SSL_ERROR_NONE			0
#define SSL_ERROR_SSL			1
#define SSL_ERROR_WANT_READ		2
#define SSL_ERROR_WANT_WRITE		3
#define SSL_ERROR_WANT_X509_LOOKUP	4
#define SSL_ERROR_SYSCALL		5 // look at error stack/return value/errno
#define SSL_ERROR_ZERO_RETURN		6
#define SSL_ERROR_WANT_CONNECT		7
#define SSL_ERROR_WANT_ACCEPT		8
*/

/*
 *	recv more than 1 bytes using SSL.
 */
static int SOCK_SSL_recv(SocketClass *sock, void *buffer, int len)
{
	CSTR	func = "SOCK_SSL_recv";
	int n, err, gerrno, retry_count = 0;

retry:
	n = SSL_read(sock->ssl, buffer, len);
	err = SSL_get_error(sock->ssl, len);
	gerrno = SOCK_ERRNO;
inolog("%s: %d get_error=%d Lasterror=%d\n", func, n, err, gerrno);
	switch (err)
	{
		case	SSL_ERROR_NONE:
			break;
		case	SSL_ERROR_WANT_READ:
			retry_count++;
			if (SOCK_wait_for_ready(sock, FALSE, retry_count) >= 0)
				goto retry;
			n = -1;
			break;
		case	SSL_ERROR_WANT_WRITE:
			goto retry;
			break;
		case	SSL_ERROR_SYSCALL:
			if (-1 != n)
			{
				n = -1;
				SOCK_ERRNO_SET(ECONNRESET);
			}
			break;
		case	SSL_ERROR_SSL:
		case	SSL_ERROR_ZERO_RETURN:
			n = -1;
			SOCK_ERRNO_SET(ECONNRESET);
			break;
		default:
			n = -1;
	}

	return n;
}

/*
 *	send more than 1 bytes using SSL.
 */
static int SOCK_SSL_send(SocketClass *sock, void *buffer, int len)
{
	CSTR	func = "SOCK_SSL_send";
	int n, err, gerrno, retry_count = 0;

retry:
	n = SSL_write(sock->ssl, buffer, len);
	err = SSL_get_error(sock->ssl, len);
	gerrno = SOCK_ERRNO;
inolog("%s: %d get_error=%d Lasterror=%d\n", func,  n, err, gerrno);
	switch (err)
	{
		case	SSL_ERROR_NONE:
			break;
		case	SSL_ERROR_WANT_READ:
		case	SSL_ERROR_WANT_WRITE:
			retry_count++;
			if (SOCK_wait_for_ready(sock, TRUE, retry_count) >= 0)
				goto retry;
			n = -1;
			break;
		case	SSL_ERROR_SYSCALL:
			if (-1 != n)
			{
				n = -1;
				SOCK_ERRNO_SET(ECONNRESET);
			}
			break;
		case	SSL_ERROR_SSL:
		case	SSL_ERROR_ZERO_RETURN:
			n = -1;
			SOCK_ERRNO_SET(ECONNRESET);
			break;
		default:
			n = -1;
	}

	return n;
}

#endif /* USE_SSL */

int
SOCK_get_id(SocketClass *self)
{
	int	id;

	if (0 != self->errornumber)
		return 0;
	if (self->reslen > 0)
	{
		mylog("SOCK_get_id has to eat %d bytes\n", self->reslen);
		do
		{
			SOCK_get_next_byte(self, FALSE);
			if (0 != self->errornumber)
				return 0;
		} while (self->reslen > 0);
	}
	id = SOCK_get_next_byte(self, FALSE);
	self->reslen = 0;
	return id;
}

void
SOCK_get_n_char(SocketClass *self, char *buffer, Int4 len)
{
	int			lf;

	if (!self)
		return;
	if (!buffer)
	{
		SOCK_set_error(self, SOCKET_NULLPOINTER_PARAMETER, "get_n_char was called with NULL-Pointer");
		return;
	}

	for (lf = 0; lf < len; lf++)
	{
		if (0 != self->errornumber)
			break;
		buffer[lf] = SOCK_get_next_byte(self, FALSE);
	}
}


void
SOCK_put_n_char(SocketClass *self, const char *buffer, Int4 len)
{
	int			lf;

	if (!self)
		return;
	if (!buffer)
	{
		SOCK_set_error(self, SOCKET_NULLPOINTER_PARAMETER, "put_n_char was called with NULL-Pointer");
		return;
	}

	for (lf = 0; lf < len; lf++)
	{
		if (0 != self->errornumber)
			break;
		SOCK_put_next_byte(self, (UCHAR) buffer[lf]);
	}
}


/*
 *	bufsize must include room for the null terminator
 *	will read at most bufsize-1 characters + null.
 *	returns TRUE if truncation occurs.
 */
BOOL
SOCK_get_string(SocketClass *self, char *buffer, Int4 bufsize)
{
	int lf;

	for (lf = 0; lf < bufsize - 1; lf++)
		if (!(buffer[lf] = SOCK_get_next_byte(self, FALSE)))
			return FALSE;

	buffer[bufsize - 1] = '\0';
	return TRUE;
}


void
SOCK_put_string(SocketClass *self, const char *string)
{
	size_t	lf, len;

	len = strlen(string) + 1;

	for (lf = 0; lf < len; lf++)
	{
		if (0 != self->errornumber)
			break;
		SOCK_put_next_byte(self, (UCHAR) string[lf]);
	}
}

#define	REVERSE_SHORT(val)	((val & 0xff) << 8) | (val >> 8)
#define	REVERSE_INT(val) ((val & 0xff) << 24) | ((val & 0xff00) << 8) | ((val & 0xff0000) >> 8) | (val >> 24)

int
SOCK_get_int(SocketClass *self, short len)
{
	if (!self)
		return 0;
	switch (len)
	{
		case 2:
			{
				unsigned short buf;

				SOCK_get_n_char(self, (char *) &buf, len);
				if (self->reverse)
					return REVERSE_SHORT(ntohs(buf));
				else
					return ntohs(buf);
			}

		case 4:
			{
				unsigned int buf;

				SOCK_get_n_char(self, (char *) &buf, len);
				if (self->reverse)
					return REVERSE_INT(htonl(buf));
				else
					return ntohl(buf);
			}

		default:
			SOCK_set_error(self, SOCKET_GET_INT_WRONG_LENGTH, "Cannot read ints of that length");
			return 0;
	}
}


void
SOCK_put_int(SocketClass *self, int value, short len)
{
	unsigned int rv;
	unsigned short rsv;

	if (!self)
		return;
	switch (len)
	{
		case 2:
			rsv = htons((unsigned short) value);
			if (self->reverse)
				rsv = REVERSE_SHORT(rsv);
			SOCK_put_n_char(self, (char *) &rsv, 2);
			return;

		case 4:
			rv = htonl((unsigned int) value);
			if (self->reverse)
				rv = REVERSE_INT(rv);
			SOCK_put_n_char(self, (char *) &rv, 4);
			return;

		default:
			SOCK_set_error(self, SOCKET_PUT_INT_WRONG_LENGTH, "Cannot write ints of that length");
			return;
	}
}


Int4
SOCK_flush_output(SocketClass *self)
{
	int	written, pos = 0, retry_count = 0, ttlsnd = 0, gerrno;

	if (!self)
		return -1;
	if (0 != self->errornumber)
		return -1;
	while (self->buffer_filled_out > 0)
	{
#ifdef USE_SSL 
		if (self->ssl)
			written = SOCK_SSL_send(self, (char *) self->buffer_out + pos, self->buffer_filled_out);
		else
#endif /* USE_SSL */
		{
			written = SOCK_SSPI_send(self, (char *) self->buffer_out + pos, self->buffer_filled_out);
		}
		gerrno = SOCK_ERRNO;
		if (written < 0)
		{
			switch (gerrno)
			{
				case EINTR:
					continue;
					break;
#ifdef EAGAIN
				case EAGAIN:
#endif /* EAGAIN */
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
				case EWOULDBLOCK:
#endif /* EWOULDBLOCK */
					retry_count++;
					if (SOCK_wait_for_ready(self, TRUE, retry_count) >= 0)
						continue;
					break;
			}
			SOCK_set_error(self, SOCKET_WRITE_ERROR, "Could not flush socket buffer.");
			return -1;
		}
		pos += written;
		self->buffer_filled_out -= written;
		ttlsnd += written;
		retry_count = 0;
	}
	
	return ttlsnd;
}


UCHAR
SOCK_get_next_byte(SocketClass *self, BOOL peek)
{
	int	retry_count = 0, gerrno;
	BOOL	maybeEOF = FALSE;

	if (!self)
		return 0;
	if (self->buffer_read_in >= self->buffer_filled_in)
	{
		/*
		 * there are no more bytes left in the buffer so reload the buffer
		 */
		self->buffer_read_in = 0;
retry:
#ifdef USE_SSL 
		if (self->ssl)
			self->buffer_filled_in = SOCK_SSL_recv(self, (char *) self->buffer_in, self->buffer_size);
		else
#endif /* USE_SSL */
			self->buffer_filled_in = SOCK_SSPI_recv(self, (char *) self->buffer_in, self->buffer_size);
		gerrno = SOCK_ERRNO;

		mylog("read %d, global_socket_buffersize=%d\n", self->buffer_filled_in, self->buffer_size);

		if (self->buffer_filled_in < 0)
		{
mylog("Lasterror=%d\n", gerrno);
			switch (gerrno)
			{
				case	EINTR:
					goto retry;
					break;
#ifdef EAGAIN
				case	EAGAIN:
#endif /* EAGAIN */
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
				case	EWOULDBLOCK:
#endif /* EWOULDBLOCK */
					retry_count++;
					if (SOCK_wait_for_ready(self, FALSE, retry_count) >= 0)
						goto retry;
					break;
				case	ECONNRESET:
inolog("ECONNRESET\n");
					maybeEOF = TRUE;
					SOCK_set_error(self, SOCKET_CLOSED, "Connection reset by peer.");
					break;
			}
			if (0 == self->errornumber)
				SOCK_set_error(self, SOCKET_READ_ERROR, "Error while reading from the socket.");
			self->buffer_filled_in = 0;
			return 0;
		}
		if (self->buffer_filled_in == 0)
		{
			if (!maybeEOF)
			{
				int	nbytes = SOCK_wait_for_ready(self, FALSE, 0);
				if (nbytes > 0)
				{
					maybeEOF = TRUE;
					goto retry;
				}
				else if (0 == nbytes)
					maybeEOF = TRUE;
			}
			if (maybeEOF)
				SOCK_set_error(self, SOCKET_CLOSED, "Socket has been closed.");
			else
				SOCK_set_error(self, SOCKET_READ_ERROR, "Error while reading from the socket.");
			return 0;
		}
	}
	if (peek)
		return self->buffer_in[self->buffer_read_in];
	if (PG_PROTOCOL_74 == self->pversion)
		self->reslen--;
	return self->buffer_in[self->buffer_read_in++];
}

void
SOCK_put_next_byte(SocketClass *self, UCHAR next_byte)
{
	int	bytes_sent, pos = 0, retry_count = 0, gerrno;

	if (!self)
		return;
	if (0 != self->errornumber)
		return;
	self->buffer_out[self->buffer_filled_out++] = next_byte;

	if (self->buffer_filled_out == self->buffer_size)
	{
		/* buffer is full, so write it out */
		do
		{
#ifdef USE_SSL
			if (self->ssl)
				bytes_sent = SOCK_SSL_send(self, (char *) self->buffer_out + pos, self->buffer_filled_out);
			else
#endif /* USE_SSL */
			{
				bytes_sent = SOCK_SSPI_send(self, (char *) self->buffer_out + pos, self->buffer_filled_out);
			}
			gerrno = SOCK_ERRNO;
			if (bytes_sent < 0)
			{
				switch (gerrno)
				{
					case	EINTR:
						continue;
#ifdef EAGAIN
					case	EAGAIN:
#endif /* EAGAIN */
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
					case	EWOULDBLOCK:
#endif /* EWOULDBLOCK */
						retry_count++;
						if (SOCK_wait_for_ready(self, TRUE, retry_count) >= 0)
							continue;
				}
				if (0 == self->errornumber)
					SOCK_set_error(self, SOCKET_WRITE_ERROR, "Error while writing to the socket.");
				break;
			}
			pos += bytes_sent;
			self->buffer_filled_out -= bytes_sent;
			retry_count = 0;
		} while (self->buffer_filled_out > 0);
	}
}

Int4
SOCK_get_response_length(SocketClass *self)
{
	int     leng = -1;

	if (PG_PROTOCOL_74 == self->pversion)
	{
		leng = SOCK_get_int(self, 4) - 4;
		self->reslen = leng;
	}

	return leng;
}
