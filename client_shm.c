#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_SIZE 1024 // defining size

typedef struct {
    int client_id;
    char message[256];
} Message;

int main() {
    int shmid;
    key_t key = ftok("shared_memory", 65); // Use the same key as server to access shared memory

    // Get shared memory segment created by server
    shmid = shmget(key, SHM_SIZE, 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    printf("Client: Shared memory segment accessed with shmid = %d\n", shmid);

    // Attach shared memory segment to client's address space
    Message* shared_memory = (Message*)shmat(shmid, NULL, 0);
    if (shared_memory == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Client: Shared memory segment attached to client's address space\n");

    // Send request to server
    int client_id = getpid(); // Use client process ID as the client ID
    snprintf(shared_memory->message, sizeof(shared_memory->message), "Client %d: Request to server", client_id);
    shared_memory->client_id = client_id;

    // Wait for server's reply
    while (shared_memory->client_id != 0) {
        // Do nothing
    }

    // Print server's reply
    printf("Client: Received reply from server: %s\n", shared_memory->message);

    // Detach shared memory segment from client's address space
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(1);
    }

    return 0;
}
