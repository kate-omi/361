#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include "interface.h"

#define MAX_SESSIONS 100
#define SESSION_CAPACITY 100

typedef struct {
    char session_id[MAX_NAME];
    client *clients[SESSION_CAPACITY];
    int num_clients;
} session;

void *handle_client(void *arg);
void handle_message(int sockfd, message *msg);
void handle_login(int sockfd, message *msg);
void handle_join(int sockfd, message *msg);
void handle_leave(int sockfd, message *msg);
void handle_create_session(int sockfd, message *msg);
void handle_query(int sockfd, message *msg);
void handle_forward_message(int sockfd, message *msg);

client* get_client_by_id(char *client_id);
session* get_session_by_id(char *session_id);
void add_client(int sockfd, char* client_id, char* password, char** failure_reason);
void remove_client(int sockfd);
session *create_session(char *session_id, char **failure_reason);
void delete_session(char *session_id);
void join_session(char *session_id, client *c, char **failure_reason);
void leave_session(char *session_id, client *c);
int get_server_status(char *buffer, size_t size);
void *get_in_addr(struct sockaddr *sa);

#endif