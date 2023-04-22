#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

#define SHM_NAME "/connect" 
#define SHM_SIZE sizeof(struct connect_data)

#define MAX_CLIENTS 100 
#define MAX_USERNAME_LENGTH 100 

//connect channel
struct connect_data { 
    pthread_rwlock_t lock; 
    int response_code; //0 - ok | 1 - username in use | 2 - username too long
    int served_registration_request; 
    char request[MAX_USERNAME_LENGTH]; 
    char response[MAX_USERNAME_LENGTH*2]; 
}; 


// client channel
struct client_request { 
    int service_code; //0 - operation | 1 - even or odd | 2 - is prime | 3 - is negative 
    int n1, n2, op_type; 
    int evenOdd; 
    int isPrime; 
    int isNegative; 
};

struct server_response {
    int ans; 
    int even, odd; 
    int isPrime; 
    int isNegative; 
};

struct client_data { 
    pthread_rwlock_t lock; //unique channel lock 
    pthread_t tid; 
    int times_serviced; 
    int active; //when active = 0, termination request
    int served_registration_request;
    struct client_request request; 
    struct server_response response;
};


//client struct
struct client {
    char username[MAX_USERNAME_LENGTH];
    int times_serviced; 
    pthread_t tid; 
};

int CLIENTS_SERVICED = 0; 

struct client clients[MAX_CLIENTS]; 


//Thread handler
void* thread_handler(void* arg); 


int main() {
    //CREATING CONNECTION SHM

    int connect_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (connect_fd < 0) {
        perror("shm_open");
        return 1;
    }
    printf("Connected to shm fd :: %d\n", connect_fd); 

    if(ftruncate(connect_fd, SHM_SIZE) != 0) {
        perror("ftruncate"); 
        return 1; 
    }

    //mmaping the data for the connection shm 
    struct connect_data* connect_data = (struct connect_data*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, connect_fd, 0);
    if(connect_data == MAP_FAILED) {
        perror("mmap"); 
        return 1;
    }

    connect_data->response_code = 0;
    connect_data->request[0] = '\0'; 
    connect_data->request[0] = '\0'; 
    connect_data->served_registration_request = 1; 
    pthread_rwlock_init(&connect_data->lock, NULL);
    printf("Server initialised\n"); 
    //SERVER LOOP

    while(CLIENTS_SERVICED < MAX_CLIENTS) {

        if(connect_data->served_registration_request==0) {
            //Start serving the request
            //Bit flipped to 0 only after all write funcs done 
            char current_username[MAX_USERNAME_LENGTH];
            strcpy(current_username, connect_data->request); 

            int clash = 0; 
            for(int i=0; i<CLIENTS_SERVICED; i++) 
                if(strcmp(clients[i].username, current_username)==0) clash++;

            if(clash) {
                printf("Connect : !!Username clash :: %s!!\n", current_username); 
                connect_data->response_code = 1; 
                connect_data->response[0] = '\0'; 
            } 
            else {
                //new user 
                printf("Connect : New Username :: %s\n", current_username);
                strcpy(clients[CLIENTS_SERVICED].username, current_username); 
                clients[CLIENTS_SERVICED].times_serviced = 1; 

                connect_data->response_code = 0; //ok

                char channel_name[MAX_USERNAME_LENGTH*2]; 
                strcpy(channel_name, current_username); 
                strcat(channel_name, current_username); 
                strcpy(connect_data->response, channel_name);
                printf("Connect : Created a channel :: %s for user :: %s\n", channel_name, current_username); 
                CLIENTS_SERVICED++;
                printf("Connect : Clients Serviced :: %d\n", CLIENTS_SERVICED);

                //creating thread to handle client
                pthread_t* x; 
                for(int i=0; i<=CLIENTS_SERVICED; i++) {
                    if(strcmp(clients[i].username, current_username) == 0) {
                        x = &clients[i].tid; 
                        break; 
                    }
                }
                pthread_create(x, NULL, thread_handler, channel_name); 
                printf("Connect : Created thread to handle user :: %s\n", current_username); 
            }
            
            connect_data->served_registration_request =1; 
        }

        // sleep(1); //TODO : remove 
    }
}


void* thread_handler(void* arg){
    char channel_name[MAX_USERNAME_LENGTH*2]; 
    strcpy(channel_name, (char*)arg); 

    int client_fd = shm_open(channel_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); 
    if(client_fd < 0) {
        perror("shm_open"); 
        return NULL; 
    } else printf("%s's channel : Succesfully connected to client shm. fd :: %d\n", channel_name, client_fd); 

    if(ftruncate(client_fd, sizeof(struct client_data)) != 0) {
        perror("ftruncate"); 
        return NULL; 
    }

    struct client_data* data = (struct client_data*) mmap(NULL, sizeof(struct client_data), PROT_READ | PROT_WRITE, MAP_SHARED, client_fd, 0);
    if(data == MAP_FAILED) {
        perror("mmap"); 
        return NULL; 
    }

    data->served_registration_request = 1; 
    data->active = 1; 
    data->times_serviced = 0; 
    pthread_rwlock_init(&data->lock, NULL); 
    printf("%s's channel : Succesfully Initialised\n", channel_name);

    while(data->active) {
        if(data->served_registration_request == 0 && data->active) {

            int code = data->request.service_code; 
            
            printf("%s's channel : received response of code :: %d \n", channel_name, code); 

            if(code == 0) {
                int n1 = data->request.n1, n2 = data->request.n2, op= data->request.op_type; 
                int ans;
                switch(op) {
                    case 0:
                        ans = n1 + n2; 
                        break; 
                    case 1: 
                        ans = n1 - n2; 
                        break; 
                    case 2: 
                        ans = n1*n2; 
                        break; 
                    case 3: 
                        ans = n1/n2; 
                        break; 
                }
                data->response.ans = ans; 
            } else if(code == 1) {
                int x = data->request.evenOdd;
                data->response.even = !(x%2); 
                data->response.odd = x%2; 
            } else if(code == 2) {
                int x = data->request.isPrime; 
                int prime = 1; 
                for(int i=2; i<x/2; i++) if(x%i == 0) prime = 0;
                data->response.isPrime = prime; 
            } else {
                int x = data->request.isNegative; 
                data->response.isNegative = x < 0; 
            }
            data->served_registration_request = 1;
            printf("%s's channel : Sent Response to client\n", channel_name);  
        }
         
    }
    printf("%s's channel : Connection has been terminated, terminating thread gracefully\n", channel_name); 
    printf("%s's channel : Times Pinged : %d", channel_name, data->times_serviced); 
}