#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <errno.h>

#include "ff_config.h"
#include "ff_api.h"

#define MAXLINE 1024

#define MAX_EVENTS 1000000
int kq;
int server_fd;
struct kevent kevSet;
struct kevent events[MAX_EVENTS];


extern void start_compaction();
extern void SET(char*, char*);
extern void DEL(char*);
extern const char* GET(char*);


ssize_t send_message(int sockfd, const char *message, size_t length) {
    ssize_t total_sent = 0; // Total bytes sent so far
    ssize_t bytes_sent;

    while (total_sent < length) {
        bytes_sent = ff_write(sockfd, message + total_sent, length - total_sent);
        if (bytes_sent < 0) {
            perror("Send failed");
            return -1;
        }
        total_sent += bytes_sent;
    }

    printf("Message sent\n");
    printf("%s\n",message);
    return total_sent;
}

// Parse the RESP2 protocol
int parse_resp(char* message, char** command, char** arg1, char** arg2) {
    if (message[0] == '*') {
        // Split the message by "\r\n"
        char* token = strtok(message, "\r\n");

        // Ensure the message is correctly formatted and has enough parts
        if (token == NULL) return 0;

        // Skip the '*' marker
        token = strtok(NULL, "\r\n");
        if (token == NULL) return 0;

        // Get the command (SET/GET/DEL or other commands)
        *command = strtok(NULL, "\r\n");
        if (*command == NULL) return 0;

        // Get the first argument (key) if it exists
        char* len_key = strtok(NULL, "\r\n");
        *arg1 = strtok(NULL, "\r\n");

        char* len_value = strtok(NULL, "\r\n");
        if(len_value == NULL) return 1;
        // Get the second argument (value for SET) if it exists
        *arg2 = strtok(NULL, "\r\n");

        // If there are arguments and the command is valid, return success
        return 1;
    }

    // Return failure if the message does not start with '*'
    return 0;
}

// Build the RESP2 response
char* build_resp(const char* message) {
    size_t len = strlen(message);
    char* response = (char*)malloc(len + 4); // Adding 4 for the RESP header (`+` and `\r\n`)
    sprintf(response, "+%s\r\n", message);
    return response;
}

char* build_resp_get(const char* message) {
    size_t len = strlen(message);
    char* response = (char*)malloc(len + 6); // Adding 7 for the RESP header (`$len\r\n` and `\r\n`)
    sprintf(response, "$%zu\r\n%s\r\n", len, message);
    return response;
}

char* build_error(const char* message) {
    size_t len = strlen(message);
    char* response = (char*)malloc(len + 8); // Adding 9 for the RESP header (`-ERR ` and `\r\n`)
    sprintf(response, "-ERR %s\r\n", message);
    return response;
}

// Handle SET command
void handle_set(int sockfd, const char* key, const char* value) {
    SET(key, value);
    char* response = build_resp("OK");
    send_message(sockfd, response, strlen(response));  // Use send instead of write
    free(response);
}

// Handle GET command
void handle_get(int sockfd, const char* key) {
    const char* result = GET(key);
    if (result && strcmp(result, "tombstone") != 0) {
        char* response = build_resp_get(result);
        send_message(sockfd, response, strlen(response));  // Use send instead of write
        free(response);
    } else {
        char* response = build_error("Key not found");
        send_message(sockfd, response, strlen(response));  // Use send instead of write
        free(response);
    }
}

// Handle DEL command
void handle_del(int sockfd, const char* key) {
    const char* result = GET(key);
    if (result && strcmp(result, "tombstone") != 0) {
        DEL(key);
        char* response = build_resp("OK");
        send_message(sockfd, response, strlen(response));  // Use send instead of write
        free(response);
    } else {
        char* response = build_error("Key not found");
        send_message(sockfd, response, strlen(response));  // Use send instead of write
        free(response);
    }
}


int loop(void *arg)
{
    int nevents = ff_kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);
    int i;

    if (nevents < 0) {
        printf("ff_kevent failed:%d, %s\n", errno, strerror(errno));
        return -1;
    }

    for (i = 0; i < nevents; ++i) {
        struct kevent event = events[i];
        int clientfd = (int)event.ident;

        if (event.flags & EV_EOF) {
            ff_close(clientfd);
        } else if (clientfd == server_fd) {
            int available = (int)event.data;
            do {
                int nclientfd = ff_accept(clientfd, NULL, NULL);
                if (nclientfd < 0) {
                    printf("ff_accept failed:%d, %s\n", errno, strerror(errno));
                    break;
                }

                EV_SET(&kevSet, nclientfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                if (ff_kevent(kq, &kevSet, 1, NULL, 0, NULL) < 0) {
                    printf("ff_kevent error:%d, %s\n", errno, strerror(errno));
                    return -1;
                }

                available--;
            } while (available);
        } else if (event.filter == EVFILT_READ) {
            char buffer[1024]; // Buffer size large enough for 1KB of data
            ssize_t readlen = ff_read(clientfd, buffer, sizeof(buffer));

           
            if (readlen < 0) {
                printf("ff_read failed:%d, %s\n", errno, strerror(errno));
                ff_close(clientfd);
                continue;
            }

            buffer[readlen] = '\0'; // Null terminate the message
            char *command = NULL, *arg1 = NULL, *arg2 = NULL;

            if (parse_resp(buffer, &command, &arg1, &arg2)) {
                if (strcmp(command, "SET") == 0 && arg1 && arg2) {
                    handle_set(clientfd, arg1, arg2);
                } else if (strcmp(command, "GET") == 0 && arg1) {
                    handle_get(clientfd, arg1);
                } else if (strcmp(command, "DEL") == 0 && arg1) {
                    handle_del(clientfd, arg1);
                } else {
                    char* response = build_error("Invalid command or arguments");
                    send_message(clientfd, response, strlen(response));  // Use send instead of write
                    free(response);
                }
            }

            // Send back the length of the data received
            // char reply[64]; // Buffer to store the response (length of data)
            // int reply_len = snprintf(reply, sizeof(reply), "Received %ld bytes\n", readlen);
           
            // ssize_t writelen = ff_write(clientfd, reply, reply_len);
            // if (writelen < 0) {
            //     printf("ff_write failed:%d, %s\n", errno, strerror(errno));
            //     ff_close(clientfd);
            // }
        } else {
            printf("unknown event: %8.8X\n", event.flags);
        }
    }

    // Print server CPU and memory usage every 10 seconds (example)
    // print_server_usage();

    return 0;
}


// Start server
void start_server(int argc,char* argv[]) {

    ff_init(argc,argv);
    kq = ff_kqueue();
    if (kq < 0) {
        printf("ff_kqueue failed, errno:%d, %s\n", errno, strerror(errno));
        exit(1);
    }

    server_fd = ff_socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("ff_socket failed, sockfd:%d, errno:%d, %s\n", server_fd, errno, strerror(errno));
        exit(1);
    }
    int on = 1;
    ff_ioctl(server_fd, FIONBIO, &on);

    struct sockaddr_in my_addr;
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(80);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (ff_bind(server_fd, (struct linux_sockaddr*)&my_addr, sizeof(my_addr)) < 0) {
        perror("ERROR on binding");
        exit(EXIT_FAILURE);
    }

    int ret = ff_listen(server_fd, MAX_EVENTS);
    if (ret < 0) {
        printf("ff_listen failed, sockfd:%d, errno:%d, %s\n", server_fd, errno, strerror(errno));
        exit(1);
    }

    EV_SET(&kevSet, server_fd, EVFILT_READ, EV_ADD, 0, MAX_EVENTS, NULL);
    ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);
    ff_run(loop, NULL);

}

int main(int argc, char* argv[]) {
    start_compaction();
    start_server(argc,argv);

    return 0;
}
