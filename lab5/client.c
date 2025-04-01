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

int main() {
    while(1) {
        char input[256];
        char command[15];
        int return_value;

        printf("> ");
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
            return_value = login(input);
        } else if (strcmp(command, "/quit") == 0) {
            if (logged_in) {
                logout();
            }
            exit(0);
        } else if (strcmp(command, "/register") == 0) {
            return_value = register_client(input);
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
            return_value = request_list();
        } else if (strcmp(command, "/dm") == 0) {
            return_value = send_direct_message(input);
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
            printf("successfully joined session: %.*s\n", msg->size, msg->data);
            break;
        case JN_NAK:
            printf("failed to join session: %.*s\n", msg->size, msg->data);
            break;
        case NS_ACK:
            printf("successfully created session\n");
            break;
        case QU_ACK:
            printf("%.*s\n", msg->size, msg->data);
            break;
        case MESSAGE:
            print_session_message(msg);
            break;
        case DIRECT_MESSAGE:
            print_direct_message(msg);
            break;
        case DM_NAK:
            printf("failed to send dm: %.*s\n", msg->size, msg->data);
            break;
        case REGISTER_ACK:
            printf("sucessfully registered user!\n");
            break;
        case REGISTER_NACK:
            printf("failed to register user: %.*s\n", msg->size, msg->data);
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
    sockfd = new_connection(server_ip, server_port);

    message login_request = {0};
    login_request.type = LOGIN;
    strncpy(login_request.source, client_id, MAX_NAME);
    login_request.size = strlen(password);
    memcpy(login_request.data, password, MAX_DATA);

    if (send_message(sockfd, &login_request) == -1) return -1;

    message login_response;
    if (recv_message(sockfd, &login_response) == -1) return -1;

    if (login_response.type == LO_NAK || login_response.type != LO_ACK) {
        print_raw_data(&login_response);
        return -1;
    }
    logged_in = true;
    pthread_create(&recv_thread, NULL, receive_thread, &sockfd);
    printf("successfully logged in!\n");
    return 0;
}

int logout() {
    if (!logged_in) return 0;
    logged_in = false;

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd); 
    sockfd = -1;

    pthread_join(recv_thread, NULL); 
    memset(client_id, 0, sizeof(client_id));
    printf("logged out!\n");
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

    return send_message(sockfd, &request);
}

int request_leave_session() {
    message request = {0};
    request.type = LEAVE_SESS;
    strncpy(request.source, client_id, MAX_NAME);
    request.size = 0;

    return send_message(sockfd, &request);
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

    return send_message(sockfd, &request);
}

int request_list() {
    message request = {0};
    request.type = QUERY;
    strncpy(request.source, client_id, MAX_NAME);
    request.size = 0;

    return send_message(sockfd, &request);
}

int send_session_message(char* input) {
    message request = {0};
    request.type = MESSAGE;
    strncpy(request.source, client_id, MAX_NAME);
    request.size = strlen(input);
    memcpy(request.data, input, MAX_DATA);

    return send_message(sockfd, &request);
}

int send_direct_message(char* input) {
    char recipient_id[MAX_NAME];
    char msg_content[BUFFER_SIZE];

    if (strncmp(input, "/dm ", 4) != 0) {
        printf("Invalid input\n");
        return -1;
    }
    char *rest = input + 4;
    char *id = strtok(rest, " ");
    char *msg = strtok(NULL, "");
    if (!id || !msg) {
        printf("Invalid input\n");
        return -1;
    }
    strncpy(recipient_id, id, MAX_NAME);
    strncpy(msg_content, msg, BUFFER_SIZE);

    message request = {0};
    request.type = DIRECT_MESSAGE;
    strncpy(request.source, client_id, MAX_NAME);
    request.size = MAX_NAME + strlen(msg_content) + 1;
    encode_direct_message(request.data, recipient_id, msg_content);

    return send_message(sockfd, &request);
}

int register_client(char* input) {
    char *new_client_id;
    char password[MAX_DATA];
    char server_ip[15];
    char server_port[15];

    if (sscanf(input, "/register %s %s %s %s", new_client_id, password, server_ip, server_port) != 4) {
        printf("Invalid input\n");
        return -1;
    }
    int temp_sockfd = new_connection(server_ip, server_port);

    message request = {0};
    request.type = REGISTER;
    strncpy(request.source, new_client_id, MAX_NAME);
    request.size = 2 * MAX_NAME;
    encode_register_message(request.data, new_client_id, password);

    if (send_message(temp_sockfd, &request) == -1) return -1;

    message register_response;
    if (recv_message(temp_sockfd, &register_response) == -1) {
        printf("failed receive\n");
        return -1;
    }

    int status;
    if (register_response.type == REGISTER_NACK || register_response.type != REGISTER_ACK) {
        status = -1;
    }

    print_raw_data(&register_response);
    close(temp_sockfd);
    return status;
}

int new_connection(char* server_ip, char* server_port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(server_ip, server_port, &hints, &res) != 0) return -1;

    int temp_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (temp_sockfd == -1) return -1;
    
    if (connect(temp_sockfd, res->ai_addr, res->ai_addrlen) == -1) return -1;

    return temp_sockfd;
}

void print_session_message(message *msg) {
    printf("[session][%s] %.*s\n", msg->source, msg->size, msg->data);
}

void print_direct_message(message *msg) {
    char recipient_id[MAX_NAME], message_content[BUFFER_SIZE];
    decode_direct_message(msg->data, recipient_id, message_content);

    printf("[DM][%s] %.*s\n", msg->source, msg->size, message_content);
}