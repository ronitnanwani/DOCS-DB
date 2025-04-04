#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 12345
#define SERVER_IP "192.168.1.4" // IP address of the server (NS3)
#define BUFFER_SIZE 1024

// Function to generate random data of a given size (between 512 bytes to 1024 bytes)
void generate_random_data(char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = rand() % 256;  // Random byte
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_sent, bytes_received;
    size_t data_size;
    time_t start_time, end_time;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server at %s:%d\n", SERVER_IP, PORT);

    // Send 10,000, 100,000, and 1,000,000 packets
    size_t num_packets_list[] = {10000, 100000, 1000000};

    for (int i = 0; i < 3; i++) {
        size_t num_packets = num_packets_list[i];
        double total_time = 0;
        size_t total_bytes_sent = 0;

        printf("Sending %zu packets...\n", num_packets);

        for (size_t j = 0; j < num_packets; j++) {
            data_size = rand() % (1024 - 512 + 1) + 512;  // Random size between 512 and 1024 bytes
            generate_random_data(buffer, data_size);
            printf("Packet %zu\n", j + 1);

            // Measure start time
            start_time = time(NULL);

            bytes_sent = send(sock, buffer, data_size, 0);
            if (bytes_sent == -1) {
                perror("Send failed");
                close(sock);
                exit(EXIT_FAILURE);
            }

            bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_received == -1) {
                perror("Receive failed");
                close(sock);
                exit(EXIT_FAILURE);
            }

            // Measure end time
            end_time = time(NULL);

            // Update total time and total bytes sent
            total_time += difftime(end_time, start_time);
            total_bytes_sent += bytes_sent;
        }

        // Calculate latency and bandwidth
        double latency = total_time / num_packets;
        double bandwidth = (total_bytes_sent / 1024.0) / total_time;  // in KB/s

        printf("Latency: %.6f seconds, Bandwidth: %.2f KB/s\n", latency, bandwidth);
    }

    close(sock);
    return 0;
}
