#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>

#define MAX_USERS 100
#define MAX_NAME 100

typedef struct {
    char id[MAX_NAME];
    char password[MAX_NAME];
} ClientCredential;

void load_credentials();
void add_credential(char *client_id, char *password);
bool validate_credential(char *client_id, char *password);
bool check_user_existence(char *client_id);

#endif