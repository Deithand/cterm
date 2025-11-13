#define _POSIX_C_SOURCE 200809L
#include "imap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#define BUFFER_SIZE 8192

/* Base64 decode table */
static const unsigned char base64_decode_table[256] = {
    ['A'] = 0,  ['B'] = 1,  ['C'] = 2,  ['D'] = 3,  ['E'] = 4,  ['F'] = 5,
    ['G'] = 6,  ['H'] = 7,  ['I'] = 8,  ['J'] = 9,  ['K'] = 10, ['L'] = 11,
    ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15, ['Q'] = 16, ['R'] = 17,
    ['S'] = 18, ['T'] = 19, ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
    ['Y'] = 24, ['Z'] = 25, ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29,
    ['e'] = 30, ['f'] = 31, ['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35,
    ['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39, ['o'] = 40, ['p'] = 41,
    ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45, ['u'] = 46, ['v'] = 47,
    ['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51, ['0'] = 52, ['1'] = 53,
    ['2'] = 54, ['3'] = 55, ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59,
    ['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63, ['='] = 0
};

/* Decode base64 string */
static int base64_decode(const char *input, int input_len, char *output, int output_size) {
    int i = 0, j = 0;
    unsigned char buf[4];
    int buf_pos = 0;

    while (i < input_len && j < output_size - 1) {
        unsigned char c = input[i++];

        if (c == '=' || c == '\0') break;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;

        buf[buf_pos++] = base64_decode_table[c];

        if (buf_pos == 4) {
            output[j++] = (buf[0] << 2) | (buf[1] >> 4);
            if (j < output_size - 1)
                output[j++] = (buf[1] << 4) | (buf[2] >> 2);
            if (j < output_size - 1)
                output[j++] = (buf[2] << 6) | buf[3];
            buf_pos = 0;
        }
    }

    /* Handle remaining bytes */
    if (buf_pos >= 2 && j < output_size - 1) {
        output[j++] = (buf[0] << 2) | (buf[1] >> 4);
        if (buf_pos >= 3 && j < output_size - 1) {
            output[j++] = (buf[1] << 4) | (buf[2] >> 2);
        }
    }

    output[j] = '\0';
    return j;
}

/* Decode MIME encoded-words (RFC 2047) - format: =?charset?encoding?encoded-text?= */
static void decode_mime_header(const char *input, char *output, int output_size) {
    const char *p = input;
    char *out = output;
    int out_len = 0;

    while (*p && out_len < output_size - 1) {
        /* Look for encoded-word start */
        if (p[0] == '=' && p[1] == '?') {
            const char *start = p;
            p += 2;

            /* Find charset end */
            const char *charset_end = strchr(p, '?');
            if (!charset_end) {
                /* Invalid format, copy literal */
                *out++ = *start++;
                out_len++;
                p = start;
                continue;
            }

            /* Get encoding type */
            p = charset_end + 1;
            char encoding = toupper(*p);

            if (encoding != 'B' && encoding != 'Q') {
                /* Unknown encoding, copy literal */
                *out++ = *start++;
                out_len++;
                p = start;
                continue;
            }

            /* Find encoded text */
            p++;
            if (*p != '?') {
                *out++ = *start++;
                out_len++;
                p = start;
                continue;
            }
            p++;

            /* Find end marker ?= */
            const char *encoded_end = strstr(p, "?=");
            if (!encoded_end) {
                /* No end marker, copy literal */
                *out++ = *start++;
                out_len++;
                p = start;
                continue;
            }

            /* Decode the encoded text */
            int encoded_len = encoded_end - p;
            char temp_buf[1024];

            if (encoding == 'B') {
                /* Base64 decode */
                int decoded_len = base64_decode(p, encoded_len, temp_buf, sizeof(temp_buf));
                if (decoded_len > 0 && out_len + decoded_len < output_size - 1) {
                    memcpy(out, temp_buf, decoded_len);
                    out += decoded_len;
                    out_len += decoded_len;
                }
            } else if (encoding == 'Q') {
                /* Quoted-printable decode (simplified) */
                for (int i = 0; i < encoded_len && out_len < output_size - 1; i++) {
                    if (p[i] == '_') {
                        *out++ = ' ';
                        out_len++;
                    } else if (p[i] == '=' && i + 2 < encoded_len) {
                        /* Decode hex */
                        char hex[3] = {p[i+1], p[i+2], 0};
                        *out++ = (char)strtol(hex, NULL, 16);
                        out_len++;
                        i += 2;
                    } else {
                        *out++ = p[i];
                        out_len++;
                    }
                }
            }

            p = encoded_end + 2;

            /* Skip whitespace between encoded words */
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
                /* Check if next is another encoded word */
                const char *next = p;
                while (*next == ' ' || *next == '\t' || *next == '\r' || *next == '\n') {
                    next++;
                }
                if (next[0] == '=' && next[1] == '?') {
                    /* Another encoded word follows, skip whitespace */
                    p = next;
                    break;
                }
                /* Not another encoded word, keep one space */
                if (out_len < output_size - 1 && out > output && *(out-1) != ' ') {
                    *out++ = ' ';
                    out_len++;
                }
                break;
            }
        } else {
            /* Regular character */
            *out++ = *p++;
            out_len++;
        }
    }

    *out = '\0';
}

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
    char temp_header[BUFFER_SIZE];

    strncpy(buffer, data, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    line = strtok_r(buffer, "\r\n", &saveptr);
    while (line != NULL) {
        if (strncasecmp(line, "Subject:", 8) == 0) {
            /* Collect full subject (may span multiple lines) */
            strncpy(temp_header, line + 9, sizeof(temp_header) - 1);
            temp_header[sizeof(temp_header) - 1] = '\0';

            /* Check for continuation lines (start with space or tab) */
            char *next_line = strtok_r(NULL, "\r\n", &saveptr);
            while (next_line && (next_line[0] == ' ' || next_line[0] == '\t')) {
                /* Append continuation line, removing leading whitespace */
                strncat(temp_header, next_line, sizeof(temp_header) - strlen(temp_header) - 1);
                next_line = strtok_r(NULL, "\r\n", &saveptr);
            }

            /* Decode MIME header and store */
            decode_mime_header(temp_header, email->subject, MAX_SUBJECT_LEN);
            email->subject[MAX_SUBJECT_LEN - 1] = '\0';

            /* Continue from the line we just read */
            line = next_line;
            continue;
        } else if (strncasecmp(line, "From:", 5) == 0) {
            strncpy(temp_header, line + 6, sizeof(temp_header) - 1);
            temp_header[sizeof(temp_header) - 1] = '\0';
            decode_mime_header(temp_header, email->from, MAX_FROM_LEN);
            email->from[MAX_FROM_LEN - 1] = '\0';
        } else if (strncasecmp(line, "Date:", 5) == 0) {
            strncpy(email->date, line + 6, sizeof(email->date) - 1);
            email->date[sizeof(email->date) - 1] = '\0';
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
    char *body_start, *body_end;

    snprintf(command, sizeof(command),
             "A%d UID FETCH %u BODY[TEXT]",
             session->tag_counter++, uid);

    if (imap_send_command(session, command, response, sizeof(response)) < 0) {
        return -1;
    }

    /* Find body start - look for the opening brace and skip IMAP protocol data */
    body_start = strstr(response, "BODY[TEXT] {");
    if (body_start) {
        /* Skip to after the {size} and the following \r\n */
        body_start = strchr(body_start, '\n');
        if (body_start) {
            body_start++; /* Skip the \n */
        }
    } else {
        /* Try alternative format */
        body_start = strstr(response, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
        }
    }

    if (body_start) {
        strncpy(email->body, body_start, MAX_BODY_LEN - 1);
        email->body[MAX_BODY_LEN - 1] = '\0';

        /* Remove trailing IMAP protocol markers */
        /* Remove trailing )\r\n or ) */
        body_end = email->body + strlen(email->body) - 1;
        while (body_end > email->body && (*body_end == ')' || *body_end == '\r' || *body_end == '\n' || *body_end == ' ')) {
            *body_end = '\0';
            body_end--;
        }

        /* Remove any leading whitespace */
        char *start = email->body;
        while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
            start++;
        }
        if (start != email->body) {
            memmove(email->body, start, strlen(start) + 1);
        }
    } else {
        /* No body found */
        strcpy(email->body, "(Empty message)");
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
