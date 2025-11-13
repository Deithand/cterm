#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include "imap.h"
#include "smtp.h"
#include "config.h"

typedef enum {
    VIEW_EMAIL_LIST,
    VIEW_EMAIL_CONTENT,
    VIEW_COMPOSE
} ViewMode;

typedef struct {
    WINDOW *main_win;
    WINDOW *status_win;
    ViewMode current_view;
    int selected_index;
    int scroll_offset;
    ImapSession *imap_session;
    SmtpSession *smtp_session;
    Config *config;
    int running;
} UIContext;

/* UI initialization and cleanup */
int ui_init(UIContext *ctx, ImapSession *imap, SmtpSession *smtp, Config *cfg);
void ui_cleanup(UIContext *ctx);

/* Main UI loop */
void ui_run(UIContext *ctx);

/* View rendering */
void ui_draw_email_list(UIContext *ctx);
void ui_draw_email_content(UIContext *ctx);
void ui_draw_compose(UIContext *ctx);
void ui_draw_status(UIContext *ctx, const char *message);

/* Input handling */
void ui_handle_input(UIContext *ctx, int ch);

#endif /* UI_H */
