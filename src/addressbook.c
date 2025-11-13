#define _POSIX_C_SOURCE 200809L
#include "addressbook.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initialize address book */
int addressbook_init(AddressBook *book, const char *filename) {
    book->contacts = NULL;
    book->contact_count = 0;
    book->contact_capacity = 0;
    strncpy(book->filename, filename, sizeof(book->filename) - 1);
    book->filename[sizeof(book->filename) - 1] = '\0';
    return 0;
}

/* Load address book from file */
int addressbook_load(AddressBook *book) {
    FILE *f = fopen(book->filename, "r");
    if (!f) {
        /* File doesn't exist yet, that's okay */
        return 0;
    }

    addressbook_free(book);

    book->contact_capacity = 10;
    book->contacts = malloc(sizeof(Contact) * book->contact_capacity);
    book->contact_count = 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        /* Remove newline */
        line[strcspn(line, "\r\n")] = '\0';

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        /* Parse: name|email|notes */
        char *name = line;
        char *email = strchr(line, '|');
        if (!email) continue;
        *email++ = '\0';

        char *notes = strchr(email, '|');
        if (notes) {
            *notes++ = '\0';
        } else {
            notes = "";
        }

        /* Add contact */
        if (book->contact_count >= book->contact_capacity) {
            book->contact_capacity *= 2;
            book->contacts = realloc(book->contacts, sizeof(Contact) * book->contact_capacity);
        }

        Contact *contact = &book->contacts[book->contact_count];
        strncpy(contact->name, name, MAX_NAME_LEN - 1);
        contact->name[MAX_NAME_LEN - 1] = '\0';
        strncpy(contact->email, email, MAX_EMAIL_LEN - 1);
        contact->email[MAX_EMAIL_LEN - 1] = '\0';
        strncpy(contact->notes, notes, sizeof(contact->notes) - 1);
        contact->notes[sizeof(contact->notes) - 1] = '\0';

        book->contact_count++;
    }

    fclose(f);
    return book->contact_count;
}

/* Save address book to file */
int addressbook_save(AddressBook *book) {
    FILE *f = fopen(book->filename, "w");
    if (!f) {
        return -1;
    }

    fprintf(f, "# cterm Address Book\n");
    fprintf(f, "# Format: name|email|notes\n\n");

    for (int i = 0; i < book->contact_count; i++) {
        Contact *contact = &book->contacts[i];
        fprintf(f, "%s|%s|%s\n", contact->name, contact->email, contact->notes);
    }

    fclose(f);
    return 0;
}

/* Free address book */
void addressbook_free(AddressBook *book) {
    if (book->contacts) {
        free(book->contacts);
        book->contacts = NULL;
    }
    book->contact_count = 0;
    book->contact_capacity = 0;
}

/* Add contact */
int addressbook_add_contact(AddressBook *book, const char *name, const char *email, const char *notes) {
    if (book->contact_count >= MAX_CONTACTS) {
        return -1;
    }

    if (!book->contacts) {
        book->contact_capacity = 10;
        book->contacts = malloc(sizeof(Contact) * book->contact_capacity);
        book->contact_count = 0;
    }

    if (book->contact_count >= book->contact_capacity) {
        book->contact_capacity *= 2;
        book->contacts = realloc(book->contacts, sizeof(Contact) * book->contact_capacity);
    }

    Contact *contact = &book->contacts[book->contact_count];
    strncpy(contact->name, name, MAX_NAME_LEN - 1);
    contact->name[MAX_NAME_LEN - 1] = '\0';
    strncpy(contact->email, email, MAX_EMAIL_LEN - 1);
    contact->email[MAX_EMAIL_LEN - 1] = '\0';
    strncpy(contact->notes, notes ? notes : "", sizeof(contact->notes) - 1);
    contact->notes[sizeof(contact->notes) - 1] = '\0';

    book->contact_count++;
    return 0;
}

/* Remove contact */
int addressbook_remove_contact(AddressBook *book, int index) {
    if (index < 0 || index >= book->contact_count) {
        return -1;
    }

    /* Shift contacts down */
    for (int i = index; i < book->contact_count - 1; i++) {
        book->contacts[i] = book->contacts[i + 1];
    }

    book->contact_count--;
    return 0;
}

/* Find contact by email */
int addressbook_find_by_email(AddressBook *book, const char *email) {
    for (int i = 0; i < book->contact_count; i++) {
        if (strcasecmp(book->contacts[i].email, email) == 0) {
            return i;
        }
    }
    return -1;
}

/* Find contact by name */
int addressbook_find_by_name(AddressBook *book, const char *name) {
    for (int i = 0; i < book->contact_count; i++) {
        if (strcasecmp(book->contacts[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Get contact by index */
Contact* addressbook_get_contact(AddressBook *book, int index) {
    if (index < 0 || index >= book->contact_count) {
        return NULL;
    }
    return &book->contacts[index];
}
