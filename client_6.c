#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_CLIENTS 10
#define SHM_SIZE sizeof(int)

int main()
{
    printf("Client started\n");
    char *shm_name = "/connect";

    int connect_fd = shm_open(shm_name, O_RDWR, 0666);
    if (connect_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    int *connect_ptr = mmap(NULL, MAX_CLIENTS * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, connect_fd, 0);
    if (connect_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    close(connect_fd);

    int client_id = -1;
    while (client_id == -1) {
        // Look for a free slot
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (connect_ptr[i] == 1) {

                connect_ptr[i] = getpid();
                client_id = i + 1;
                break;
            }
        }
        if (client_id == -1) {
            printf("Server is busy, retrying in 1 second...\n");
            sleep(1);
        }
    }

    printf("Connected to server as client %d\n", client_id);

    char *client_shm_name = malloc(sizeof(char) * 10);
    sprintf(client_shm_name, "/client%d", client_id);

    int client_shm_fd = shm_open(client_shm_name, O_RDWR, 0666);
    if (client_shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    int *client_shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, client_shm_fd, 0);
    if (client_shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    close(client_shm_fd);
    free(client_shm_name);

    while (1) {
        // Read from stdin
        printf("Enter an integer: ");
        int num;
        scanf("%d", &num);

        // Write to server shared memory
        *client_shm_ptr = num;

        // Wait for server response
        while (*client_shm_ptr == num)
            sleep(1);

        // Read server response
        int response_len = *client_shm_ptr;
        char response[response_len];
        memcpy(response, client_shm_ptr + 1, response_len);

        printf("Server response: %s\n", response);
    }

    if (munmap(client_shm_ptr, SHM_SIZE) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    printf("Disconnected from server\n");

    return 0;
}
