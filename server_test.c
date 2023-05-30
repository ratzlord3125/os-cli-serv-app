#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>

#define SHM_NAME "/my_shared_memory6"

typedef struct {
    int request;
    int response;
    pthread_mutex_t mutex;
} SharedMemory;

SharedMemory *shared_mem;

int is_running = 1;

void handle_signal(int sig) {
    // Set running flag to false to indicate termination
    is_running = 0;
}

void process_request(int client_id, int request) {
    // Process the request (check if number is even or odd)
    int response = (request % 2 == 0) ? 1 : 0;

    // Send response to client
    pthread_mutex_lock(&shared_mem->mutex);
    shared_mem->response = response;
    pthread_mutex_unlock(&shared_mem->mutex);
    printf("Server sent response to client %d: %s\n", client_id, (response == 1) ? "EVEN" : "ODD");
}

void *server_thread_function(void *arg) {
    int client_id = *((int *)arg);

    // Wait for requests from clients
    while (is_running) {
        pthread_mutex_lock(&shared_mem->mutex);
        int request = shared_mem->request;
        pthread_mutex_unlock(&shared_mem->mutex);

        if (request != 0) {
            printf("Server received request from client %d: %d\n", client_id, request);
            process_request(client_id, request);

            // Reset request in shared memory
            pthread_mutex_lock(&shared_mem->mutex);
            shared_mem->request = 0;
            pthread_mutex_unlock(&shared_mem->mutex);
        }

        usleep(100000); // Sleep for 100ms to avoid busy waiting
    }

    return NULL;
}

int main() {
    const int NUM_CLIENTS = 5;
    const int SHM_SIZE = sizeof(SharedMemory);

    // Open shared memory segment
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error opening shared memory");
        exit(1);
    }
    ftruncate(shm_fd, SHM_SIZE);
    shared_mem = (SharedMemory*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("Error mapping shared memory");
        exit(1);
    }

    // Install signal handler for SIGINT
    signal(SIGINT, handle_signal);

    // Initialize mutex
    pthread_mutex_init(&shared_mem->mutex, NULL);
 /*  while(1) {

        pthread_mutex_lock(&shared_mem->mutex);
        if(shared_mem->request is available?)
            handle req func called
        else
            unlock and then sleep for 1s


    */
    // Create server threads
    pthread_t server_threads[NUM_CLIENTS];
    int client_ids[NUM_CLIENTS];
    for (int i = 0; i < NUM_CLIENTS; i++) {
        client_ids[i] = i;
        pthread_create(&server_threads[i], NULL, server_thread_function, &client_ids[i]);
    }

    // Wait for server threads to finish
    for (int i = 0; i < NUM_CLIENTS; i++) {
        pthread_join(server_threads[i], NULL);
    }

    // Clean up
    pthread_mutex_destroy(&shared_mem->mutex);
    munmap(shared_mem, SHM_SIZE);
    shm_unlink(SHM_NAME);
    
    return 0;
}
