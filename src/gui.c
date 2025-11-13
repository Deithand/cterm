#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui.h"
#include "imap.h"
#include "smtp.h"

/* Column enumeration for email list */
enum {
    COL_INDEX = 0,
    COL_FROM,
    COL_SUBJECT,
    COL_DATE,
    N_COLUMNS
};

/* Forward declarations */
static void on_email_selected(GtkTreeView *tree_view, gpointer user_data);
static void on_refresh_clicked(GtkButton *button, gpointer user_data);
static void on_compose_clicked(GtkButton *button, gpointer user_data);
static void on_delete_clicked(GtkButton *button, gpointer user_data);
static void on_send_email_clicked(GtkButton *button, gpointer user_data);
static void on_window_destroy(GtkWidget *widget, gpointer user_data);

/* Initialize GUI */
int gui_init(GUIContext *ctx, ImapSession *imap, SmtpSession *smtp, Config *config, int *argc, char ***argv) {
    GtkWidget *window;
    GtkWidget *main_vbox;
    GtkWidget *toolbar;
    GtkWidget *notebook;
    GtkWidget *paned;
    GtkWidget *scrolled_list;
    GtkWidget *scrolled_view;
    GtkWidget *tree_view;
    GtkWidget *text_view;
    GtkWidget *status_bar;
    GtkListStore *store;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkWidget *btn_refresh, *btn_compose, *btn_delete;

    ctx->imap = imap;
    ctx->smtp = smtp;
    ctx->config = config;
    ctx->selected_email_index = -1;

    /* Initialize GTK */
    gtk_init(argc, argv);

    /* Create main window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "cterm - Email Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), ctx);
    ctx->window = window;

    /* Main vertical box */
    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);

    /* Toolbar */
    toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 5);
    gtk_box_pack_start(GTK_BOX(main_vbox), toolbar, FALSE, FALSE, 0);

    btn_refresh = gtk_button_new_with_label("Refresh");
    g_signal_connect(btn_refresh, "clicked", G_CALLBACK(on_refresh_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(toolbar), btn_refresh, FALSE, FALSE, 0);

    btn_compose = gtk_button_new_with_label("Compose");
    g_signal_connect(btn_compose, "clicked", G_CALLBACK(on_compose_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(toolbar), btn_compose, FALSE, FALSE, 0);

    btn_delete = gtk_button_new_with_label("Delete");
    g_signal_connect(btn_delete, "clicked", G_CALLBACK(on_delete_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(toolbar), btn_delete, FALSE, FALSE, 0);

    /* Notebook for tabs */
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(main_vbox), notebook, TRUE, TRUE, 0);
    ctx->notebook = notebook;

    /* Inbox Tab */
    paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), paned, gtk_label_new("Inbox"));

    /* Email list */
    scrolled_list = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_list),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_paned_pack1(GTK_PANED(paned), scrolled_list, TRUE, TRUE);

    store = gtk_list_store_new(N_COLUMNS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    ctx->email_store = store;

    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    ctx->email_list = tree_view;
    gtk_container_add(GTK_CONTAINER(scrolled_list), tree_view);

    /* Index column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("#", renderer, "text", COL_INDEX, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    /* From column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("From", renderer, "text", COL_FROM, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, 250);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    /* Subject column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Subject", renderer, "text", COL_SUBJECT, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, 400);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    /* Date column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Date", renderer, "text", COL_DATE, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, 150);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    g_signal_connect(tree_view, "cursor-changed", G_CALLBACK(on_email_selected), ctx);

    /* Email viewing area */
    scrolled_view = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_view),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_paned_pack2(GTK_PANED(paned), scrolled_view, TRUE, TRUE);

    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 10);
    ctx->text_view = text_view;
    gtk_container_add(GTK_CONTAINER(scrolled_view), text_view);

    /* Status bar */
    status_bar = gtk_statusbar_new();
    ctx->status_bar = status_bar;
    gtk_box_pack_start(GTK_BOX(main_vbox), status_bar, FALSE, FALSE, 0);

    /* Set initial status */
    gtk_statusbar_push(GTK_STATUSBAR(status_bar), 0, "Ready");

    /* Load initial emails */
    gui_refresh_emails(ctx);

    gtk_widget_show_all(window);

    return 0;
}

/* Run main GUI loop */
void gui_run(GUIContext *ctx) {
    gtk_main();
}

/* Cleanup GUI resources */
void gui_cleanup(GUIContext *ctx) {
    /* GTK cleanup is handled by destroy signal */
}

/* Refresh email list */
void gui_refresh_emails(GUIContext *ctx) {
    GtkListStore *store = GTK_LIST_STORE(ctx->email_store);
    GtkTreeIter iter;
    ImapSession *imap = ctx->imap;

    /* Clear existing emails */
    gtk_list_store_clear(store);

    /* Update status */
    gtk_statusbar_push(GTK_STATUSBAR(ctx->status_bar), 0, "Refreshing emails...");

    /* Fetch emails from server */
    int count = imap_fetch_emails(imap);
    if (count < 0) {
        gtk_statusbar_push(GTK_STATUSBAR(ctx->status_bar), 0, "Error fetching emails");
        return;
    }

    /* Add emails to list */
    for (int i = 0; i < imap->email_count && i < MAX_EMAILS; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                          COL_INDEX, i + 1,
                          COL_FROM, imap->emails[i].from,
                          COL_SUBJECT, imap->emails[i].subject,
                          COL_DATE, imap->emails[i].date,
                          -1);
    }

    /* Update status */
    char status_msg[256];
    snprintf(status_msg, sizeof(status_msg), "Loaded %d emails", count);
    gtk_statusbar_push(GTK_STATUSBAR(ctx->status_bar), 0, status_msg);
}

/* Email selected callback */
static void on_email_selected(GtkTreeView *tree_view, gpointer user_data) {
    GUIContext *ctx = (GUIContext *)user_data;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint index;

    selection = gtk_tree_view_get_selection(tree_view);
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, COL_INDEX, &index, -1);
        ctx->selected_email_index = index - 1;

        if (ctx->selected_email_index >= 0 && ctx->selected_email_index < ctx->imap->email_count) {
            Email *email = &ctx->imap->emails[ctx->selected_email_index];

            /* Build email display text */
            char email_text[8192];
            snprintf(email_text, sizeof(email_text),
                    "From: %s\nTo: %s\nDate: %s\nSubject: %s\n\n%s",
                    email->from, email->to, email->date, email->subject, email->body);

            GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->text_view));
            gtk_text_buffer_set_text(buffer, email_text, -1);
        }
    }
}

/* Refresh button clicked */
static void on_refresh_clicked(GtkButton *button, gpointer user_data) {
    GUIContext *ctx = (GUIContext *)user_data;
    gui_refresh_emails(ctx);
}

/* Compose button clicked */
static void on_compose_clicked(GtkButton *button, gpointer user_data) {
    GUIContext *ctx = (GUIContext *)user_data;
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *grid;
    GtkWidget *label_to, *label_subject, *label_body;
    GtkWidget *entry_to, *entry_subject;
    GtkWidget *scrolled;
    GtkWidget *text_view;

    /* Create compose dialog */
    dialog = gtk_dialog_new_with_buttons("Compose Email",
                                         GTK_WINDOW(ctx->window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "Send", GTK_RESPONSE_ACCEPT,
                                         "Cancel", GTK_RESPONSE_CANCEL,
                                         NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    /* Grid layout */
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    /* To field */
    label_to = gtk_label_new("To:");
    gtk_widget_set_halign(label_to, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label_to, 0, 0, 1, 1);

    entry_to = gtk_entry_new();
    gtk_widget_set_hexpand(entry_to, TRUE);
    gtk_grid_attach(GTK_GRID(grid), entry_to, 1, 0, 1, 1);
    ctx->compose_to = entry_to;

    /* Subject field */
    label_subject = gtk_label_new("Subject:");
    gtk_widget_set_halign(label_subject, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label_subject, 0, 1, 1, 1);

    entry_subject = gtk_entry_new();
    gtk_widget_set_hexpand(entry_subject, TRUE);
    gtk_grid_attach(GTK_GRID(grid), entry_subject, 1, 1, 1, 1);
    ctx->compose_subject = entry_subject;

    /* Body field */
    label_body = gtk_label_new("Body:");
    gtk_widget_set_halign(label_body, GTK_ALIGN_START);
    gtk_widget_set_valign(label_body, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label_body, 0, 2, 1, 1);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_grid_attach(GTK_GRID(grid), scrolled, 1, 2, 1, 1);

    text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    ctx->compose_body = text_view;

    gtk_widget_show_all(dialog);

    /* Run dialog */
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        /* Get email data */
        const char *to = gtk_entry_get_text(GTK_ENTRY(entry_to));
        const char *subject = gtk_entry_get_text(GTK_ENTRY(entry_subject));

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        char *body = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

        /* Send email */
        gtk_statusbar_push(GTK_STATUSBAR(ctx->status_bar), 0, "Sending email...");

        if (smtp_send_email(ctx->smtp, ctx->config->smtp_from, to, subject, body) == 0) {
            gtk_statusbar_push(GTK_STATUSBAR(ctx->status_bar), 0, "Email sent successfully");
        } else {
            gtk_statusbar_push(GTK_STATUSBAR(ctx->status_bar), 0, "Failed to send email");
        }

        g_free(body);
    }

    gtk_widget_destroy(dialog);
}

/* Delete button clicked */
static void on_delete_clicked(GtkButton *button, gpointer user_data) {
    GUIContext *ctx = (GUIContext *)user_data;

    if (ctx->selected_email_index < 0) {
        gtk_statusbar_push(GTK_STATUSBAR(ctx->status_bar), 0, "No email selected");
        return;
    }

    /* Confirm deletion */
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(ctx->window),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_YES_NO,
                                               "Delete this email?");
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (result == GTK_RESPONSE_YES) {
        /* Delete email */
        Email *email = &ctx->imap->emails[ctx->selected_email_index];
        if (imap_delete_email(ctx->imap, email->uid) == 0) {
            gtk_statusbar_push(GTK_STATUSBAR(ctx->status_bar), 0, "Email deleted");
            gui_refresh_emails(ctx);
        } else {
            gtk_statusbar_push(GTK_STATUSBAR(ctx->status_bar), 0, "Failed to delete email");
        }
    }
}

/* Window destroy callback */
static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    gtk_main_quit();
}
