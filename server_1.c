#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
 
#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
 
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_read] = '\0'; // Null terminate the buffer
        printf("Received message from client: %s\n", buffer);
        // Process the request from the client and generate a response
        // Here you can add your application logic
        // In this example, we simply echo the message back to the client
        send(client_socket, buffer, bytes_read, 0);
    }
    if (bytes_read == 0) {
        printf("Client disconnected.\n");
    } else {
        perror("Error receiving data from client");
    }
    close(client_socket);
}
 
int main() {
    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_socket;
    socklen_t client_addr_size = sizeof(client_addr);
 
    // Create a TCP socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(1);
    }
 
    // Set up the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
 
    // Bind the server address to the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        exit(1);
    }
 
    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Error listening for connections");
        exit(1);
    }
 
    printf("Server is running and listening for connections...\n");
 
    // Accept incoming connections and handle them in separate threads
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket == -1) {
            perror("Error accepting connection");
            exit(1);
        }
        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
        handle_client(client_socket);
    }
 
    return 0;
}