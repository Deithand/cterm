#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

/* Initialize SSL library */
int net_init_ssl(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    return 0;
}

/* Cleanup SSL library */
void net_cleanup_ssl(void) {
    EVP_cleanup();
    ERR_free_strings();
}

/* Establish TCP connection */
static int create_socket(const char *host, int port) {
    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent *server;

    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    /* Resolve hostname */
    server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "Error: Cannot resolve host %s\n", host);
        close(sockfd);
        return -1;
    }

    /* Setup server address structure */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    server_addr.sin_port = htons(port);

    /* Connect */
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/* Connect to a server (with optional SSL) */
int net_connect(const char *host, int port, int use_ssl, Connection *conn) {
    conn->sockfd = -1;
    conn->ssl = NULL;
    conn->ssl_ctx = NULL;
    conn->use_ssl = use_ssl;

    /* Create TCP socket */
    conn->sockfd = create_socket(host, port);
    if (conn->sockfd < 0) {
        return -1;
    }

    /* Setup SSL if needed */
    if (use_ssl) {
        conn->ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (!conn->ssl_ctx) {
            fprintf(stderr, "Error: Cannot create SSL context\n");
            close(conn->sockfd);
            return -1;
        }

        /* Set SSL options for compatibility */
        SSL_CTX_set_options(conn->ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

        conn->ssl = SSL_new(conn->ssl_ctx);
        if (!conn->ssl) {
            fprintf(stderr, "Error: Cannot create SSL structure\n");
            SSL_CTX_free(conn->ssl_ctx);
            close(conn->sockfd);
            return -1;
        }

        SSL_set_fd(conn->ssl, conn->sockfd);

        /* Perform SSL handshake */
        if (SSL_connect(conn->ssl) <= 0) {
            fprintf(stderr, "Error: SSL handshake failed\n");
            ERR_print_errors_fp(stderr);
            SSL_free(conn->ssl);
            SSL_CTX_free(conn->ssl_ctx);
            close(conn->sockfd);
            return -1;
        }
    }

    return 0;
}

/* Disconnect from server */
void net_disconnect(Connection *conn) {
    if (conn->ssl) {
        SSL_shutdown(conn->ssl);
        SSL_free(conn->ssl);
        conn->ssl = NULL;
    }
    if (conn->ssl_ctx) {
        SSL_CTX_free(conn->ssl_ctx);
        conn->ssl_ctx = NULL;
    }
    if (conn->sockfd >= 0) {
        close(conn->sockfd);
        conn->sockfd = -1;
    }
}

/* Send data over connection */
int net_send(Connection *conn, const char *data, int len) {
    int bytes_sent;

    if (conn->use_ssl && conn->ssl) {
        bytes_sent = SSL_write(conn->ssl, data, len);
    } else {
        bytes_sent = write(conn->sockfd, data, len);
    }

    return bytes_sent;
}

/* Receive data from connection */
int net_recv(Connection *conn, char *buffer, int buffer_size) {
    int bytes_received;

    if (conn->use_ssl && conn->ssl) {
        bytes_received = SSL_read(conn->ssl, buffer, buffer_size - 1);
    } else {
        bytes_received = read(conn->sockfd, buffer, buffer_size - 1);
    }

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
    }

    return bytes_received;
}

/* Receive a line of data (until \n) */
int net_recv_line(Connection *conn, char *buffer, int buffer_size) {
    int total = 0;
    char c;
    int n;

    while (total < buffer_size - 1) {
        if (conn->use_ssl && conn->ssl) {
            n = SSL_read(conn->ssl, &c, 1);
        } else {
            n = read(conn->sockfd, &c, 1);
        }

        if (n <= 0) {
            break;
        }

        buffer[total++] = c;

        if (c == '\n') {
            break;
        }
    }

    buffer[total] = '\0';
    return total;
}
