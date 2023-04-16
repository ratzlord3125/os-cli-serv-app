#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
 
#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024
 
int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
 
    // Create a TCP socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Error creating socket");
        exit(1);
    }
 
    // Set up the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);
 
    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to server");
        exit(1);
    }
 
    printf("Connected to server...\n");
 
    // Send request to server
    printf("Enter a message to send to server: ");
    fgets(buffer, BUFFER_SIZE, stdin);
    int bytes_sent = send(client_socket, buffer, strlen(buffer), 0);
    if (bytes_sent == -1) {
        perror("Error sending data to server");
        exit(1);
    }
 
    // Receive response from server
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received == -1) {
        perror("Error receiving data from server");
        exit(1);
    }
 
    buffer[bytes_received] = '\0'; // Null terminate the buffer
    printf("Received message from server: %s\n", buffer);
 
    // Close the client socket
    close(client_socket);
 
    return 0;
}