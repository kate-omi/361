#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define MAX_NAME 100
#define MAX_DATA 1000
#define NUM_CLIENTS 5
#define BUFFER_SIZE 1000

#define CONTROL 0
#define DATA 1

typedef struct {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
} message;

typedef struct {
    char client_id[MAX_NAME];
    char password[MAX_NAME];
}client;

client client_list[NUM_CLIENTS] = {
    {"jill", "abc"},
    {"jack", "def"},
    {"humpty", "ghi"},
    {"dumpty", "jkl"},
    {"james", "mno"}
};

int login(char* input);
int logout();
int join_session(char* input);
int leave_session();
int create_session(char* input);
int list();
int send_message(char* input);

#endif