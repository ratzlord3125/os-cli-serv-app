#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHM_SIZE  sizeof(int)

int main()
{
    int shm_fd;
    void* ptr;
    int num;

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

    // generate random number between 1 and 100
    num = rand() % 100 + 1;

    // write random number to shared memory
    *(int*)ptr = num;

    // print message to client
    printf("Number sent to server: %d\n", num);

    // wait for server to process number
    sleep(1);

    // read result from shared memory
    int result = *(int*)ptr;

    // print result to client
    printf("Server response: %d is %s\n", num, result % 2 == 0 ? "even" : "odd");

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

    return 0;
}
