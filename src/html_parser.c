#define _POSIX_C_SOURCE 200809L
#include "html_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* HTML entity table */
typedef struct {
    const char *name;
    const char *value;
} HtmlEntity;

static const HtmlEntity html_entities[] = {
    {"&nbsp;", " "},
    {"&lt;", "<"},
    {"&gt;", ">"},
    {"&amp;", "&"},
    {"&quot;", "\""},
    {"&apos;", "'"},
    {"&#39;", "'"},
    {"&ndash;", "-"},
    {"&mdash;", "--"},
    {"&hellip;", "..."},
    {"&copy;", "(c)"},
    {"&reg;", "(R)"},
    {"&trade;", "(TM)"},
    {"&deg;", "°"},
    {"&plusmn;", "±"},
    {"&para;", "¶"},
    {"&sect;", "§"},
    {"&bull;", "•"},
    {"&middot;", "·"},
    {"&laquo;", "«"},
    {"&raquo;", "»"},
    {NULL, NULL}
};

/* Check if content is HTML */
int html_is_html_content(const char *content) {
    if (!content) return 0;

    /* Look for common HTML tags */
    const char *html_indicators[] = {
        "<html", "<HTML", "<body", "<BODY",
        "<div", "<DIV", "<p", "<P",
        "<br", "<BR", "<table", "<TABLE",
        "<!DOCTYPE", "<!doctype",
        NULL
    };

    for (int i = 0; html_indicators[i] != NULL; i++) {
        if (strstr(content, html_indicators[i])) {
            return 1;
        }
    }

    return 0;
}

/* Decode HTML entities */
void html_decode_entities(char *text) {
    char buffer[MAX_TEXT_LEN];
    char *src = text;
    char *dst = buffer;
    int dst_len = 0;

    while (*src && dst_len < MAX_TEXT_LEN - 1) {
        if (*src == '&') {
            /* Try to match entity */
            int matched = 0;
            for (int i = 0; html_entities[i].name != NULL; i++) {
                int len = strlen(html_entities[i].name);
                if (strncmp(src, html_entities[i].name, len) == 0) {
                    /* Copy entity value */
                    const char *val = html_entities[i].value;
                    while (*val && dst_len < MAX_TEXT_LEN - 1) {
                        *dst++ = *val++;
                        dst_len++;
                    }
                    src += len;
                    matched = 1;
                    break;
                }
            }

            if (!matched) {
                /* Check for numeric entities &#123; or &#xAB; */
                if (src[1] == '#') {
                    char *end;
                    long code;

                    if (src[2] == 'x' || src[2] == 'X') {
                        code = strtol(src + 3, &end, 16);
                    } else {
                        code = strtol(src + 2, &end, 10);
                    }

                    if (*end == ';' && code > 0 && code < 256) {
                        *dst++ = (char)code;
                        dst_len++;
                        src = end + 1;
                        matched = 1;
                    }
                }
            }

            if (!matched) {
                *dst++ = *src++;
                dst_len++;
            }
        } else {
            *dst++ = *src++;
            dst_len++;
        }
    }

    *dst = '\0';
    strcpy(text, buffer);
}

/* Strip HTML tags */
void html_strip_tags(const char *html, char *text, int text_size) {
    const char *src = html;
    char *dst = text;
    int dst_len = 0;
    int in_tag = 0;
    int in_script = 0;
    int in_style = 0;
    int add_newline = 0;

    while (*src && dst_len < text_size - 1) {
        if (*src == '<') {
            in_tag = 1;

            /* Check for special tags */
            if (strncasecmp(src, "<script", 7) == 0) {
                in_script = 1;
            } else if (strncasecmp(src, "</script", 8) == 0) {
                in_script = 0;
            } else if (strncasecmp(src, "<style", 6) == 0) {
                in_style = 1;
            } else if (strncasecmp(src, "</style", 7) == 0) {
                in_style = 0;
            }
            /* Add newlines for block elements */
            else if (strncasecmp(src, "<br", 3) == 0 ||
                     strncasecmp(src, "<p", 2) == 0 ||
                     strncasecmp(src, "</p", 3) == 0 ||
                     strncasecmp(src, "<div", 4) == 0 ||
                     strncasecmp(src, "</div", 5) == 0 ||
                     strncasecmp(src, "<li", 3) == 0 ||
                     strncasecmp(src, "<tr", 3) == 0 ||
                     strncasecmp(src, "</tr", 4) == 0) {
                add_newline = 1;
            }

            src++;
        } else if (*src == '>') {
            in_tag = 0;

            if (add_newline && !in_script && !in_style && dst_len < text_size - 2) {
                *dst++ = '\n';
                dst_len++;
                add_newline = 0;
            }

            src++;
        } else if (!in_tag && !in_script && !in_style) {
            /* Copy text content */
            if (*src == '\r') {
                src++;
                continue;
            }

            /* Collapse multiple spaces/newlines */
            if (isspace((unsigned char)*src)) {
                if (dst > text && !isspace((unsigned char)*(dst - 1))) {
                    *dst++ = ' ';
                    dst_len++;
                }
                src++;
            } else {
                *dst++ = *src++;
                dst_len++;
            }
        } else {
            src++;
        }
    }

    *dst = '\0';

    /* Decode entities */
    html_decode_entities(text);

    /* Trim leading/trailing whitespace */
    char *start = text;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    char *end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
}

/* Convert HTML to plain text */
int html_to_text(const char *html, char *text, int text_size) {
    if (!html || !text || text_size <= 0) {
        return -1;
    }

    /* Check if it's HTML */
    if (!html_is_html_content(html)) {
        /* Not HTML, just copy */
        strncpy(text, html, text_size - 1);
        text[text_size - 1] = '\0';
        return 0;
    }

    /* Strip tags */
    html_strip_tags(html, text, text_size);

    return 0;
}
