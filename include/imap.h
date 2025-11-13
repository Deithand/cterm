#ifndef IMAP_H
#define IMAP_H

#include "network.h"
#include <time.h>

#define MAX_SUBJECT_LEN 256
#define MAX_FROM_LEN 128
#define MAX_BODY_LEN 4096
#define MAX_FILENAME_LEN 256
#define MAX_ATTACHMENTS 10
#define MAX_MAILBOX_NAME 128

typedef struct {
    char filename[MAX_FILENAME_LEN];
    char mime_type[64];
    unsigned long size;
    char encoding[32];
    int part_number;
} Attachment;

typedef struct {
    unsigned int uid;
    char subject[MAX_SUBJECT_LEN];
    char from[MAX_FROM_LEN];
    char to[MAX_FROM_LEN];
    char date[64];
    int seen;
    int deleted;
    int has_attachments;
    int attachment_count;
    Attachment attachments[MAX_ATTACHMENTS];
    char body[MAX_BODY_LEN];
    char body_html[MAX_BODY_LEN];
    int is_html;
} Email;

typedef struct {
    char name[MAX_MAILBOX_NAME];
    int exists;
    int recent;
    int unseen;
} Mailbox;

typedef struct {
    Connection conn;
    int logged_in;
    int tag_counter;
    Email *emails;
    int email_count;
    int email_capacity;
    char current_mailbox[MAX_MAILBOX_NAME];
    Mailbox *mailboxes;
    int mailbox_count;
    int mailbox_capacity;
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

/* Mailbox operations */
int imap_list_mailboxes(ImapSession *session);
int imap_create_mailbox(ImapSession *session, const char *mailbox);
int imap_delete_mailbox(ImapSession *session, const char *mailbox);
void imap_free_mailboxes(ImapSession *session);

/* Search and filter */
int imap_search_emails(ImapSession *session, const char *criteria);
int imap_filter_by_subject(ImapSession *session, const char *subject);
int imap_filter_by_sender(ImapSession *session, const char *sender);
int imap_filter_unseen(ImapSession *session);

/* Attachment operations */
int imap_fetch_attachment(ImapSession *session, unsigned int uid, int part_num, const char *filename);
int imap_get_attachments(ImapSession *session, unsigned int uid, Email *email);

#endif /* IMAP_H */
