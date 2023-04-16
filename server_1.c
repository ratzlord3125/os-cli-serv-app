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

void* handle_request(void* arg) {
    int client_id = *((int*)arg);
    printf("Handling request from client %d\n", client_id);
    
    // Process request
    // In this example, we simply add 1 to the request value and set it as response
    pthread_mutex_lock(&mutex);
    shared_mem->response = shared_mem->request + 1;
    pthread_mutex_unlock(&mutex);
    
    return NULL;
}

int main() {
    const int NUM_CLIENTS = 10;  // Number of clients
    const int SHM_SIZE = sizeof(SharedMemory);  // Size of shared memory segment
    const char* SHM_NAME = "/my_shared_memory";  // Name of shared memory segment

    // Create shared memory segment
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error creating shared memory");
        exit(1);
    }
    ftruncate(shm_fd, SHM_SIZE);
    shared_mem = (SharedMemory*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("Error mapping shared memory");
        exit(1);
    }

    // Initialize shared memory
    shared_mem->request = 0;
    shared_mem->response = 0;
    
    // Initialize mutex
    pthread_mutex_init(&mutex, NULL);

    // Create threads for clients
    pthread_t threads[NUM_CLIENTS];
    int client_ids[NUM_CLIENTS];
    for (int i = 0; i < NUM_CLIENTS; i++) {
        client_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, handle_request, &client_ids[i]);
    }

    // Wait for threads to finish
    for (int i = 0; i < NUM_CLIENTS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Clean up
    pthread_mutex_destroy(&mutex);
    munmap(shared_mem, SHM_SIZE);
    shm_unlink(SHM_NAME);
    
    return 0;
}
