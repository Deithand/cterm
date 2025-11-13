#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include "imap.h"
#include "smtp.h"
#include "config.h"
#include "addressbook.h"

typedef struct {
    /* GTK Widgets */
    GtkWidget *window;
    GtkWidget *main_box;
    GtkWidget *toolbar;
    GtkWidget *paned;

    /* Left panel - folders */
    GtkWidget *folder_tree;
    GtkListStore *folder_store;

    /* Top right - email list */
    GtkWidget *email_list;
    GtkListStore *email_store;

    /* Bottom right - email content */
    GtkWidget *email_view;
    GtkTextBuffer *email_buffer;

    /* Status bar */
    GtkWidget *statusbar;
    guint statusbar_context;

    /* Toolbar buttons */
    GtkWidget *btn_refresh;
    GtkWidget *btn_compose;
    GtkWidget *btn_delete;
    GtkWidget *btn_search;

    /* Dialogs */
    GtkWidget *compose_dialog;
    GtkWidget *search_dialog;
    GtkWidget *addressbook_dialog;

    /* Mail sessions */
    ImapSession *imap_session;
    SmtpSession *smtp_session;
    Config *config;

    /* Address book */
    AddressBook addressbook;

    /* Current state */
    int selected_email_index;
    char search_text[256];
    int filter_active;
} GUIContext;

/* GUI initialization and main loop */
int gui_init(GUIContext *ctx, ImapSession *imap, SmtpSession *smtp, Config *cfg, int *argc, char ***argv);
void gui_run(GUIContext *ctx);
void gui_cleanup(GUIContext *ctx);

/* UI updates */
void gui_update_folder_list(GUIContext *ctx);
void gui_update_email_list(GUIContext *ctx);
void gui_update_email_view(GUIContext *ctx);
void gui_set_status(GUIContext *ctx, const char *message);

/* Dialogs */
void gui_show_compose_dialog(GUIContext *ctx);
void gui_show_search_dialog(GUIContext *ctx);
void gui_show_addressbook_dialog(GUIContext *ctx);
void gui_show_about_dialog(GUIContext *ctx);

#endif /* GUI_H */
