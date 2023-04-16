#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>

typedef struct {
    int request;
    int response;
} SharedMemory;

SharedMemory *shared_mem;
pthread_mutex_t mutex;

void send_request(int client_id, int request) {
    // Send request to server
    pthread_mutex_lock(&mutex);
    shared_mem->request = request;
    pthread_mutex_unlock(&mutex);
    printf("Client %d sent request: %d\n", client_id, request);

    // Wait for response from server
    while (1) {
        pthread_mutex_lock(&mutex);
        int response = shared_mem->response;
        pthread_mutex_unlock(&mutex);

        if (response != 0) {
            printf("Client %d received response: %s\n", client_id, (response == 1) ? "EVEN" : "ODD");

            // Notify server that response has been received
            pthread_mutex_lock(&mutex);
            shared_mem->response = 0;
            pthread_mutex_unlock(&mutex);

            break;
        }
    }
}

int main() {
    const int NUM_REQUESTS = 5;
    const int SHM_SIZE = sizeof(SharedMemory);
    const char *SHM_NAME = "/my_shared_memory7";

    // Open shared memory segment
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error opening shared memory");
        exit(1);
    }
    shared_mem = (SharedMemory *)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("Error mapping shared memory");
        exit(1);
    }

    // Initialize mutex
    pthread_mutex_init(&mutex, NULL);

    // Send requests to server
    // for (int i = 1; i <= NUM_REQUESTS; i++) {
    //     send_request(i, i);
    // }
    send_request(3,666);

    // Clean up
    pthread_mutex_destroy(&mutex);
    munmap(shared_mem, SHM_SIZE);

    return 0;
}
