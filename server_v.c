// gcc server_v.c -o server_v -lpthread -lrt && ./server_v

/* 
--> remove more plag
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
    int response_code; //0 - ok | 1 - uid rereg | 2 - uid in use
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
struct connect_data* connect_data;

void* handle_client(void* arg); // client handler thread func
void handle_signal(int sig); 

int main() {
    printf("Server has been started!!\n");

    signal(SIGINT,handle_signal);

    // creating shared memory
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
    connect_data = (struct connect_data*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, conn_fd, 0);
    if(connect_data == MAP_FAILED) {
        perror("mmap"); 
        return 1;
    }

    connect_data->response_code = 0;
    connect_data->request[0] = '\0'; 
    connect_data->reg_flag = 1; 
    pthread_rwlock_init(&connect_data->lock, NULL);

    printf("Server Initialisation Successful\n"); 
    //SERVER LOOP

    while(CLIENTS_SERVICED < MAX_CLIENTS) {
        if(connect_data->reg_flag==0) {
            //Start serving the request
            //Bit flipped to 0 only after all write funcs done 
            char current_uid[MAX_UID_LENGTH];
            strcpy(current_uid, connect_data->request); 

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
                printf("Connect : !!UID clash :: %s!!\n", current_uid); 
                connect_data->response_code = 2; 
                connect_data->response[0] = '\0'; 
            } 
            else if(clash == 2) {
                //existing user
                printf("Connect : Existing UID :: %s\n", current_uid);
                clients_active[exist_ptr].times_serviced++;
                clients_active[exist_ptr].inactive_reg_flag = 0;

                connect_data->response_code = 1; //rereg

                char channel_name[MAX_UID_LENGTH]; 
                strcpy(channel_name, current_uid); 
                strcpy(connect_data->response, channel_name);
                printf("Connect : Recreated a channel :: %s for user :: %s\n", channel_name, current_uid); 
                
                printf("Connect : Clients Serviced :: %d\n", CLIENTS_SERVICED); // remains the same

                //creating thread to handle client
                pthread_t* x; 
                for(int i=0; i<=CLIENTS_SERVICED; i++) {
                    if(strcmp(clients_active[i].uid, current_uid) == 0) {
                        x = &clients_active[i].tid; 
                        break; 
                    }
                }
                pthread_create(x, NULL, handle_client, channel_name); 
                printf("Connect : Recreated thread to handle user :: %s\n", current_uid); 
            }
            else {
                //new user 
                printf("Connect : New UID :: %s\n", current_uid);
                strcpy(clients_active[CLIENTS_SERVICED].uid, current_uid); 
                clients_active[CLIENTS_SERVICED].times_serviced = 1; 
                clients_active[CLIENTS_SERVICED].inactive_reg_flag = 0;

                connect_data->response_code = 0; //ok

                char channel_name[MAX_UID_LENGTH]; 
                strcpy(channel_name, current_uid); 
                strcpy(connect_data->response, channel_name);
                printf("Connect : Created a channel :: %s for user :: %s\n", channel_name, current_uid); 
                CLIENTS_SERVICED++;
                printf("Connect : Clients Serviced :: %d\n", CLIENTS_SERVICED);

                //creating thread to handle client
                pthread_t* x; 
                for(int i=0; i<=CLIENTS_SERVICED; i++) {
                    if(strcmp(clients_active[i].uid, current_uid) == 0) {
                        x = &clients_active[i].tid; 
                        break; 
                    }
                }
                pthread_create(x, NULL, handle_client, channel_name); 
                printf("Connect : Created thread to handle user :: %s\n", current_uid); 
            }
            
            connect_data->reg_flag =1; 
        }
    }
}

void* handle_client(void* arg) {
    char channel_name[MAX_UID_LENGTH*2]; 
    strcpy(channel_name, (char*)arg); 

    int client_fd = shm_open(channel_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); 
    if(client_fd < 0) {
        perror("shm_open"); 
        exit(EXIT_FAILURE); 
    } 
    
    printf("Thread for %s :: Connected to client shared memory successfully (fd = %d)\n", channel_name, client_fd); 

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
    printf("%s's channel : Succesfully Initialised\n", channel_name);

    while(data->active) {
        THREAD_STATE = 1;
        if(data->request_flag == 0 && data->active) {

            int code = data->request.action_code; 
            char temp[MAX_STRING_LENGTH];
            int x,n1,n2,op,ans;
            
            printf("%s's channel : received response of code :: %d \n", channel_name, code); 

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
            printf("%s's channel : Response generated = %s\n",channel_name, temp);
            strcpy(data->response,temp);
            data->request_flag = 1;
            printf("%s's channel : Sent Response to client\n", channel_name);  
        }
         
    }
    // finding the inactive client
    int p;
    for(int i=0; i<CLIENTS_SERVICED; i++) {
        if(strcmp(clients_active[i].uid, channel_name)==0) {
            p=i;
        }
    }
    if(data->registered) {
        printf("%s's channel : Connection has been terminated but still registered\n", channel_name); 
        clients_active[p].inactive_reg_flag = 1;
        // not changing THREAD_STATE because memory still needs to be cleared if SIGINT occurs
    } 
    else {
        printf("%s's channel : Unregistered! Connection has been terminated and memory cleared\n", channel_name); 
        // unmapping memory and unlinking shared memory of this client
        munmap(data,sizeof(struct client_data));
        shm_unlink(channel_name);

        clients_active[p].inactive_reg_flag = 0;
        // also shifting the array so that space can be saved
        for(int i=p; i<CLIENTS_SERVICED-1; i++) {
            clients_active[i] = clients_active[i+1];
        }
        CLIENTS_SERVICED--;
        THREAD_STATE = 0;
    }
    //printf("%s's channel : Times Pinged : %d", channel_name, data->times_action_requested); 
    
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
    munmap(connect_data, SHM_SIZE);
    shm_unlink(SHM_NAME);
    exit(EXIT_SUCCESS);
}
