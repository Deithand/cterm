#ifndef NETWORK_H
#define NETWORK_H

#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct {
    int sockfd;
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    int use_ssl;
} Connection;

/* Connection management */
int net_connect(const char *host, int port, int use_ssl, Connection *conn);
void net_disconnect(Connection *conn);

/* Data transmission */
int net_send(Connection *conn, const char *data, int len);
int net_recv(Connection *conn, char *buffer, int buffer_size);
int net_recv_line(Connection *conn, char *buffer, int buffer_size);

/* SSL/TLS utilities */
int net_init_ssl(void);
void net_cleanup_ssl(void);

#endif /* NETWORK_H */
