#define _POSIX_C_SOURCE 200809L
#include "imap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#define BUFFER_SIZE 8192

/* Send IMAP command and get response */
static int imap_send_command(ImapSession *session, const char *command, char *response, int response_size) {
    char buffer[BUFFER_SIZE];
    int len;

    /* Send command */
    len = snprintf(buffer, sizeof(buffer), "%s\r\n", command);
    if (net_send(&session->conn, buffer, len) < 0) {
        return -1;
    }

    /* Read response */
    response[0] = '\0';
    int total = 0;
    while (total < response_size - 1) {
        len = net_recv_line(&session->conn, buffer, sizeof(buffer));
        if (len <= 0) break;

        /* Append to response */
        if (total + len < response_size) {
            strcat(response, buffer);
            total += len;
        }

        /* Check for completion (tagged response) */
        if (strstr(buffer, "OK") || strstr(buffer, "NO") || strstr(buffer, "BAD")) {
            if (buffer[0] == 'A' && isdigit(buffer[1])) {
                break; /* Tagged response */
            }
        }
    }

    return total;
}

/* Connect to IMAP server */
int imap_connect(ImapSession *session, const char *host, int port, int use_ssl) {
    char response[BUFFER_SIZE];

    session->logged_in = 0;
    session->tag_counter = 1;
    session->emails = NULL;
    session->email_count = 0;
    session->email_capacity = 0;

    if (net_connect(host, port, use_ssl, &session->conn) < 0) {
        return -1;
    }

    /* Read greeting */
    net_recv_line(&session->conn, response, sizeof(response));

    return 0;
}

/* Login to IMAP server */
int imap_login(ImapSession *session, const char *username, const char *password) {
    char command[512];
    char response[BUFFER_SIZE];

    snprintf(command, sizeof(command), "A%d LOGIN %s %s",
             session->tag_counter++, username, password);

    if (imap_send_command(session, command, response, sizeof(response)) < 0) {
        return -1;
    }

    if (strstr(response, "OK") == NULL) {
        fprintf(stderr, "IMAP login failed\n");
        return -1;
    }

    session->logged_in = 1;
    return 0;
}

/* Disconnect from IMAP server */
void imap_disconnect(ImapSession *session) {
    char command[128];
    char response[BUFFER_SIZE];

    if (session->logged_in) {
        snprintf(command, sizeof(command), "A%d LOGOUT", session->tag_counter++);
        imap_send_command(session, command, response, sizeof(response));
    }

    net_disconnect(&session->conn);
    imap_free_emails(session);
}

/* Select mailbox (e.g., INBOX) */
int imap_select_mailbox(ImapSession *session, const char *mailbox) {
    char command[256];
    char response[BUFFER_SIZE];

    snprintf(command, sizeof(command), "A%d SELECT %s", session->tag_counter++, mailbox);

    if (imap_send_command(session, command, response, sizeof(response)) < 0) {
        return -1;
    }

    if (strstr(response, "OK") == NULL) {
        fprintf(stderr, "IMAP select mailbox failed\n");
        return -1;
    }

    return 0;
}

/* Parse email header from FETCH response */
static void parse_email_header(const char *data, Email *email) {
    char *line, *saveptr;
    char buffer[BUFFER_SIZE];

    strncpy(buffer, data, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    line = strtok_r(buffer, "\r\n", &saveptr);
    while (line != NULL) {
        if (strncasecmp(line, "Subject:", 8) == 0) {
            strncpy(email->subject, line + 9, MAX_SUBJECT_LEN - 1);
        } else if (strncasecmp(line, "From:", 5) == 0) {
            strncpy(email->from, line + 6, MAX_FROM_LEN - 1);
        } else if (strncasecmp(line, "Date:", 5) == 0) {
            strncpy(email->date, line + 6, sizeof(email->date) - 1);
        }
        line = strtok_r(NULL, "\r\n", &saveptr);
    }
}

/* Fetch list of emails from current mailbox */
int imap_fetch_emails(ImapSession *session) {
    char command[256];
    char response[BUFFER_SIZE * 4];
    char *line, *saveptr;

    /* Free previous emails */
    imap_free_emails(session);

    /* Fetch email headers */
    snprintf(command, sizeof(command),
             "A%d FETCH 1:* (UID FLAGS BODY.PEEK[HEADER.FIELDS (FROM SUBJECT DATE)])",
             session->tag_counter++);

    if (imap_send_command(session, command, response, sizeof(response)) < 0) {
        return -1;
    }

    /* Parse response */
    session->email_capacity = 10;
    session->emails = malloc(sizeof(Email) * session->email_capacity);
    session->email_count = 0;

    line = strtok_r(response, "\n", &saveptr);
    while (line != NULL) {
        /* Look for FETCH responses */
        if (strstr(line, "FETCH")) {
            if (session->email_count >= session->email_capacity) {
                session->email_capacity *= 2;
                session->emails = realloc(session->emails, sizeof(Email) * session->email_capacity);
            }

            Email *email = &session->emails[session->email_count];
            memset(email, 0, sizeof(Email));

            /* Extract UID */
            char *uid_str = strstr(line, "UID ");
            if (uid_str) {
                email->uid = atoi(uid_str + 4);
            }

            /* Check FLAGS for \Seen */
            if (strstr(line, "\\Seen")) {
                email->seen = 1;
            }

            /* Parse headers */
            char header_buffer[BUFFER_SIZE] = {0};
            int header_len = 0;

            /* Collect header lines */
            line = strtok_r(NULL, "\n", &saveptr);
            while (line != NULL) {
                if (strstr(line, ")") && strlen(line) < 5) break; /* End of headers */

                if (header_len + strlen(line) < sizeof(header_buffer) - 2) {
                    strcat(header_buffer, line);
                    strcat(header_buffer, "\r\n");
                    header_len += strlen(line) + 2;
                }

                line = strtok_r(NULL, "\n", &saveptr);
            }

            parse_email_header(header_buffer, email);

            /* Default values if parsing failed */
            if (strlen(email->subject) == 0) {
                strcpy(email->subject, "(No subject)");
            }
            if (strlen(email->from) == 0) {
                strcpy(email->from, "(Unknown)");
            }

            session->email_count++;
            continue;
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    return session->email_count;
}

/* Fetch email body */
int imap_fetch_email_body(ImapSession *session, unsigned int uid, Email *email) {
    char command[256];
    char response[BUFFER_SIZE * 2];
    char *body_start;

    snprintf(command, sizeof(command),
             "A%d UID FETCH %u BODY[TEXT]",
             session->tag_counter++, uid);

    if (imap_send_command(session, command, response, sizeof(response)) < 0) {
        return -1;
    }

    /* Find body start */
    body_start = strstr(response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        strncpy(email->body, body_start, MAX_BODY_LEN - 1);
        email->body[MAX_BODY_LEN - 1] = '\0';

        /* Remove trailing ) */
        char *end = strrchr(email->body, ')');
        if (end && end > email->body) {
            *end = '\0';
        }
    }

    return 0;
}

/* Mark email as seen */
int imap_mark_seen(ImapSession *session, unsigned int uid) {
    char command[256];
    char response[BUFFER_SIZE];

    snprintf(command, sizeof(command),
             "A%d UID STORE %u +FLAGS (\\Seen)",
             session->tag_counter++, uid);

    return imap_send_command(session, command, response, sizeof(response));
}

/* Mark email as unseen */
int imap_mark_unseen(ImapSession *session, unsigned int uid) {
    char command[256];
    char response[BUFFER_SIZE];

    snprintf(command, sizeof(command),
             "A%d UID STORE %u -FLAGS (\\Seen)",
             session->tag_counter++, uid);

    return imap_send_command(session, command, response, sizeof(response));
}

/* Delete email (mark as deleted) */
int imap_delete_email(ImapSession *session, unsigned int uid) {
    char command[256];
    char response[BUFFER_SIZE];

    snprintf(command, sizeof(command),
             "A%d UID STORE %u +FLAGS (\\Deleted)",
             session->tag_counter++, uid);

    return imap_send_command(session, command, response, sizeof(response));
}

/* Expunge deleted emails */
int imap_expunge(ImapSession *session) {
    char command[128];
    char response[BUFFER_SIZE];

    snprintf(command, sizeof(command), "A%d EXPUNGE", session->tag_counter++);

    return imap_send_command(session, command, response, sizeof(response));
}

/* Free email list */
void imap_free_emails(ImapSession *session) {
    if (session->emails) {
        free(session->emails);
        session->emails = NULL;
    }
    session->email_count = 0;
    session->email_capacity = 0;
}
