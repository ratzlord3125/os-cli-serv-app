#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#define MAX_BUF_SIZE 1024

struct Request
{
    int clientId;
    int value;
    int disconnect;
};

struct Response
{
    char result[MAX_BUF_SIZE];
};

int shmid_mutex, shmid_req, shmid_resp;
key_t key_mutex = 1500, key_req = 1600, key_resp = 1700;
int *mutex;
struct Request *request;
struct Response *response;

void signal_handler(int signum)
{
    if (signum == SIGINT)
    {
        printf("\nClient shutting down\n");
        request->disconnect = 1;
        shmdt(mutex);
        shmdt(request);
        shmdt(response);
        exit(0);
    }
}

int main()
{
    signal(SIGINT, signal_handler);

    int clientId = -1;
    printf("Enter client id: ");
    scanf("%d", &clientId);

    if (clientId < 0)
    {
        printf("Invalid client id\n");
        return -1;
    }

    shmid_mutex = shmget(key_mutex, sizeof(int), 0666 | IPC_CREAT);
    if (shmid_mutex == -1)
    {
        perror("Failed to create mutex shared memory segment");
        return -1;
    }

    mutex = (int *)shmat(shmid_mutex, NULL, 0);
    if (mutex == (int *)-1)
    {
        perror("Failed to attach mutex shared memory segment");
        return -1;
    }

    shmid_req = shmget(key_req + clientId, sizeof(struct Request), 0666 | IPC_CREAT);
    if (shmid_req == -1)
    {
        perror("Failed to create request shared memory segment");
        return -1;
    }

    request = (struct Request *)shmat(shmid_req, NULL, 0);
    if (request == (struct Request *)-1)
    {
        perror("Failed to attach request shared memory segment");
        return -1;
    }

    shmid_resp = shmget(key_resp + clientId, sizeof(struct Response), 0666 | IPC_CREAT);
    if (shmid_resp == -1)
    {
        perror("Failed to create response shared memory segment");
        return -1;
    }

    response = (struct Response *)shmat(shmid_resp, NULL, 0);
    if (response == (struct Response *)-1)
    {
        perror("Failed to attach response shared memory segment");
        return -1;
    }

    while (1)
    {
        if (*mutex == 1)
        {
            printf("Server is busy. Please wait...\n");
            sleep(1);
            continue;
        }

        printf("Enter a number: ");
        scanf("%d", &request->value);
        request->clientId = clientId;
        request->disconnect = 0;

        *mutex = 1;

        while (*mutex != 0)
        {
            // Wait for server to complete processing
            sleep(1);
        }

        printf("Result: %s\n", response->result);

        if (request->disconnect == 1)
        {
            printf("Disconnecting from server\n");
            break;
        }
    }

    shmdt(mutex);
    shmdt(request);
    shmdt(response);

    return 0;
}
