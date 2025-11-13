#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Trim whitespace from both ends of a string */
static char *trim(char *str) {
    char *end;

    /* Trim leading space */
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0) return str;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

/* Parse a single configuration line */
static int parse_line(char *line, Config *config) {
    char *key, *value;
    char *delimiter;

    /* Skip comments and empty lines */
    if (line[0] == '#' || line[0] == '\0') {
        return 0;
    }

    /* Find the delimiter */
    delimiter = strchr(line, '=');
    if (!delimiter) {
        return 0; /* Not a valid key=value line */
    }

    *delimiter = '\0';
    key = trim(line);
    value = trim(delimiter + 1);

    /* Parse IMAP settings */
    if (strcmp(key, "imap_server") == 0) {
        strncpy(config->imap_server, value, MAX_STRING_LEN - 1);
    } else if (strcmp(key, "imap_port") == 0) {
        config->imap_port = atoi(value);
    } else if (strcmp(key, "imap_use_ssl") == 0) {
        config->imap_use_ssl = (strcmp(value, "yes") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "imap_username") == 0) {
        strncpy(config->imap_username, value, MAX_STRING_LEN - 1);
    } else if (strcmp(key, "imap_password") == 0) {
        strncpy(config->imap_password, value, MAX_STRING_LEN - 1);
    }
    /* Parse SMTP settings */
    else if (strcmp(key, "smtp_server") == 0) {
        strncpy(config->smtp_server, value, MAX_STRING_LEN - 1);
    } else if (strcmp(key, "smtp_port") == 0) {
        config->smtp_port = atoi(value);
    } else if (strcmp(key, "smtp_use_ssl") == 0) {
        config->smtp_use_ssl = (strcmp(value, "yes") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "smtp_use_starttls") == 0) {
        config->smtp_use_starttls = (strcmp(value, "yes") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "smtp_username") == 0) {
        strncpy(config->smtp_username, value, MAX_STRING_LEN - 1);
    } else if (strcmp(key, "smtp_password") == 0) {
        strncpy(config->smtp_password, value, MAX_STRING_LEN - 1);
    }
    /* Parse user info */
    else if (strcmp(key, "email_address") == 0) {
        strncpy(config->email_address, value, MAX_STRING_LEN - 1);
    } else if (strcmp(key, "display_name") == 0) {
        strncpy(config->display_name, value, MAX_STRING_LEN - 1);
    }

    return 0;
}

/* Load configuration from file */
int config_load(const char *filename, Config *config) {
    FILE *file;
    char line[512];

    /* Initialize config with defaults */
    memset(config, 0, sizeof(Config));
    config->imap_port = 993;
    config->imap_use_ssl = 1;
    config->smtp_port = 587;
    config->smtp_use_ssl = 0;
    config->smtp_use_starttls = 1;

    file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open config file: %s\n", filename);
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        /* Remove newline */
        line[strcspn(line, "\n")] = 0;
        parse_line(line, config);
    }

    fclose(file);

    /* Validate required fields */
    if (strlen(config->imap_server) == 0) {
        fprintf(stderr, "Error: imap_server not configured\n");
        return -1;
    }
    if (strlen(config->smtp_server) == 0) {
        fprintf(stderr, "Error: smtp_server not configured\n");
        return -1;
    }
    if (strlen(config->email_address) == 0) {
        fprintf(stderr, "Error: email_address not configured\n");
        return -1;
    }

    return 0;
}

/* Free configuration resources */
void config_free(Config *config) {
    /* Clear sensitive data */
    memset(config->imap_password, 0, sizeof(config->imap_password));
    memset(config->smtp_password, 0, sizeof(config->smtp_password));
}

/* Print configuration (for debugging) */
void config_print(const Config *config) {
    printf("Configuration:\n");
    printf("  IMAP Server: %s:%d (SSL: %s)\n",
           config->imap_server, config->imap_port,
           config->imap_use_ssl ? "yes" : "no");
    printf("  IMAP User: %s\n", config->imap_username);
    printf("  SMTP Server: %s:%d (SSL: %s, STARTTLS: %s)\n",
           config->smtp_server, config->smtp_port,
           config->smtp_use_ssl ? "yes" : "no",
           config->smtp_use_starttls ? "yes" : "no");
    printf("  SMTP User: %s\n", config->smtp_username);
    printf("  Email: %s (%s)\n", config->email_address, config->display_name);
}
