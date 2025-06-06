#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "packet.h"

#define BUFFER_SIZE 1000
#define PACKET_SIZE 1028

// BASED ON NOTES FROM "Beej's Guide to Network Programming", Chapter 5

void setup_server_address(struct sockaddr_in *server_addr, const char *ip, const char *port);
int create_udp_socket();
int send_file(char *buffer, struct sockaddr_in *server_addr, socklen_t addr_size, int sockfd);
void failure(char *msg, int sockfd);
long get_file_size(FILE *file);
void packet_to_string(struct packet *p, char *str);

int main(int argc, char *argv[])
{
    if (argc != 3)
        failure("Usage: ./deliver <server_ip> <server_port>", -1);

    char buffer[BUFFER_SIZE];

    struct sockaddr_in server_addr;
    socklen_t addr_size = sizeof(server_addr);
    setup_server_address(&server_addr, argv[1], argv[2]);

    int sockfd = create_udp_socket();
    if (sockfd == -1)
        return EXIT_FAILURE;

    printf("Server is running at port %s\n", argv[2]);

    printf("Input  'ftp < filename >':\n");
    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = '\0';

    if (strncmp(buffer, "ftp ", 4) != 0)
        failure("Invalid input", sockfd);

    char *filename = buffer + 4;

    if (access(filename, F_OK) == -1)
        failure("access", sockfd);

    if (sendto(sockfd, "ftp", BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, addr_size) == -1)
        failure("sendto", sockfd);

    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_size);
    if (n == -1)
        failure("recvfrom", sockfd);

    buffer[n] = '\0';

    if (strcmp(buffer, "yes") != 0)
        failure("Server did not respond with 'yes'", sockfd);

    printf("A file transfer can start\n");

    FILE *file = fopen(filename, "r");
    if (file == NULL)
        failure("fopen", sockfd);

    long file_size = get_file_size(file);
    int total_frag = file_size / 1000;
    if (file_size % 1000 != 0)
        total_frag++;

    int frag_no = 1;

    while (frag_no <= total_frag)
    {
        int n = fread(buffer, 1, BUFFER_SIZE, file);
        if (n == -1)
            failure("fread", sockfd);

        struct packet p;
        p.total_frag = total_frag;
        p.frag_no = frag_no;
        p.size = n;
        p.filename = filename;
        memcpy(p.filedata, buffer, n);

        char packet_str[PACKET_SIZE];
        packet_to_string(&p, packet_str);

        if (sendto(sockfd, packet_str, PACKET_SIZE, 0, (struct sockaddr *)&server_addr, addr_size) == -1)
            failure("sendto", sockfd);

        n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_size);
        if (n == -1)
            failure("recvfrom", sockfd);
        buffer[n] = '\0';
        if (strcmp(buffer, "ACK") != 0)
            failure("Server did not respond with 'ACK'", sockfd);

        printf("Sent fragment %d of %d, acknowledged\n", frag_no, total_frag);
        frag_no++;
    }

    // Close
    printf("File transfer complete\n");
    fclose(file);
    close(sockfd);
    return 0;
}

int create_udp_socket()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
        failure("socket", -1);
    return sockfd;
}

void setup_server_address(struct sockaddr_in *server_addr, const char *ip, const char *port)
{
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(atoi(port));
    server_addr->sin_addr.s_addr = inet_addr(ip);
}

void failure(char *msg, int sockfd)
{
    perror(msg);
    if (sockfd != -1)
        close(sockfd);
    exit(EXIT_FAILURE);
}

long get_file_size(FILE *file)
{
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size == -1)
        failure("ftell", -1);
    return size;
}

void packet_to_string(struct packet *p, char *str)
{
    int offset = 0;
    offset += sprintf(str, "%d:", p->total_frag);
    offset += sprintf(str + offset, "%d:", p->frag_no);
    offset += sprintf(str + offset, "%d:", p->size);
    offset += sprintf(str + offset, "%s:", p->filename);
    memcpy(str + offset, p->filedata, p->size);
}