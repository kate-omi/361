#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

#include "server.h"
#include "client.h"

#define errno_exit(str) \
	do { int err = errno; perror(str); exit(err); } while (0)

int num_clients = 0;
int num_sessions = 0;
client *connected_clients[MAX_CLIENTS] = {NULL}; 
session *sessions[MAX_SESSIONS] = {NULL}; 
pthread_mutex_t mut_clients;
pthread_mutex_t mut_sessions;

int main(int argc, char *argv[]) {
    // set up tcp socket to accept connections
    if (argc != 2) {
        printf("usage: server <port>\n");
        exit(0);
    }
    const char* PORT = argv[1];

    int status;
    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // dont specify ipv
    hints.ai_socktype = SOCK_STREAM; // datagram socket
    hints.ai_flags = AI_PASSIVE; // use my ip

    // build address info
    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    // create socket
    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) {
        errno_exit("socket");
    }
    // bind to port
    int b = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (b == -1) {
        errno_exit("bind");
    }
    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1) {
        errno_exit("listen");
    }
    printf("server: waiting for connections...\n");
    
    // accept connections and handle concurrently
    pthread_mutex_init(&mut_clients, NULL);
    pthread_mutex_init(&mut_sessions, NULL);

    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("Server: got connection from %s\n", s);

        int *client_sock = malloc(sizeof(int));
        *client_sock = new_fd;
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, client_sock);
        pthread_detach(thread_id);
    }

    close(sockfd);
    return 0;
}

void *handle_client(void *arg) {
    int clientfd = *(int *)arg;
    free(arg);

    while (1) {
        int msg_len;
        char buffer[BUFFER_SIZE];

        // read msg size
        int bytes_received = recv(clientfd, &msg_len, HEADER_SIZE, 0);
        if (bytes_received == 0) {
            printf("client disconnected\n");
            remove_client(clientfd);
            break;
        }
        if (bytes_received != HEADER_SIZE) {
            perror("recv msg length failed");
            break;
        }

        // read msg
        bytes_received = recv(clientfd, buffer, msg_len, 0);
        if (bytes_received == 0) {
            printf("client disconnected\n");
            remove_client(clientfd);
            break;
        }
        if (bytes_received != msg_len) {
            perror("recv msg content failed");
            break;
        }

        message msg;
        deserialize_message(buffer, &msg);
        print_received(msg_len, &msg);

        handle_message(clientfd, &msg);
    }

    close(clientfd);
}

void handle_message(int sockfd, message *msg) {
    switch (msg->type) {
        case LOGIN:
            handle_login(sockfd, msg);
            break;
        case JOIN:
            handle_join(sockfd, msg);
            break;
        case LEAVE_SESS:
            handle_leave(sockfd, msg);
            break;
        case NEW_SESS:
            handle_create_session(sockfd, msg);
            break;
        case QUERY:
            handle_query(sockfd, msg);
            break;
        case MESSAGE:
            handle_forward_message(sockfd, msg);
            break;
        default:
            printf("unexpected msg type: %d\n", msg->type);
            break;
    }
}

void handle_login(int sockfd, message *msg) {
    printf("handling login\n");
    char password[msg->size + 1]; 
    memcpy(password, msg->data, msg->size);
    password[msg->size] = '\0'; 

    message response = {0};
    response.type = LO_NAK;

    bool client_found = false, password_valid = false;
    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (strcmp(client_list[i].client_id, msg->source) == 0) {
            client_found = true;
            if (strcmp(client_list[i].password, password) == 0) {
                response.type = LO_ACK;
                password_valid = true;
            }
            break;
        }
    }
    printf("client found: %d, pass valid: %d\n", client_found, password_valid);

    if (response.type == LO_NAK) {
        char* reason;
        if (!client_found) {
            reason = "client ID not found";
        } else if (!password_valid) {
            reason = "incorrect password";
        } else {
            reason = "reason unknown";
        }
        strncpy(response.data, reason, MAX_DATA);
        response.size = strlen(reason);

        if (send_message(sockfd, &response) == -1) {
            printf("failed to send message (login)\n");
        }
        return;
    }

    char* failure_reason;
    add_client(sockfd, msg->source, password, &failure_reason);

    if (failure_reason) {
        printf("failed to add client during login: %s\n", failure_reason);
        response.type = LO_NAK;
        strncpy(response.data, failure_reason, MAX_DATA);
        response.size = strlen(failure_reason);
    } else {
        printf("successfully logged in\n");
    }
    
    if (send_message(sockfd, &response) == -1) {
        printf("failed to send message\n");
        return;
    }
}

void handle_join(int sockfd, message *msg) {
    char session_id[msg->size + 1]; 
    memcpy(session_id, msg->data, msg->size);
    session_id[msg->size] = '\0'; 

    char* failure_reason;
    client *c = get_client_by_id(msg->source);
    if (c == NULL) {
        failure_reason = "client not found";
    } else if (c->session_id[0] != '\0') {
        failure_reason = "client must leave session before joining another session";
    } else {
        join_session(session_id, c, &failure_reason);
    }

    message response = {0};
    if (failure_reason) {
        response.type = JN_NAK;
        response.size = snprintf(response.data, MAX_DATA, "%s, %s", session_id, failure_reason);
    } else {
        response.type = JN_ACK;
        strncpy(response.data, session_id, MAX_DATA);
        response.size = strlen(session_id);
        printf("client '%s' joined session '%s'\n", c->client_id, session_id);
    }

    // Send response
    if (send_message(sockfd, &response) == -1) {
        printf("failed to send join session response\n");
    }
}

void handle_leave(int sockfd, message *msg) {
    client *c = get_client_by_id(msg->source);
    if (c->session_id[0] == '\0') {
        printf("client '%s' is already not in a session\n", c->client_id);
        return;
    }
    printf("client '%s' leaving session '%s'\n", c->client_id, c->session_id);
    leave_session(c->session_id, c);
}

void handle_create_session(int sockfd, message *msg) {
    char session_id[msg->size + 1]; 
    memcpy(session_id, msg->data, msg->size);
    session_id[msg->size] = '\0';

    client *c = get_client_by_id(msg->source);
    if (c->session_id[0] != '\0') {
        printf("client must leave session '%s' before creating new session\n", c->session_id);
        return;
    }

    char *failure_reason;
    session *new_session = create_session(session_id, &failure_reason);
    if (failure_reason) {
        printf("failed to create session: %s\n", failure_reason);
        return;
    }

    join_session(session_id, c, &failure_reason);
    if (failure_reason) {
        printf("failed to create session: %s\n", failure_reason);
        return;
    }

    message response = {0};
    response.type = NS_ACK;
    if (send_message(sockfd, &response) == -1) {
        printf("failed to send join session response\n");
    }
    printf("created session '%s'\n", session_id);
}

void handle_query(int sockfd, message *msg) {
    char data[MAX_DATA];
    int data_size = get_server_status(data, MAX_DATA);

    message response = {0};
    response.type = QU_ACK;
    response.size = data_size;
    memcpy(response.data, data, data_size);

    if (send_message(sockfd, &response) == -1) {
        printf("failed to send join session response\n");
    }
}

void handle_forward_message(int sockfd, message *msg) {
    client *c = get_client_by_id(msg->source);
    char *session_id = c->session_id;
    if (session_id[0] == '\0') {
        printf("client '%s' not in session, will not forward message\n", msg->source);
        return;
    }

    message fwd_message = {0};
    fwd_message.type = MESSAGE;
    strncpy(fwd_message.source, msg->source, MAX_NAME);
    fwd_message.size = msg->size;
    memcpy(fwd_message.data, msg->data, MAX_DATA);

    session *s = get_session_by_id(session_id);
    if (s == NULL) {
        printf("msg forwarding, session not found\n");
        return;
    }

    for (int i = 0; i < SESSION_CAPACITY; i++) {
        if (s->clients[i] && strcmp(s->clients[i]->client_id, msg->source) != 0) {
            send_message(s->clients[i]->sockfd, &fwd_message);
        }
    }
}

client* get_client_by_id(char *client_id) {
    pthread_mutex_lock(&mut_clients);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (connected_clients[i] && strcmp(connected_clients[i]->client_id, client_id) == 0) {
            pthread_mutex_unlock(&mut_clients);
            return connected_clients[i];
        }
    }
    pthread_mutex_unlock(&mut_clients);
    return NULL;
}

session* get_session_by_id(char *session_id) {
    pthread_mutex_lock(&mut_sessions);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i] && strcmp(sessions[i]->session_id, session_id) == 0) {
            pthread_mutex_unlock(&mut_sessions);
            return sessions[i];
        }
    }
    pthread_mutex_unlock(&mut_sessions);
    return NULL;
}

void add_client(int sockfd, char* client_id, char* password, char** failure_reason) {
    *failure_reason = NULL;
    pthread_mutex_lock(&mut_clients);

    // check if client id already exists
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (connected_clients[i] != NULL && strcmp(connected_clients[i]->client_id, client_id) == 0) {
            *failure_reason = "client ID already exists";
            pthread_mutex_unlock(&mut_clients);
            return;
        }
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (connected_clients[i] == NULL) {
            connected_clients[i] = malloc(sizeof(client));

            strncpy(connected_clients[i]->client_id, client_id, MAX_NAME);
            strncpy(connected_clients[i]->password, password, MAX_NAME);
            connected_clients[i]->sockfd = sockfd;
            num_clients++;

            pthread_mutex_unlock(&mut_clients);
            return;
        }
    }
    *failure_reason = "connected clients at maximum capacity";
    pthread_mutex_unlock(&mut_clients);
}

void remove_client(int sockfd) {
    pthread_mutex_lock(&mut_clients);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (connected_clients[i] != NULL && connected_clients[i]->sockfd == sockfd) {
            if (connected_clients[i]->session_id[0] != '\0') {
                leave_session(connected_clients[i]->session_id, connected_clients[i]);
            }
            close(sockfd);
            free(connected_clients[i]);
            connected_clients[i] = NULL;
            num_clients--;
            break;
        }
    }
    pthread_mutex_unlock(&mut_clients);
}

session *create_session(char *session_id, char **failure_reason) {
    *failure_reason = NULL;
    session *new_session;
    pthread_mutex_lock(&mut_sessions);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i] && strcmp(sessions[i]->session_id, session_id) == 0) {
            *failure_reason = "session ID already exists";
            pthread_mutex_unlock(&mut_sessions);
            return new_session;
        }
    }

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i] == NULL) {
            sessions[i] = malloc(sizeof(session));
            strncpy(sessions[i]->session_id, session_id, MAX_NAME);
            sessions[i]->num_clients = 0;
            new_session = sessions[i];
            num_sessions++;
            break;
        }
    }
    pthread_mutex_unlock(&mut_sessions);
    return new_session;
}

void delete_session(char *session_id) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i] && strcmp(sessions[i]->session_id, session_id) == 0) {
            if (sessions[i]->num_clients == 0) {
                free(sessions[i]);
                sessions[i] = NULL;
                num_sessions--;
            }
            return;
        }
    }
}

void join_session(char *session_id, client *c, char **failure_reason) {
    *failure_reason = "session with given id does not exist";
    pthread_mutex_lock(&mut_sessions);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i] && strcmp(sessions[i]->session_id, session_id) == 0) {
            *failure_reason = NULL; 
            if (sessions[i]->num_clients >= SESSION_CAPACITY) {
                *failure_reason = "session is full";
                break;
            }
            for (int j = 0; j < SESSION_CAPACITY; j++) {
                if (sessions[i]->clients[j] == NULL) {
                    sessions[i]->clients[j] = c;
                    sessions[i]->num_clients++;
                    strncpy(c->session_id, session_id, MAX_NAME);
                    break;
                }
            }
            break;
        }
    }
    pthread_mutex_unlock(&mut_sessions);
}

void leave_session(char *session_id, client *c) {
    pthread_mutex_lock(&mut_sessions);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i] && strcmp(sessions[i]->session_id, session_id) == 0) {
            for (int j = 0; j < SESSION_CAPACITY; j++) {
                if (sessions[i]->clients[j] && strcmp(sessions[i]->clients[j]->client_id, c->client_id) == 0) {
                    sessions[i]->clients[j] = NULL;
                    sessions[i]->num_clients--;
                    memset(c->session_id, 0, sizeof(c->session_id));

                    // remove session if empty
                    if (sessions[i]->num_clients <= 0) {
                        delete_session(sessions[i]->session_id);
                    }

                    pthread_mutex_unlock(&mut_sessions);
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&mut_sessions);
}

// writes list of users and sessions to buffer
int get_server_status(char *buffer, size_t size) {
    pthread_mutex_lock(&mut_clients);
    pthread_mutex_lock(&mut_sessions);

    char *ptr = buffer;
    int remaining = size;

    // users list
    int written = snprintf(ptr, remaining, "\n---- clients ----\n");
    ptr += written;
    remaining -= written;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (connected_clients[i]) {
            char *session;
            if (connected_clients[i]->session_id[0] == '\0') {
                session = "not in session";
            } else {
                session = connected_clients[i]->session_id;
            }
            written = snprintf(ptr, remaining, "  - %s [ %s ]\n", connected_clients[i]->client_id, session);
            ptr += written;
            remaining -= written;
        }
    }

    // sessions list
    written = snprintf(ptr, remaining, "\n---- sessions ----\n");
    ptr += written;
    remaining -= written;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i]) {  
            written = snprintf(ptr, remaining, "  - %s\n", sessions[i]->session_id);
            ptr += written;
            remaining -= written;
        }
    }
    if (num_sessions == 0) {
        written = snprintf(ptr, remaining, "  - currently no sessions");
        ptr += written;
        remaining -= written;
    }

    pthread_mutex_unlock(&mut_sessions);
    pthread_mutex_unlock(&mut_clients);
    return size - remaining;
}

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}