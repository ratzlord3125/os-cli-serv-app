/*
Team members:
Aditya Kumar
Aditya Seth
Ayush Mishra
Anmol Goyal
Puneet Agarwal
Kaushik Chetluri
*/

// run here : gcc A2_25_server.c -o A2_25_server -lpthread -lrt && ./A2_25_server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define SHM_NAME "/myshm" // shared memory for connect channel
#define SHM_SIZE sizeof(struct connect_data)

#define MAX_CLIENTS 10 
#define MAX_UID_LENGTH 100 
#define MAX_STRING_LENGTH 10000

// struct for connect channel
struct connect_data { 
    pthread_rwlock_t lock; // unique connect channel lock 
    int response_code; // 0-success, 1-uid rereg, 2 - uid in use
    int reg_flag; 
    char request[MAX_UID_LENGTH]; 
    char response[MAX_UID_LENGTH]; 
};

// struct for request in comm channel of each client
struct action_request { 
    int action_code; // server based actions: 1-calc, 2-odd/even, 3-prime, 4-negative
    int n1, n2, operator;  
};

// struct for comm channel of each client
struct client_data { 
    pthread_mutex_t mtx; 
    struct action_request request;
    char response[MAX_STRING_LENGTH];
    pthread_t tid; 
    int request_flag;  // conditional variable to check for request
    int times_action_requested; 
    int active; // state to check if client thread active or not
    int registered; // state to check if client unregistered while terminating
};

// struct for each client reg details
struct client {
    char uid[MAX_UID_LENGTH];
    int times_serviced; 
    int inactive_reg_flag; // registration state when client inactive
    pthread_t tid; 
};

int CLIENTS_ACTIV = 0; 
int TOTAL_CLIENTS_SERVED = 0; 
int TOTAL_SERVICES = 0;
int THREAD_STATE = 0;
struct client clients_active[MAX_CLIENTS]; 
struct connect_data* connect_data_ptr;

void* handle_client(void* arg); // client handler thread func
void handle_signal(int sig); // handling SIGINT

int main() {
    printf("Server has been started!!\n");

    signal(SIGINT,handle_signal);

    // creating connect channel shared memory
    int conn_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (conn_fd < 0) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    printf("Connected to shared memory (fd = %d)\n", conn_fd); 

    if(ftruncate(conn_fd, SHM_SIZE) != 0) {
        perror("ftruncate"); 
        exit(EXIT_FAILURE);
    }

    // mmaping the data for the connect channel shared mem
    connect_data_ptr = (struct connect_data*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, conn_fd, 0);
    if(connect_data_ptr == MAP_FAILED) {
        perror("mmap"); 
        return 1;
    }

    // initialising connect_data contents and rw lock
    connect_data_ptr->response_code = 0;
    connect_data_ptr->request[0] = '\0'; 
    connect_data_ptr->reg_flag = 1; 
    pthread_rwlock_init(&connect_data_ptr->lock, NULL);

    printf("Server Initialisation Successful\n"); 

    // loop for connect channel
    while(1) {
        while(CLIENTS_ACTIV == MAX_CLIENTS) { // if max clients limit is reached
            printf("Max number of clients reached! No more clients can join now unless any unregistration occurs\n");
            usleep(1000000); // sleep 1 sec
        }

        if(connect_data_ptr->reg_flag==0) { // when server is ready to receive requests
            TOTAL_SERVICES++;
            char current_uid[MAX_UID_LENGTH];
            printf("Connect channel : UID received from client = %s\n",connect_data_ptr->request);
            strcpy(current_uid, connect_data_ptr->request); 

            int exist_flag = 0; 
            int exist_ptr = 0;
            for(int i=0; i<CLIENTS_ACTIV; i++) {
                if(strcmp(clients_active[i].uid, current_uid)==0) {
                    if(clients_active[i].inactive_reg_flag == 1) {
                        exist_flag = 1;
                        exist_ptr = i;
                    }
                    else 
                        exist_flag = 2;
                }
            }

            if(exist_flag == 0) {  // new and unique client
                printf("Connect channel : New UID received = %s\n", current_uid);
                strcpy(clients_active[CLIENTS_ACTIV].uid, current_uid); 
                clients_active[CLIENTS_ACTIV].inactive_reg_flag = 0;
                clients_active[CLIENTS_ACTIV].times_serviced = 1; 

                connect_data_ptr->response_code = 0; // success

                char channel_name[MAX_UID_LENGTH]; 
                strcpy(channel_name, current_uid); // keeping new comm channel name same as uid
                strcpy(connect_data_ptr->response, channel_name);
                printf("Connect channel : Created a comm channel \"%s\" for user = %s\n", channel_name, current_uid); 

                //creating thread to handle client
                pthread_t* curr_tid = &clients_active[CLIENTS_ACTIV].tid; 
                pthread_create(curr_tid, NULL, handle_client, channel_name); 
                printf("Connect channel : Created thread (tid = %lu) to handle user :: %s\n", curr_tid, current_uid);

                CLIENTS_ACTIV++;
                TOTAL_CLIENTS_SERVED++; 
                printf("Connect channel : Clients active updated = %d\n", CLIENTS_ACTIV);
            }
            
            else if(exist_flag == 1) { // existing inactive client
                printf("Connect channel : Existing UID received = %s\n", current_uid);
                clients_active[exist_ptr].inactive_reg_flag = 0;
                clients_active[exist_ptr].times_serviced++;

                connect_data_ptr->response_code = 1; // rereg

                char channel_name[MAX_UID_LENGTH]; 
                strcpy(channel_name, current_uid); 
                strcpy(connect_data_ptr->response, channel_name);
                printf("Connect channel : Recreated a comm channel \"%s\" for user = %s\n", channel_name, current_uid); 

                //creating thread to handle client
                pthread_t* curr_tid = &clients_active[exist_ptr].tid; 
                pthread_create(curr_tid, NULL, handle_client, channel_name); 
                printf("Connect channel : Recreated thread (tid = %lu) to handle user = %s\n",curr_tid, current_uid); 
                
                printf("Connect channel : Clients served unchanged = %d\n", CLIENTS_ACTIV); // remains the same
            }
            else if(exist_flag == 2) { // existing active client
                printf("Connect channel : Received UID %s already in use!!\n", current_uid); 
                connect_data_ptr->response_code = 2; // in use
                connect_data_ptr->response[0] = '\0'; 
            } 

            // logging list of clients
            putchar('\n');
            printf("List of clients currently active : \n");
            for(int i=0; i<CLIENTS_ACTIV; i++) {
                printf("Client uid \"%s\" :: currently %s\n",clients_active[i].uid,clients_active[i].inactive_reg_flag?"inactive":"active");
            }
            putchar('\n');
            printf("Total clients served = %d\n", TOTAL_CLIENTS_SERVED);
            printf("Total number of requests serviced = %d\n",TOTAL_SERVICES);
            putchar('\n');
            
            // flag change only after complete registration response is sent and server is started 
            connect_data_ptr->reg_flag =1; 
        }
    }
}

void* handle_client(void* arg) {
    char client_channel_name[MAX_UID_LENGTH]; 
    strcpy(client_channel_name, (char*)arg); 

    int client_fd = shm_open(client_channel_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); 
    if(client_fd < 0) {
        perror("shm_open"); 
        exit(EXIT_FAILURE); 
    } 
    
    printf("Comm channel \"%s\" : Connected to client shared memory successfully (fd = %d)\n", client_channel_name, client_fd); 

    if(ftruncate(client_fd, sizeof(struct client_data)) != 0) {
        perror("ftruncate"); 
        exit(EXIT_FAILURE); 
    }

    struct client_data* client_data_ptr = (struct client_data*) mmap(NULL, sizeof(struct client_data), PROT_READ | PROT_WRITE, MAP_SHARED, client_fd, 0);
    if(client_data_ptr == MAP_FAILED) {
        perror("mmap"); 
        exit(EXIT_FAILURE); 
    }

    client_data_ptr->request_flag = 1; 
    client_data_ptr->active = 1; 
    client_data_ptr->registered = 1;
    client_data_ptr->times_action_requested = 0; 
    pthread_mutex_init(&client_data_ptr->mtx, NULL); 
    printf("Comm channel \"%s\" : Succesfully Initialised\n", client_channel_name);

    while(client_data_ptr->active) {
        THREAD_STATE = 1; // thread state active all the time if client is in active state
        if(client_data_ptr->request_flag == 0 && client_data_ptr->active) {
            int code = client_data_ptr->request.action_code; 
            char temp[MAX_STRING_LENGTH]; // for storing the formatted result based on action
            int x,n1,n2,op,ans;
            
            printf("Comm channel \"%s\" : Received action-response of code %d \n", client_channel_name, code); 

            switch(code) {
                case 0 :
                    n1 = client_data_ptr->request.n1; 
                    n2 = client_data_ptr->request.n2; 
                    op = client_data_ptr->request.operator;
                    char* oper;
                    switch(op) {
                        case 1:
                            ans = n1 + n2; 
                            oper = "+";
                            break; 
                        case 2: 
                            ans = n1 - n2; 
                            oper = "-";
                            break; 
                        case 3: 
                            ans = n1*n2; 
                            oper = "*";
                            break; 
                        case 4: 
                            ans = n2!=0?(n1/n2):__INT_MAX__; 
                            oper = "/";
                            break; 
                    }
                    sprintf(temp,"Result : %d%s%d = %d",n1,oper,n2,ans);
                    break;
                case 1 :
                    x = client_data_ptr->request.n1;
                    sprintf(temp,"Result : %d is an %s number",x,(x%2)?"odd":"even");
                    break;
                case 2 :
                    x = client_data_ptr->request.n1; 
                    int prime = 1; 
                    for(int i=2; i<x/2; i++) 
                        if(x%i == 0) 
                            prime = 0;
                    
                    sprintf(temp,"Result : %d is %s prime number",x,prime?"a":"not a");
                    break;
                case 3 :
                    x = client_data_ptr->request.n1; 
                    sprintf(temp,"Result : %d is a %s number",x,(x<0)?"negative":"positive");
                    break;
            }
            printf("Comm channel \"%s\" : Response generated = \"%s\"\n",client_channel_name, temp);
            strcpy(client_data_ptr->response,temp);
            client_data_ptr->request_flag = 1;
            printf("Comm channel \"%s\" : Sent Response to client\n", client_channel_name);  
        }
         
    }

    int curr_actions_count = ++(client_data_ptr->times_action_requested);  // counting termination/unreg as a request

    // finding the current terminated client
    int p;
    for(int i=0; i<CLIENTS_ACTIV; i++) {
        if(strcmp(clients_active[i].uid, client_channel_name)==0) {
            p=i;
        }
    }

    // checking the registration status of the current terminated client
    if(client_data_ptr->registered) {
        printf("Comm channel \"%s\" : Connection has been terminated but user still registered\n", client_channel_name); 
        clients_active[p].inactive_reg_flag = 1;
        // not changing THREAD_STATE because memory still needs to be cleared if SIGINT occurs
    } 
    else {
        printf("Comm channel \"%s\" : Unregistered! Connection has been terminated and memory is being cleared\n", client_channel_name); 
        // unmapping memory and unlinking shared memory of this client
        munmap(client_data_ptr,sizeof(struct client_data));
        shm_unlink(client_channel_name);

        clients_active[p].inactive_reg_flag = 0;
        // also shifting the array so that space can be saved
        for(int i=p; i<CLIENTS_ACTIV-1; i++) {
            clients_active[i] = clients_active[i+1];
        }
        CLIENTS_ACTIV--;
        THREAD_STATE = 0;
    }
    
    TOTAL_SERVICES += curr_actions_count;
    pthread_mutex_destroy(&client_data_ptr->mtx); 
    pthread_exit(EXIT_SUCCESS);
}

void handle_signal(int sig) {
    if(THREAD_STATE) {
        printf(" Clearing and unlinking thread memory (if any)...\n");
        for(int i=0; i<CLIENTS_ACTIV; i++) {
            printf("Unlinking shared memory for %s\n",clients_active[i].uid );
            //munmap(??, sizeof(struct client_data));
            shm_unlink(clients_active[i].uid);
        }
    }
    printf("Final total clients served = %d\n",TOTAL_CLIENTS_SERVED);
    printf("Final total requests served = %d\n",TOTAL_SERVICES);

    // clear memory and unlink shm
    printf("Clearing and unlinking connect memory...\n");
    munmap(connect_data_ptr, SHM_SIZE);
    shm_unlink(SHM_NAME);
    exit(EXIT_SUCCESS);
}