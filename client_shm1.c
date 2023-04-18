#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>

#define SHM_SIZE 4
#define SHM_NAME "/shm_obj"

int generate_random_number() {
    return rand() % 100 + 1;
}

void* send_number(void* arg) {
    int client_id = *((int*)arg);

    // open shared memory object
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // map shared memory object
    void* ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // generate random number and write to shared memory
    int num = generate_random_number();
    printf("Client %d: Sending number %d to server\n", client_id, num);
    *(int*)ptr = num;

    // wait for server to respond with even/odd message
    while (*(int*)ptr == num) {
        sleep(1);
    }

    // print server's even/odd message
    if (*(int*)ptr == 1) {
        printf("Client %d: Server says number %d is odd\n", client_id, num);
    } else {
        printf("Client %d: Server says number %d is even\n", client_id, num);
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

    pthread_exit(NULL);
}

int main() {
    pthread_t thread;
    int thread_arg = 1;
    int rc;

    // create thread to send number to server
    rc = pthread_create(&thread, NULL, send_number, (void*)&thread_arg);
    if (rc) {
        printf("Error: return code from pthread_create() is %d\n", rc);
        exit(EXIT_FAILURE);
    }

    // wait for thread to finish
    rc = pthread_join(thread, NULL);
    if (rc) {
        printf("Error: return code from pthread_join() is %d\n", rc);
        exit(EXIT_FAILURE);
    }

    return 0;
}
