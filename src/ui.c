#include "ui.h"
#include <stdlib.h>
#include <string.h>

#define STATUS_HEIGHT 2
#define INPUT_SIZE 256

/* Initialize UI */
int ui_init(UIContext *ctx, ImapSession *imap, SmtpSession *smtp, Config *cfg) {
    ctx->imap_session = imap;
    ctx->smtp_session = smtp;
    ctx->config = cfg;
    ctx->current_view = VIEW_EMAIL_LIST;
    ctx->selected_index = 0;
    ctx->scroll_offset = 0;
    ctx->running = 1;
    ctx->search_query[0] = '\0';
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

    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    /* Initialize colors */
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, -1);      /* Headers */
        init_pair(2, COLOR_GREEN, -1);     /* Status OK */
        init_pair(3, COLOR_YELLOW, -1);    /* Unread marker */
        init_pair(4, COLOR_RED, -1);       /* Errors */
        init_pair(5, COLOR_BLUE, -1);      /* Borders */
        init_pair(6, COLOR_MAGENTA, -1);   /* Highlights */
        init_pair(7, COLOR_WHITE, COLOR_BLUE); /* Selected item */
    }

    /* Create windows */
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    ctx->main_win = newwin(max_y - STATUS_HEIGHT, max_x, 0, 0);
    ctx->status_win = newwin(STATUS_HEIGHT, max_x, max_y - STATUS_HEIGHT, 0);

    if (!ctx->main_win || !ctx->status_win) {
        endwin();
        fprintf(stderr, "Error: Failed to create windows\n");
        return -1;
    }

    /* Enable scrolling and keypad for main window */
    scrollok(ctx->main_win, TRUE);
    keypad(ctx->main_win, TRUE);

    return 0;
}

/* Cleanup UI */
void ui_cleanup(UIContext *ctx) {
    if (ctx->main_win) {
        delwin(ctx->main_win);
    }
    if (ctx->status_win) {
        delwin(ctx->status_win);
    }

    /* Save and free address book */
    addressbook_save(&ctx->addressbook);
    addressbook_free(&ctx->addressbook);

    endwin();
}

/* Draw status bar */
void ui_draw_status(UIContext *ctx, const char *message) {
    werase(ctx->status_win);

    /* Color border */
    wattron(ctx->status_win, COLOR_PAIR(5));
    box(ctx->status_win, 0, 0);
    wattroff(ctx->status_win, COLOR_PAIR(5));

    /* Show current view and controls */
    const char *view_name = "";
    const char *controls = "";

    switch (ctx->current_view) {
        case VIEW_EMAIL_LIST:
            view_name = "📧 Email List";
            controls = "[Enter]Open [C]Compose [D]Delete [R]Refresh [S]Search [F]Folders [A]Addressbook [Q]Quit";
            break;
        case VIEW_EMAIL_CONTENT:
            view_name = "📖 Email Content";
            controls = "[Esc]Back [D]Delete [M]Mark Unseen [A]Save Attachment";
            break;
        case VIEW_COMPOSE:
            view_name = "✉️  Compose Email";
            controls = "[F2]Send [Esc]Cancel";
            break;
        case VIEW_SEARCH:
            view_name = "🔍 Search";
            controls = "[Esc]Cancel";
            break;
        case VIEW_FOLDERS:
            view_name = "📁 Folders";
            controls = "[Enter]Select [Esc]Cancel";
            break;
        case VIEW_ADDRESSBOOK:
            view_name = "📇 Address Book";
            controls = "[Esc]Cancel";
            break;
    }

    wattron(ctx->status_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->status_win, 0, 2, "[ %s ]", view_name);
    wattroff(ctx->status_win, COLOR_PAIR(1) | A_BOLD);

    wattron(ctx->status_win, COLOR_PAIR(6));
    mvwprintw(ctx->status_win, 1, 2, "%s", controls);
    wattroff(ctx->status_win, COLOR_PAIR(6));

    if (message && strlen(message) > 0) {
        int max_x = getmaxx(ctx->status_win);
        int msg_color = (strstr(message, "Failed") || strstr(message, "Error")) ? 4 : 2;
        wattron(ctx->status_win, COLOR_PAIR(msg_color) | A_BOLD);
        mvwprintw(ctx->status_win, 0, max_x - strlen(message) - 3, " %s ", message);
        wattroff(ctx->status_win, COLOR_PAIR(msg_color) | A_BOLD);
    }

    wrefresh(ctx->status_win);
}

/* Draw email list view */
void ui_draw_email_list(UIContext *ctx) {
    werase(ctx->main_win);

    /* Color border */
    wattron(ctx->main_win, COLOR_PAIR(5));
    box(ctx->main_win, 0, 0);
    wattroff(ctx->main_win, COLOR_PAIR(5));

    int max_y = getmaxy(ctx->main_win);
    int max_x = getmaxx(ctx->main_win);
    int email_count = ctx->imap_session->email_count;

    /* Header */
    wattron(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->main_win, 1, 2, "📬 INBOX - %d messages", email_count);
    wattroff(ctx->main_win, COLOR_PAIR(1) | A_BOLD);

    /* Draw separator line */
    wattron(ctx->main_win, COLOR_PAIR(5));
    mvwhline(ctx->main_win, 2, 1, ACS_HLINE, max_x - 2);
    wattroff(ctx->main_win, COLOR_PAIR(5));

    if (email_count == 0) {
        wattron(ctx->main_win, COLOR_PAIR(3));
        mvwprintw(ctx->main_win, 4, 2, "No emails in inbox");
        wattroff(ctx->main_win, COLOR_PAIR(3));
        wrefresh(ctx->main_win);
        return;
    }

    /* Adjust scroll offset if needed */
    int visible_lines = max_y - 6;
    if (ctx->selected_index < ctx->scroll_offset) {
        ctx->scroll_offset = ctx->selected_index;
    }
    if (ctx->selected_index >= ctx->scroll_offset + visible_lines) {
        ctx->scroll_offset = ctx->selected_index - visible_lines + 1;
    }

    /* Draw emails */
    int y = 3;
    for (int i = ctx->scroll_offset; i < email_count && y < max_y - 1; i++) {
        Email *email = &ctx->imap_session->emails[i];

        /* Highlight selected */
        if (i == ctx->selected_index) {
            wattron(ctx->main_win, COLOR_PAIR(7) | A_BOLD);
        }

        /* Format: [*] From: Subject */
        char status_icon[8];
        if (email->seen) {
            strcpy(status_icon, " ");
        } else {
            strcpy(status_icon, "●");
        }

        char line[512];
        int from_len = max_x > 100 ? 30 : 20;
        int subject_len = max_x - from_len - 15;
        snprintf(line, sizeof(line), " %s %-*.*s │ %.*s",
                status_icon, from_len, from_len, email->from, subject_len, email->subject);

        /* Truncate if too long */
        if ((int)strlen(line) > max_x - 4) {
            line[max_x - 4] = '\0';
        }

        /* Color unread emails differently */
        if (!email->seen && i != ctx->selected_index) {
            wattron(ctx->main_win, COLOR_PAIR(3) | A_BOLD);
        }

        mvwprintw(ctx->main_win, y, 2, "%s", line);

        if (!email->seen && i != ctx->selected_index) {
            wattroff(ctx->main_win, COLOR_PAIR(3) | A_BOLD);
        }

        if (i == ctx->selected_index) {
            wattroff(ctx->main_win, COLOR_PAIR(7) | A_BOLD);
        }

        y++;
    }

    /* Show scroll indicator */
    if (email_count > visible_lines) {
        wattron(ctx->main_win, COLOR_PAIR(5));
        mvwprintw(ctx->main_win, max_y - 1, max_x - 20, "[%d/%d]", ctx->selected_index + 1, email_count);
        wattroff(ctx->main_win, COLOR_PAIR(5));
    }

    wrefresh(ctx->main_win);
}

/* Draw email content view */
void ui_draw_email_content(UIContext *ctx) {
    werase(ctx->main_win);

    /* Color border */
    wattron(ctx->main_win, COLOR_PAIR(5));
    box(ctx->main_win, 0, 0);
    wattroff(ctx->main_win, COLOR_PAIR(5));

    int max_y = getmaxy(ctx->main_win);
    int max_x = getmaxx(ctx->main_win);

    if (ctx->selected_index < 0 || ctx->selected_index >= ctx->imap_session->email_count) {
        wrefresh(ctx->main_win);
        return;
    }

    Email *email = &ctx->imap_session->emails[ctx->selected_index];

    /* Headers */
    wattron(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->main_win, 1, 2, "From: ");
    wattroff(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    wprintw(ctx->main_win, "%s", email->from);

    wattron(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->main_win, 2, 2, "Subject: ");
    wattroff(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    wprintw(ctx->main_win, "%s", email->subject);

    wattron(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->main_win, 3, 2, "Date: ");
    wattroff(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    wprintw(ctx->main_win, "%s", email->date);

    /* Separator */
    wattron(ctx->main_win, COLOR_PAIR(5));
    mvwhline(ctx->main_win, 4, 1, ACS_HLINE, max_x - 2);
    wattroff(ctx->main_win, COLOR_PAIR(5));

    /* Body - CRITICAL FIX: Use a copy of the body to avoid corrupting original with strtok */
    int y = 5;
    char body_copy[MAX_BODY_LEN];
    strncpy(body_copy, email->body, MAX_BODY_LEN - 1);
    body_copy[MAX_BODY_LEN - 1] = '\0';

    char *line = strtok(body_copy, "\n");
    while (line && y < max_y - 1) {
        /* Word wrap simple implementation */
        int len = strlen(line);
        if (len > max_x - 4) {
            char temp[2048];
            strncpy(temp, line, max_x - 7);
            temp[max_x - 7] = '\0';
            mvwprintw(ctx->main_win, y, 2, "%s...", temp);
        } else {
            mvwprintw(ctx->main_win, y, 2, "%s", line);
        }
        y++;
        line = strtok(NULL, "\n");
    }

    /* Show if there's more content */
    if (line != NULL || y >= max_y - 1) {
        wattron(ctx->main_win, COLOR_PAIR(3));
        mvwprintw(ctx->main_win, max_y - 1, max_x - 20, "[More content...]");
        wattroff(ctx->main_win, COLOR_PAIR(3));
    }

    wrefresh(ctx->main_win);
}

/* Draw compose email view */
void ui_draw_compose(UIContext *ctx) {
    werase(ctx->main_win);

    /* Color border */
    wattron(ctx->main_win, COLOR_PAIR(5));
    box(ctx->main_win, 0, 0);
    wattroff(ctx->main_win, COLOR_PAIR(5));

    int max_y = getmaxy(ctx->main_win);
    int max_x = getmaxx(ctx->main_win);

    /* Header */
    wattron(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->main_win, 1, 2, "✉️  Compose New Email");
    wattroff(ctx->main_win, COLOR_PAIR(1) | A_BOLD);

    /* Separator */
    wattron(ctx->main_win, COLOR_PAIR(5));
    mvwhline(ctx->main_win, 2, 1, ACS_HLINE, max_x - 2);
    wattroff(ctx->main_win, COLOR_PAIR(5));

    /* Labels */
    wattron(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->main_win, 3, 2, "To: ");
    mvwprintw(ctx->main_win, 4, 2, "Subject: ");
    mvwprintw(ctx->main_win, 6, 2, "Body:");
    wattroff(ctx->main_win, COLOR_PAIR(1) | A_BOLD);

    wattron(ctx->main_win, COLOR_PAIR(6));
    mvwprintw(ctx->main_win, 5, 2, "(Press F2 to send, Esc to cancel)");
    wattroff(ctx->main_win, COLOR_PAIR(6));

    /* Input fields */
    char to[INPUT_SIZE] = "";
    char subject[INPUT_SIZE] = "";
    char body[1024] = "";

    echo();
    curs_set(1);

    /* Get To */
    wattron(ctx->main_win, COLOR_PAIR(2));
    mvwgetnstr(ctx->main_win, 3, 6, to, INPUT_SIZE - 1);
    wattroff(ctx->main_win, COLOR_PAIR(2));

    /* Get Subject */
    wattron(ctx->main_win, COLOR_PAIR(2));
    mvwgetnstr(ctx->main_win, 4, 11, subject, INPUT_SIZE - 1);
    wattroff(ctx->main_win, COLOR_PAIR(2));

    /* Get Body (simple multiline) */
    int body_y = 7;
    int body_len = 0;
    wattron(ctx->main_win, COLOR_PAIR(6));
    mvwprintw(ctx->main_win, body_y, 2, "│ ");
    wattroff(ctx->main_win, COLOR_PAIR(6));
    wrefresh(ctx->main_win);

    int ch;
    while (body_len < (int)sizeof(body) - 2 && body_y < max_y - 2) {
        ch = wgetch(ctx->main_win);

        if (ch == KEY_F(2)) {
            break; /* Send */
        } else if (ch == 27) { /* ESC */
            noecho();
            curs_set(0);
            ctx->current_view = VIEW_EMAIL_LIST;
            return;
        } else if (ch == '\n') {
            body[body_len++] = '\n';
            body_y++;
            if (body_y < max_y - 2) {
                wattron(ctx->main_win, COLOR_PAIR(6));
                mvwprintw(ctx->main_win, body_y, 2, "│ ");
                wattroff(ctx->main_win, COLOR_PAIR(6));
                wrefresh(ctx->main_win);
            }
        } else if (ch >= 32 && ch < 127) {
            body[body_len++] = ch;
            waddch(ctx->main_win, ch);
        }
    }

    body[body_len] = '\0';

    noecho();
    curs_set(0);

    /* Send email */
    ui_draw_status(ctx, "Sending email...");

    if (strlen(to) > 0 && strlen(subject) > 0) {
        if (smtp_send_email(ctx->smtp_session, ctx->config->email_address,
                           to, subject, body) == 0) {
            ui_draw_status(ctx, "✓ Email sent successfully!");
        } else {
            ui_draw_status(ctx, "✗ Failed to send email");
        }
    } else {
        ui_draw_status(ctx, "✗ Invalid email (missing To or Subject)");
    }

    /* Wait for key press */
    wgetch(ctx->main_win);

    ctx->current_view = VIEW_EMAIL_LIST;
}

/* Handle keyboard input */
void ui_handle_input(UIContext *ctx, int ch) {
    switch (ctx->current_view) {
        case VIEW_EMAIL_LIST:
            switch (ch) {
                case KEY_UP:
                case 'k':
                    if (ctx->selected_index > 0) {
                        ctx->selected_index--;
                    }
                    break;

                case KEY_DOWN:
                case 'j':
                    if (ctx->selected_index < ctx->imap_session->email_count - 1) {
                        ctx->selected_index++;
                    }
                    break;

                case '\n':
                case KEY_ENTER:
                    /* Open email */
                    if (ctx->selected_index >= 0 && ctx->selected_index < ctx->imap_session->email_count) {
                        Email *email = &ctx->imap_session->emails[ctx->selected_index];
                        ui_draw_status(ctx, "Loading email...");
                        if (imap_fetch_email_body(ctx->imap_session, email->uid, email) == 0) {
                            imap_mark_seen(ctx->imap_session, email->uid);
                            email->seen = 1;
                            ctx->current_view = VIEW_EMAIL_CONTENT;
                        } else {
                            ui_draw_status(ctx, "Failed to load email");
                        }
                    }
                    break;

                case 'c':
                case 'C':
                    /* Compose new email */
                    ctx->current_view = VIEW_COMPOSE;
                    break;

                case 'd':
                case 'D':
                    /* Delete email */
                    if (ctx->selected_index >= 0 && ctx->selected_index < ctx->imap_session->email_count) {
                        Email *email = &ctx->imap_session->emails[ctx->selected_index];
                        imap_delete_email(ctx->imap_session, email->uid);
                        imap_expunge(ctx->imap_session);
                        ui_draw_status(ctx, "Email deleted");
                        /* Refresh list */
                        imap_fetch_emails(ctx->imap_session);
                        if (ctx->selected_index >= ctx->imap_session->email_count) {
                            ctx->selected_index = ctx->imap_session->email_count - 1;
                        }
                        if (ctx->selected_index < 0) ctx->selected_index = 0;
                    }
                    break;

                case 'r':
                case 'R':
                    /* Refresh */
                    ui_draw_status(ctx, "Refreshing...");
                    imap_fetch_emails(ctx->imap_session);
                    ui_draw_status(ctx, "Refreshed");
                    break;

                case 's':
                case 'S':
                    /* Search */
                    ctx->current_view = VIEW_SEARCH;
                    break;

                case 'f':
                case 'F':
                    /* Show folders */
                    ctx->current_view = VIEW_FOLDERS;
                    break;

                case 'a':
                case 'A':
                    /* Show address book */
                    ctx->current_view = VIEW_ADDRESSBOOK;
                    break;

                case 'q':
                case 'Q':
                    ctx->running = 0;
                    break;
            }
            break;

        case VIEW_EMAIL_CONTENT:
            switch (ch) {
                case 27: /* ESC */
                case 'q':
                    ctx->current_view = VIEW_EMAIL_LIST;
                    break;

                case 'd':
                case 'D':
                    /* Delete current email */
                    if (ctx->selected_index >= 0 && ctx->selected_index < ctx->imap_session->email_count) {
                        Email *email = &ctx->imap_session->emails[ctx->selected_index];
                        imap_delete_email(ctx->imap_session, email->uid);
                        imap_expunge(ctx->imap_session);
                        ctx->current_view = VIEW_EMAIL_LIST;
                        imap_fetch_emails(ctx->imap_session);
                        ui_draw_status(ctx, "Email deleted");
                    }
                    break;

                case 'm':
                case 'M':
                    /* Mark as unseen */
                    if (ctx->selected_index >= 0 && ctx->selected_index < ctx->imap_session->email_count) {
                        Email *email = &ctx->imap_session->emails[ctx->selected_index];
                        imap_mark_unseen(ctx->imap_session, email->uid);
                        email->seen = 0;
                        ui_draw_status(ctx, "Marked as unseen");
                    }
                    break;

                case 'a':
                case 'A':
                    /* Save attachments */
                    if (ctx->selected_index >= 0 && ctx->selected_index < ctx->imap_session->email_count) {
                        Email *email = &ctx->imap_session->emails[ctx->selected_index];
                        if (email->has_attachments) {
                            for (int i = 0; i < email->attachment_count; i++) {
                                char filename[512];
                                snprintf(filename, sizeof(filename), "/tmp/%s", email->attachments[i].filename);
                                if (imap_fetch_attachment(ctx->imap_session, email->uid,
                                                         email->attachments[i].part_number, filename) == 0) {
                                    char msg[256];
                                    snprintf(msg, sizeof(msg), "Saved: %s", filename);
                                    ui_draw_status(ctx, msg);
                                } else {
                                    ui_draw_status(ctx, "Failed to save attachment");
                                }
                            }
                        } else {
                            ui_draw_status(ctx, "No attachments");
                        }
                    }
                    break;
            }
            break;

        case VIEW_COMPOSE:
            /* Handled in ui_draw_compose */
            break;

        case VIEW_SEARCH:
        case VIEW_FOLDERS:
        case VIEW_ADDRESSBOOK:
            /* Handled in their respective draw functions */
            if (ch == 27) { /* ESC */
                ctx->current_view = VIEW_EMAIL_LIST;
            }
            break;
    }
}

/* Forward declarations for new view functions */
void ui_draw_search(UIContext *ctx);
void ui_draw_folders(UIContext *ctx);
void ui_draw_addressbook(UIContext *ctx);

/* Main UI loop */
void ui_run(UIContext *ctx) {
    int ch;

    /* Initial draw */
    ui_draw_email_list(ctx);
    ui_draw_status(ctx, "Ready");

    while (ctx->running) {
        /* Draw current view */
        switch (ctx->current_view) {
            case VIEW_EMAIL_LIST:
                ui_draw_email_list(ctx);
                break;
            case VIEW_EMAIL_CONTENT:
                ui_draw_email_content(ctx);
                break;
            case VIEW_COMPOSE:
                ui_draw_compose(ctx);
                continue; /* Compose handles its own input */
            case VIEW_SEARCH:
                ui_draw_search(ctx);
                break;
            case VIEW_FOLDERS:
                ui_draw_folders(ctx);
                break;
            case VIEW_ADDRESSBOOK:
                ui_draw_addressbook(ctx);
                break;
        }

        ui_draw_status(ctx, "");

        /* Get input */
        ch = wgetch(ctx->main_win);
        ui_handle_input(ctx, ch);
    }
}

/* Draw search view */
void ui_draw_search(UIContext *ctx) {
    werase(ctx->main_win);

    wattron(ctx->main_win, COLOR_PAIR(5));
    box(ctx->main_win, 0, 0);
    wattroff(ctx->main_win, COLOR_PAIR(5));

    wattron(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->main_win, 1, 2, "🔍 Search Emails");
    wattroff(ctx->main_win, COLOR_PAIR(1) | A_BOLD);

    mvwprintw(ctx->main_win, 3, 2, "Search query: ");
    wrefresh(ctx->main_win);

    /* Get search input */
    echo();
    curs_set(1);

    char search[256] = "";
    wgetnstr(ctx->main_win, search, sizeof(search) - 1);

    noecho();
    curs_set(0);

    if (strlen(search) > 0) {
        ui_draw_status(ctx, "Searching...");
        int count = imap_search_emails(ctx->imap_session, search);

        char msg[256];
        snprintf(msg, sizeof(msg), "Found %d emails matching '%s'", count, search);
        ui_draw_status(ctx, msg);

        wgetch(ctx->main_win); /* Wait for key */
    }

    ctx->current_view = VIEW_EMAIL_LIST;
}

/* Draw folders view */
void ui_draw_folders(UIContext *ctx) {
    werase(ctx->main_win);

    wattron(ctx->main_win, COLOR_PAIR(5));
    box(ctx->main_win, 0, 0);
    wattroff(ctx->main_win, COLOR_PAIR(5));

    wattron(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->main_win, 1, 2, "📁 Select Folder");
    wattroff(ctx->main_win, COLOR_PAIR(1) | A_BOLD);

    /* Get mailboxes */
    ui_draw_status(ctx, "Loading folders...");
    int count = imap_list_mailboxes(ctx->imap_session);

    int y = 3;
    for (int i = 0; i < count && y < getmaxy(ctx->main_win) - 2; i++) {
        mvwprintw(ctx->main_win, y++, 4, "[%d] %s", i + 1, ctx->imap_session->mailboxes[i].name);
    }

    mvwprintw(ctx->main_win, y + 1, 2, "Enter folder number (0 to cancel): ");
    wrefresh(ctx->main_win);

    /* Get input */
    echo();
    curs_set(1);

    char input[32];
    wgetnstr(ctx->main_win, input, sizeof(input) - 1);

    noecho();
    curs_set(0);

    int choice = atoi(input);
    if (choice > 0 && choice <= count) {
        const char *folder = ctx->imap_session->mailboxes[choice - 1].name;
        ui_draw_status(ctx, "Selecting folder...");

        if (imap_select_mailbox(ctx->imap_session, folder) == 0) {
            imap_fetch_emails(ctx->imap_session);
            char msg[256];
            snprintf(msg, sizeof(msg), "Switched to %s", folder);
            ui_draw_status(ctx, msg);
        } else {
            ui_draw_status(ctx, "Failed to select folder");
        }
    }

    ctx->current_view = VIEW_EMAIL_LIST;
}

/* Draw address book view */
void ui_draw_addressbook(UIContext *ctx) {
    werase(ctx->main_win);

    wattron(ctx->main_win, COLOR_PAIR(5));
    box(ctx->main_win, 0, 0);
    wattroff(ctx->main_win, COLOR_PAIR(5));

    wattron(ctx->main_win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(ctx->main_win, 1, 2, "📇 Address Book (%d contacts)", ctx->addressbook.contact_count);
    wattroff(ctx->main_win, COLOR_PAIR(1) | A_BOLD);

    int max_y = getmaxy(ctx->main_win);
    int y = 3;

    for (int i = 0; i < ctx->addressbook.contact_count && y < max_y - 2; i++) {
        Contact *contact = &ctx->addressbook.contacts[i];
        mvwprintw(ctx->main_win, y++, 4, "%-30s <%s>", contact->name, contact->email);
    }

    if (ctx->addressbook.contact_count == 0) {
        mvwprintw(ctx->main_win, 3, 4, "No contacts yet");
    }

    mvwprintw(ctx->main_win, max_y - 2, 2, "Press any key to return...");
    wrefresh(ctx->main_win);

    wgetch(ctx->main_win);
    ctx->current_view = VIEW_EMAIL_LIST;
}
