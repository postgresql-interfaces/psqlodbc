/* File:			sspisvcs.h
 *
 * Description:		See "sspisvcs.c"
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *
 */

#ifndef __SSPISVCS_H__
#define __SSPISVCS_H__

#include "socket.h"

/* SSPI Services */
typedef enum {
	SchannelService = 1L
	,KerberosService = (1L << 1)
} SSPI_Service;

void	ReleaseSvcSpecData(SocketClass *self);
int	StartupSspiService(SocketClass *self, SSPI_Service svc, const char *opt);
int	SSPI_recv(SocketClass *self, void *buf, int len);
int	SSPI_send(SocketClass *self, const void *buf, int len);

#endif /* __SSPISVCS_H__ */
