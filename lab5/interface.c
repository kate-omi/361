#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#include "interface.h"

#define DEBUG false

// writes serialized message to buffer
int serialize_message(char *buffer, message *msg) {
    int offset = snprintf(buffer, BUFFER_SIZE, "%u:%u:", msg->type, msg->size);
    if (offset < 0 || offset >= BUFFER_SIZE) return -1;

    memcpy(buffer + offset, msg->source, MAX_NAME);
    offset += MAX_NAME;

    memcpy(buffer + offset, msg->data, msg->size);
    offset += MAX_DATA;

    return offset;
}

// deserializes buffer content into message struct
int deserialize_message(char *buffer, message *msg) {
    int offset;
    if (sscanf(buffer, "%u:%u:%n", &msg->type, &msg->size, &offset) != 2) {
        return -1;
    }
    memcpy(msg->source, buffer + offset, MAX_NAME);
    memcpy(msg->data, buffer + offset + MAX_NAME, msg->size);
    return 0;
}

// serializes message struct and sends it through socket
int send_message(int sockfd, message *msg) {
    char buffer[BUFFER_SIZE] = {0};
    int size = serialize_message(buffer, msg);
    if (size < 0) return -1;

    if (send(sockfd, &size, HEADER_SIZE, 0) != HEADER_SIZE) {
        return -1;
    }
    if (send(sockfd, buffer, size, 0) != size) {
        return -1;
    }
    if (DEBUG) print_sent(size, msg);
    return 0;
}

int recv_message(int sockfd, message *msg) {
    char buffer[BUFFER_SIZE];
    // read msg size
    int msg_len;
    if (recv(sockfd, &msg_len, HEADER_SIZE, 0) != HEADER_SIZE) {
        perror("recv msg length");
        return -1;
    }
    // read msg
    if (recv(sockfd, buffer, msg_len, 0) != msg_len) {
        perror("recv msg content");
        return -1;
    }
    deserialize_message(buffer, msg);
    if (DEBUG) print_received(msg_len, msg);
    return 0;
}

void print_sent(int size, message *msg) {
    printf("\n--- Sent (%d) ---\n", size);
    printf("Type: %d\n", msg->type);
    printf("Size: %d\n", msg->size);
    printf("Source: %.*s\n", MAX_NAME, msg->source);
    printf("Data: %.*s\n", msg->size, msg->data);
}

void print_received(int size, message *msg) {
    printf("\n--- Received (%d) ---\n", size);
    printf("Type: %d\n", msg->type);
    printf("Size: %d\n", msg->size);
    printf("Source: %.*s\n", MAX_NAME, msg->source);
    printf("Data: %.*s\n", msg->size, msg->data);
}

void print_raw_data(message *msg) {
    printf("%.*s\n", msg->size, msg->data);
}

int encode_direct_message(char *buffer, char *recipient, char *message) {
    memcpy(buffer, recipient, MAX_NAME);
    memcpy(buffer + MAX_NAME, message, BUFFER_SIZE - MAX_NAME);
}

int decode_direct_message(char *buffer, char *recipient, char *message) {
    strcpy(recipient, buffer);
    strcpy(message, buffer + MAX_NAME);
}

int encode_register_message(char *buffer, char *client_id, char *password) {
    memcpy(buffer, client_id, MAX_NAME);
    memcpy(buffer + MAX_NAME, password, MAX_NAME);
}

int decode_register_message(char *buffer, char *client_id, char *password) {
    memcpy(client_id, buffer, MAX_NAME);
    memcpy(password, buffer + MAX_NAME, MAX_NAME);
}