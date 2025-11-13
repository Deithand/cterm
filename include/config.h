#ifndef CONFIG_H
#define CONFIG_H

#define MAX_STRING_LEN 256

typedef struct {
    /* IMAP settings */
    char imap_server[MAX_STRING_LEN];
    int imap_port;
    int imap_use_ssl;
    char imap_username[MAX_STRING_LEN];
    char imap_password[MAX_STRING_LEN];

    /* SMTP settings */
    char smtp_server[MAX_STRING_LEN];
    int smtp_port;
    int smtp_use_ssl;
    int smtp_use_starttls;
    char smtp_username[MAX_STRING_LEN];
    char smtp_password[MAX_STRING_LEN];

    /* User info */
    char email_address[MAX_STRING_LEN];
    char display_name[MAX_STRING_LEN];
} Config;

/* Function prototypes */
int config_load(const char *filename, Config *config);
void config_free(Config *config);
void config_print(const Config *config);

#endif /* CONFIG_H */
