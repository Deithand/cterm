#ifndef SMTP_H
#define SMTP_H

#include "network.h"

typedef struct {
    Connection conn;
    int connected;
} SmtpSession;

/* Session management */
int smtp_connect(SmtpSession *session, const char *host, int port, int use_ssl);
int smtp_starttls(SmtpSession *session);
int smtp_auth_login(SmtpSession *session, const char *username, const char *password);
void smtp_disconnect(SmtpSession *session);

/* Email sending */
int smtp_send_email(SmtpSession *session,
                   const char *from,
                   const char *to,
                   const char *subject,
                   const char *body);

#endif /* SMTP_H */
