#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CLIENTS 5
#define SHM_SIZE  sizeof(int)

void* client_handler(void* arg)
{
    int client_num = *(int*)arg;
    int shm_fd;
    void* ptr;
    int num;

    // create shared memory object
    shm_fd = shm_open("/myshm", O_RDWR, 0666);
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

// int main()
// {
//     pthread_t threads[MAX_CLIENTS];
//     int thread_args[MAX_CLIENTS];
//     int i, rc;

//     // create shared memory object
//     int shm_fd = shm_open("/shm_obj", O_CREAT | O_RDWR, 0666);
//     if (shm_fd == -1) {
//         perror("shm_open");
//         exit(EXIT_FAILURE);
//     }

//     // resize shared memory object
//     if (ftruncate(shm_fd, SHM_SIZE) == -1) {
//         perror("ftruncate");
//         exit(EXIT_FAILURE);
//     }

//     // initialize shared memory to 0
//     void* ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
//     if (ptr == MAP_FAILED) {
//         perror("mmap");
//         exit(EXIT_FAILURE);
//     }
//     *(int*)ptr = 0;
//     if (munmap(ptr, SHM_SIZE) == -1) {
//         perror("munmap");
//         exit(EXIT_FAILURE);
//     }

//     while (1) {
//         int num_clients = 0;

//         // create client threads for up to 5 clients
//         for (i = 0; i < MAX_CLIENTS; i++) {
//             thread_args[i] = i + 1;

//             // check if shared memory has been written to
//             ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
//             if (ptr == MAP_FAILED) {
//                 perror("mmap");
//                 exit(EXIT_FAILURE);
//             }

//             if (*(int*)ptr == 0) {
//                 // create client thread
//                 rc = pthread_create(&threads[i], NULL, client_handler, (void*)&thread_args[i]);
//                 if (rc) {
//                     printf("Error: return code from pthread_create() is %d\n", rc);
//                     exit(EXIT_FAILURE);
//                 }

//                 // increment number of clients
//                 num_clients++;

//                 // print message to server
//                 printf("Client %d connected\n", i + 1);
//             }

//             // unmap shared memory object
//             if (munmap(ptr, SHM_SIZE) == -1) {
//                 perror("munmap");
//                 exit(EXIT_FAILURE);
//             }

//             // break out of loop if maximum number of clients has been reached
//             if (num_clients == MAX_CLIENTS) {
//                 break;
//             }

//             // sleep for 1 second before checking shared memory again
//             sleep(1);
//         }

//         // wait for client threads to finish
//         for (i = 0; i < num_clients; i++) {
//             rc = pthread_join(threads[i], NULL);
//             if (rc) {
//                 printf("Error: return code from pthread_join() is %d\n", rc);
//                 exit(EXIT_FAILURE);
//             }
//         }
//     }

//     // close shared memory object
//     if (close(shm_fd) == -1) {
//         perror("close");
//         exit(EXIT_FAILURE);
//     }

//     // remove shared memory object
//     if (shm_unlink("/shm_obj") == -1) {
//         perror("shm_unlink");
//         exit(EXIT_FAILURE);
//     }

//     return 0;
// }
pthread_mutex_t mutex;
void* client_fn(void* arg) {
    int client_num = *(int*)arg;
    const char* name = "/myshm";
    int shm_fd;
    int* ptr;
    char sem_name[20];

    // create shared memory object
    shm_fd = shm_open(name, O_RDWR, 0666);
    ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // generate random number
    srand(time(NULL) + client_num);
    int num = rand() % 100 + 1;

    // write number to shared memory
    sprintf(sem_name, "/mysem_%d", client_num);
    sem_t* sem = sem_open(sem_name, O_CREAT, 0666, 0);
    pthread_mutex_lock(&mutex);
    *ptr = num;
    printf("Client %d: wrote %d to shared memory.\n", client_num, num);
    pthread_mutex_unlock(&mutex);
    sem_post(sem);

    // wait for server to read number
    sem_wait(sem);

    // read result from shared memory
    pthread_mutex_lock(&mutex);
    int result = *ptr;
    printf("Client %d: read %d from shared memory. It is %s.\n", client_num, result, result % 2 == 0 ? "even" : "odd");
    pthread_mutex_unlock(&mutex);

    // cleanup
    sem_close(sem);
    sem_unlink(sem_name);
    return NULL;
}

void* server_fn(void* arg) {
    int* ptr = (int*)arg;
    int client_num;

    while (*ptr < MAX_CLIENTS) {
        // wait for client to connect
        pthread_mutex_lock(&mutex);
        client_num = ++(*ptr);
        printf("New client connected. Total clients: %d\n", client_num);
        pthread_mutex_unlock(&mutex);

        // create thread for client
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, client_fn, (void*)&client_num);
        pthread_detach(client_thread);
    }

    return NULL;
}

int main() {
    int shm_fd;
    int* ptr;
    const char* name = "/myshm";

    // create shared memory object
    shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);

    // map shared memory object
    ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // initialize client count and mutex
    *ptr = 0;
    
    pthread_mutex_init(&mutex, NULL);

    // create server thread
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_fn, (void*)ptr);

    // wait for server thread to exit
    pthread_join(server_thread, NULL);

    // cleanup
    shm_unlink(name);
    pthread_mutex_destroy(&mutex);

    return 0;
}
