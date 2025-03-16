#include "functions.h"

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

char buffer[BUFFER_SIZE];
char data[BUFFER_SIZE];
int sockfd;

void failure(char *msg, int sockfd) {
    perror(msg);
    if (sockfd != -1)
        close(sockfd);
    exit(EXIT_FAILURE);
}

int main() {
    
    printf("Waiting for input...\n");
    char input[256];
    char command[15];
    int return_value;

    while(1) {
        if (fgets(input, sizeof(input), stdin) == NULL)
            failure("fgets", -1);

        input[strcspn(input, "\n")] = 0;
        sscanf(input, "%s", command);

        if (strcmp(command, "/login")) {
            sockfd = login(input);
        } else if (strcmp(command, "/logout")) {
            return_value = logout();
        } else if (strcmp(command, "/joinsession")) {
            return_value = join_session(input);
        } else if (strcmp(command, "/leavesession")) {
            return_value = leave_session();
        } else if (strcmp(command, "/createsession")) {
            return_value = create_session(input);
        } else if (strcmp(command, "/list")) {
            return_value = list();
        } else if (strcmp(command, "/quit")) {
            if (sockfd != -1)
                close(sockfd);
            return 0;
        } else if (strcmp(command, "/send")) {
            return_value = send_message(input);
        } else {
            printf("Invalid command\n");
        }

        if (return_value == -1)
            printf("Error\n");
    }

    return 0;
}

int login(char* input) {
    char client_id[50];
    char password[50];
    char server_ip[15];
    char server_port[15];


    if (sscanf(input, "/login %s %s %s %s", client_id, password, server_ip, server_port) != 4) {
        printf("Invalid input\n");
        return -1;
    }

    strcpy(buffer, "Login: ");
    strcat(buffer, client_id);

    int login_successful = 0;
    int i = 0;

    while (!login_successful) {
        if(strcmp(client_id, client_list[i].client_id) == 0) {
            if (strcmp(password, client_list[i].password) == 0) {
                printf("Password correct\n");
                login_successful = 1;
            } else {
                printf("Password incorrect\n");
                return -1;
            }
        }
        i++;
    }

    // Taken from: Beej's Guide to Network Programming, Chapter 5 //
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(server_ip, server_port, &hints, &res) != 0)
        failure("getaddrinfo", -1);

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
        failure("socket", -1);

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
        failure("connect", sockfd);
        
    printf("Connecting to server, checking user... \n");
    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
        failure("send", sockfd);

    login_successful = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (login_successful == -1)
        failure("recv", sockfd);
    else if (login_successful == 0)
        printf("Login failed\n");
    else
        printf("Login successful\n");

    return 1;
}

int logout() {
    close(sockfd);
    return 1;
}

int join_session(char* input) { 

    char* session_id;

    if (sscanf(input, "/joinsession %s", session_id) != 1) {
        printf("Invalid input\n");
        return -1;
    }

    strcpy(buffer, "Join session: ");
    strcat(buffer, session_id);

    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
        failure("send", sockfd);

    if (recv(sockfd, buffer, BUFFER_SIZE, 0) == -1)
        failure("recv", sockfd);
    
    if (strcmp(buffer, "Session joined") == 0) {
        printf("Session joined\n");
        return 1;
    } else {
        printf("Session not joined\n");
        return -1;
    }

    return -1;
}

int leave_session() {
    strcpy(buffer, "Leave session");

    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
        failure("send", sockfd);
    
    if (recv(sockfd, buffer, BUFFER_SIZE, 0) == -1)
        failure("recv", sockfd);
    
    if (strcmp(buffer, "Session left") == 0) {
        printf("Session left\n");
        return 1;
    } else {
        printf("Session not left\n");
        return -1;
    }
    
    return -1;
}

int create_session(char* input) {
    char* session_id;

    if (sscanf(input, "/createsession %s", session_id) != 1) {
        printf("Invalid input\n");
        return -1;
    }

    strcpy(buffer, "Create session: ");
    strcat(buffer, session_id);

    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
        failure("send", sockfd);

    if (recv(sockfd, buffer, BUFFER_SIZE, 0) == -1)
        failure("recv", sockfd);
    
    if (strcmp(buffer, "Session created") == 0) {
        printf("Session created\n");
        return 1;
    } else {
        printf("Session not created\n");
        return -1;
    }

    return -1;
}

int list() {
    strcpy(buffer, "List");

    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
        failure("send", sockfd);
    
    if (recv(sockfd, buffer, BUFFER_SIZE, 0) == -1)
        failure("recv", sockfd);
    
    printf("List of clients/session: %s\n", buffer);

    return 1;
}

int send_message(char* input) {
    if (sscanf(input, "/send %s", data) != 1) {
        printf("Invalid input\n");
        return -1;
    }

    strcpy(buffer, "Send: ");
    strcat(buffer, data);

    if (send(sockfd, buffer, strlen(buffer), 0) == -1)
        failure("send", sockfd);
    
    if (recv(sockfd, buffer, BUFFER_SIZE, 0) == -1)
        failure("recv", sockfd);

    return 1;
}