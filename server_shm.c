#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#define SHM_SIZE  sizeof(int)

void* client_handler(void* arg);

int main()
{
    int shm_fd;
    void* ptr;
    pthread_t thread;
    int num_clients = 0;

    // create shared memory object
    shm_fd = shm_open("/shm_obj", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // resize shared memory object
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    // map shared memory object to process address space
    ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // initialize shared memory value to 0
    *(int*)ptr = 0;

    while (1) {
        // check if maximum number of threads reached
        if (num_clients >= 5) {
            sleep(1);
            continue;
        }

        // wait for a new client to connect
        printf("Waiting for a new client to connect...\n");

        // create new thread for client
        if (pthread_create(&thread, NULL, client_handler, ptr) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        // increment number of clients
        num_clients++;

        // detach thread to prevent memory leak
        if (pthread_detach(thread) != 0) {
            perror("pthread_detach");
            exit(EXIT_FAILURE);
        }

        // print message to server
        printf("New client connected\n");
    }

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

    // unlink shared memory object
    if (shm_unlink("/shm_obj") == -1) {
        perror("shm_unlink");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void* client_handler(void* arg)
{
    int* shm_value = (int*)arg;
    int num = rand() % 100 + 1;

    // write random number to shared memory
    *shm_value = num;

    // print message to client
    printf("Number sent to server: %d\n", num);

    // wait for server to process number
    sleep(1);

    // read result from shared memory
    int result = *shm_value;

    // print result to client
    printf("Server response: %d is %s\n", num, result % 2 == 0 ? "even" : "odd");

    return NULL;
}
