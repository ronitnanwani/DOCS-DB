#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/select.h>
#include <dlfcn.h>
#include <errno.h>

#define BUFFER_SIZE 1024

/**
 * @brief Builds the SET command in RESP2 format.
 * 
 * The RESP2 format for the SET command is as follows:
 * *3\r\n$3\r\nSET\r\n$<key_len>\r\n<key>\r\n$<value_len>\r\n<value>\r\n
 * 
 * @param key The key to be set in the Redis server.
 * @param value The value to be associated with the key.
 * @return A dynamically allocated string representing the formatted SET command.
 */
char* build_set_command(const char* key, const char* value) {
    int key_len = strlen(key);
    int value_len = strlen(value);
    int command_len = snprintf(NULL, 0, "*3\r\n$3\r\nSET\r\n$%d\r\n%s\r\n$%d\r\n%s\r\n", key_len, key, value_len, value);
    char* command = (char*)malloc(command_len + 1);
    snprintf(command, command_len + 1, "*3\r\n$3\r\nSET\r\n$%d\r\n%s\r\n$%d\r\n%s\r\n", key_len, key, value_len, value);
    return command;
}

/**
 * @brief Builds the GET command in RESP2 format.
 * 
 * The RESP2 format for the GET command is as follows:
 * *2\r\n$3\r\nGET\r\n$<key_len>\r\n<key>\r\n
 * 
 * @param key The key to retrieve from the Redis server.
 * @return A dynamically allocated string representing the formatted GET command.
 */
char* build_get_command(const char* key) {
    int key_len = strlen(key);
    int command_len = snprintf(NULL, 0, "*2\r\n$3\r\nGET\r\n$%d\r\n%s\r\n", key_len, key);
    char* command = (char*)malloc(command_len + 1);
    snprintf(command, command_len + 1, "*2\r\n$3\r\nGET\r\n$%d\r\n%s\r\n", key_len, key);
    return command;
}

/**
 * @brief Builds the DEL command in RESP2 format.
 * 
 * The RESP2 format for the DEL command is as follows:
 * *2\r\n$3\r\nDEL\r\n$<key_len>\r\n<key>\r\n
 * 
 * @param key The key to delete from the Redis server.
 * @return A dynamically allocated string representing the formatted DEL command.
 */
char* build_del_command(const char* key) {
    int key_len = strlen(key);
    int command_len = snprintf(NULL, 0, "*2\r\n$3\r\nDEL\r\n$%d\r\n%s\r\n", key_len, key);
    char* command = (char*)malloc(command_len + 1);
    snprintf(command, command_len + 1, "*2\r\n$3\r\nDEL\r\n$%d\r\n%s\r\n", key_len, key);
    return command;
}

/**
 * @brief Parses the response from the Redis server and displays it.
 * 
 * This function will handle the different response types in Redis:
 * - Simple responses (e.g., "+OK").
 * - Error responses (e.g., "-Error").
 * - Bulk responses (e.g., "$<len>\r\n<value>\r\n").
 * - Nil responses (e.g., "$-1\r\n" for non-existent key).
 * 
 * @param response The response from the Redis server in RESP2 format.
 */
void parse_response(char* response) {
    if(response == NULL){
        printf("Invalid response");
        return;
    }
    if (response[0] == '+') {
        printf("%s\n", response + 1); // Simple response, just remove the '+'
    } else if (response[0] == '-') {
        printf("Error: %s\n", response + 1); // Error response
    } else if (response[0] == '$') {
        if (strncmp(response + 1, "-1", 2) == 0) {
            printf("nil\n"); // Null response
        } else {
            printf("%s\n", response + 1); // Data response
        }
    } else {
        printf("Invalid response\n");
    }
}

/**
 * @brief Sets the given file descriptor to non-blocking mode.
 * 
 * This is useful for handling non-blocking sockets and using the select() function to manage multiple file descriptors.
 * 
 * @param fd The file descriptor to set to non-blocking mode.
 */
void set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
    }
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * @brief Sends a message to the specified socket.
 * 
 * This function ensures that the entire message is sent by repeatedly calling the send() function
 * until all data is transmitted.
 * 
 * @param sockfd The socket file descriptor to send the message to.
 * @param message The message to send.
 * @param length The length of the message to send.
 * @return The total number of bytes sent on success, or -1 on failure.
 */
ssize_t send_message(int sockfd, const char *message, size_t length) {
    ssize_t total_sent = 0; // Total bytes sent so far
    ssize_t bytes_sent;

    while (total_sent < length) {
        bytes_sent = send(sockfd, message + total_sent, length - total_sent, 0);
        if (bytes_sent < 0) {
            perror("Send failed");
            return -1;
        }
        total_sent += bytes_sent;
    }

    return total_sent;
}

/**
 * @brief Receives a message from the specified socket.
 * 
 * This function uses the recv() function to receive data from the socket in a non-blocking manner.
 * It will keep reading data until the buffer is full or the connection is closed.
 * 
 * @param sockfd The socket file descriptor to receive data from.
 * @param buffer The buffer to store the received data.
 * @param buffer_size The size of the buffer.
 * @return The number of bytes received on success, or -1 on failure.
 */
ssize_t receive_message(int sockfd, char *buffer, size_t buffer_size) {
    ssize_t total_received = 0; // Total bytes received so far
    ssize_t bytes_received;

    while (total_received < buffer_size - 1) {
        bytes_received = recv(sockfd, buffer + total_received, buffer_size - 1 - total_received, 0);
        if (bytes_received < 0) {
            break;
        }
        if (bytes_received == 0) {
            break; // Connection closed by the peer
        }
        total_received += bytes_received;
    }

    buffer[total_received] = '\0'; // Null-terminate the buffer
    return total_received;
}

/**
 * @brief Main entry point for the client application.
 * 
 * This program connects to a Redis server and allows the user to send commands (SET, GET, DEL)
 * interactively via the terminal. The program will use non-blocking sockets and the select() function
 * to handle responses asynchronously.
 * 
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @return 0 on success, or an error code on failure.
 */
int main(int argc, char *argv[]) {
    const char* host = "127.0.0.1";
    int port = 6379;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    printf("Connecting to server at %s:%d...\n", host, port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(host);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    set_non_blocking(sockfd);

    printf("Connected. Enter commands (SET <key> \"<value>\" / GET <key> / DEL <key>):\n");

    while (1) {
        char input[BUFFER_SIZE];
        printf("> ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        int input_length = strlen(input);
        input[input_length-1]='\0';
        input_length--;

        if (strcasecmp(input, "exit") == 0) {
            printf("Exiting client.\n");
            break;
        }

        char* command = NULL;
        if (strncmp(input, "SET", 3) == 0) {
            char key[BUFFER_SIZE], value[BUFFER_SIZE];
            if (sscanf(input, "SET %s \"%[^\"]\"", key, value) == 2) {
                // If parsing is successful, build the command
                command = build_set_command(key, value);
            } else {
                printf("Invalid input format\n");
            }
        } else if (strncmp(input, "GET", 3) == 0) {
            char key[BUFFER_SIZE];
            if (sscanf(input, "GET %s", key) == 1) {
                command = build_get_command(key);
            } else {
                printf("Invalid GET command format. Usage: GET <key>\n");
                continue;
            }
        } else if (strncmp(input, "DEL", 3) == 0) {
            char key[BUFFER_SIZE];
            if (sscanf(input, "DEL %s", key) == 1) {
                command = build_del_command(key);
            } else {
                printf("Invalid DEL command format. Usage: DEL <key>\n");
                continue;
            }
        } else {
            printf("Unknown command. Use 'SET <key> \"<value>\"' or 'GET <key>' or 'DEL <key>'\n");
            continue;
        }
        
        if (command != NULL) {

            send_message(sockfd,command,strlen(command));
            free(command);
            char response[BUFFER_SIZE];
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(sockfd,&read_fds);
            int activity = select(sockfd+1, &read_fds, NULL, NULL, NULL);
            
            if(FD_ISSET(sockfd,&read_fds)){
                int bytes_received = receive_message(sockfd,response,BUFFER_SIZE);
                parse_response(response);
            }
        }
    }

    close(sockfd);
    return 0;
}

