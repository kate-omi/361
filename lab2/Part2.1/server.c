#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#define BUFFER_SIZE 100

// BASED ON NOTES FROM "Beej's Guide to Network Programming", Chapter 5

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res;
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);
    int sockfd, new_fd;
    char buffer[BUFFER_SIZE];

    // Get address info
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, argv[1], &hints, &res) != 0)
    {
        perror("getaddrinfo");
        return 1;
    }

    // Create socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
    {
        perror("socket");
        return 1;
    }

    // Bind socket
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("bind");
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    freeaddrinfo(res);
    printf("Client is running at port %s\n", argv[1]);

    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                     (struct sockaddr *)&their_addr, &addr_size);
    if (n == -1)
    {
        perror("recvfrom");
        return 1;
    }
    buffer[n] = '\0';
    printf("Received: %s\n", buffer);

    if (strcmp(buffer, "ftp") == 0)
    {
        if (sendto(sockfd, "yes", 3, 0, (struct sockaddr *)&their_addr,
                   addr_size) == -1)
        {
            perror("sendto");
            close(sockfd);
            return 1;
        }
    }
    else
    {
        // Otherwise, reply with "no"
        if (sendto(sockfd, "no", 2, 0, (struct sockaddr *)&their_addr,
                   addr_size) == -1)
        {
            perror("sendto");
            close(sockfd);
            return 1;
        }
    }

    // Close
    close(sockfd);
    return 0;
}