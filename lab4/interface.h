#ifndef INTERFACE_H
#define INTERFACE_H

#define MAX_NAME 100
#define MAX_DATA 1000
#define MAX_CLIENTS 100
#define NUM_CLIENTS 5
#define BUFFER_SIZE 2000

#define BACKLOG 10
#define HEADER_SIZE 4 // prepending message length (4 bytes)

typedef enum {
    LOGIN = 1,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK
} MessageType;

typedef struct {
    unsigned int type;
    unsigned int size;
    char source[MAX_NAME];
    char data[MAX_DATA];
} message;

typedef struct {
    int sockfd;
    char client_id[MAX_NAME];
    char password[MAX_NAME];
    char session_id[MAX_NAME];
} client;

typedef struct {
    char client_id[MAX_NAME];
    char password[MAX_NAME];
} client_db_entry;

extern client_db_entry client_list[NUM_CLIENTS];

int send_message(int sockfd, message *msg);
int recv_message(int sockfd, message *msg);
int serialize_message(char *buffer, message *msg);
int deserialize_message(char *buffer, message *msg);
void print_sent(int size, message *msg);
void print_received(int size, message *msg);

#endif