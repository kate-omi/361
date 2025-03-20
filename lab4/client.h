#ifndef CLIENT_H
#define CLIENT_H

#include "interface.h"

void *receive_thread(void *arg);
void handle_response(int sockfd, message *msg);

int login(char* input);
int logout();
int request_join_session(char* input);
int request_leave_session();
int request_create_session(char* input);
int list();
int send_session_message(char* input);

#endif