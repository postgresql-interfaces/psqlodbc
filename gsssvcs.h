#ifdef	USE_GSS
int pg_GSS_continue(ConnectionClass *conn, Int4 inlen);
int pg_GSS_startup(ConnectionClass *conn, void *opt);
void pg_GSS_cleanup(SocketClass *sock);
#endif   /* USE_GSS */
