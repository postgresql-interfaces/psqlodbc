/*-------
 * Module:			sspi_proc.c
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

#ifdef	USE_SSPI
#define	SECURITY_WIN32
#define	WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <security.h>
#include <sspi.h>
#pragma comment(lib, "secur32.lib")

#include "sspisvcs.h"
#include "socket.h"

/*
 *	To handle EWOULDBLOCK etc (mainly for libpq non-blocking connection).
 */
#define	MAX_RETRY_COUNT	30
static int Socket_wait_for_ready(SOCKET socket, BOOL output, int retry_count)
{
	int	ret, gerrno;
	fd_set	fds, except_fds;
	struct	timeval	tm;
	BOOL	no_timeout = (retry_count < 0);

	do {
		FD_ZERO(&fds);
		FD_ZERO(&except_fds);
		FD_SET(socket, &fds);
		FD_SET(socket, &except_fds);
		if (!no_timeout)
		{
			tm.tv_sec = retry_count;
			tm.tv_usec = 0;
		}
		ret = select((int) socket + 1, output ? NULL : &fds, output ? &fds : NULL, &except_fds, no_timeout ? NULL : &tm);
		gerrno = SOCK_ERRNO;
	} while (ret < 0 && EINTR == gerrno);
	if (retry_count < 0)
		retry_count *= -1;
	if (0 == ret && retry_count > MAX_RETRY_COUNT)
	{
		ret = -1;
	}
	return ret;
}

static int sendall(SOCKET sock, const void *buf, int len)
{
	CSTR func = "sendall";
	int	wrtlen, ttllen, reqlen, retry_count;

	retry_count = 0;
	for (ttllen = 0, reqlen = len; reqlen > 0;)
	{
		if (0 > (wrtlen = send(sock, (const char *) buf + ttllen, reqlen, SEND_FLAG)))
		{
			int	gerrno = SOCK_ERRNO;

			mylog("%s:errno=%d\n", func, gerrno);
			switch (gerrno)
			{
				case EINTR:
					continue;
				case EWOULDBLOCK:
					retry_count++;
					if (Socket_wait_for_ready(sock, TRUE, retry_count) >= 0)
						continue;
					break;
				case ECONNRESET:
					return 0;
			}
			return SOCKET_ERROR;
		}
		ttllen += wrtlen;
		reqlen -= wrtlen;
		retry_count = 0; 
	}
	return ttllen;
}
 
static int recvall(SOCKET sock, void *buf, int len)
{
	CSTR	func = "recvall";
	int	rcvlen, ttllen, reqlen, retry_count = 0;

	for (ttllen = 0, reqlen = len; reqlen > 0;)
	{
		if (0 > (rcvlen = recv(sock, (char *) buf + ttllen, reqlen, RECV_FLAG)))
		{
			int	gerrno = SOCK_ERRNO;

			switch (gerrno)
			{
				case EINTR:
					continue;
				case EWOULDBLOCK:
					retry_count++;
					if (Socket_wait_for_ready(sock, FALSE, retry_count) >= 0)
						continue;
					break;
				case ECONNRESET:
					return 0;
			}
			return -1;
		}
		ttllen += rcvlen;
		reqlen -= rcvlen;
		retry_count = 0; 
	}
	return ttllen;
}
 
/*	
 *	service specific data
 */

/*	Schannel specific data */
typedef	struct {
	CredHandle	hCred;
	CtxtHandle	hCtxt;
	PBYTE		ioovrbuf;
	size_t		ioovrlen;
	PBYTE		iobuf;
	size_t		iobuflen;
	size_t		ioread;
} SchannelSpec;

/*	Kerberos/Negotiate common specific data */
typedef	struct {
	LPTSTR		svcprinc;
	CredHandle	hKerbEtcCred;
	BOOL		ValidCtxt;
	CtxtHandle	hKerbEtcCtxt;
} KerberosEtcSpec;

typedef struct {
	SchannelSpec	sdata;
	KerberosEtcSpec	kdata;
} SspiData;

static int DoSchannelNegotiation(SocketClass *, SspiData *, const void *opt);
static int DoKerberosNegotiation(SocketClass *, SspiData *, const void *opt);
static int DoNegotiateNegotiation(SocketClass *, SspiData *, const void *opt);
static int DoKerberosEtcProcessAuthentication(SocketClass *, const void *opt);

static SspiData *SspiDataAlloc(SocketClass *self)
{
	SspiData	*sspidata;

	if (sspidata = self->ssd, !sspidata)
		sspidata = calloc(sizeof(SspiData), 1);
	return sspidata;
}

int StartupSspiService(SocketClass *self, SSPI_Service svc, const void *opt)
{
	CSTR func = "DoServicelNegotiation";
	SspiData	*sspidata;

	if (NULL == (sspidata = SspiDataAlloc(self)))
		return -1;
	switch (svc)
	{
		case SchannelService:
			return DoSchannelNegotiation(self, sspidata, opt);
		case KerberosService:
			return DoKerberosNegotiation(self, sspidata, opt);
		case NegotiateService:
			return DoNegotiateNegotiation(self, sspidata, opt);
	}

	free(sspidata);
	return -1;
}

int ContinueSspiService(SocketClass *self, SSPI_Service svc, const void *opt)
{
	CSTR func = "ContinueSspiService";

	switch (svc)
	{
		case KerberosService:
		case NegotiateService:
			return DoKerberosEtcProcessAuthentication(self, opt);
	}

	return -1;
}

static BOOL format_sspierr(char *errmsg, size_t buflen, SECURITY_STATUS r, const char *cmd, const char *cmd2)
{
	BOOL ret = FALSE;

	if (!cmd2)
		cmd2 = ""; 
	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL,
		r, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
		errmsg, (DWORD)buflen, NULL))
		ret = TRUE;
	if (ret)
	{
		size_t	tlen = strlen(errmsg);
		errmsg += tlen;
		buflen -= tlen;
		snprintf(errmsg, buflen, " in %s:%s", cmd, cmd2);
	}
	else
		snprintf(errmsg, buflen, "%s:%s failed ", cmd, cmd2);
	return ret;
}

static void SSPI_set_error(SocketClass *s, SECURITY_STATUS r, const char *cmd, const char *cmd2)
{
	int	gerrno = SOCK_ERRNO;
	char	emsg[256];

	format_sspierr(emsg, sizeof(emsg), r, cmd, cmd2);
	s->errornumber = r;
	if (NULL != s->_errormsg_)
		free(s->_errormsg_);
	if (NULL != emsg)
		s->_errormsg_ = strdup(emsg);
	else
		s->_errormsg_ = NULL;
	mylog("(%d)%s ERRNO=%d\n", r, emsg, gerrno);
}

/*
 *	Stuff for Schannel service
 */
#include	<schannel.h>
#pragma comment(lib, "crypt32")
#define	UNI_SCHANNEL TEXT("sChannel")
#define	IO_BUFFER_SIZE	0x10000

static SECURITY_STATUS CreateSchannelCredentials(LPCTSTR, LPSTR, PCredHandle);
static SECURITY_STATUS PerformSchannelClientHandshake(SOCKET, PCredHandle, LPSTR, CtxtHandle *, SecBuffer *);
static SECURITY_STATUS SchannelClientHandshakeLoop(SOCKET, PCredHandle, CtxtHandle *, BOOL, SecBuffer *);
static void GetNewSchannelClientCredentials(PCredHandle, CtxtHandle *);

static HCERTSTORE	hMyCertStore = NULL;

static int DoSchannelNegotiation(SocketClass *self, SspiData *sspidata, const void *opt)
{
	CSTR func = "DoSchannelNegotiation";
	SECURITY_STATUS	r = SEC_E_OK;
	const char	*cmd = NULL;
	SecBuffer	ExtraData;
	BOOL		ret = 0, cCreds = FALSE, cCtxt = FALSE;
	SchannelSpec	*ssd = &(sspidata->sdata);

	if (SEC_E_OK != (r = CreateSchannelCredentials(NULL, NULL, &ssd->hCred)))
	{
		cmd = "CreateSchannelCredentials";
		mylog("%s:%s failed\n", func, cmd);
		goto cleanup;
	}
	cCreds = TRUE;
	if (SEC_E_OK != (r = PerformSchannelClientHandshake(self->socket, &ssd->hCred, NULL, &ssd->hCtxt, &ExtraData)))
	{
		cmd = "PerformSchannelClientHandshake";
		mylog("%s:%s failed\n", func, cmd);
		goto cleanup;
	}
	cCtxt = TRUE;
	if (NULL != ExtraData.pvBuffer && 0 != ExtraData.cbBuffer)
	{
		ssd->iobuf = malloc(ExtraData.cbBuffer);
		ssd->iobuflen =
		ssd->ioread = ExtraData.cbBuffer;
		memcpy(ssd->iobuf, ExtraData.pvBuffer, ssd->ioread);
		free(ExtraData.pvBuffer);
	}
	ret = TRUE;
cleanup:
	if (ret)
	{
		self->sspisvcs |= SchannelService;
		self->ssd = sspidata;
	}
	else
	{
		SSPI_set_error(self, r, __FUNCTION__, cmd); 
		if (cCreds)
			FreeCredentialHandle(&ssd->hCred);
		if (cCtxt)
			DeleteSecurityContext(&ssd->hCtxt);
		if (ssd->iobuf)
			free(ssd->iobuf);
		if (!self->ssd)
			free(sspidata);
	}
	return ret;
}

static
SECURITY_STATUS
CreateSchannelCredentials(
	LPCTSTR	opt,		/* in */
	LPSTR pszUserName,	/* in */
	PCredHandle phCreds)	/* out */
{
	TimeStamp	tsExpiry;
	SECURITY_STATUS	Status;
	SCHANNEL_CRED	SchannelCred;	

	DWORD		cSupportedAlgs = 0;
	ALG_ID		rgbSupportedAlgs[16];
	DWORD		dwProtocol = SP_PROT_SSL3 | SP_PROT_SSL2;
	DWORD		aiKeyExch = 0;

	PCCERT_CONTEXT  pCertContext = NULL;

	/*
	 * If a user name is specified, then attempt to find a client
	 * certificate. Otherwise, just create a NULL credential.
	 */

	if (pszUserName)
	{
		/* Find client certificate. Note that this sample just searchs for a 
		 * certificate that contains the user name somewhere in the subject name.
		 * A real application should be a bit less casual.
		 */
		pCertContext = CertFindCertificateInStore(hMyCertStore, 
							X509_ASN_ENCODING, 
							0,
							CERT_FIND_SUBJECT_STR_A,
							pszUserName,
							NULL);
		if (pCertContext == NULL)
		{
			mylog("**** Error 0x%p returned by CertFindCertificateInStore\n",
					GetLastError());
			return SEC_E_NO_CREDENTIALS;
		}
	}


	/*
	 * Build Schannel credential structure. Currently, this sample only
	 * specifies the protocol to be used (and optionally the certificate, 
	 * of course). Real applications may wish to specify other parameters 
	 * as well.
	 */

	ZeroMemory(&SchannelCred, sizeof(SchannelCred));

	SchannelCred.dwVersion  = SCHANNEL_CRED_VERSION;
	if (pCertContext)
	{
		SchannelCred.cCreds	= 1;
		SchannelCred.paCred	= &pCertContext;
	}

	SchannelCred.grbitEnabledProtocols = dwProtocol;

	if (aiKeyExch)
	{
		rgbSupportedAlgs[cSupportedAlgs++] = aiKeyExch;
	}

	if (cSupportedAlgs)
	{
		SchannelCred.cSupportedAlgs	= cSupportedAlgs;
		SchannelCred.palgSupportedAlgs	= rgbSupportedAlgs;
	}

	SchannelCred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS;

	/* The SCH_CRED_MANUAL_CRED_VALIDATION flag is specified because
	 * this sample verifies the server certificate manually. 
	 * Applications that expect to run on WinNT, Win9x, or WinME 
	 * should specify this flag and also manually verify the server
	 * certificate. Applications running on newer versions of Windows can
	 * leave off this flag, in which case the InitializeSecurityContext
	 * function will validate the server certificate automatically.
 	 */
	SchannelCred.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;

	/*
	 * Create an SSPI credential.
	 */

	Status = AcquireCredentialsHandle(
			NULL,			/* Name of principal */    
			UNI_SCHANNEL,		/* Name of package */
			SECPKG_CRED_OUTBOUND,	/* Flags indicating use */
			NULL,			/* Pointer to logon ID */
			&SchannelCred,		/* Package specific data */
			NULL,			/* Pointer to GetKey() func */
			NULL,			/* Value to pass to GetKey() */
			phCreds,		/* (out) Cred Handle */
			&tsExpiry);		/* (out) Lifetime (optional) */
	if (Status != SEC_E_OK)
	{
		mylog("**** Error 0x%p returned by AcquireCredentialsHandle\n", Status);
		goto cleanup;
	}

cleanup:

    /*
     * Free the certificate context. Schannel has already made its own copy.
     */

    if(pCertContext)
    {
        CertFreeCertificateContext(pCertContext);
    }


    return Status;
}

static
SECURITY_STATUS
PerformSchannelClientHandshake(
	SOCKET		Socket,		/* in */
	PCredHandle	phCreds,	/* in */
	LPSTR		pszServerName,	/* in */
	CtxtHandle	*phContext,	/* out */
	SecBuffer	*pExtraData)	/* out */
{
	SecBufferDesc	OutBuffer;
	SecBuffer	OutBuffers[1];
	DWORD		dwSSPIFlags;
	DWORD		dwSSPIOutFlags;
	TimeStamp	tsExpiry;
	SECURITY_STATUS	scRet;
	DWORD		cbData;

	dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT	|
			ISC_REQ_REPLAY_DETECT	|
			ISC_REQ_CONFIDENTIALITY	|
			ISC_RET_EXTENDED_ERROR	|
			ISC_REQ_ALLOCATE_MEMORY	|
			ISC_REQ_STREAM;

	/*
	 *  Initiate a ClientHello message and generate a token.
	 */

	OutBuffers[0].pvBuffer	= NULL;
	OutBuffers[0].BufferType = SECBUFFER_TOKEN;
	OutBuffers[0].cbBuffer	= 0;

	OutBuffer.cBuffers = 1;
	OutBuffer.pBuffers = OutBuffers;
	OutBuffer.ulVersion = SECBUFFER_VERSION;

	scRet = InitializeSecurityContext(
					phCreds,
					NULL,
					pszServerName,
					dwSSPIFlags,
					0,
					SECURITY_NATIVE_DREP,
					NULL,
					0,
					phContext,
					&OutBuffer,
					&dwSSPIOutFlags,
					&tsExpiry);

	if (scRet != SEC_I_CONTINUE_NEEDED)
	{
		mylog("**** Error %x returned by InitializeSecurityContext (1)\n", scRet);
		return scRet;
	}

	/* Send response to server if there is one. */
	if (OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != NULL)
	{
		cbData = sendall(Socket,
				OutBuffers[0].pvBuffer,
				OutBuffers[0].cbBuffer);
		if (cbData <= 0)
		{
			mylog("**** Error %x sending data to server\n", SOCK_ERRNO);
			FreeContextBuffer(OutBuffers[0].pvBuffer);
			DeleteSecurityContext(phContext);
			return SEC_E_INTERNAL_ERROR;
		}

		mylog("%d bytes of handshake data sent\n", cbData);

		/* Free output buffer. */
		FreeContextBuffer(OutBuffers[0].pvBuffer);
		OutBuffers[0].pvBuffer = NULL;
	}


	return SchannelClientHandshakeLoop(Socket, phCreds, phContext, TRUE, pExtraData);
}

static
SECURITY_STATUS
SchannelClientHandshakeLoop(
	SOCKET		Socket,		/* in */
	PCredHandle	phCreds,	/* in */
	CtxtHandle	*phContext,	/* i-o */
	BOOL		fDoInitialRead,	/* in */
	SecBuffer	*pExtraData)	/* out */
{
	SecBufferDesc	InBuffer;
	SecBuffer	InBuffers[2];
	SecBufferDesc	OutBuffer;
	SecBuffer	OutBuffers[1];
	DWORD		dwSSPIFlags;
	DWORD		dwSSPIOutFlags;
	TimeStamp	tsExpiry;
	SECURITY_STATUS	scRet;
	DWORD		cbData;

	PUCHAR		IoBuffer;
	DWORD		cbIoBuffer;
	BOOL		fDoRead;
	int		retry_count;

	dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT	|
			ISC_REQ_REPLAY_DETECT	|
			ISC_REQ_CONFIDENTIALITY	|
			ISC_RET_EXTENDED_ERROR	|
			ISC_REQ_ALLOCATE_MEMORY	|
			ISC_REQ_STREAM;

	/*
	 * Allocate data buffer.
	 */

	IoBuffer = malloc(IO_BUFFER_SIZE);
	if (IoBuffer == NULL)
	{
		mylog("**** Out of memory (1)\n");
		return SEC_E_INTERNAL_ERROR;
	}

	cbIoBuffer = 0;
	fDoRead = fDoInitialRead;
	/* 
	 * Loop until the handshake is finished or an error occurs.
	 */

	retry_count = 0;
	scRet = SEC_I_CONTINUE_NEEDED;
	while (scRet == SEC_I_CONTINUE_NEEDED	||
		scRet == SEC_E_INCOMPLETE_MESSAGE	||
		scRet == SEC_I_INCOMPLETE_CREDENTIALS) 
	{
		/*
		 * Read data from server.
		 */
		if( 0 == cbIoBuffer || scRet == SEC_E_INCOMPLETE_MESSAGE)
		{
			if (fDoRead)
			{
				cbData = recv(Socket, 
						IoBuffer + cbIoBuffer, 
						IO_BUFFER_SIZE - cbIoBuffer, 
						RECV_FLAG);
				if (cbData == SOCKET_ERROR)
				{
					int	gerrno = SOCK_ERRNO;

					mylog("**** Error %d reading data from server\n", gerrno);
					switch (gerrno)
					{
						case EINTR:
							continue;
						case ECONNRESET:
							break;
						case EWOULDBLOCK:
							retry_count++;
							if (Socket_wait_for_ready(Socket, FALSE, retry_count) >= 0)
								continue;
						default:
							scRet = SEC_E_INTERNAL_ERROR;
							SOCK_ERRNO_SET(gerrno);
							break;
					}
					break;
				}
				else if(cbData == 0)
				{
					mylog("**** Server unexpectedly disconnected\n");
					scRet = SEC_E_INTERNAL_ERROR;
					break;
				}
				mylog("%d bytes of handshake data received\n", cbData);
				cbIoBuffer += cbData;
				retry_count = 0;
			}
			else
			{
				fDoRead = TRUE;
			}
		}
		/*
		 * Set up the input buffers. Buffer 0 is used to pass in data
		 * received from the server. Schannel will consume some or all
		 * of this. Leftover data (if any) will be placed in buffer 1 and
		 * given a buffer type of SECBUFFER_EXTRA.
		 */
		InBuffers[0].pvBuffer	= IoBuffer;
		InBuffers[0].cbBuffer	= cbIoBuffer;
		InBuffers[0].BufferType	= SECBUFFER_TOKEN;

		InBuffers[1].pvBuffer	= NULL;
		InBuffers[1].cbBuffer	= 0;
		InBuffers[1].BufferType	= SECBUFFER_EMPTY;

		InBuffer.cBuffers	= 2;
		InBuffer.pBuffers	= InBuffers;
		InBuffer.ulVersion	= SECBUFFER_VERSION;

		/*
		 * Set up the output buffers. These are initialized to NULL
		 * so as to make it less likely we'll attempt to free random
		 * garbage later.
		 */

		OutBuffers[0].pvBuffer	= NULL;
		OutBuffers[0].BufferType= SECBUFFER_TOKEN;
		OutBuffers[0].cbBuffer	= 0;

		OutBuffer.cBuffers	= 1;
		OutBuffer.pBuffers	= OutBuffers;
		OutBuffer.ulVersion	= SECBUFFER_VERSION;

		/*
		 * Call InitializeSecurityContext.
		 */

		scRet = InitializeSecurityContext(phCreds,
						phContext,
						NULL,
						dwSSPIFlags,
						0,
						SECURITY_NATIVE_DREP,
						&InBuffer,
						0,
						NULL,
						&OutBuffer,
						&dwSSPIOutFlags,
						&tsExpiry);

		/*
		 * If InitializeSecurityContext was successful (or if the error was 
		 * one of the special extended ones), send the contends of the output
		 * buffer to the server.
		 */

		if( scRet == SEC_E_OK	||
		    scRet == SEC_I_CONTINUE_NEEDED	||
		    FAILED(scRet) && (dwSSPIOutFlags & ISC_RET_EXTENDED_ERROR))
		{
			if (OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != NULL)
			{
				cbData = sendall(Socket,
						OutBuffers[0].pvBuffer,
						OutBuffers[0].cbBuffer);
				if (cbData == SOCKET_ERROR || cbData == 0)
				{
					mylog("**** Error %d sending data to server (2)\n", 
						SOCK_ERRNO);
					FreeContextBuffer(OutBuffers[0].pvBuffer);
					DeleteSecurityContext(phContext);
					return SEC_E_INTERNAL_ERROR;
				}

				mylog("%d bytes of handshake data sent\n", cbData);
				/* Free output buffer. */
				FreeContextBuffer(OutBuffers[0].pvBuffer);
				OutBuffers[0].pvBuffer = NULL;
			}
		}

		/*
		 * If InitializeSecurityContext returned SEC_E_INCOMPLETE_MESSAGE,
		 * then we need to read more data from the server and try again.
		 */

		if (scRet == SEC_E_INCOMPLETE_MESSAGE)
		{
			continue;
		}

		/*
		 * If InitializeSecurityContext returned SEC_E_OK, then the 
		 * handshake completed successfully.
		 */

		if(scRet == SEC_E_OK)
		{
			/*
			 * If the "extra" buffer contains data, this is encrypted application
			 * protocol layer stuff. It needs to be saved. The application layer
			 * will later decrypt it with DecryptMessage.
			 */
			mylog("Handshake was successful\n");

			if (InBuffers[1].BufferType == SECBUFFER_EXTRA)
			{
				pExtraData->pvBuffer = malloc(InBuffers[1].cbBuffer);
				if (pExtraData->pvBuffer == NULL)
				{
					mylog("**** Out of memory (2)\n");
					return SEC_E_INTERNAL_ERROR;
				}

				memmove(pExtraData->pvBuffer,
					IoBuffer + (cbIoBuffer - InBuffers[1].cbBuffer),
					InBuffers[1].cbBuffer);

				pExtraData->cbBuffer   = InBuffers[1].cbBuffer;
				pExtraData->BufferType = SECBUFFER_TOKEN;

				mylog("%d bytes of app data was bundled with handshake data\n",
					pExtraData->cbBuffer);
			}
			else
			{
				pExtraData->pvBuffer   = NULL;
				pExtraData->cbBuffer   = 0;
				pExtraData->BufferType = SECBUFFER_EMPTY;
			}

			/*
			 * Bail out to quit
			 */

			break;
		}

		/*
		 * Check for fatal error.
		 */

		if(FAILED(scRet))
		{
			mylog("**** Error 0x%p returned by InitializeSecurityContext (2)\n", scRet);
			break;
		}

		/*
		 * If InitializeSecurityContext returned SEC_I_INCOMPLETE_CREDENTIALS,
		 * then the server just requested client authentication. 
		 */

		if (scRet == SEC_I_INCOMPLETE_CREDENTIALS)
		{
			/*
			 * Busted. The server has requested client authentication and
			 * the credential we supplied didn't contain a client certificate.
			 *

			 * 
			 * This function will read the list of trusted certificate
			 * authorities ("issuers") that was received from the server
			 * and attempt to find a suitable client certificate that
			 * was issued by one of these. If this function is successful, 
			 * then we will connect using the new certificate. Otherwise,
			 * we will attempt to connect anonymously (using our current
			 * credentials).
			 */
            
			GetNewSchannelClientCredentials(phCreds, phContext);

			/* Go around again. */
			fDoRead = FALSE;
			scRet = SEC_I_CONTINUE_NEEDED;
			continue;
		}
		/*
		 * Copy any leftover data from the "extra" buffer, and go around
		 * again.
		 */

		if ( InBuffers[1].BufferType == SECBUFFER_EXTRA )
		{
			memmove(IoBuffer,
				IoBuffer + (cbIoBuffer - InBuffers[1].cbBuffer),
				InBuffers[1].cbBuffer);

			cbIoBuffer = InBuffers[1].cbBuffer;
		}
		else
		{
			cbIoBuffer = 0;
		}
	}

	/* Delete the security context in the case of a fatal error. */
	if (FAILED(scRet))
	{
		DeleteSecurityContext(phContext);
	}

	free(IoBuffer);

	return scRet;
}

static void
GetNewSchannelClientCredentials(
		CredHandle *phCreds,
		CtxtHandle *phContext)
{
	SCHANNEL_CRED	SchannelCred;
	CredHandle	hCreds;
	SecPkgContext_IssuerListInfoEx	IssuerListInfo;
	PCCERT_CHAIN_CONTEXT		pChainContext;
	CERT_CHAIN_FIND_BY_ISSUER_PARA	FindByIssuerPara;
	PCCERT_CONTEXT	pCertContext;
	TimeStamp	tsExpiry;
	SECURITY_STATUS	Status;

	/*
	 * Read list of trusted issuers from schannel.
	 */

	Status = QueryContextAttributes(phContext,
					SECPKG_ATTR_ISSUER_LIST_EX,
					(PVOID)&IssuerListInfo);
	if (Status != SEC_E_OK)
	{
		mylog("Error 0x%p querying issuer list info\n", Status);
		return;
	}

	/*
	 * Enumerate the client certificates.
	 */

	ZeroMemory(&FindByIssuerPara, sizeof(FindByIssuerPara));

	FindByIssuerPara.cbSize = sizeof(FindByIssuerPara);
	FindByIssuerPara.pszUsageIdentifier = szOID_PKIX_KP_CLIENT_AUTH;
	FindByIssuerPara.dwKeySpec	= 0;
	FindByIssuerPara.cIssuer	= IssuerListInfo.cIssuers;
	FindByIssuerPara.rgIssuer	= IssuerListInfo.aIssuers;

	pChainContext = NULL;

	while (TRUE)
	{
		/* Find a certificate chain. */
		pChainContext = CertFindChainInStore(hMyCertStore,
						X509_ASN_ENCODING,
						0,
						CERT_CHAIN_FIND_BY_ISSUER,
						&FindByIssuerPara,
						pChainContext);
		if (pChainContext == NULL)
		{
			mylog("Error 0x%p finding cert chain\n", GetLastError());
			break;
		}
		mylog("\ncertificate chain found\n");

		/* Get pointer to leaf certificate context. */
		pCertContext = pChainContext->rgpChain[0]->rgpElement[0]->pCertContext;

		ZeroMemory(&SchannelCred, sizeof(SchannelCred));
        	/* Create schannel credential. */
        	SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
        	SchannelCred.cCreds = 1;
        	SchannelCred.paCred = &pCertContext;

        	Status = AcquireCredentialsHandle(
                            NULL,		/* Name of principal */
                            UNI_SCHANNEL,	/* Name of package */
                            SECPKG_CRED_OUTBOUND,	/* Flags indicating use */
                            NULL,		/* Pointer to logon ID */
                            &SchannelCred,	/* Package specific data */
                            NULL,		/* Pointer to GetKey() func */
                            NULL,		/* Value to pass to GetKey() */
                            &hCreds,		/* (out) Cred Handle */
                            &tsExpiry);		/* (out) Lifetime (optional) */
		if (Status != SEC_E_OK)
		{
			 mylog("**** Error 0x%p returned by AcquireCredentialsHandle\n", Status);
			continue;
		}
		mylog("\nnew schannel credential created\n");

		/* Destroy the old credentials. */
		FreeCredentialsHandle(phCreds);

		*phCreds = hCreds;

		/*
		 * As you can see, this sample code maintains a single credential
		 * handle, replacing it as necessary. This is a little unusual.
		 *
		 * Many applications maintain a global credential handle that's
		 * anonymous (that is, it doesn't contain a client certificate),
		 * which is used to connect to all servers. If a particular server
		 * should require client authentication, then a new credential 
		 * is created for use when connecting to that server. The global
		 * anonymous credential is retained for future connections to
		 * other servers.
		 *
		 * Maintaining a single anonymous credential that's used whenever
		 * possible is most efficient, since creating new credentials all
		 * the time is rather expensive.
		 */

		break;
	}
}

/*
 *	Stuff for Kerberos etc service
 */
#define	UNI_KERBEROS TEXT("Kerberos")
#define	UNI_NEGOTIATE TEXT("Negotiate")
#define	IO_BUFFER_SIZE	0x10000


static SECURITY_STATUS CreateKerberosEtcCredentials(LPCTSTR, SEC_CHAR *, LPCTSTR, PCredHandle);
static SECURITY_STATUS PerformKerberosEtcClientHandshake(SocketClass *, KerberosEtcSpec *ssd, size_t);

static int DoKerberosNegotiation(SocketClass *self, SspiData *sspidata, const void *opt)
{
	CSTR func = "DoKerberosNegotiation";
	SECURITY_STATUS	r = SEC_E_OK;
	const char *	cmd = NULL;
	BOOL		ret = 0;
	KerberosEtcSpec	*ssd = &(sspidata->kdata);

mylog("!!! %s in\n", __FUNCTION__);
	if (SEC_E_OK != (r = CreateKerberosEtcCredentials(NULL, UNI_KERBEROS, (LPCTSTR) opt, &ssd->hKerbEtcCred)))
	{
		cmd = "CreateKerberosCredentials";
		mylog("%s:%s failed\n", func, cmd);
		SSPI_set_error(self, r, __FUNCTION__, cmd); 
		return 0;
	}
mylog("!!! CreateKerberosCredentials passed\n");

	ssd->svcprinc = (LPTSTR) opt;
	self->sspisvcs |= KerberosService;
	self->ssd = sspidata;
	return DoKerberosEtcProcessAuthentication(self, NULL);
}

static int DoNegotiateNegotiation(SocketClass *self, SspiData *sspidata, const void *opt)
{
	CSTR func = "DoNegotiateNegotiation";
	SECURITY_STATUS	r = SEC_E_OK;
	const char *	cmd = NULL;
	BOOL		ret = 0;
	KerberosEtcSpec	*ssd = &(sspidata->kdata);

mylog("!!! %s in\n", __FUNCTION__);
	if (SEC_E_OK != (r = CreateKerberosEtcCredentials(NULL, UNI_NEGOTIATE, (LPCTSTR) opt, &ssd->hKerbEtcCred)))
	{
		cmd = "CreateNegotiateCredentials";
		mylog("%s:%s failed\n", func, cmd);
		SSPI_set_error(self, r, __FUNCTION__, cmd); 
		return 0;
	}
mylog("!!! CreateNegotiateCredentials passed\n");

	ssd->svcprinc = (LPTSTR) opt;
	self->sspisvcs |= NegotiateService;
	self->ssd = sspidata;
	return DoKerberosEtcProcessAuthentication(self, NULL);
}

static int DoKerberosEtcProcessAuthentication(SocketClass *self, const void *opt)
{
	CSTR func = "DoKerberosEtcProcessAuthentication";
	SECURITY_STATUS	r = SEC_E_OK;
	const char *	cmd = NULL;
	BOOL		ret = 0, cCtxt = FALSE;
	KerberosEtcSpec	*ssd;

mylog("!!! %s in\n", __FUNCTION__);
	ssd = &(((SspiData *)(self->ssd))->kdata);
	if (SEC_E_OK != (r = PerformKerberosEtcClientHandshake(self, ssd, (size_t) opt)))
	{
		cmd = "PerformKerberosEtcClientHandshake";
		mylog("%s:%s failed\n", func, cmd);
		goto cleanup;
	}
mylog("!!! PerformKerberosEtcClientHandshake passed\n");
	cCtxt = TRUE;
	ret = TRUE;
cleanup:
	if (!ret)
	{
		SSPI_set_error(self, r, __FUNCTION__, cmd); 
		FreeCredentialHandle(&ssd->hKerbEtcCred);
		if (cCtxt)
		{
			DeleteSecurityContext(&ssd->hKerbEtcCtxt);
		}
		self->sspisvcs &= (~(KerberosService | NegotiateService));
	}
	return ret;
}

static
SECURITY_STATUS
CreateKerberosEtcCredentials(
	LPCTSTR	opt,		/* in */
	SEC_CHAR *packname,	/* in */
	LPCTSTR pszUserName,	/* in */
	PCredHandle phCreds)	/* out */
{
	TimeStamp	tsExpiry;
	SECURITY_STATUS	Status;

	/*
	 * Create an SSPI credential.
	 */

	Status = AcquireCredentialsHandle(
			NULL,			/* Name of principal */    
			packname,		/* Name of package */
			SECPKG_CRED_OUTBOUND,   /* Flags indicating use */
			NULL,			/* Pointer to logon ID */
			NULL,			/* Package specific data */
			NULL,			/* Pointer to GetKey() func */
			NULL,			/* Value to pass to GetKey() */
			phCreds,		/* (out) Cred Handle */
			&tsExpiry);		/* (out) Lifetime (optional) */
	if (Status != SEC_E_OK)
	{
		mylog("**** Error 0x%p returned by AcquireCredentialsHandle\n", Status);
		goto cleanup;
	}

cleanup:

    return Status;
}

static
SECURITY_STATUS
PerformKerberosEtcClientHandshake(
	SocketClass	*sock,		/* in */
	KerberosEtcSpec	*ssd,		/* i-o */
	size_t		inlen)
{
	SecBufferDesc	InBuffer;
	SecBuffer	InBuffers[1];
	SecBufferDesc	OutBuffer;
	SecBuffer	OutBuffers[1];
	DWORD		dwSSPIFlags;
	DWORD		dwSSPIOutFlags;
	TimeStamp	tsExpiry;
	SECURITY_STATUS	scRet;
	CtxtHandle	hContext;
	PBYTE		inbuf = NULL;

mylog("!!! inlen=%u svcprinc=%s\n", inlen, ssd->svcprinc); 
	if (ssd->ValidCtxt && inlen > 0)
	{
		if (NULL == (inbuf = malloc(inlen + 1)))
		{
			return SEC_E_INTERNAL_ERROR;
		}
		SOCK_get_n_char(sock, inbuf, inlen);
		if (SOCK_get_errcode(sock) != 0)
		{
			mylog("**** Error %d receiving data from server (1)\n", SOCK_ERRNO);
			free(inbuf);
			return SEC_E_INTERNAL_ERROR;
		}

		InBuffer.ulVersion = SECBUFFER_VERSION;
		InBuffer.cBuffers = 1;
		InBuffer.pBuffers = InBuffers;
		InBuffers[0].pvBuffer = inbuf; 
		InBuffers[0].cbBuffer = inlen; 
		InBuffers[0].BufferType = SECBUFFER_TOKEN; 
	}

	dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT	|
			ISC_REQ_REPLAY_DETECT	|
			ISC_REQ_CONFIDENTIALITY	|
			ISC_RET_EXTENDED_ERROR	|
			ISC_REQ_ALLOCATE_MEMORY	|
			ISC_REQ_STREAM;

	/*
	 *  Initiate a ClientHello message and generate a token.
	 */

	OutBuffers[0].pvBuffer	= NULL;
	OutBuffers[0].BufferType = SECBUFFER_TOKEN;
	OutBuffers[0].cbBuffer	= 0;

	OutBuffer.cBuffers = 1;
	OutBuffer.pBuffers = OutBuffers;
	OutBuffer.ulVersion = SECBUFFER_VERSION;

mylog("!!! before InitializeSecurityContext\n"); 
	scRet = InitializeSecurityContext(
					&ssd->hKerbEtcCred,
					ssd->ValidCtxt ? &ssd->hKerbEtcCtxt : NULL,
					ssd->svcprinc,
					dwSSPIFlags,
					0,
					SECURITY_NATIVE_DREP,
					ssd->ValidCtxt ? &InBuffer : NULL,
					0,
					&hContext,
					&OutBuffer,
					&dwSSPIOutFlags,
					&tsExpiry);
mylog("!!! %s:InitializeSecurityContext ret=%x\n", __FUNCTION__, scRet); 

	if (inbuf)
		free(inbuf);
	if (SEC_E_OK != scRet && SEC_I_CONTINUE_NEEDED != scRet)
	{
		mylog("**** Error %x returned by InitializeSecurityContext\n", scRet);
		return scRet;
	}
	if (!ssd->ValidCtxt)
	{
		memcpy(&ssd->hKerbEtcCtxt, &hContext, sizeof(CtxtHandle));
		ssd->ValidCtxt = TRUE;
	}

mylog("!!! cbBuffer=%d pvBuffer=%p\n", OutBuffers[0].cbBuffer, OutBuffers[0].pvBuffer); 
	/* Send response to server if there is one. */
	if (OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != NULL)
	{
		int	reslen = OutBuffers[0].cbBuffer;
mylog("!!! responding 'p' + int(%d) + %dbytes of data\n", reslen + 4, reslen); 
		SOCK_put_char(sock, 'p');
		SOCK_put_int(sock, reslen + 4, 4);
		SOCK_put_n_char(sock, OutBuffers[0].pvBuffer, reslen);
		SOCK_flush_output(sock);
		if (SOCK_get_errcode(sock) != 0)
		{
			mylog("**** Error %d sending data to server (1)\n", SOCK_ERRNO);
			FreeContextBuffer(OutBuffers[0].pvBuffer);
			return SEC_E_INTERNAL_ERROR;
		}

		mylog("%d bytes of handshake data sent\n", OutBuffers[0].cbBuffer);

		/* Free output buffer. */
		FreeContextBuffer(OutBuffers[0].pvBuffer);
		OutBuffers[0].pvBuffer = NULL;
	}

	return SEC_E_OK;
	// return KerberosEtcClientHandshakeLoop(Socket, ssd, TRUE, pExtraData);
}


int SSPI_recv(SocketClass *self, void *buffer, int len)
{
	CSTR func = "SSPI_recv";

	if (0 != (self->sspisvcs & SchannelService))
	{
		SECURITY_STATUS	scRet;
		SecBuffer	Buffers[4];
		SecBufferDesc	Message;
		SecBuffer	*pDataBuffer;
		SecBuffer	*pExtraBuffer;
		SecBuffer	ExtraBuffer;
		int	i, retry_count, reqlen, rtnlen = -1;
		PBYTE	pbIoBuffer;
		DWORD	cbIoBuffer, cbIoBufferLength;

		DWORD	cbData;
		SchannelSpec *ssd = &(((SspiData *)(self->ssd))->sdata);

mylog("buflen=%d,%d ovrlen=%d\n", ssd->iobuflen, ssd->ioread, ssd->ioovrlen);
		if (ssd->ioovrlen > 0)
		{
			if (rtnlen = ssd->ioovrlen, rtnlen > len)
				rtnlen = len;
			memmove(buffer, ssd->ioovrbuf, rtnlen);
			if (rtnlen < ssd->ioovrlen)
			{
				memmove(ssd->ioovrbuf, ssd->ioovrbuf + rtnlen, ssd->ioovrlen - rtnlen);
			}
			ssd->ioovrlen -= rtnlen;
			return rtnlen;
		}
		/*
		 * Read data from server until done.
		 */
		retry_count = 0;
		cbIoBufferLength = ((len - 1) / 16 + 1) * 16;
		pbIoBuffer = ssd->iobuf;
		cbIoBuffer = ssd->ioread;
		if (cbIoBuffer > 0)
			reqlen = 0;
		else
			reqlen = len - cbIoBuffer;
		while (TRUE)
		{
			/*
			 * Read some data.
			 */
mylog("buf=%p read=%d req=%d\n", pbIoBuffer, cbIoBuffer, reqlen);
			if (reqlen > 0)
			{
				if (cbIoBuffer + reqlen > ssd->iobuflen)
				{
					void *iobuf;

					cbIoBufferLength = cbIoBuffer + reqlen;
					iobuf = realloc(ssd->iobuf, cbIoBufferLength);
					if (NULL == iobuf)
					{
						mylog("failed to realloc ssd->iobuf\n");
						if (ssd->iobuf)
						{
							free(ssd->iobuf);
							ssd->iobuf = NULL;
						}
						ssd->iobuflen = 0;
						return -1;
					}
					pbIoBuffer = ssd->iobuf = iobuf;
					ssd->iobuflen = cbIoBufferLength;
				}
				cbData = recv(self->socket, 
						pbIoBuffer + cbIoBuffer, 
						reqlen, 
						RECV_FLAG);
				if (cbData == SOCKET_ERROR)
				{
					int	gerrno = SOCK_ERRNO;

					mylog("**** Error %d reading data from server\n", gerrno);
					switch (gerrno)
					{
						case EINTR:
							continue;
						case EWOULDBLOCK:
							retry_count++;
							if (Socket_wait_for_ready(self->socket, FALSE, retry_count) >= 0)
								continue;
						default:
							SOCK_ERRNO_SET(gerrno);
							scRet = SEC_E_INTERNAL_ERROR;
							break;
					}
					break;
				}
				else if (cbData == 0)
				{
					/* Server disconnected. */
					if (cbIoBuffer)
					{
						mylog("**** Server unexpectedly disconnected\n");
						scRet = SEC_E_INTERNAL_ERROR;
						goto cleanup;
					}
					else
					{
						rtnlen = 0;
						break;
					}
				}
				else
				{
					mylog("%d bytes of (encrypted) application data received\n", cbData);

                			cbIoBuffer += cbData;
					reqlen -= cbData;
					retry_count = 0;
            			}
        		}

			/* 
			 * Attempt to decrypt the received data.
			 */

			Buffers[0].pvBuffer     = pbIoBuffer;
			Buffers[0].cbBuffer     = cbIoBuffer;
			Buffers[0].BufferType   = SECBUFFER_DATA;

			Buffers[1].BufferType   = SECBUFFER_EMPTY;
			Buffers[2].BufferType   = SECBUFFER_EMPTY;
			Buffers[3].BufferType   = SECBUFFER_EMPTY;

			Message.ulVersion       = SECBUFFER_VERSION;
			Message.cBuffers        = 4;
			Message.pBuffers        = Buffers;

			scRet = DecryptMessage(&ssd->hCtxt, &Message, 0, NULL);

			if (scRet == SEC_E_INCOMPLETE_MESSAGE)
			{
				/* The input buffer contains only a fragment of an
				 * encrypted record. Loop around and read some more
				 * data.
				 */
				if (reqlen <= 0)
				{
					if (cbIoBuffer < len)
						reqlen = len - cbIoBuffer;
					else
						reqlen = len;
				}
				continue;
			}

			/* Server signalled end of session */
			if (scRet == SEC_I_CONTEXT_EXPIRED)
				break;

			if (scRet != SEC_E_OK && 
			    scRet != SEC_I_RENEGOTIATE) 
			{
				mylog("**** Error 0x%p returned by DecryptMessage\n", scRet);
				goto cleanup;
			}

			/* Locate data and (optional) extra buffers. */
			pDataBuffer  = NULL;
			pExtraBuffer = NULL;
			for(i = 1; i < 4; i++)
			{
				if (pDataBuffer == NULL && Buffers[i].BufferType == SECBUFFER_DATA)
				{
					pDataBuffer = &Buffers[i];
					mylog("%p Buffers[%d].BufferType = SECBUFFER_DATA\n", pDataBuffer->pvBuffer, i);
				}
				if (pExtraBuffer == NULL && Buffers[i].BufferType == SECBUFFER_EXTRA)
				{
					pExtraBuffer = &Buffers[i];
					mylog("%p Buffers[%d].BufferType = SECBUFFER_EXTRA\n", pExtraBuffer->pvBuffer, i);
				}
			}

			/* Display or otherwise process the decrypted data. */
			if (pDataBuffer)
			{
				mylog("Decrypted data: %d bytes\n", pDataBuffer->cbBuffer);
				if (len < pDataBuffer->cbBuffer)
				{
					rtnlen = len;
					memcpy(buffer, pDataBuffer->pvBuffer, rtnlen);

					ssd->ioovrlen = pDataBuffer->cbBuffer - len;
					ssd->ioovrbuf = realloc(ssd->ioovrbuf, ssd->ioovrlen);
					memcpy(ssd->ioovrbuf, (const char *) pDataBuffer->pvBuffer + len, ssd->ioovrlen); 
				}
				else
				{
					rtnlen = pDataBuffer->cbBuffer;
					memcpy(buffer, pDataBuffer->pvBuffer, rtnlen);
				}
			}

			/* Move any "extra" data to the input buffer. */
			if (pExtraBuffer)
			{
				mylog("Extra data: %d bytes\n", pExtraBuffer->cbBuffer);
				pbIoBuffer = ssd->iobuf;
				cbIoBuffer = pExtraBuffer->cbBuffer;
				memmove(pbIoBuffer, pExtraBuffer->pvBuffer, cbIoBuffer);
			}
			else
				cbIoBuffer = 0;
			ssd->ioread = cbIoBuffer;

			if (scRet == SEC_I_RENEGOTIATE)
			{
				/* The server wants to perform another handshake
				 * sequence.
				 */
				mylog("Server requested renegotiate!\n");
				scRet = SchannelClientHandshakeLoop(
							self->socket, 
							&ssd->hCred, 
							&ssd->hCtxt, 
							FALSE, 
							&ExtraBuffer);
				if (scRet != SEC_E_OK)
				{
					goto cleanup;
				}

				/* Move any "extra" data to the input buffer. */
				if (ExtraBuffer.pvBuffer)
				{
					memmove(pbIoBuffer, ExtraBuffer.pvBuffer, ExtraBuffer.cbBuffer);
					cbIoBuffer = ExtraBuffer.cbBuffer;
				}
			}
			break;
		}
cleanup:
		return rtnlen;
	}
	else
		return recv(self->socket, (char *) buffer, len, RECV_FLAG);
}

int SSPI_send(SocketClass *self, const void *buffer, int len)
{
	CSTR func = "SSPI_send";

	if (0 != (self->sspisvcs & SchannelService))
	{
		SecPkgContext_StreamSizes	sizes;
		int	ttllen, wrtlen, slen;
		LPVOID	lpHead;
		LPVOID	lpMsg;
		LPVOID	lpTrail;
		SecBuffer	sb[4];
		SecBufferDesc	sbd;
		SchannelSpec *ssd = &(((SspiData *)(self->ssd))->sdata);

		QueryContextAttributes(&ssd->hCtxt, SECPKG_ATTR_STREAM_SIZES, &sizes);
		slen = len;
		ttllen = sizes.cbHeader + len + sizes.cbTrailer;
		if (ttllen > sizes.cbMaximumMessage)
		{
			ttllen = sizes.cbMaximumMessage;
			slen = ttllen - sizes.cbHeader - sizes.cbTrailer;
		}
		lpHead = malloc(ttllen);
		lpMsg = (char *) lpHead + sizes.cbHeader;
		memcpy(lpMsg, buffer, slen);
		lpTrail = (char *) lpMsg + slen;

		sb[0].cbBuffer	= sizes.cbHeader;
		sb[0].pvBuffer	= lpHead;
		sb[0].BufferType = SECBUFFER_STREAM_HEADER;
		sb[1].cbBuffer	= slen;
		sb[1].pvBuffer	= lpMsg;
		sb[1].BufferType = SECBUFFER_DATA;
		sb[2].cbBuffer	= sizes.cbTrailer;
		sb[2].pvBuffer	= lpTrail;
		sb[2].BufferType = SECBUFFER_STREAM_TRAILER;
	/*	sb[3].cbBuffer	= 16;
		sb[3].pvBuffer	= lpPad; */
		sb[3].BufferType = SECBUFFER_EMPTY;

		sbd.cBuffers = 4;
		sbd.pBuffers = sb;
		sbd.ulVersion = SECBUFFER_VERSION;

		EncryptMessage(&ssd->hCtxt, 0, &sbd, 0);

mylog("EMPTY=%p %d %d\n", sb[3].pvBuffer, sb[3].cbBuffer, sb[3].BufferType);
		if (wrtlen = sendall(self->socket, lpHead, ttllen), wrtlen < 0)
		{
			int	gerrno = SOCK_ERRNO;

			free(lpHead);
			SOCK_ERRNO_SET(gerrno);
			return -1;
		}

		free(lpHead);
		return slen;
	}
	else
		return send(self->socket, (char *) buffer, len, SEND_FLAG);
}

void ReleaseSvcSpecData(SocketClass *self, UInt4 svc)
{
	if (!self->ssd)
		return;
	if (0 != (self->sspisvcs & (svc & SchannelService)))
	{
		SchannelSpec *ssd = &(((SspiData *)(self->ssd))->sdata);

		if (ssd->iobuf)
		{
			free(ssd->iobuf);
			ssd->iobuf = NULL;
		}
		if (ssd->ioovrbuf)
		{
			free(ssd->ioovrbuf);
			ssd->ioovrbuf = NULL;
		}
		FreeCredentialHandle(&ssd->hCred);
		DeleteSecurityContext(&ssd->hCtxt);
		self->sspisvcs &= (~SchannelService);
	}
	if (0 != (self->sspisvcs & (svc & (KerberosService | NegotiateService))))
	{
		KerberosEtcSpec *ssd = &(((SspiData *)(self->ssd))->kdata);

		if (ssd->svcprinc)
		{
			free(ssd->svcprinc);
			ssd->svcprinc = NULL;
		}
		FreeCredentialHandle(&ssd->hKerbEtcCred);
		if (ssd->ValidCtxt)
		{
			DeleteSecurityContext(&ssd->hKerbEtcCtxt);
			ssd->ValidCtxt = FALSE;
		}
		self->sspisvcs &= (~(KerberosService | NegotiateService));
	}
}
#endif /* USE_SSPI */
