// gcc server_v.c -o server_v -lpthread -lrt && ./server_v

/* 
--> remove more plag
--> 
--> make more SERVER RESPONSE functionalities
*/
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
    pthread_rwlock_t lock; 
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
    pthread_rwlock_t lock; // unique comm channel lock 
    struct action_request request;
    char response[MAX_STRING_LENGTH];
    pthread_t tid; 
    int request_flag;
    int times_action_requested; 
    int active; // state to check if client thread active or not
    int registered;
};

// struct for each client reg details
struct client {
    char uid[MAX_UID_LENGTH];
    int times_serviced; 
    int inactive_reg_flag; // registration state when client inactive
    pthread_t tid; 
};

int CLIENTS_SERVICED = 0; 
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
        return 1;
    }

    printf("Connected to shared memory (fd = %d)\n", conn_fd); 

    if(ftruncate(conn_fd, SHM_SIZE) != 0) {
        perror("ftruncate"); 
        return 1; 
    }

    // mmaping the data for the connect channel shared mem
    connect_data_ptr = (struct connect_data*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, conn_fd, 0);
    if(connect_data_ptr == MAP_FAILED) {
        perror("mmap"); 
        return 1;
    }

    connect_data_ptr->response_code = 0;
    connect_data_ptr->request[0] = '\0'; 
    connect_data_ptr->reg_flag = 1; 
    pthread_rwlock_init(&connect_data_ptr->lock, NULL);

    printf("Server Initialisation Successful\n"); 

    // loop for connect channel
    while(1) {
        while(CLIENTS_SERVICED == MAX_CLIENTS) { // if max clients limit is reached
            printf("Max number of clients reached! No more clients can join now unless any unregistration occurs\n");
            usleep(1000000);
        }

        if(connect_data_ptr->reg_flag==0) {
            //Start serving the request
            //Bit flipped to 0 only after all write funcs done 
            char current_uid[MAX_UID_LENGTH];
            printf("UID received from client = %s\n",connect_data_ptr->request);
            strcpy(current_uid, connect_data_ptr->request); 

            int clash = 0; 
            int exist_ptr = 0;
            for(int i=0; i<CLIENTS_SERVICED; i++) {
                if(strcmp(clients_active[i].uid, current_uid)==0) {
                    if(clients_active[i].inactive_reg_flag == 1) {
                        clash = 2;
                        exist_ptr = i;
                    }
                    else 
                        clash = 1;
                }
            }

            if(clash == 1) {
                printf("Connect channel : Received UID %s already in use!!\n", current_uid); 
                connect_data_ptr->response_code = 2; 
                connect_data_ptr->response[0] = '\0'; 
            } 
            else if(clash == 2) {
                //existing user
                printf("Connect channel : Existing UID received = %s\n", current_uid);
                clients_active[exist_ptr].times_serviced++;
                clients_active[exist_ptr].inactive_reg_flag = 0;

                connect_data_ptr->response_code = 1; // rereg

                char channel_name[MAX_UID_LENGTH]; 
                strcpy(channel_name, current_uid); 
                strcpy(connect_data_ptr->response, channel_name);
                printf("Connect channel : Recreated a comm channel \"%s\" for user = %s\n", channel_name, current_uid); 
                
                printf("Connect channel : Clients serviced unchanged = %d\n", CLIENTS_SERVICED); // remains the same

                //creating thread to handle client
                pthread_t* x; 
                for(int i=0; i<=CLIENTS_SERVICED; i++) {
                    if(strcmp(clients_active[i].uid, current_uid) == 0) {
                        x = &clients_active[i].tid; 
                        break; 
                    }
                }
                pthread_create(x, NULL, handle_client, channel_name); 
                printf("Connect channel : Recreated thread to handle user = %s\n", current_uid); 
            }
            else {
                //new user 
                printf("Connect channel : New UID received = %s\n", current_uid);
                strcpy(clients_active[CLIENTS_SERVICED].uid, current_uid); 
                clients_active[CLIENTS_SERVICED].times_serviced = 1; 
                clients_active[CLIENTS_SERVICED].inactive_reg_flag = 0;

                connect_data_ptr->response_code = 0; // success

                char channel_name[MAX_UID_LENGTH]; 
                strcpy(channel_name, current_uid); 
                strcpy(connect_data_ptr->response, channel_name);
                printf("Connect channel : Created a comm channel \"%s\" for user = %s\n", channel_name, current_uid); 
                CLIENTS_SERVICED++;
                printf("Connect channel : Clients serviced updated = %d\n", CLIENTS_SERVICED);

                //creating thread to handle client
                pthread_t* x; 
                for(int i=0; i<=CLIENTS_SERVICED; i++) {
                    if(strcmp(clients_active[i].uid, current_uid) == 0) {
                        x = &clients_active[i].tid; 
                        break; 
                    }
                }
                pthread_create(x, NULL, handle_client, channel_name); 
                printf("Connect channel : Created thread to handle user :: %s\n", current_uid); 
            }
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

    struct client_data* data = (struct client_data*) mmap(NULL, sizeof(struct client_data), PROT_READ | PROT_WRITE, MAP_SHARED, client_fd, 0);
    if(data == MAP_FAILED) {
        perror("mmap"); 
        exit(EXIT_FAILURE); 
    }

    data->request_flag = 1; 
    data->active = 1; 
    data->registered = 1;
    data->times_action_requested = 0; 
    pthread_rwlock_init(&data->lock, NULL); 
    printf("Comm channel \"%s\" : Succesfully Initialised\n", client_channel_name);

    while(data->active) {
        THREAD_STATE = 1;
        if(data->request_flag == 0 && data->active) {

            int code = data->request.action_code; 
            char temp[MAX_STRING_LENGTH]; // for storing the formatted result based on action
            int x,n1,n2,op,ans;
            
            printf("Comm channel \"%s\" : Received action-response of code %d \n", client_channel_name, code); 

            switch(code) {
                case 0 :
                    n1 = data->request.n1; 
                    n2 = data->request.n2; 
                    op = data->request.operator;
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
                            ans = n1/n2; 
                            oper = "/";
                            break; 
                    }
                    sprintf(temp,"Result : %d%s%d = %d",n1,oper,n2,ans);
                    break;
                case 1 :
                    x = data->request.n1;
                    sprintf(temp,"Result : %d is an %s number",x,(x%2)?"odd":"even");
                    break;
                case 2 :
                    x = data->request.n1; 
                    int prime = 1; 
                    for(int i=2; i<x/2; i++) 
                        if(x%i == 0) 
                            prime = 0;
                    
                    sprintf(temp,"Result : %d is %s prime number",x,prime?"a":"not a");
                    break;
                case 3 :
                    x = data->request.n1; 
                    sprintf(temp,"Result : %d is a %s number",x,(x<0)?"negative":"positive");
                    break;
            }
            printf("Comm channel \"%s\" : Response generated = \"%s\"\n",client_channel_name, temp);
            strcpy(data->response,temp);
            data->request_flag = 1;
            printf("Comm channel \"%s\" : Sent Response to client\n", client_channel_name);  
        }
         
    }

    // finding the current terminated client
    int p;
    for(int i=0; i<CLIENTS_SERVICED; i++) {
        if(strcmp(clients_active[i].uid, client_channel_name)==0) {
            p=i;
        }
    }

    // checking the registration status of the current terminated client
    if(data->registered) {
        printf("Comm channel \"%s\" : Connection has been terminated but still registered\n", client_channel_name); 
        clients_active[p].inactive_reg_flag = 1;
        // not changing THREAD_STATE because memory still needs to be cleared if SIGINT occurs
    } 
    else {
        printf("Comm channel \"%s\" : Unregistered! Connection has been terminated and memory cleared\n", client_channel_name); 
        // unmapping memory and unlinking shared memory of this client
        munmap(data,sizeof(struct client_data));
        shm_unlink(client_channel_name);

        clients_active[p].inactive_reg_flag = 0;
        // also shifting the array so that space can be saved
        for(int i=p; i<CLIENTS_SERVICED-1; i++) {
            clients_active[i] = clients_active[i+1];
        }
        CLIENTS_SERVICED--;
        THREAD_STATE = 0;
    }
    //printf("%s's channel : Times Pinged : %d", client_channel_name, data->times_action_requested); 
    
    pthread_exit(EXIT_SUCCESS);
}

void handle_signal(int sig) {
    if(THREAD_STATE) {
        printf(" Clearing and unlinking thread memory (if any)...\n");
        for(int i=0; i<CLIENTS_SERVICED; i++) {
            printf("Unlinking shared memory for %s\n",clients_active[i].uid );
            //munmap(??, sizeof(struct client_data));
            shm_unlink(clients_active[i].uid);
        }
    }
    // clear memory and unlink shm
    printf(" Clearing and unlinking connect memory...\n");
    munmap(connect_data_ptr, SHM_SIZE);
    shm_unlink(SHM_NAME);
    exit(EXIT_SUCCESS);
}