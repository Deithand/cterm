#ifndef IMAP_H
#define IMAP_H

#include "network.h"
#include <time.h>

#define MAX_SUBJECT_LEN 256
#define MAX_FROM_LEN 128
#define MAX_BODY_LEN 4096

typedef struct {
    unsigned int uid;
    char subject[MAX_SUBJECT_LEN];
    char from[MAX_FROM_LEN];
    char date[64];
    int seen;
    int deleted;
    char body[MAX_BODY_LEN];
} Email;

typedef struct {
    Connection conn;
    int logged_in;
    int tag_counter;
    Email *emails;
    int email_count;
    int email_capacity;
} ImapSession;

/* Session management */
int imap_connect(ImapSession *session, const char *host, int port, int use_ssl);
int imap_login(ImapSession *session, const char *username, const char *password);
void imap_disconnect(ImapSession *session);

/* Mailbox operations */
int imap_select_mailbox(ImapSession *session, const char *mailbox);
int imap_fetch_emails(ImapSession *session);
int imap_fetch_email_body(ImapSession *session, unsigned int uid, Email *email);

/* Email operations */
int imap_mark_seen(ImapSession *session, unsigned int uid);
int imap_mark_unseen(ImapSession *session, unsigned int uid);
int imap_delete_email(ImapSession *session, unsigned int uid);
int imap_expunge(ImapSession *session);

/* Utility functions */
void imap_free_emails(ImapSession *session);

#endif /* IMAP_H */
