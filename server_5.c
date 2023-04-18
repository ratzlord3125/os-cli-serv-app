#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define SHM_SIZE sizeof(int)

int client_ids[MAX_CLIENTS];
int num_clients = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg)
{
    int client_id = *(int*)arg;
    key_t key = ftok(".", client_id);
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    int *shm_ptr = (int *) shmat(shmid, NULL, 0);
    if (shm_ptr == (int *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

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

    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);

    printf("Client %d disconnected\n", client_id);

    pthread_exit(NULL);
}

int main()
{
    int connect_shmid = shmget(IPC_PRIVATE, MAX_CLIENTS * sizeof(int), IPC_CREAT | 0666);
    if (connect_shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    int *connect_ptr = (int *) shmat(connect_shmid, NULL, 0);
    if (connect_ptr == (int *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

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

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (connect_ptr[i] == 0) {
                // New client found
                connect_ptr[i] = i + 1;
                client_ids[num_clients] = i + 1;
                num_clients++;

                pthread_create(&threads[i], NULL, handle_client, &client_ids[num_clients - 1]);
            }
        }

        sleep(1);
    }

    shmdt(connect_ptr);
    shmctl(connect_shmid, IPC_RMID, NULL);
    pthread_mutex_destroy(&client_mutex);

    return 0;
}