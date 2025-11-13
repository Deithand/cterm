#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "network.h"
#include "imap.h"
#include "smtp.h"
#include "ui.h"

#define DEFAULT_CONFIG_FILE ".cterm.conf"

static void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -c <config>  Configuration file (default: ~/%s)\n", DEFAULT_CONFIG_FILE);
    printf("  -h           Show this help message\n");
}

int main(int argc, char *argv[]) {
    Config config;
    ImapSession imap_session;
    SmtpSession smtp_session;
    UIContext ui_ctx;
    char config_file[512];
    int opt;

    /* Default config file path */
    const char *home = getenv("HOME");
    if (home) {
        snprintf(config_file, sizeof(config_file), "%s/%s", home, DEFAULT_CONFIG_FILE);
    } else {
        snprintf(config_file, sizeof(config_file), "%s", DEFAULT_CONFIG_FILE);
    }

    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "c:h")) != -1) {
        switch (opt) {
            case 'c':
                strncpy(config_file, optarg, sizeof(config_file) - 1);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* Load configuration */
    printf("Loading configuration from: %s\n", config_file);
    if (config_load(config_file, &config) < 0) {
        fprintf(stderr, "Error: Failed to load configuration\n");
        return 1;
    }

    /* Initialize SSL */
    net_init_ssl();

    /* Connect to IMAP server */
    printf("Connecting to IMAP server: %s:%d\n", config.imap_server, config.imap_port);
    if (imap_connect(&imap_session, config.imap_server, config.imap_port, config.imap_use_ssl) < 0) {
        fprintf(stderr, "Error: Failed to connect to IMAP server\n");
        config_free(&config);
        net_cleanup_ssl();
        return 1;
    }

    /* Login to IMAP */
    printf("Logging in as: %s\n", config.imap_username);
    if (imap_login(&imap_session, config.imap_username, config.imap_password) < 0) {
        fprintf(stderr, "Error: Failed to login to IMAP server\n");
        imap_disconnect(&imap_session);
        config_free(&config);
        net_cleanup_ssl();
        return 1;
    }

    /* Select INBOX */
    printf("Selecting INBOX...\n");
    if (imap_select_mailbox(&imap_session, "INBOX") < 0) {
        fprintf(stderr, "Error: Failed to select INBOX\n");
        imap_disconnect(&imap_session);
        config_free(&config);
        net_cleanup_ssl();
        return 1;
    }

    /* Fetch emails */
    printf("Fetching emails...\n");
    int email_count = imap_fetch_emails(&imap_session);
    if (email_count < 0) {
        fprintf(stderr, "Error: Failed to fetch emails\n");
        imap_disconnect(&imap_session);
        config_free(&config);
        net_cleanup_ssl();
        return 1;
    }
    printf("Found %d emails\n", email_count);

    /* Connect to SMTP server */
    printf("Connecting to SMTP server: %s:%d\n", config.smtp_server, config.smtp_port);
    if (smtp_connect(&smtp_session, config.smtp_server, config.smtp_port, config.smtp_use_ssl) < 0) {
        fprintf(stderr, "Error: Failed to connect to SMTP server\n");
        imap_disconnect(&imap_session);
        config_free(&config);
        net_cleanup_ssl();
        return 1;
    }

    /* Use STARTTLS if configured */
    if (config.smtp_use_starttls) {
        printf("Starting TLS...\n");
        if (smtp_starttls(&smtp_session) < 0) {
            fprintf(stderr, "Error: Failed to start TLS\n");
            smtp_disconnect(&smtp_session);
            imap_disconnect(&imap_session);
            config_free(&config);
            net_cleanup_ssl();
            return 1;
        }
    }

    /* Authenticate with SMTP */
    printf("Authenticating with SMTP...\n");
    if (smtp_auth_login(&smtp_session, config.smtp_username, config.smtp_password) < 0) {
        fprintf(stderr, "Error: Failed to authenticate with SMTP server\n");
        smtp_disconnect(&smtp_session);
        imap_disconnect(&imap_session);
        config_free(&config);
        net_cleanup_ssl();
        return 1;
    }

    printf("Starting TUI...\n");
    sleep(1); /* Give user time to read messages */

    /* Initialize and run UI */
    if (ui_init(&ui_ctx, &imap_session, &smtp_session, &config) < 0) {
        fprintf(stderr, "Error: Failed to initialize UI\n");
        smtp_disconnect(&smtp_session);
        imap_disconnect(&imap_session);
        config_free(&config);
        net_cleanup_ssl();
        return 1;
    }

    ui_run(&ui_ctx);

    /* Cleanup */
    ui_cleanup(&ui_ctx);
    smtp_disconnect(&smtp_session);
    imap_disconnect(&imap_session);
    config_free(&config);
    net_cleanup_ssl();

    printf("Goodbye!\n");
    return 0;
}
