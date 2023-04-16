#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_SIZE 1024

typedef struct {
    int client_id;
    char message[256];
} Message;

int main() {
    int shmid;
    key_t key = ftok("shared_memory", 65); // Generate a unique key for shared memory

    // Create shared memory segment
    shmid = shmget(key, SHM_SIZE, 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    printf("Server: Shared memory segment created with shmid = %d\n", shmid);

    // Attach shared memory segment to server's address space
    Message* shared_memory = (Message*)shmat(shmid, NULL, 0);
    if (shared_memory == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Server: Shared memory segment attached to server's address space\n");

    while (1) {
        // Wait for client to write a request
        while (shared_memory->client_id == 0) {
            // Do nothing
        }

        // Process client's request
        printf("Server: Received request from Client %d: %s\n", shared_memory->client_id, shared_memory->message);

        // Reply to client
        char reply[256];
        snprintf(reply, sizeof(reply), "Server: Reply to Client %d", shared_memory->client_id);
        strncpy(shared_memory->message, reply, sizeof(shared_memory->message));
        shared_memory->client_id = 0; // Reset client ID

        // Exit server if "exit" request received from client
        if (strcmp(reply, "Server: Reply to Client exit") == 0) {
            break;
        }
    }

    // Detach shared memory segment from server's address space
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(1);
    }

    // Remove shared memory segment
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    printf("Server: Shared memory segment removed\n");

    return 0;
}
