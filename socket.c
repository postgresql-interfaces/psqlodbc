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

#include "connection.h"

#ifndef WIN32
#include <stdlib.h>
#include <string.h>				/* for memset */
#endif /* WIN32 */

extern GLOBAL_VALUES globals;

#ifndef BOOL
#define BOOL	int
#endif
#ifndef TRUE
#define TRUE	(BOOL)1
#endif
#ifndef FALSE
#define FALSE	(BOOL)0
#endif


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
		rv->socket = (SOCKETFD) - 1;
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
		rv->sadr = NULL;
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
		SOCK_put_char(self, 'X');
		SOCK_flush_output(self);
		closesocket(self->socket);
	}

	if (self->buffer_in)
		free(self->buffer_in);

	if (self->buffer_out)
		free(self->buffer_out);
	if (self->sadr != (struct sockaddr *) &(self->sadr_in))
		free(self->sadr);

	free(self);
}


char
SOCK_connect_to(SocketClass *self, unsigned short port, char *hostname)
{
#if defined (POSIX_MULTITHREAD_SUPPORT)
	const int bufsz = 8192; 
	char buf[bufsz];
	int error = 0;
	struct hostent host;
	struct hostent* hp = &host;
#else
	struct hostent* hp;
#endif /* POSIX_MULTITHREAD_SUPPORT */
	struct sockaddr_in *in;
#ifdef	HAVE_UNIX_SOCKETS
	struct sockaddr_un *un;
#endif /* HAVE_UNIX_SOCKETS */
	int	family, sLen; 
#ifdef WIN32
        UInt4 iaddr;
#else
	in_addr_t iaddr;
#endif

	if (self->socket != -1)
	{
		self->errornumber = SOCKET_ALREADY_CONNECTED;
		self->errormsg = "Socket is already connected";
		return 0;
	}


	/*
	 * If it is a valid IP address, use it. Otherwise use hostname lookup.
	 */
	if (hostname && hostname[0])
	{
		iaddr = inet_addr(hostname);
		memset((char *) &(self->sadr_in), 0, sizeof(self->sadr_in));
		in = &(self->sadr_in);
		in->sin_family = family = AF_INET;
		in->sin_port = htons(port);
		sLen = sizeof(self->sadr_in);
		if (iaddr == INADDR_NONE)
		{
#if defined (POSIX_MULTITHREAD_SUPPORT) 
  #if defined (HAVE_GETIPNODEBYNAME) /* Free-BSD ? */
			hp = getipnodebyname(hostname, AF_INET, 0, &error); 
  #elif defined (PGS_REENTRANT_API_1) /* solaris, irix */
			hp = gethostbyname_r(hostname, hp, buf, bufsz, &error);
  #elif defined (PGS_REENTRANT_API_2) /* linux */
			int result = 0;
			result = gethostbyname_r(hostname, hp, buf, bufsz, &hp, &error);
			if (result)
				hp = NULL;
  #else
			hp = gethostbyname(hostname);
  #endif /* HAVE_GETIPNODEBYNAME */
#else
			hp = gethostbyname(hostname);
#endif /* POSIX_MULTITHREAD_SUPPORT */
			if (hp == NULL)
			{
				self->errornumber = SOCKET_HOST_NOT_FOUND;
				self->errormsg = "Could not resolve hostname.";
				return 0;
			}
			memcpy(&(in->sin_addr), hp->h_addr, hp->h_length);
		}
		else
			memcpy(&(in->sin_addr), (struct in_addr *) & iaddr, sizeof(iaddr));
		self->sadr = (struct sockaddr *) in;

#if defined (HAVE_GETIPNODEBYNAME)
		freehostent(hp);
#endif /* HAVE_GETIPNODEBYNAME */
	}
	else
#ifdef	HAVE_UNIX_SOCKETS
	{
		un = (struct sockaddr_un *) malloc(sizeof(struct sockaddr_un));
		if (!un)
		{
			self->errornumber = SOCKET_COULD_NOT_CREATE_SOCKET;
			self->errormsg = "coulnd't allocate memory for un.";
			return 0;
		}
		un->sun_family = family = AF_UNIX;
		/* passing NULL means that this only suports the pg default "/tmp" */
		UNIXSOCK_PATH(un, port, ((char *) NULL));
		sLen = UNIXSOCK_LEN(un);
		self->sadr = (struct sockaddr *) un;
	}
#else
	{
		self->errornumber = SOCKET_HOST_NOT_FOUND;
		self->errormsg = "Hostname isn't specified.";
		return 0;
	}
#endif /* HAVE_UNIX_SOCKETS */

	self->socket = socket(family, SOCK_STREAM, 0);
	if (self->socket == -1)
	{
		self->errornumber = SOCKET_COULD_NOT_CREATE_SOCKET;
		self->errormsg = "Could not create Socket.";
		return 0;
	}
#ifdef	TCP_NODELAY
	if (family == AF_INET)
	{
		int i, len;

		i = 1;
		len = sizeof(i);
		if (setsockopt(self->socket, IPPROTO_TCP, TCP_NODELAY, (char *) &i, len) < 0)
		{
			self->errornumber = SOCKET_COULD_NOT_CONNECT;
			self->errormsg = "Could not set socket to NODELAY.";
			closesocket(self->socket);
			self->socket = (SOCKETFD) - 1;
			return 0;
		}
	}
#endif /* TCP_NODELAY */

	self->sadr_len = sLen;
	if (connect(self->socket, self->sadr, sLen) < 0)
	{
		self->errornumber = SOCKET_COULD_NOT_CONNECT;
		self->errormsg = "Could not connect to remote socket.";
		closesocket(self->socket);
		self->socket = (SOCKETFD) - 1;
		return 0;
	}

	return 1;
}


void
SOCK_get_n_char(SocketClass *self, char *buffer, int len)
{
	int			lf;

	if (!self)
		return;
	if (!buffer)
	{
		self->errornumber = SOCKET_NULLPOINTER_PARAMETER;
		self->errormsg = "get_n_char was called with NULL-Pointer";
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
		self->errornumber = SOCKET_NULLPOINTER_PARAMETER;
		self->errormsg = "put_n_char was called with NULL-Pointer";
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
SOCK_put_string(SocketClass *self, char *string)
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
			self->errornumber = SOCKET_GET_INT_WRONG_LENGTH;
			self->errormsg = "Cannot read ints of that length";
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
			self->errornumber = SOCKET_PUT_INT_WRONG_LENGTH;
			self->errormsg = "Cannot write ints of that length";
			return;
	}
}


void
SOCK_flush_output(SocketClass *self)
{
	int			written, pos = 0;

	if (!self)
		return;
	do
	{
		written = send(self->socket, (char *) self->buffer_out + pos, self->buffer_filled_out, 0);
		if (written < 0)
		{
			if (SOCK_ERRNO == EINTR)
				continue;
			self->errornumber = SOCKET_WRITE_ERROR;
			self->errormsg = "Could not flush socket buffer.";
			break;
		}
		pos += written;
		self->buffer_filled_out -= written;
	} while (self->buffer_filled_out > 0);
}


UCHAR
SOCK_get_next_byte(SocketClass *self)
{
	if (!self)
		return 0;
	if (self->buffer_read_in >= self->buffer_filled_in)
	{
		/*
		 * there are no more bytes left in the buffer so reload the buffer
		 */
		self->buffer_read_in = 0;
retry:
		self->buffer_filled_in = recv(self->socket, (char *) self->buffer_in, self->buffer_size, 0);

		mylog("read %d, global_socket_buffersize=%d\n", self->buffer_filled_in, self->buffer_size);

		if (self->buffer_filled_in < 0)
		{
			if (SOCK_ERRNO == EINTR)
				goto retry;
			self->errornumber = SOCKET_READ_ERROR;
			self->errormsg = "Error while reading from the socket.";
			self->buffer_filled_in = 0;
			return 0;
		}
		if (self->buffer_filled_in == 0)
		{
			self->errornumber = SOCKET_CLOSED;
			self->errormsg = "Socket has been closed.";
			self->buffer_filled_in = 0;
			return 0;
		}
	}
	return self->buffer_in[self->buffer_read_in++];
}


void
SOCK_put_next_byte(SocketClass *self, UCHAR next_byte)
{
	int			bytes_sent, pos = 0;

	if (!self)
		return;
	self->buffer_out[self->buffer_filled_out++] = next_byte;

	if (self->buffer_filled_out == self->buffer_size)
	{
		/* buffer is full, so write it out */
		do
		{
			bytes_sent = send(self->socket, (char *) self->buffer_out + pos, self->buffer_filled_out, 0);
			if (bytes_sent < 0)
			{
				if (SOCK_ERRNO == EINTR)
					continue;
				self->errornumber = SOCKET_WRITE_ERROR;
				self->errormsg = "Error while writing to the socket.";
				break;
			}
			pos += bytes_sent;
			self->buffer_filled_out -= bytes_sent;
		} while (self->buffer_filled_out > 0);
	}
}
