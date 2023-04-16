#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

typedef struct {
    int request;
    int response;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} SharedMemory;

SharedMemory *shared_mem;

void *process_requests(void *arg) {
    while (1) {
        pthread_mutex_lock(&shared_mem->mutex);

        while (shared_mem->request == 0) {
            pthread_cond_wait(&shared_mem->cond, &shared_mem->mutex);
        }

        int request = shared_mem->request;
        if (request == -1) {
            // Termination signal received
            pthread_mutex_unlock(&shared_mem->mutex);
            break;
        }

        // Process request
        int response = (request % 2 == 0) ? 1 : 0;
        shared_mem->response = response;
        pthread_cond_signal(&shared_mem->cond); // Signal response
        shared_mem->request = 0;

        pthread_mutex_unlock(&shared_mem->mutex);
    }

    return NULL;
}

int main() {
    const char *SHM_NAME = "/my_shared_memory";
    const int MAX_CLIENTS = 10;

    // Create shared memory segment
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error creating shared memory");
        exit(1);
    }

    ftruncate(shm_fd, sizeof(SharedMemory));

    shared_mem = (SharedMemory *)mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("Error mapping shared memory");
        exit(1);
    }

    // Initialize shared memory
    shared_mem->request = 0;
    shared_mem->response = 0;
    pthread_mutex_init(&shared_mem->mutex, NULL);
    pthread_cond_init(&shared_mem->cond, NULL);

    pthread_t tid;
    pthread_create(&tid, NULL, process_requests, NULL);

    // Wait for termination signal
    printf("Server is running...\n");
    printf("Press Ctrl+C to terminate\n");
    while (1) {
        sleep(1);
    }

    // Clean up
    munmap(shared_mem, sizeof(SharedMemory));
    close(shm_fd);
    shm_unlink(SHM_NAME);
    pthread_mutex_destroy(&shared_mem->mutex);
    pthread_cond_destroy(&shared_mem->cond);

    return 0;
}
