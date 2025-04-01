#include <stdio.h>
#include <string.h>
#include "auth.h"

#define DB_FILE "auth.txt"

static ClientCredential client_db[MAX_USERS];
static int db_size = 0;

void load_credentials() {
    FILE *fp = fopen(DB_FILE, "r");
    if (!fp) return;

    while (fscanf(fp, "%s %s", client_db[db_size].id, client_db[db_size].password) == 2) {
        db_size++;
    }
    fclose(fp);
}

void add_credential(char *client_id, char *password) {
    FILE *fp = fopen(DB_FILE, "a");
    if (!fp) return;

    fprintf(fp, "\n%s %s", client_id, password);
    fclose(fp);

    strncpy(client_db[db_size].id, client_id, MAX_NAME);
    strncpy(client_db[db_size].password, password, MAX_NAME);
    db_size++;
}

bool validate_credential(char *client_id, char *password) {
    for (int i = 0; i < db_size; i++) {
        if (strcmp(client_db[i].id, client_id) == 0 &&
            strcmp(client_db[i].password, password) == 0) {
            return true;
        }
    }
    return false;
}

bool check_user_existence(char *client_id) {
    for (int i = 0; i < db_size; i++) {
        if (strcmp(client_db[i].id, client_id) == 0) return true;
    }
    return false;
}