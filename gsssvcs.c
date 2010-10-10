#ifndef	WIN32
#include	"config.h"
#endif /* WIN32 */

#ifdef	USE_GSS
#if defined(WIN32) && defined( _MSC_VER)
#ifdef	_WIN64
#pragma comment(lib, "gssapi64")
#else
#pragma comment(lib, "gssapi32")
#endif /* _WIN64 */
#endif /* WIN32 */

#include "connection.h"
#ifdef	_WIN64
CSTR	GSSAPIDLL = "gssapi64";
#else
CSTR	GSSAPIDLL = "gssapi32";
#endif /* _WIN64 */
#include "socket.h"
#include "environ.h"
#ifndef	MAXHOSTNAMELEN
#include <netdb.h>
#endif

#ifndef	STATUS_ERROR
#define	STATUS_ERROR (-1)
#endif
#ifndef	STATUS_OK
#define	STATUS_OK (0)
#endif

/*
 * GSSAPI authentication system.
 */

#if defined(WIN32) && !defined(_MSC_VER)
/*
 * MIT Kerberos GSSAPI DLL doesn't properly export the symbols for MingW
 * that contain the OIDs required. Redefine here, values copied
 * from src/athena/auth/krb5/src/lib/gssapi/generic/gssapi_generic.c
 */
static const gss_OID_desc GSS_C_NT_HOSTBASED_SERVICE_desc =
{10, (void *) "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04"};
static GSS_DLLIMP gss_OID GSS_C_NT_HOSTBASED_SERVICE = &GSS_C_NT_HOSTBASED_SERVICE_desc;
#endif

/*
 * Fetch all errors of a specific type and append to "str".
 */
static void
pg_GSS_error_int(ConnectionClass *conn, const char *mprefix,
				 OM_uint32 stat, int type)
{
	char		*errmsg, *nerrmsg;
	size_t		alclen;
	OM_uint32	lmaj_s,
				lmin_s;
	gss_buffer_desc lmsg;
	OM_uint32	msg_ctx = 0;

	do
	{
		lmaj_s = gss_display_status(&lmin_s, stat, type,
									GSS_C_NO_OID, &msg_ctx, &lmsg);

		errmsg = CC_get_errormsg(conn);
		alclen = strlen(errmsg) + strlen(mprefix) + 2 + strlen(lmsg.value) + strlen("\n") + 1;
		nerrmsg = malloc(alclen);
		snprintf(nerrmsg, alclen, "%s%s: %s\n", errmsg, mprefix, (char *) lmsg.value);
		CC_set_errormsg(conn, nerrmsg);
		free(nerrmsg);

		gss_release_buffer(&lmin_s, &lmsg);
	} while (msg_ctx);
}

/*
 * GSSAPI errors contain two parts; put both into conn->errorMessage.
 */
static void
pg_GSS_error(const char *mprefix, ConnectionClass *conn,
			 OM_uint32 maj_stat, OM_uint32 min_stat)
{
	CC_set_errormsg(conn, "");

	/* Fetch major error codes */
	pg_GSS_error_int(conn, mprefix, maj_stat, GSS_C_GSS_CODE);

	/* Add the minor codes as well */
	pg_GSS_error_int(conn, mprefix, min_stat, GSS_C_MECH_CODE);
}

/*
 * Continue GSS authentication with next token as needed.
 */
int pg_GSS_continue(ConnectionClass *conn, Int4 inlen)
{
	SocketClass	*sock = CC_get_socket(conn);
	OM_uint32	maj_stat,
				min_stat,
				lmin_s;
	gss_buffer_desc	ginbuf, goutbuf;

	ginbuf.length = 0;
mylog("!!! %s inlen=%d in\n", __FUNCTION__, inlen);
	if (sock->gctx != GSS_C_NO_CONTEXT)
	{
mylog("!!! %s -1\n", __FUNCTION__);
		if (inlen > 0)
		{
			if (NULL == (ginbuf.value = malloc(inlen)))
			{
				CC_set_error(conn, CONN_NO_MEMORY_ERROR,
					"realloc error during autehntication", __FUNCTION__);
				return STATUS_ERROR;
			}
mylog("!!! %s -2\n", __FUNCTION__);
			ginbuf.length = inlen;
			SOCK_get_n_char(conn->sock, ginbuf.value, inlen);
			if (0 != SOCK_get_errcode(conn->sock))
			{
				CC_set_error(conn, CONN_INVALID_AUTHENTICATION,
					"communication error during autehntication", __FUNCTION__); 
				free(ginbuf.value);
				return STATUS_ERROR;
			} 
		}
	}

mylog("!!! %s 0\n", __FUNCTION__);
	maj_stat = gss_init_sec_context(&min_stat,
									GSS_C_NO_CREDENTIAL,
									&sock->gctx,
									sock->gtarg_nam,
									GSS_C_NO_OID,
									GSS_C_MUTUAL_FLAG,
									0,
									GSS_C_NO_CHANNEL_BINDINGS,
		  (sock->gctx == GSS_C_NO_CONTEXT) ? GSS_C_NO_BUFFER : &ginbuf,
									NULL,
									&goutbuf,
									NULL,
									NULL);

mylog("!!! %s 1\n", __FUNCTION__);
	if (ginbuf.length != 0)
	{
		free(ginbuf.value);
		ginbuf.length = 0;
	}

mylog("!!! %s 1-1 outlen=%d\n", __FUNCTION__, goutbuf.length);
	if (goutbuf.length != 0)
	{
		SocketClass	*sock = conn->sock;
		int		slen;
		/*
		 * GSS generated data to send to the server. We don't care if it's the
		 * first or subsequent packet, just send the same kind of password
		 * packet.
		 */
mylog("!!! %s 2\n", __FUNCTION__);
		if (PROTOCOL_74(&(conn->connInfo)))
			SOCK_put_char(sock, 'p');
		slen = goutbuf.length;
		SOCK_put_int(sock, slen + 4, 4);
		SOCK_put_n_char(sock, goutbuf.value, slen);
		SOCK_flush_output(sock);
		gss_release_buffer(&lmin_s, &goutbuf);
		goutbuf.length = 0;
		if (0 != SOCK_get_errcode(sock))
		{
mylog("!!! %s 3\n", __FUNCTION__);
			CC_set_error(conn, CONN_INVALID_AUTHENTICATION,
				"communication error during autehntication", __FUNCTION__); 
		}
	}

mylog("!!! %s maj_stat=%d\n", __FUNCTION__, maj_stat);
	if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED)
	{
mylog("!!! %s 5\n", __FUNCTION__);
		pg_GSS_error("GSSAPI continuation error",
					 conn,
					 maj_stat, min_stat);
		gss_release_name(&lmin_s, &sock->gtarg_nam);
		if (sock->gctx)
			gss_delete_sec_context(&lmin_s, &sock->gctx, GSS_C_NO_BUFFER);
		return STATUS_ERROR;
	}
mylog("!!! %s 6\n", __FUNCTION__);

	if (maj_stat == GSS_S_COMPLETE)
		gss_release_name(&lmin_s, &sock->gtarg_nam);

	return STATUS_OK;
}

/*
 *	This is needed to make /Delayload:gssapi32(64).dll possible
 *	under Windows.
 */
static gss_OID get_c_nt_hostbased_service()
{
#if	defined(WIN32) && defined(_MSC_VER)
	static FARPROC	proc = NULL;

	shortterm_common_lock();
	if (NULL == proc)
	{
		HMODULE	hmodule = GetModuleHandle(GSSAPIDLL);

		if (NULL != hmodule)
			proc = GetProcAddress(hmodule, "GSS_C_NT_HOSTBASED_SERVICE");
	}
	shortterm_common_unlock();
	if (NULL != proc)
		return *((gss_OID *) proc);
	return 0;
#else
	return	GSS_C_NT_HOSTBASED_SERVICE;
#endif
}

/*
 * Send initial GSS authentication token
 */
int pg_GSS_startup(ConnectionClass *conn, void *opt)
{
	SocketClass	*sock = CC_get_socket(conn);
	OM_uint32	maj_stat,
				min_stat;
	gss_buffer_desc temp_gbuf;

mylog("!!! %s in\n", __FUNCTION__);
	if (sock->gctx)
	{
		char	mesg[100];

mylog("!!! %s 0\n", __FUNCTION__);
		snprintf(mesg, sizeof(mesg), "duplicate GSS authentication request");
		CC_set_error(conn, CONN_INVALID_AUTHENTICATION, 
				mesg, __FUNCTION__);
		free(opt);
		return STATUS_ERROR;
	}
mylog("!!! %s 2\n", __FUNCTION__);

	/*
	 * Import service principal name so the proper ticket can be acquired by
	 * the GSSAPI system.
	 */
	temp_gbuf.value = (char *) opt;
	temp_gbuf.length = strlen(temp_gbuf.value);

mylog("!!! temp_gbuf.value=%s\n", temp_gbuf.value);
	maj_stat = gss_import_name(&min_stat, &temp_gbuf,
				   // GSS_C_NT_HOSTBASED_SERVICE,
				   get_c_nt_hostbased_service(),
		 &sock->gtarg_nam);
	free(temp_gbuf.value);

	if (maj_stat != GSS_S_COMPLETE)
	{
mylog("!!! %s 3\n", __FUNCTION__);
		pg_GSS_error("GSSAPI name import error",
					 conn,
					 maj_stat, min_stat);
		return STATUS_ERROR;
	}

mylog("!!! %s 4\n", __FUNCTION__);
	/*
	 * Initial packet is the same as a continuation packet with no initial
	 * context.
	 */
	sock->gctx = GSS_C_NO_CONTEXT;

	return pg_GSS_continue(conn, 0);
}

void pg_GSS_cleanup(SocketClass *sock)
{
	OM_uint32	min_s;

	if (sock->gctx)
	{
		gss_delete_sec_context(&min_s, &sock->gctx, GSS_C_NO_BUFFER);
	}
	if (sock->gtarg_nam)
	{
		gss_release_name(&min_s, &sock->gtarg_nam);
	}
}
#endif   /* USE_GSS */
