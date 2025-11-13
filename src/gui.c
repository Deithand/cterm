#define _POSIX_C_SOURCE 200809L
#include "gui.h"
#include "html_parser.h"
#include <stdlib.h>
#include <string.h>

enum {
    FOLDER_COL_NAME = 0,
    FOLDER_COL_COUNT,
    FOLDER_N_COLS
};

enum {
    EMAIL_COL_INDEX = 0,
    EMAIL_COL_SEEN,
    EMAIL_COL_FROM,
    EMAIL_COL_SUBJECT,
    EMAIL_COL_DATE,
    EMAIL_COL_ATTACHMENTS,
    EMAIL_N_COLS
};

/* Forward declarations */
static void on_refresh_clicked(GtkWidget *widget, gpointer data);
static void on_compose_clicked(GtkWidget *widget, gpointer data);
static void on_delete_clicked(GtkWidget *widget, gpointer data);
static void on_search_clicked(GtkWidget *widget, gpointer data);
static void on_email_selected(GtkTreeSelection *selection, gpointer data);
static void on_folder_selected(GtkTreeSelection *selection, gpointer data);
static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data);

/* Initialize GUI */
int gui_init(GUIContext *ctx, ImapSession *imap, SmtpSession *smtp, Config *cfg, int *argc, char ***argv) {
    ctx->imap_session = imap;
    ctx->smtp_session = smtp;
    ctx->config = cfg;
    ctx->selected_email_index = -1;
    ctx->filter_active = 0;

    /* Initialize address book */
    char addressbook_file[512];
    const char *home = getenv("HOME");
    if (home) {
        snprintf(addressbook_file, sizeof(addressbook_file), "%s/.cterm_addressbook", home);
    } else {
        strcpy(addressbook_file, ".cterm_addressbook");
    }
    addressbook_init(&ctx->addressbook, addressbook_file);
    addressbook_load(&ctx->addressbook);

    /* Initialize GTK */
    gtk_init(argc, argv);

    /* Create main window */
    ctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ctx->window), "cterm - Mail Client");
    gtk_window_set_default_size(GTK_WINDOW(ctx->window), 1024, 768);
    g_signal_connect(ctx->window, "delete-event", G_CALLBACK(on_window_delete), ctx);

    /* Main vertical box */
    ctx->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ctx->window), ctx->main_box);

    /* Toolbar */
    ctx->toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(ctx->main_box), ctx->toolbar, FALSE, FALSE, 0);

    GtkToolItem *btn_refresh = gtk_tool_button_new(NULL, "Refresh");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_refresh), "view-refresh");
    g_signal_connect(btn_refresh, "clicked", G_CALLBACK(on_refresh_clicked), ctx);
    gtk_toolbar_insert(GTK_TOOLBAR(ctx->toolbar), btn_refresh, -1);

    GtkToolItem *btn_compose = gtk_tool_button_new(NULL, "Compose");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_compose), "mail-message-new");
    g_signal_connect(btn_compose, "clicked", G_CALLBACK(on_compose_clicked), ctx);
    gtk_toolbar_insert(GTK_TOOLBAR(ctx->toolbar), btn_compose, -1);

    GtkToolItem *btn_delete = gtk_tool_button_new(NULL, "Delete");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_delete), "edit-delete");
    g_signal_connect(btn_delete, "clicked", G_CALLBACK(on_delete_clicked), ctx);
    gtk_toolbar_insert(GTK_TOOLBAR(ctx->toolbar), btn_delete, -1);

    gtk_toolbar_insert(GTK_TOOLBAR(ctx->toolbar), gtk_separator_tool_item_new(), -1);

    GtkToolItem *btn_search = gtk_tool_button_new(NULL, "Search");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_search), "edit-find");
    g_signal_connect(btn_search, "clicked", G_CALLBACK(on_search_clicked), ctx);
    gtk_toolbar_insert(GTK_TOOLBAR(ctx->toolbar), btn_search, -1);

    /* Paned widget for left/right split */
    ctx->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(ctx->main_box), ctx->paned, TRUE, TRUE, 0);

    /* Left panel - folder tree */
    ctx->folder_store = gtk_list_store_new(FOLDER_N_COLS, G_TYPE_STRING, G_TYPE_INT);
    ctx->folder_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ctx->folder_store));

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Folders", renderer, "text", FOLDER_COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->folder_tree), column);

    GtkTreeSelection *folder_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctx->folder_tree));
    g_signal_connect(folder_selection, "changed", G_CALLBACK(on_folder_selected), ctx);

    GtkWidget *folder_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(folder_scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(folder_scroll), ctx->folder_tree);
    gtk_paned_pack1(GTK_PANED(ctx->paned), folder_scroll, FALSE, TRUE);

    /* Right panel - paned for email list and content */
    GtkWidget *right_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_pack2(GTK_PANED(ctx->paned), right_paned, TRUE, TRUE);

    /* Email list */
    ctx->email_store = gtk_list_store_new(EMAIL_N_COLS,
                                          G_TYPE_INT,     /* index */
                                          G_TYPE_BOOLEAN, /* seen */
                                          G_TYPE_STRING,  /* from */
                                          G_TYPE_STRING,  /* subject */
                                          G_TYPE_STRING,  /* date */
                                          G_TYPE_STRING); /* attachments */

    ctx->email_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ctx->email_store));

    /* Columns */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Status", renderer, "text", EMAIL_COL_SEEN, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->email_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("From", renderer, "text", EMAIL_COL_FROM, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->email_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Subject", renderer, "text", EMAIL_COL_SUBJECT, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->email_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Date", renderer, "text", EMAIL_COL_DATE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->email_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Att.", renderer, "text", EMAIL_COL_ATTACHMENTS, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->email_list), column);

    GtkTreeSelection *email_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctx->email_list));
    g_signal_connect(email_selection, "changed", G_CALLBACK(on_email_selected), ctx);

    GtkWidget *email_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(email_scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(email_scroll), ctx->email_list);
    gtk_paned_pack1(GTK_PANED(right_paned), email_scroll, FALSE, TRUE);

    /* Email content view */
    ctx->email_buffer = gtk_text_buffer_new(NULL);
    ctx->email_view = gtk_text_view_new_with_buffer(ctx->email_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(ctx->email_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctx->email_view), GTK_WRAP_WORD);

    GtkWidget *content_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(content_scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(content_scroll), ctx->email_view);
    gtk_paned_pack2(GTK_PANED(right_paned), content_scroll, TRUE, TRUE);

    /* Status bar */
    ctx->statusbar = gtk_statusbar_new();
    ctx->statusbar_context = gtk_statusbar_get_context_id(GTK_STATUSBAR(ctx->statusbar), "main");
    gtk_box_pack_start(GTK_BOX(ctx->main_box), ctx->statusbar, FALSE, FALSE, 0);

    /* Set paned positions */
    gtk_paned_set_position(GTK_PANED(ctx->paned), 200);
    gtk_paned_set_position(GTK_PANED(right_paned), 300);

    /* Show all widgets */
    gtk_widget_show_all(ctx->window);

    /* Load initial data */
    gui_update_folder_list(ctx);
    gui_update_email_list(ctx);
    gui_set_status(ctx, "Ready");

    return 0;
}

/* Update folder list */
void gui_update_folder_list(GUIContext *ctx) {
    gtk_list_store_clear(ctx->folder_store);

    /* Add INBOX first */
    GtkTreeIter iter;
    gtk_list_store_append(ctx->folder_store, &iter);
    gtk_list_store_set(ctx->folder_store, &iter,
                       FOLDER_COL_NAME, "INBOX",
                       FOLDER_COL_COUNT, ctx->imap_session->email_count,
                       -1);

    /* Get mailbox list */
    int count = imap_list_mailboxes(ctx->imap_session);
    for (int i = 0; i < count; i++) {
        if (strcmp(ctx->imap_session->mailboxes[i].name, "INBOX") == 0) {
            continue; /* Already added */
        }

        gtk_list_store_append(ctx->folder_store, &iter);
        gtk_list_store_set(ctx->folder_store, &iter,
                           FOLDER_COL_NAME, ctx->imap_session->mailboxes[i].name,
                           FOLDER_COL_COUNT, 0,
                           -1);
    }
}

/* Update email list */
void gui_update_email_list(GUIContext *ctx) {
    gtk_list_store_clear(ctx->email_store);

    for (int i = 0; i < ctx->imap_session->email_count; i++) {
        Email *email = &ctx->imap_session->emails[i];

        GtkTreeIter iter;
        gtk_list_store_append(ctx->email_store, &iter);

        const char *status_icon = email->seen ? " " : "●";
        const char *att_icon = email->has_attachments ? "📎" : "";

        gtk_list_store_set(ctx->email_store, &iter,
                           EMAIL_COL_INDEX, i,
                           EMAIL_COL_SEEN, email->seen,
                           EMAIL_COL_FROM, email->from,
                           EMAIL_COL_SUBJECT, email->subject,
                           EMAIL_COL_DATE, email->date,
                           EMAIL_COL_ATTACHMENTS, att_icon,
                           -1);
    }

    char status[256];
    snprintf(status, sizeof(status), "%d messages", ctx->imap_session->email_count);
    gui_set_status(ctx, status);
}

/* Update email view */
void gui_update_email_view(GUIContext *ctx) {
    if (ctx->selected_email_index < 0 ||
        ctx->selected_email_index >= ctx->imap_session->email_count) {
        gtk_text_buffer_set_text(ctx->email_buffer, "", 0);
        return;
    }

    Email *email = &ctx->imap_session->emails[ctx->selected_email_index];

    /* Fetch body if not loaded */
    if (strlen(email->body) == 0) {
        imap_fetch_email_body(ctx->imap_session, email->uid, email);
        imap_mark_seen(ctx->imap_session, email->uid);
        email->seen = 1;

        /* Get attachments info */
        imap_get_attachments(ctx->imap_session, email->uid, email);
    }

    /* Build email display */
    char display[MAX_BODY_LEN + 1024];
    char body_text[MAX_BODY_LEN];

    /* Convert HTML to text if needed */
    if (html_is_html_content(email->body)) {
        html_to_text(email->body, body_text, sizeof(body_text));
    } else {
        strncpy(body_text, email->body, sizeof(body_text) - 1);
        body_text[sizeof(body_text) - 1] = '\0';
    }

    snprintf(display, sizeof(display),
             "From: %s\n"
             "To: %s\n"
             "Subject: %s\n"
             "Date: %s\n",
             email->from,
             email->to[0] ? email->to : "(unknown)",
             email->subject,
             email->date);

    if (email->has_attachments) {
        char att_line[256];
        snprintf(att_line, sizeof(att_line), "Attachments: %d\n", email->attachment_count);
        strcat(display, att_line);

        for (int i = 0; i < email->attachment_count; i++) {
            snprintf(att_line, sizeof(att_line), "  [%d] %s\n",
                     i + 1, email->attachments[i].filename);
            strcat(display, att_line);
        }
    }

    strcat(display, "\n");
    strcat(display, body_text);

    gtk_text_buffer_set_text(ctx->email_buffer, display, -1);
}

/* Set status bar message */
void gui_set_status(GUIContext *ctx, const char *message) {
    gtk_statusbar_pop(GTK_STATUSBAR(ctx->statusbar), ctx->statusbar_context);
    gtk_statusbar_push(GTK_STATUSBAR(ctx->statusbar), ctx->statusbar_context, message);
}

/* Run GUI main loop */
void gui_run(GUIContext *ctx) {
    gtk_main();
}

/* Cleanup GUI */
void gui_cleanup(GUIContext *ctx) {
    addressbook_free(&ctx->addressbook);
}

/* Event handlers */
static void on_refresh_clicked(GtkWidget *widget, gpointer data) {
    GUIContext *ctx = (GUIContext*)data;
    gui_set_status(ctx, "Refreshing...");

    imap_fetch_emails(ctx->imap_session);
    gui_update_email_list(ctx);
    gui_update_folder_list(ctx);

    gui_set_status(ctx, "Refreshed");
}

static void on_compose_clicked(GtkWidget *widget, gpointer data) {
    GUIContext *ctx = (GUIContext*)data;
    gui_show_compose_dialog(ctx);
}

static void on_delete_clicked(GtkWidget *widget, gpointer data) {
    GUIContext *ctx = (GUIContext*)data;

    if (ctx->selected_email_index < 0) {
        return;
    }

    Email *email = &ctx->imap_session->emails[ctx->selected_email_index];

    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(ctx->window),
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_QUESTION,
                                                GTK_BUTTONS_YES_NO,
                                                "Delete this email?");

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (result == GTK_RESPONSE_YES) {
        imap_delete_email(ctx->imap_session, email->uid);
        imap_expunge(ctx->imap_session);
        gui_set_status(ctx, "Email deleted");

        imap_fetch_emails(ctx->imap_session);
        gui_update_email_list(ctx);
    }
}

static void on_search_clicked(GtkWidget *widget, gpointer data) {
    GUIContext *ctx = (GUIContext*)data;
    gui_show_search_dialog(ctx);
}

static void on_email_selected(GtkTreeSelection *selection, gpointer data) {
    GUIContext *ctx = (GUIContext*)data;

    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint index;
        gtk_tree_model_get(model, &iter, EMAIL_COL_INDEX, &index, -1);
        ctx->selected_email_index = index;
        gui_update_email_view(ctx);
    }
}

static void on_folder_selected(GtkTreeSelection *selection, gpointer data) {
    GUIContext *ctx = (GUIContext*)data;

    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *folder_name;
        gtk_tree_model_get(model, &iter, FOLDER_COL_NAME, &folder_name, -1);

        gui_set_status(ctx, "Selecting folder...");
        imap_select_mailbox(ctx->imap_session, folder_name);
        imap_fetch_emails(ctx->imap_session);
        gui_update_email_list(ctx);

        g_free(folder_name);
    }
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    gtk_main_quit();
    return FALSE;
}

/* Show compose dialog */
void gui_show_compose_dialog(GUIContext *ctx) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Compose Email",
                                                      GTK_WINDOW(ctx->window),
                                                      GTK_DIALOG_MODAL,
                                                      "Send", GTK_RESPONSE_OK,
                                                      "Cancel", GTK_RESPONSE_CANCEL,
                                                      NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content), grid);

    /* To field */
    GtkWidget *to_label = gtk_label_new("To:");
    GtkWidget *to_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), to_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), to_entry, 1, 0, 1, 1);
    gtk_widget_set_hexpand(to_entry, TRUE);

    /* Subject field */
    GtkWidget *subject_label = gtk_label_new("Subject:");
    GtkWidget *subject_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), subject_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), subject_entry, 1, 1, 1, 1);

    /* Body field */
    GtkWidget *body_label = gtk_label_new("Body:");
    GtkWidget *body_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(body_view), GTK_WRAP_WORD);
    GtkWidget *body_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(body_scroll), 300);
    gtk_container_add(GTK_CONTAINER(body_scroll), body_view);
    gtk_grid_attach(GTK_GRID(grid), body_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), body_scroll, 1, 2, 1, 1);

    gtk_widget_show_all(dialog);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_OK) {
        const char *to = gtk_entry_get_text(GTK_ENTRY(to_entry));
        const char *subject = gtk_entry_get_text(GTK_ENTRY(subject_entry));

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(body_view));
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        char *body = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

        if (strlen(to) > 0 && strlen(subject) > 0) {
            gui_set_status(ctx, "Sending email...");

            if (smtp_send_email(ctx->smtp_session, ctx->config->email_address,
                               to, subject, body) == 0) {
                gui_set_status(ctx, "Email sent successfully");
            } else {
                gui_set_status(ctx, "Failed to send email");
            }
        }

        g_free(body);
    }

    gtk_widget_destroy(dialog);
}

/* Show search dialog */
void gui_show_search_dialog(GUIContext *ctx) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Search Emails",
                                                      GTK_WINDOW(ctx->window),
                                                      GTK_DIALOG_MODAL,
                                                      "Search", GTK_RESPONSE_OK,
                                                      "Clear", GTK_RESPONSE_APPLY,
                                                      "Close", GTK_RESPONSE_CANCEL,
                                                      NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);
    gtk_container_add(GTK_CONTAINER(content), box);

    GtkWidget *search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Enter search text...");
    gtk_box_pack_start(GTK_BOX(box), search_entry, FALSE, FALSE, 0);

    GtkWidget *filter_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), "All fields");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), "Subject");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), "From");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), "Unread only");
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo), 0);
    gtk_box_pack_start(GTK_BOX(box), filter_combo, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_OK) {
        const char *search_text = gtk_entry_get_text(GTK_ENTRY(search_entry));
        int filter_type = gtk_combo_box_get_active(GTK_COMBO_BOX(filter_combo));

        if (strlen(search_text) > 0) {
            gui_set_status(ctx, "Searching...");

            int count = 0;
            switch (filter_type) {
                case 1: /* Subject */
                    count = imap_filter_by_subject(ctx->imap_session, search_text);
                    break;
                case 2: /* From */
                    count = imap_filter_by_sender(ctx->imap_session, search_text);
                    break;
                case 3: /* Unread */
                    count = imap_filter_unseen(ctx->imap_session);
                    break;
                default:
                    count = imap_search_emails(ctx->imap_session, search_text);
                    break;
            }

            char status[256];
            snprintf(status, sizeof(status), "Found %d emails", count);
            gui_set_status(ctx, status);
        }
    } else if (result == GTK_RESPONSE_APPLY) {
        /* Clear filter */
        imap_fetch_emails(ctx->imap_session);
        gui_update_email_list(ctx);
        gui_set_status(ctx, "Filter cleared");
    }

    gtk_widget_destroy(dialog);
}

/* Show address book dialog */
void gui_show_addressbook_dialog(GUIContext *ctx) {
    /* Simple address book dialog - can be expanded */
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(ctx->window),
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_INFO,
                                                GTK_BUTTONS_OK,
                                                "Address Book\n\nContacts: %d",
                                                ctx->addressbook.contact_count);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/* Show about dialog */
void gui_show_about_dialog(GUIContext *ctx) {
    GtkWidget *dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "cterm");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "2.0");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), "Lightweight Mail Client");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://github.com/example/cterm");

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
