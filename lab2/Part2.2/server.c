#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define errno_exit(str) \
	do { int err = errno; perror(str); exit(err); } while (0)

struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[1000];
} typedef packet;
    
#define MAXBUFLEN 1000
#define PACKET_SIZE 1028
#define FTP_YES "yes"
#define FTP_NO "no"
#define FTP_ACK "ACK"

void *get_in_addr(struct sockaddr *sa);
packet parse_packet_message(char *message);


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: server <port>\n");
        exit(0);
    }
    const char* PORT = argv[1];

    int status;
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // dont specify ipv
    hints.ai_socktype = SOCK_DGRAM; // datagram socket
    hints.ai_flags = AI_PASSIVE; // use my ip

    // build address info
    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    // create socket
    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) {
        errno_exit("socket");
    }
    // bind to port
    int b = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (b == -1) {
        errno_exit("bind");
    }
    freeaddrinfo(servinfo);
    
    int numbytes;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof client_addr;

    while (1) {
        char buf[MAXBUFLEN];
        printf("server: waiting to recvfrom...\n");

        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
            errno_exit("recvfrom");
        }
        buf[numbytes] = '\0';

        // check for ftp command
        if (strcmp(buf, "ftp") != 0) {
            if ((numbytes = sendto(sockfd, FTP_NO, strlen(FTP_NO), 0, (struct sockaddr *)&client_addr, client_addr_len)) == -1) {
                errno_exit("sendto");
            }
            continue;
        } 

        // begin ftp session
        printf("server: beginning ftp session\n");
        if ((numbytes = sendto(sockfd, FTP_YES, strlen(FTP_YES), 0, (struct sockaddr *)&client_addr, client_addr_len)) == -1) {
            errno_exit("sendto");
        }

        packet *file_fragments = NULL;
        char *filename = NULL;
        int num_frags = 0;
        int num_frags_received = 0;

        while (1) {
            char message[PACKET_SIZE];
            if ((numbytes = recvfrom(sockfd, message, PACKET_SIZE , 0, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
                errno_exit("recvfrom");
            }
            packet data = parse_packet_message(message);
            
            // create fragment array if not created
            if (!file_fragments) {
                file_fragments = (packet *)malloc(data.total_frag * sizeof(packet));
                num_frags = data.total_frag;
                filename = strdup(data.filename);
            }

            file_fragments[data.frag_no - 1] = data;
            file_fragments[data.frag_no - 1].filename = strdup(data.filename);
            num_frags_received++;

            // send acknowledgement
            if ((numbytes = sendto(sockfd, FTP_ACK, strlen(FTP_ACK), 0, (struct sockaddr *)&client_addr, client_addr_len)) == -1) {
                errno_exit("sendto");
            }

            if (num_frags_received == num_frags) break;
        }
        
        printf("File Name: %s, Num Frags: %d, Num Frags Received: %d\n", filename ? filename : "NULL", num_frags, num_frags_received);   

        char new_filename[MAXBUFLEN];
        snprintf(new_filename, sizeof(new_filename), "server_%s", filename);

        FILE *file = fopen(new_filename, "wb");
        if (!file) {
            errno_exit("fopen");
        }
        
        // write file to disk
        for (int i = 0; i < num_frags; i++) {
            packet frag = file_fragments[i];
            size_t w = fwrite(frag.filedata, 1, frag.size, file);
            if (w != frag.size) {
                errno_exit("fwrite file data");
            }
        }
        fclose(file);
        printf("server: finished writing file data to disk!\n");
    }

    int c = close(sockfd);
    if (c == -1) {
        errno_exit("close");
    }

}

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

packet parse_packet_message(char *message) {
    packet data;
    char *filename_start;
    
    int offset;
    if (sscanf(message, "%u:%u:%u:%n", &data.total_frag, &data.frag_no, &data.size, &offset) != 3) {
        errno_exit("sscanf");
    }

    filename_start = message + offset;
    char *filedata_start = strchr(filename_start, ':');
    if (!filedata_start) {
        errno_exit("strchr");
    }

    *filedata_start = '\0';
    data.filename = strdup(filename_start);
    if (!data.filename) {
        errno_exit("strdup failed");
    }

    filedata_start++;
    memcpy(data.filedata, filedata_start, data.size);

    return data;
}
