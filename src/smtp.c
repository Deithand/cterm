#include "smtp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 1024

/* Base64 encoding for authentication */
static void base64_encode(const char *input, char *output, int output_size) {
    static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    int len = strlen(input);
    int out_idx = 0;

    while (len--) {
        char_array_3[i++] = *(input++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4 && out_idx < output_size - 1; i++) {
                output[out_idx++] = base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1 && out_idx < output_size - 1; j++) {
            output[out_idx++] = base64_chars[char_array_4[j]];
        }

        while(i++ < 3 && out_idx < output_size - 1) {
            output[out_idx++] = '=';
        }
    }

    output[out_idx] = '\0';
}

/* Read SMTP server response */
static int smtp_read_response(Connection *conn, char *buffer, int buffer_size) {
    int len = net_recv_line(conn, buffer, buffer_size);
    return len;
}

/* Check if SMTP response is positive */
static int smtp_check_response(const char *response, int expected_code) {
    int code = atoi(response);
    return (code == expected_code || code / 100 == expected_code / 100);
}

/* Connect to SMTP server */
int smtp_connect(SmtpSession *session, const char *host, int port, int use_ssl) {
    char response[BUFFER_SIZE];

    session->connected = 0;

    if (net_connect(host, port, use_ssl, &session->conn) < 0) {
        return -1;
    }

    /* Read greeting */
    smtp_read_response(&session->conn, response, sizeof(response));
    if (!smtp_check_response(response, 220)) {
        fprintf(stderr, "SMTP connection failed: %s\n", response);
        net_disconnect(&session->conn);
        return -1;
    }

    /* Send EHLO */
    char command[256];
    snprintf(command, sizeof(command), "EHLO localhost\r\n");
    net_send(&session->conn, command, strlen(command));

    /* Read EHLO response (may be multiline) */
    do {
        smtp_read_response(&session->conn, response, sizeof(response));
    } while (response[3] == '-'); /* Continue if response has continuation */

    if (!smtp_check_response(response, 250)) {
        fprintf(stderr, "SMTP EHLO failed: %s\n", response);
        net_disconnect(&session->conn);
        return -1;
    }

    session->connected = 1;
    return 0;
}

/* Upgrade connection to TLS using STARTTLS */
int smtp_starttls(SmtpSession *session) {
    char command[128];
    char response[BUFFER_SIZE];

    /* Send STARTTLS command */
    snprintf(command, sizeof(command), "STARTTLS\r\n");
    net_send(&session->conn, command, strlen(command));

    smtp_read_response(&session->conn, response, sizeof(response));
    if (!smtp_check_response(response, 220)) {
        fprintf(stderr, "SMTP STARTTLS failed: %s\n", response);
        return -1;
    }

    /* Setup SSL on existing connection */
    session->conn.use_ssl = 1;
    session->conn.ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!session->conn.ssl_ctx) {
        fprintf(stderr, "Error: Cannot create SSL context\n");
        return -1;
    }

    session->conn.ssl = SSL_new(session->conn.ssl_ctx);
    if (!session->conn.ssl) {
        fprintf(stderr, "Error: Cannot create SSL structure\n");
        SSL_CTX_free(session->conn.ssl_ctx);
        return -1;
    }

    SSL_set_fd(session->conn.ssl, session->conn.sockfd);

    if (SSL_connect(session->conn.ssl) <= 0) {
        fprintf(stderr, "Error: SSL handshake failed\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    /* Send EHLO again after STARTTLS */
    snprintf(command, sizeof(command), "EHLO localhost\r\n");
    net_send(&session->conn, command, strlen(command));

    do {
        smtp_read_response(&session->conn, response, sizeof(response));
    } while (response[3] == '-');

    return 0;
}

/* Authenticate using AUTH LOGIN */
int smtp_auth_login(SmtpSession *session, const char *username, const char *password) {
    char command[512];
    char response[BUFFER_SIZE];
    char encoded[512];

    /* Send AUTH LOGIN */
    snprintf(command, sizeof(command), "AUTH LOGIN\r\n");
    net_send(&session->conn, command, strlen(command));

    smtp_read_response(&session->conn, response, sizeof(response));
    if (!smtp_check_response(response, 334)) {
        fprintf(stderr, "SMTP AUTH LOGIN failed: %s\n", response);
        return -1;
    }

    /* Send username (base64 encoded) */
    base64_encode(username, encoded, sizeof(encoded));
    snprintf(command, sizeof(command), "%s\r\n", encoded);
    net_send(&session->conn, command, strlen(command));

    smtp_read_response(&session->conn, response, sizeof(response));
    if (!smtp_check_response(response, 334)) {
        fprintf(stderr, "SMTP AUTH username failed: %s\n", response);
        return -1;
    }

    /* Send password (base64 encoded) */
    base64_encode(password, encoded, sizeof(encoded));
    snprintf(command, sizeof(command), "%s\r\n", encoded);
    net_send(&session->conn, command, strlen(command));

    smtp_read_response(&session->conn, response, sizeof(response));
    if (!smtp_check_response(response, 235)) {
        fprintf(stderr, "SMTP AUTH password failed: %s\n", response);
        return -1;
    }

    return 0;
}

/* Disconnect from SMTP server */
void smtp_disconnect(SmtpSession *session) {
    char command[128];
    char response[BUFFER_SIZE];

    if (session->connected) {
        snprintf(command, sizeof(command), "QUIT\r\n");
        net_send(&session->conn, command, strlen(command));
        smtp_read_response(&session->conn, response, sizeof(response));
    }

    net_disconnect(&session->conn);
    session->connected = 0;
}

/* Send an email */
int smtp_send_email(SmtpSession *session,
                   const char *from,
                   const char *to,
                   const char *subject,
                   const char *body) {
    char command[1024];
    char response[BUFFER_SIZE];
    time_t now;
    struct tm *tm_info;
    char date_str[64];

    /* MAIL FROM */
    snprintf(command, sizeof(command), "MAIL FROM:<%s>\r\n", from);
    net_send(&session->conn, command, strlen(command));
    smtp_read_response(&session->conn, response, sizeof(response));
    if (!smtp_check_response(response, 250)) {
        fprintf(stderr, "SMTP MAIL FROM failed: %s\n", response);
        return -1;
    }

    /* RCPT TO */
    snprintf(command, sizeof(command), "RCPT TO:<%s>\r\n", to);
    net_send(&session->conn, command, strlen(command));
    smtp_read_response(&session->conn, response, sizeof(response));
    if (!smtp_check_response(response, 250)) {
        fprintf(stderr, "SMTP RCPT TO failed: %s\n", response);
        return -1;
    }

    /* DATA */
    snprintf(command, sizeof(command), "DATA\r\n");
    net_send(&session->conn, command, strlen(command));
    smtp_read_response(&session->conn, response, sizeof(response));
    if (!smtp_check_response(response, 354)) {
        fprintf(stderr, "SMTP DATA failed: %s\n", response);
        return -1;
    }

    /* Get current time for Date header */
    time(&now);
    tm_info = gmtime(&now);
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000", tm_info);

    /* Send message headers and body */
    snprintf(command, sizeof(command),
             "From: %s\r\n"
             "To: %s\r\n"
             "Subject: %s\r\n"
             "Date: %s\r\n"
             "\r\n"
             "%s\r\n"
             ".\r\n",
             from, to, subject, date_str, body);

    net_send(&session->conn, command, strlen(command));
    smtp_read_response(&session->conn, response, sizeof(response));
    if (!smtp_check_response(response, 250)) {
        fprintf(stderr, "SMTP message send failed: %s\n", response);
        return -1;
    }

    return 0;
}
