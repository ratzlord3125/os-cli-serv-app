#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>

// Shared memory structure for request and response
typedef struct {
    int request;
    int response;
} SharedMemory;

SharedMemory *shared_mem;  // Pointer to shared memory
pthread_mutex_t mutex;     // Mutex for synchronization

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
            printf("Client %d received response: %d\n", client_id, response);
            break;
        }
    }
}

int main() {
    const int NUM_REQUESTS = 5;  // Number of requests per client
    const int SHM_SIZE = sizeof(SharedMemory);  // Size of shared memory segment
    const char* SHM_NAME = "/my_shared_memory";  // Name of shared memory segment

    // Open shared memory segment
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error opening shared memory");
        exit(1);
    }
    shared_mem = (SharedMemory*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("Error mapping shared memory");
        exit(1);
    }

    // Initialize mutex
    pthread_mutex_init(&mutex, NULL);

    // Send requests to server
    for (int i = 0; i < NUM_REQUESTS; i++) {
        int request = i + 1;
        send_request(getpid(), request);
        sleep(1);  // Sleep for 1 second between requests
    }

    // Clean up
    pthread_mutex_destroy(&mutex);
    munmap(shared_mem, SHM_SIZE);
    
    return 0;
}
