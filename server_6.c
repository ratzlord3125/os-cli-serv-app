#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define SHM_SIZE sizeof(int)

int client_ids[MAX_CLIENTS];
int num_clients = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg)
{
    int client_id = *(int*)arg;
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

    printf("Client %d connected\n", client_id);

    while (1) {
        // Read from client shared memory
        while (*shm_ptr == 0)
            sleep(1);
        int num = *shm_ptr;
        *shm_ptr = 0;

        // Check if even or odd
        char *result;
        if (num % 2 == 0)
            result = "even";
        else
            result = "odd";

        // Write to client shared memory
        *shm_ptr = strlen(result) + 1;
        memcpy(shm_ptr + 1, result, *shm_ptr);
    }

    if (munmap(shm_ptr, SHM_SIZE) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }
    if (shm_unlink(shm_name) == -1) {
        perror("shm_unlink");
        exit(EXIT_FAILURE);
    }

    printf("Client %d disconnected\n", client_id);

    pthread_exit(NULL);
}

int main()
{
    int connect_fd = shm_open("/connect", O_RDWR | O_CREAT, 0666);
    if (connect_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(connect_fd, MAX_CLIENTS * sizeof(int)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }
    int *connect_ptr = mmap(NULL, MAX_CLIENTS * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, connect_fd, 0);
    if (connect_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    close(connect_fd);

    pthread_t threads[MAX_CLIENTS];

    while (1) {
        // Check for new clients
        for (int i = 0; i < num_clients; i++) {
            if (connect_ptr[i] != client_ids[i]) {
                printf("Client %d disconnected\n", client_ids[i]);
                pthread_cancel(threads[i]);
                connect_ptr[i] = 0;
                client_ids[i] = 0;
            }
        }

        if (num_clients >= MAX_CLIENTS) {
            sleep(1);
            continue;
        }

        int new_client_id = 0;
        for (int i = 1; i <= MAX_CLIENTS; i++) {
            int found = 0;
            for (int j = 0; j < num_clients; j++) {
                if (client_ids[j] == i) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                new_client_id = i;
                break;
            }
        }
        if (new_client_id == 0) {
            sleep(1);
            continue;
        }

        connect_ptr[num_clients] = new_client_id;

        pthread_create(&threads[num_clients], NULL, handle_client, &new_client_id);

        pthread_mutex_lock(&client_mutex);
        client_ids[num_clients] = new_client_id;
        num_clients++;
        pthread_mutex_unlock(&client_mutex);
    }

    if (munmap(connect_ptr, MAX_CLIENTS * sizeof(int)) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }
    if (shm_unlink("/connect") == -1) {
        perror("shm_unlink");
        exit(EXIT_FAILURE);
    }

    return 0;
}
