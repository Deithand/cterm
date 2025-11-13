#ifndef HTML_PARSER_H
#define HTML_PARSER_H

#define MAX_TEXT_LEN 8192

/* HTML to text conversion */
int html_to_text(const char *html, char *text, int text_size);

/* Helper functions */
void html_strip_tags(const char *html, char *text, int text_size);
void html_decode_entities(char *text);
int html_is_html_content(const char *content);

#endif /* HTML_PARSER_H */
