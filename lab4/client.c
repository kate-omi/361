#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#include "client.h"

char buffer[BUFFER_SIZE];
char data[BUFFER_SIZE];

int sockfd = -1;
bool logged_in = false;
char client_id[MAX_NAME];
pthread_t recv_thread;
bool receiving = false;

void failure(char *msg, int sockfd) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    while(1) {
        char input[256];
        char command[15];
        int return_value;

        printf("\nwaiting for input...\n");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("fgets failed");
        }
        input[strcspn(input, "\n")] = 0;
        sscanf(input, "%s", command);

        if (strcmp(command, "/login") == 0) {
            if (logged_in) {
                printf("already logged in\n");
                continue;
            }
            if (login(input) == 0) {
                printf("successfully logged in\n");
                logged_in = true;
                receiving = true;
                pthread_create(&recv_thread, NULL, receive_thread, &sockfd);
            } else {
                printf("failed to log in\n");
            }
        } else if (strcmp(command, "/quit") == 0) {
            if (logged_in) {
                logout();
            }
            exit(0);
        } else if (!logged_in) {
            printf("client must be logged in to use commands\n");
        } else if (strcmp(command, "/logout") == 0) {
            return_value = logout();
        } else if (strcmp(command, "/joinsession") == 0) {
            return_value = request_join_session(input);
        } else if (strcmp(command, "/leavesession") == 0) {
            return_value = request_leave_session();
        } else if (strcmp(command, "/createsession") == 0) {
            return_value = request_create_session(input);
        } else if (strcmp(command, "/list") == 0) {
            return_value = list();
        } else {
            return_value = send_session_message(input);
        }
    }

    return 0;
}

void *receive_thread(void *arg) {
    int sockfd = *(int *)arg;

    while (1) {
        message msg;
        if (recv_message(sockfd, &msg) == -1) {
            printf("server disconnected\n");
            logout();
            return NULL;
        }
        handle_response(sockfd, &msg);
    }
    return NULL;
}

void handle_response(int sockfd, message *msg) {
    switch (msg->type) {
        case JN_ACK:
            printf("successfully joined session: %.*s", msg->size, msg->data);
            break;
        case JN_NAK:
            printf("failed to join session: %.*s", msg->size, msg->data);
            break;
        case NS_ACK:
            printf("successfully created session\n");
            break;
    }
}

int login(char* input) {
    char password[MAX_DATA];
    char server_ip[15];
    char server_port[15];

    if (sscanf(input, "/login %s %s %s %s", client_id, password, server_ip, server_port) != 4) {
        printf("Invalid input\n");
        return -1;
    }
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(server_ip, server_port, &hints, &res) != 0) return -1;

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) return -1;
    
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) return -1;
        
    printf("connected to server\n");
    printf("logging in...\n");

    message login_request = {0};
    login_request.type = LOGIN;
    strncpy(login_request.source, client_id, MAX_NAME);
    login_request.size = strlen(password);
    memcpy(login_request.data, password, MAX_DATA);

    if (send_message(sockfd, &login_request) == -1) {
        printf("failed send\n");
        return -1;
    }
    message login_response;
    if (recv_message(sockfd, &login_response) == -1) {
        printf("failed receive\n");
        return -1;
    }
    return login_response.type == LO_ACK ? 0 : -1;
}

int logout() {
    receiving = false;
    pthread_join(recv_thread, NULL); 

    close(sockfd);
    sockfd = -1;
    logged_in = false;
    memset(client_id, 0, sizeof(client_id));
    return 0;
}

int request_join_session(char* input) { 
    char* session_id;
    if (sscanf(input, "/joinsession %s", session_id) != 1) {
        printf("Invalid input\n");
        return -1;
    }   

    message request = {0};
    request.type = JOIN;
    strncpy(request.source, client_id, MAX_NAME);
    request.size = strlen(session_id);
    memcpy(request.data, session_id, MAX_DATA);

    if (send_message(sockfd, &request) == -1) {
        printf("failed send\n");
        return -1;
    }
    // }
    // message join_response;
    // if (recv_message(sockfd, &join_response) == -1) {
    //     printf("failed receive\n");
    //     return -1;
    // }
    // return join_response.type == JN_ACK ? 0 : -1;
    return 0;
}

int request_leave_session() {
    message request = {0};
    request.type = LEAVE_SESS;
    strncpy(request.source, client_id, MAX_NAME);
    request.size = 0;

    if (send_message(sockfd, &request) == -1) {
        printf("failed send\n");
        return -1;
    }
    return 0;
}

int request_create_session(char* input) {
    char* session_id;
    if (sscanf(input, "/createsession %s", session_id) != 1) {
        printf("Invalid input\n");
        return -1;
    }

    message request = {0};
    request.type = NEW_SESS;
    strncpy(request.source, client_id, MAX_NAME);
    request.size = strlen(session_id);
    memcpy(request.data, session_id, MAX_DATA);

    if (send_message(sockfd, &request) == -1) {
        printf("failed send\n");
        return -1;
    }
    // message create_session_response;
    // if (recv_message(sockfd, &create_session_response) == -1) {
    //     printf("failed receive\n");
    //     return -1;
    // }
    // return create_session_response.type == NS_ACK ? 0 : -1;
    return 0;
}

int list() {
    message request = {0};
    request.type = QUERY;
    strncpy(request.source, client_id, MAX_NAME);
    request.size = 0;

    if (send_message(sockfd, &request) == -1) {
        printf("failed send\n");
        return -1;
    }

    // message list_response;
    // if (recv_message(sockfd, &list_response) == -1) {
    //     printf("failed receive\n");
    //     return -1;
    // }
    return 0;
}

int send_session_message(char* input) {
    message request = {0};
    request.type = MESSAGE;
    strncpy(request.source, client_id, MAX_NAME);
    request.size = strlen(input);
    memcpy(request.data, input, MAX_DATA);

    if (send_message(sockfd, &request) == -1) {
        printf("failed send\n");
        return -1;
    }
    return 0;
}