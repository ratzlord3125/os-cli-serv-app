#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CLIENTS 5
#define SHM_SIZE  sizeof(int)
#define SHM_NAME "/myshm0"

void* client_handler(void* arg)
{
    int client_num = *(int*)arg;
    int shm_fd;
    void* ptr;
    int num;

    // create shared memory object
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // map shared memory object to process address space
    ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // wait for client to write number to shared memory
    while (*(int*)ptr == 0) {
        usleep(1000); // sleep for 1 millisecond
    }

    // read number from shared memory
    num = *(int*)ptr;

    // print message to server
    printf("Client %d sent number: %d\n", client_num, num);

    // calculate result
    int result = num % 2 == 0 ? 0 : 1;

    // write result to shared memory
    *(int*)ptr = result;

    // unmap shared memory object
    if (munmap(ptr, SHM_SIZE) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    // close shared memory object
    if (close(shm_fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    pthread_exit(NULL);
}

int main() {
    printf("Server started!\n");
    int shm_fd;
    int* ptr;

    // create shared memory object
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);

    // map shared memory object
    ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // initialize client count and mutex
    *ptr = 0;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    // create server thread
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, client_handler, (void*)ptr);

    // wait for server thread to exit
    pthread_join(server_thread, NULL);

    // cleanup
    shm_unlink(SHM_NAME);
    pthread_mutex_destroy(&mutex);

    return 0;
}
