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
#include <dlfcn.h>
#include <errno.h>

#define MAXLINE 1024 /**< Maximum buffer size for reading data */
#define PORT 6379 /**< Default port for the server */
#define MAX_CLIENTS 10000 /**< Maximum number of clients the server can handle */
fd_set master_fds; /**< Master file descriptor set for select() */

// External database functions from a shared library
extern void start_compaction();
extern void SET(char *, char *);
extern void DEL(char *);
extern const char *GET(char *);

/**
 * @brief Sends a message to a socket.
 *
 * This function ensures that the entire message is sent, even if the message is large and needs 
 * to be sent in chunks.
 *
 * @param sockfd Socket file descriptor.
 * @param message Pointer to the message to be sent.
 * @param length Length of the message.
 * @return The total number of bytes sent, or -1 on error.
 */
ssize_t send_message(int sockfd, const char *message, size_t length)
{
    ssize_t total_sent = 0; /**< Total bytes sent so far */
    ssize_t bytes_sent;

    while (total_sent < length)
    {
        bytes_sent = send(sockfd, message + total_sent, length - total_sent, 0);
        if (bytes_sent < 0)
        {
            perror("Send failed");
            return -1;
        }
        total_sent += bytes_sent;
    }

    return total_sent;
}

/**
 * @brief Receives a message from a socket.
 *
 * This function reads data from a socket, ensuring that all data is received and handled properly 
 * (including closing the connection when the peer disconnects).
 *
 * @param sockfd Socket file descriptor.
 * @param buffer Buffer to store the received message.
 * @param buffer_size Size of the buffer.
 * @return The number of bytes received, or -1 on error.
 */
ssize_t receive_message(int sockfd, char *buffer, size_t buffer_size)
{
    ssize_t total_received = 0; /**< Total bytes received so far */
    ssize_t bytes_received;

    while (total_received < buffer_size - 1)
    {
        bytes_received = recv(sockfd, buffer + total_received, buffer_size - 1 - total_received, 0);
        if (bytes_received < 0)
        {
            break;
        }
        if (bytes_received == 0)
        {
            // Connection closed by the peer
            FD_CLR(sockfd, &master_fds);
            break;
        }
        total_received += bytes_received;
    }

    buffer[total_received] = '\0'; // Null-terminate the buffer
    return total_received;
}

/**
 * @brief Parses a RESP2 protocol message.
 *
 * This function parses a RESP2 (REdis Serialization Protocol) message and extracts the command 
 * and its arguments. RESP2 is the protocol used by Redis servers for communication.
 *
 * @param message The message to parse.
 * @param command Pointer to store the extracted command.
 * @param arg1 Pointer to store the first argument (key).
 * @param arg2 Pointer to store the second argument (value, for SET).
 * @return 1 if the message was successfully parsed, 0 otherwise.
 */
int parse_resp(char *message, char **command, char **arg1, char **arg2)
{
    if (message[0] == '*')
    {
        // Split the message by "\r\n"
        char *token = strtok(message, "\r\n");

        if (token == NULL)
            return 0;

        // Skip the '*' marker
        token = strtok(NULL, "\r\n");
        if (token == NULL)
            return 0;

        // Get the command (SET/GET/DEL or other commands)
        *command = strtok(NULL, "\r\n");
        if (*command == NULL)
            return 0;

        // Get the first argument (key) if it exists
        *arg1 = strtok(NULL, "\r\n");

        // Get the second argument (value for SET) if it exists
        *arg2 = strtok(NULL, "\r\n");

        return 1;
    }

    return 0; // If the message does not start with '*'
}

/**
 * @brief Builds a RESP2 response for a simple message.
 *
 * @param message The message to be sent in the response.
 * @return A newly allocated string containing the formatted RESP2 response.
 */
char *build_resp(const char *message)
{
    size_t len = strlen(message);
    char *response = (char *)malloc(len + 4); // Adding 4 for the RESP header (`+` and `\r\n`)
    sprintf(response, "+%s\r\n", message);
    return response;
}

/**
 * @brief Builds a RESP2 response for a GET command.
 *
 * @param message The message to be sent in the response.
 * @return A newly allocated string containing the formatted RESP2 response for GET.
 */
char *build_resp_get(const char *message)
{
    size_t len = strlen(message);
    char *response = (char *)malloc(len + 6); // Adding 7 for the RESP header (`$len\r\n` and `\r\n`)
    sprintf(response, "$%zu\r\n%s\r\n", len, message);
    return response;
}

/**
 * @brief Builds a RESP2 error response.
 *
 * @param message The error message to be sent in the response.
 * @return A newly allocated string containing the formatted error response.
 */
char *build_error(const char *message)
{
    size_t len = strlen(message);
    char *response = (char *)malloc(len + 8); // Adding 9 for the RESP header (`-ERR ` and `\r\n`)
    sprintf(response, "-ERR %s\r\n", message);
    return response;
}

/**
 * @brief Handles the SET command from the client.
 *
 * This function processes a SET command, which stores a key-value pair in the database.
 *
 * @param sockfd The client socket.
 * @param key The key to store.
 * @param value The value to store.
 */
void handle_set(int sockfd, const char *key, const char *value)
{
    SET(key, value);
    char *response = build_resp("OK");
    send_message(sockfd, response, strlen(response)); // Use send instead of write
    free(response);
}

/**
 * @brief Handles the GET command from the client.
 *
 * This function processes a GET command, which retrieves the value associated with a key.
 *
 * @param sockfd The client socket.
 * @param key The key to retrieve.
 */
void handle_get(int sockfd, const char *key)
{
    const char *result = GET(key);

    if (result && strcmp(result, "tombstone") != 0)
    {
        char *response = build_resp_get(result);
        send_message(sockfd, response, strlen(response)); // Use send instead of write
        free(response);
    }
    else
    {
        char *response = build_error("Key not found");
        send_message(sockfd, response, strlen(response)); // Use send instead of write
        free(response);
    }
}

/**
 * @brief Handles the DEL command from the client.
 *
 * This function processes a DEL command, which deletes a key from the database.
 *
 * @param sockfd The client socket.
 * @param key The key to delete.
 */
void handle_del(int sockfd, const char *key)
{
    const char *result = GET(key);
    if (result && strcmp(result, "tombstone") != 0)
    {
        DEL(key);
        char *response = build_resp("OK");
        send_message(sockfd, response, strlen(response)); // Use send instead of write
        free(response);
    }
    else
    {
        char *response = build_error("Key not found");
        send_message(sockfd, response, strlen(response)); // Use send instead of write
        free(response);
    }
}

/**
 * @brief Sets a file descriptor to non-blocking mode.
 *
 * This function modifies the socket file descriptor to operate in non-blocking mode,
 * which is necessary for handling multiple connections asynchronously.
 *
 * @param fd The file descriptor to modify.
 */
void set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
    }
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * @brief Handles the client communication.
 *
 * This function processes the incoming request from the client, parses the command, 
 * and calls the appropriate handler for SET, GET, or DEL commands.
 *
 * @param client_fd The client socket file descriptor.
 */
void handle_client(int client_fd)
{
    char buffer[MAXLINE];
    ssize_t n;

    n = receive_message(client_fd, buffer, MAXLINE); // Use recv instead of read
    if (n == 0)
    {
        close(client_fd);
        return;
    }

    buffer[n] = '\0'; // Null terminate the message
    char *command = NULL, *arg1 = NULL, *arg2 = NULL;

    if (parse_resp(buffer, &command, &arg1, &arg2))
    {
        if (strcmp(command, "SET") == 0 && arg1 && arg2)
        {
            handle_set(client_fd, arg1, arg2);
        }
        else if (strcmp(command, "GET") == 0 && arg1)
        {
            handle_get(client_fd, arg1);
        }
        else if (strcmp(command, "DEL") == 0 && arg1)
        {
            handle_del(client_fd, arg1);
        }
        else
        {
            char *response = build_error("Invalid command or arguments");
            send_message(client_fd, response, strlen(response)); // Use send instead of write
            free(response);
        }
    }
}

/**
 * @brief Starts the server to accept client connections and process requests.
 *
 * The server listens for incoming client connections and processes commands like SET, GET, and DEL.
 * It supports multiple clients using non-blocking sockets and the select() function for multiplexing.
 *
 * @param host The server host address.
 * @param port The server port.
 */
void start_server(const char *host, int port)
{
    int server_fd;
    int client_fds[MAX_CLIENTS + 1];
    for (int i = 0; i < MAX_CLIENTS + 1; i++)
    {
        client_fds[i] = 0;
    }
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("ERROR on binding");
        exit(EXIT_FAILURE);
    }

    listen(server_fd, MAX_CLIENTS);
    printf("Server running on %s:%d\n", host, port);

    set_non_blocking(server_fd);

    fd_set read_fds;
    int max_fd = server_fd;
    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);

    while (1)
    {
        read_fds = master_fds;

        // Wait for an activity on the sockets
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0)
        {
            printf("Error message: %s\n", strerror(errno));
            perror("select()");
            exit(EXIT_FAILURE);
        }

        // If something happened on the server socket, it's an incoming connection
        if (FD_ISSET(server_fd, &read_fds))
        {
            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd < 0)
            {
                perror("accept()");
                continue;
            }

            set_non_blocking(client_fd);
            FD_SET(client_fd, &master_fds);
            if (client_fd > max_fd)
            {
                max_fd = client_fd;
            }
        }

        // Handle data for all clients
        for (int i = 3; i <= max_fd; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (i != server_fd)
                {
                    handle_client(i);
                }
            }
        }
    }

    close(server_fd);
}

/**
 * @brief Main function to start the server.
 *
 * This function initializes the server by accepting arguments for the port number 
 * and then starts the server to accept client connections.
 *
 * @param argc The argument count.
 * @param argv The argument values.
 * @return 0 on successful execution.
 */
int main(int argc, char *argv[])
{
    const char *host = "127.0.0.1";
    int port = PORT;

    if (argc > 1)
    {
        port = atoi(argv[1]);
    }

    // Start compaction or other database tasks
    start_compaction();
    start_server(host, port);

    return 0;
}

