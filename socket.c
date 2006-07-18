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

#include "socket.h"
#include "loadlib.h"

#include "connection.h"

#ifndef WIN32
#include <stdlib.h>
#include <string.h>				/* for memset */
#if defined(TM_IN_SYS_TIME)
#include <sys/time.h>
#endif /* defined(TM_IN_SYS_TIME) */
#endif /* WIN32 */

extern GLOBAL_VALUES globals;

#define	SOCK_set_error(s, _no, _msg)	(s->errornumber = _no, s->errormsg = _msg, mylog("socket error=%d %s\n", _no, _msg))

void
SOCK_clear_error(SocketClass *self)
{
	self->errornumber = 0;
	self->errormsg = NULL;
}


SocketClass *
SOCK_Constructor(const ConnectionClass *conn)
{
	SocketClass *rv;

	rv = (SocketClass *) malloc(sizeof(SocketClass));

	if (rv != NULL)
	{
		rv->socket = (SOCKETFD) -1;
		rv->via_libpq = FALSE;
		rv->ssl = NULL;
		rv->pqconn = NULL;
		rv->pversion = 0;
		rv->reslen = 0;
		rv->buffer_filled_in = 0;
		rv->buffer_filled_out = 0;
		rv->buffer_read_in = 0;

		if (rv)
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
		rv->errormsg = NULL;
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
	if (self->socket != -1)
	{
		if (self->pqconn)
		{
			if (self->via_libpq)
			{
				if (self->pqconn)
					PQfinish(self->pqconn);
				UnloadDelayLoadedDLLs(NULL != self->ssl);
			}
			self->via_libpq = FALSE;
			self->pqconn = NULL;
			self->ssl = NULL;
		}
		else
		{
			SOCK_put_char(self, 'X');
			if (PG_PROTOCOL_74 == self->pversion)
				SOCK_put_int(self, 4, 4);
			SOCK_flush_output(self);
			closesocket(self->socket);
		}
	}

	if (self->buffer_in)
		free(self->buffer_in);

	if (self->buffer_out)
		free(self->buffer_out);

	free(self);
}


char
SOCK_connect_to(SocketClass *self, unsigned short port, char *hostname, long timeout)
{
	struct addrinfo	rest, *addrs = NULL, *curadr = NULL;
	int	family; 
	char	retval = 0;

	if (self->socket != -1)
	{
		SOCK_set_error(self, SOCKET_ALREADY_CONNECTED, "Socket is already connected");
		return 0;
	}


	/*
	 * If it is a valid IP address, use it. Otherwise use hostname lookup.
	 */
	if (hostname && hostname[0])
	{
		char	portstr[16];
		int	ret;

		memset(&rest, 0, sizeof(rest));
		rest.ai_socktype = SOCK_STREAM;
		rest.ai_family = AF_UNSPEC;
		snprintf(portstr, sizeof(portstr), "%d", port);
		if (inet_addr(hostname) != INADDR_NONE)
			rest.ai_flags = AI_NUMERICHOST;	
		ret = getaddrinfo(hostname, portstr, &rest, &addrs);
		if (ret || !addrs)
		{
			SOCK_set_error(self, SOCKET_HOST_NOT_FOUND, "Could not resolve hostname.");
			if (addrs)
				freeaddrinfo(addrs);
			return 0;
		}
		curadr = addrs;
	}
	else
#ifdef	HAVE_UNIX_SOCKETS
	{
		struct sockaddr_un *un = (struct sockaddr_un *) &(self->sadr_area);
		family = un->sun_family = AF_UNIX;
		/* passing NULL means that this only supports the pg default "/tmp" */
		UNIXSOCK_PATH(un, port, ((char *) NULL));
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
	if (self->socket == -1)
	{
		SOCK_set_error(self, SOCKET_COULD_NOT_CREATE_SOCKET, "Could not create Socket.");
		return 0;
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
			closesocket(self->socket);
			self->socket = (SOCKETFD) -1;
			return 0;
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
		self->sadr_len = curadr->ai_addrlen;
	}
	if (connect(self->socket, (struct sockaddr *) &(self->sadr_area), self->sadr_len) < 0)
	{
		int	ret, optval;
		fd_set	fds;
		struct	timeval	tm;
		socklen_t	optlen = sizeof(optval);
		time_t	t_now, t_finish;
		BOOL	tm_exp = FALSE;

		switch (SOCK_ERRNO)
		{
			case 0:
			case EINPROGRESS:
			case EINTR:
			case EWOULDBLOCK:
		    		break;
			default:
				SOCK_set_error(self, SOCKET_COULD_NOT_CONNECT, "Could not connect to remote socket ..");
				mylog("connect immediate connect error %d\n", SOCK_ERRNO);
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
			FD_SET(self->socket, &fds);
			ret = select(1, NULL, &fds, NULL, timeout > 0 ? &tm : NULL);
			if (0 < ret)
				break;
			else if (0 == ret)
				tm_exp = TRUE;
			else if (EINTR != SOCK_ERRNO)
				break;
			else if (timeout > 0)
			{
				if (t_now = time(NULL), t_now >= t_finish)
					tm_exp = TRUE;
				else
				{
					tm.tv_sec = t_finish - t_now;
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
			mylog("select error ret=%d ERROR=%d\n", ret, SOCK_ERRNO);
			goto cleanup;
		}
		if (getsockopt(self->socket, SOL_SOCKET, SO_ERROR,
				(char *) &optval, &optlen) == -1)
		{
			SOCK_set_error(self, SOCKET_COULD_NOT_CONNECT, "Could not connect .. getsockopt error.");
		}
		else if (optval != 0)
		{
			SOCK_set_error(self, SOCKET_COULD_NOT_CONNECT, "Could not connect to remote server.");
			mylog("connect getsockopt val=%d\n", optval);
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
		if (addrs)
			freeaddrinfo(addrs);
	}
	return retval;
}


/*
 *	To handle EWOULDBLOCK etc (mainly for libpq non-blocking connection).
 */
#define	MAX_RETRY_COUNT	8
static int SOCK_wait_for_ready(SocketClass *sock, BOOL output, int retry_count)
{
	int	ret;
	fd_set	fds;
	struct	timeval	tm;

	do {
		FD_ZERO(&fds);
		FD_SET(sock->socket, &fds);
		if (!output && sock->ssl)
		{
			tm.tv_sec = retry_count;
			tm.tv_usec = 0;
		}
		ret = select(1, output ? NULL : &fds, output ? &fds : NULL, NULL, sock->ssl ? &tm : NULL);
	} while (ret < 0 && EINTR == SOCK_ERRNO);
	if (0 == ret && retry_count > MAX_RETRY_COUNT)
	{
		ret = -1;
		SOCK_set_error(sock, output ? SOCKET_WRITE_TIMEOUT : SOCKET_READ_TIMEOUT, "SOCK_wait_for_ready timeout");
	}
	return ret;
}
/*
 *	The stuff for SSL.
 */
/* #include <openssl/ssl.h>*/
#define SSL_ERROR_NONE			0
#define SSL_ERROR_SSL			1
#define SSL_ERROR_WANT_READ		2
#define SSL_ERROR_WANT_WRITE		3
#define SSL_ERROR_WANT_X509_LOOKUP	4
#define SSL_ERROR_SYSCALL		5 /* look at error stack/return value/errno */
#define SSL_ERROR_ZERO_RETURN		6
#define SSL_ERROR_WANT_CONNECT		7
#define SSL_ERROR_WANT_ACCEPT		8
/*
 *	recv more than 1 bytes using SSL.
 */
static int SOCK_SSL_recv(SocketClass *sock, void *buffer, int len)
{
	CSTR	func = "SOCK_SSL_recv";
	int n, err, retry_count = 0;

retry:
	n = SSL_read(sock->ssl, buffer, len);
	err = SSL_get_error(sock->ssl, len);
inolog("%s: %d get_error=%d Lasterror=%d\n", func, n, err, SOCK_ERRNO);
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
	int n, err, retry_count = 0;

retry:
	n = SSL_write(sock->ssl, buffer, len);
	err = SSL_get_error(sock->ssl, len);
inolog("%s: %d get_error=%d Lasterror=%d\n", func,  n, err, SOCK_ERRNO);
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


int
SOCK_get_id(SocketClass *self)
{
	int	id;

	if (self->reslen > 0)
	{
		mylog("SOCK_get_id has to eat %d bytes\n", self->reslen);
		do
		{
			SOCK_get_next_byte(self);
		} while (self->reslen > 0);
	}
	id = SOCK_get_next_byte(self);
	self->reslen = 0;
	return id;
}

void
SOCK_get_n_char(SocketClass *self, char *buffer, int len)
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
		buffer[lf] = SOCK_get_next_byte(self);
}


void
SOCK_put_n_char(SocketClass *self, char *buffer, int len)
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
		SOCK_put_next_byte(self, (UCHAR) buffer[lf]);
}


/*
 *	bufsize must include room for the null terminator
 *	will read at most bufsize-1 characters + null.
 *	returns TRUE if truncation occurs.
 */
BOOL
SOCK_get_string(SocketClass *self, char *buffer, int bufsize)
{
	register int lf = 0;

	for (lf = 0; lf < bufsize - 1; lf++)
		if (!(buffer[lf] = SOCK_get_next_byte(self)))
			return FALSE;

	buffer[bufsize - 1] = '\0';
	return TRUE;
}


void
SOCK_put_string(SocketClass *self, const char *string)
{
	register int lf;
	int			len;

	len = strlen(string) + 1;

	for (lf = 0; lf < len; lf++)
		SOCK_put_next_byte(self, (UCHAR) string[lf]);
}


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
					return buf;
				else
					return ntohs(buf);
			}

		case 4:
			{
				unsigned int buf;

				SOCK_get_n_char(self, (char *) &buf, len);
				if (self->reverse)
					return buf;
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

	if (!self)
		return;
	switch (len)
	{
		case 2:
			rv = self->reverse ? value : htons((unsigned short) value);
			SOCK_put_n_char(self, (char *) &rv, 2);
			return;

		case 4:
			rv = self->reverse ? value : htonl((unsigned int) value);
			SOCK_put_n_char(self, (char *) &rv, 4);
			return;

		default:
			SOCK_set_error(self, SOCKET_PUT_INT_WRONG_LENGTH, "Cannot write ints of that length");
			return;
	}
}


void
SOCK_flush_output(SocketClass *self)
{
	int	written, pos = 0, retry_count = 0;

	if (!self)
		return;
	do
	{
		if (self->ssl)
			written = SOCK_SSL_send(self, (char *) self->buffer_out + pos, self->buffer_filled_out);
		else
			written = send(self->socket, (char *) self->buffer_out + pos, self->buffer_filled_out, 0);
		if (written < 0)
		{
			switch (SOCK_ERRNO)
			{
				case EINTR:
					continue;
					break;
				case EWOULDBLOCK:
					retry_count++;
					if (SOCK_wait_for_ready(self, TRUE, retry_count) >= 0)
						continue;
					break;
			}
			SOCK_set_error(self, SOCKET_WRITE_ERROR, "Could not flush socket buffer.");
			break;
		}
		pos += written;
		self->buffer_filled_out -= written;
	} while (self->buffer_filled_out > 0);
}


UCHAR
SOCK_get_next_byte(SocketClass *self)
{
	int	retry_count = 0;

	if (!self)
		return 0;
	if (self->buffer_read_in >= self->buffer_filled_in)
	{
		/*
		 * there are no more bytes left in the buffer so reload the buffer
		 */
		self->buffer_read_in = 0;
retry:
		if (self->ssl)
			self->buffer_filled_in = SOCK_SSL_recv(self, (char *) self->buffer_in, self->buffer_size);
		else
			self->buffer_filled_in = recv(self->socket, (char *) self->buffer_in, self->buffer_size, 0);

		mylog("read %d, global_socket_buffersize=%d\n", self->buffer_filled_in, self->buffer_size);

		if (self->buffer_filled_in < 0)
		{
mylog("Lasterror=%d\n", SOCK_ERRNO);
			switch (SOCK_ERRNO)
			{
				case	EINTR:
					goto retry;
					break;
				case	EWOULDBLOCK:
					retry_count++;
					if (SOCK_wait_for_ready(self, FALSE, retry_count) >= 0)
						goto retry;
					break;
			}
			if (0 == self->errornumber)
				SOCK_set_error(self, SOCKET_READ_ERROR, "Error while reading from the socket.");
			self->buffer_filled_in = 0;
			return 0;
		}
		if (self->buffer_filled_in == 0)
		{
			SOCK_set_error(self, SOCKET_CLOSED, "Socket has been closed.");
			self->buffer_filled_in = 0;
			return 0;
		}
	}
	if (PG_PROTOCOL_74 == self->pversion)
		self->reslen--;
	return self->buffer_in[self->buffer_read_in++];
}

void
SOCK_put_next_byte(SocketClass *self, UCHAR next_byte)
{
	int	bytes_sent, pos = 0, retry_count = 0;

	if (!self)
		return;
	self->buffer_out[self->buffer_filled_out++] = next_byte;

	if (self->buffer_filled_out == self->buffer_size)
	{
		/* buffer is full, so write it out */
		do
		{
			if (self->ssl)
				bytes_sent = SOCK_SSL_send(self, (char *) self->buffer_out + pos, self->buffer_filled_out);
			else
				bytes_sent = send(self->socket, (char *) self->buffer_out + pos, self->buffer_filled_out, 0);
			if (bytes_sent < 0)
			{
				switch (SOCK_ERRNO)
				{
					case	EINTR:
						continue;
					case	EWOULDBLOCK:
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
		} while (self->buffer_filled_out > 0);
	}
}

int
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
