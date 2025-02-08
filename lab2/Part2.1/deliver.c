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

#define BUFFER_SIZE 100

// BASED ON NOTES FROM "Beej's Guide to Network Programming", Chapter 5

int time_subtract(struct timespec *result, struct timespec *x, struct timespec *y);

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr;
    socklen_t addr_size = sizeof(server_addr);
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct timespec x, y;

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
            clock_gettime(CLOCK_MONOTONIC, &y);
            sendto(sockfd, "ftp", 3, 0, (struct sockaddr *)&server_addr, addr_size);
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                             (struct sockaddr *)&server_addr, &addr_size);
            clock_gettime(CLOCK_MONOTONIC, &x);
            if (n == -1)
            {
                perror("recvfrom");
                close(sockfd);
                return 1;
            }
            buffer[n] = '\0';

            struct timespec result;
            if (time_subtract(&result, &x, &y) == 1)
            {
                printf("\nTime elapsed: %ld.%06ld seconds\n", result.tv_sec, result.tv_nsec);
            }

            if (strcmp(buffer, "yes") == 0)
            {
                printf("A file transfer can start\n");
            }
        }
        else
        {
            printf("File does not exist\n");
        }
    }

    // Close
    close(sockfd);
    return 0;
}
// TAKEN FROM Chapter 21 of the glibc manual
#include <stdio.h>
#include <time.h>

int time_subtract(struct timespec *result, struct timespec *x, struct timespec *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_nsec < y->tv_nsec)
    {
        int nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1; // Borrow 1 second (1e9 nanoseconds)
        y->tv_nsec -= 1000000000 * nsec;                       // Subtract 1e9 nanoseconds
        y->tv_sec += nsec;                                     // Add 1 second
    }
    if (x->tv_nsec - y->tv_nsec > 1000000000)
    {
        int nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
        y->tv_nsec += 1000000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time difference.
       tv_nsec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_nsec = x->tv_nsec - y->tv_nsec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}
