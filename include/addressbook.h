#ifndef ADDRESSBOOK_H
#define ADDRESSBOOK_H

#define MAX_NAME_LEN 128
#define MAX_EMAIL_LEN 128
#define MAX_CONTACTS 1000

typedef struct {
    char name[MAX_NAME_LEN];
    char email[MAX_EMAIL_LEN];
    char notes[256];
} Contact;

typedef struct {
    Contact *contacts;
    int contact_count;
    int contact_capacity;
    char filename[512];
} AddressBook;

/* Address book operations */
int addressbook_init(AddressBook *book, const char *filename);
int addressbook_load(AddressBook *book);
int addressbook_save(AddressBook *book);
void addressbook_free(AddressBook *book);

/* Contact operations */
int addressbook_add_contact(AddressBook *book, const char *name, const char *email, const char *notes);
int addressbook_remove_contact(AddressBook *book, int index);
int addressbook_find_by_email(AddressBook *book, const char *email);
int addressbook_find_by_name(AddressBook *book, const char *name);
Contact* addressbook_get_contact(AddressBook *book, int index);

#endif /* ADDRESSBOOK_H */
