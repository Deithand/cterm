#ifndef GUI_H
#define GUI_H

#include "config.h"
#include "imap.h"
#include "smtp.h"

typedef struct {
    void *window;         /* GtkWidget* window */
    void *notebook;       /* GtkWidget* notebook */
    void *email_list;     /* GtkWidget* tree view */
    void *email_store;    /* GtkListStore* */
    void *text_view;      /* GtkWidget* text view for email body */
    void *compose_to;     /* GtkWidget* entry */
    void *compose_subject;/* GtkWidget* entry */
    void *compose_body;   /* GtkWidget* text view */
    void *status_bar;     /* GtkWidget* status bar */

    ImapSession *imap;
    SmtpSession *smtp;
    Config *config;

    int selected_email_index;
} GUIContext;

/* Initialize GUI */
int gui_init(GUIContext *ctx, ImapSession *imap, SmtpSession *smtp, Config *config, int *argc, char ***argv);

/* Run main GUI loop */
void gui_run(GUIContext *ctx);

/* Cleanup GUI resources */
void gui_cleanup(GUIContext *ctx);

/* Refresh email list */
void gui_refresh_emails(GUIContext *ctx);

#endif /* GUI_H */
