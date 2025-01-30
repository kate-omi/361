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
    struct sockaddr_in server_addr;
    socklen_t addr_size = sizeof(server_addr);
    int sockfd, new_fd;
    char buffer[BUFFER_SIZE];

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    // Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        return 1;
    }

    printf("Server is running at port %s\n", argv[2]);

    printf("Input  'ftp < filename >':\n");
    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = '\0';

    if (strncmp(buffer, "ftp ", 4) == 0)
    {
        if (access(buffer + 4, F_OK) == 0)
        {
            sendto(sockfd, "ftp", 3, 0, (struct sockaddr *)&server_addr, &addr_size);
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                             (struct sockaddr *)&server_addr, &addr_size);
            if (n == -1)
            {
                perror("recvfrom");
                close(sockfd);
                return 1;
            }
            buffer[n] = '\0';

            if (strcmp(buffer, "yes") == 0)
            {
                printf("A file transfer can start\n");
            }
        }
    }

    // Close
    close(sockfd);
    return 0;
}