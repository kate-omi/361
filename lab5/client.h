#ifndef CLIENT_H
#define CLIENT_H

#include "interface.h"

void *receive_thread(void *arg);
void handle_response(int sockfd, message *msg);
int new_connection(char* server_ip, char* server_port);

int login(char* input);
int logout();
int register_client(char* input);

int request_join_session(char* input);
int request_leave_session();
int request_create_session(char* input);
int request_list();
int send_session_message(char* input);
int send_direct_message(char* input);


void print_session_message(message *msg);
void print_direct_message(message *msg);

#endif