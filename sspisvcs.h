/* File:			sspisvcs.h
 *
 * Description:		See "sspisvcs.c"
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *
 */

#ifndef __SSPISVCS_H__
#define __SSPISVCS_H__

#include "socket.h"

/* SSPI Services */
typedef enum {
	SchannelService = 1L
	,KerberosService = (1L << 1)
	,NegotiateService = (1L << 2)
} SSPI_Service;

void	LeaveSSPIService();
void	ReleaseSvcSpecData(SocketClass *self, UInt4);
int	StartupSspiService(SocketClass *self, SSPI_Service svc, const void *opt, int *bReconnect);
int	ContinueSspiService(SocketClass *self, SSPI_Service svc, const void *opt);
int	SSPI_recv(SocketClass *self, void *buf, int len);
int	SSPI_send(SocketClass *self, const void *buf, int len);

#endif /* __SSPISVCS_H__ */
