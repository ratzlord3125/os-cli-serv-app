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

void* process_requests(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        int request = shared_mem->request;
        pthread_mutex_unlock(&mutex);
        
        if (request != 0) {
            // Check if number is even or odd
            int response = (request % 2 == 0) ? 1 : 0;
            
            pthread_mutex_lock(&mutex);
            shared_mem->response = response;
            pthread_mutex_unlock(&mutex);
            
            printf("Server processed request: %d and sent response: %s\n", request, (response == 1) ? "EVEN" : "ODD");
        }
    }
    return NULL;
}

int main() {
    const int SHM_SIZE = sizeof(SharedMemory);
    const char* SHM_NAME = "/my_shared_memory5";

    // Open shared memory segment
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error opening shared memory");
        exit(1);
    }
    ftruncate(shm_fd, SHM_SIZE);
    shared_mem = (SharedMemory*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("Error mapping shared memory");
        exit(1);
    }

    // Initialize mutex
    pthread_mutex_init(&mutex, NULL);

    // Create a thread to process requests
    pthread_t tid;
    pthread_create(&tid, NULL, process_requests, NULL);

    // Wait for thread to finish
    pthread_join(tid, NULL);

    // Clean up
    pthread_mutex_destroy(&mutex);
    munmap(shared_mem, SHM_SIZE);
    shm_unlink(SHM_NAME);
    
    return 0;
}
