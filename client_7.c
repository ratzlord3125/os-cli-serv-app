
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SHM_SIZE sizeof(int)

int main()
{
    int client_id = getpid();
    char *shm_name = malloc(sizeof(char) * 10);
    sprintf(shm_name, "/client%d", client_id);

    int shm_fd = shm_open(shm_name, O_RDWR | O_CREAT, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }
    int *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    close(shm_fd);
    free(shm_name);

    int connect_fd = shm_open("/connect", O_RDWR, 0666);
    if (connect_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    int *connect_ptr = mmap(NULL, 10 * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, connect_fd, 0);
    if (connect_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    close(connect_fd);

    printf("Connected to server\n");

    while (1) {
        // Write to server shared memory
        printf("Enter a number: ");
        int num;
        scanf("%d", &num);
        *shm_ptr = num;

        // Wait for result from server
        while (*shm_ptr == num)
            sleep(1);

        // Read result from server shared memory
        char *result = malloc(sizeof(char) * (*shm_ptr));
        memcpy(result, shm_ptr + 1, *shm_ptr);
        printf("Result: %s\n", result);
        free(result);
    }

    if (munmap(shm_ptr, SHM_SIZE) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }
    if (shm_unlink(shm_name) == -1) {
        perror("shm_unlink");
        exit(EXIT_FAILURE);
    }

    printf("Disconnected from server\n");

    return 0;
}
