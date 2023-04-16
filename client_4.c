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
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} SharedMemory;

SharedMemory *shared_mem;

int main() {
    const char *SHM_NAME = "/my_shared_memory";
    const int MAX_CLIENTS = 10;

    // Open shared memory segment
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error opening shared memory");
        exit(1);
    }

    shared_mem = (SharedMemory *)mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("Error mapping shared memory");
        exit(1);
    }

    printf("Enter a number (-1 to exit): ");
    int num;
    while (1) {
        scanf("%d", &num);

        pthread_mutex_lock(&shared_mem->mutex);
        shared_mem->request = num;
        pthread_cond_signal(&shared_mem->cond);
        pthread_mutex_unlock(&shared_mem->mutex);

        if (num == -1) {
            // Send termination signal to server
            break;
        }

        pthread_mutex_lock(&shared_mem->mutex);
        while (shared_mem->response == 0) {
            pthread_cond_wait(&shared_mem->cond, &shared_mem->mutex);
        }
        int response = shared_mem->response;
        shared_mem->response = 0;
        pthread_mutex_unlock(&shared_mem->mutex);

        printf("Number %d is %s\n", num, (response == 1) ? "even" : "odd");
    }

    // Clean up
    munmap(shared_mem, sizeof(SharedMemory));
    close(shm_fd);

    return 0;
}
