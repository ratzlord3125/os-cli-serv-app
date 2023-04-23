// gcc client_v.c -o client_v -pthread -lrt && ./client_v

/*
--> remove more plag
--> run simultaneous clients and check functionality
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

#define SHM_NAME "/myshm" 
#define SHM_SIZE sizeof(struct connect_data)

#define MAX_CLIENTS 10 
#define MAX_UID_LENGTH 100 
#define MAX_STRING_LENGTH 10000

// struct for connect channel
struct connect_data { 
    pthread_rwlock_t lock; 
    int response_code; //0 - ok | 1 - uid in use | 2 - uid too long
    int reg_flag; 
    char request[MAX_UID_LENGTH]; 
    char response[MAX_UID_LENGTH*2]; 
}; 

// struct for request in comm channel of each client
struct action_request { 
    int action_code; //0 - operation | 1 - even or odd | 2 - is prime | 3 - is negative 
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

struct connect_data* connect_data; // for shm of connect channel
struct client_data* data; // for shm of comm channel
void action_menu(); // for printing the menu
int resp_handler(); // for confirmation dialogues
void handle_signal(int sig); // handling sigint

int main() {
    signal(SIGINT,handle_signal);

    //CREATING CONNECTION SHM
    int connect_fd = shm_open(SHM_NAME, O_RDWR, S_IRUSR | S_IWUSR);
    if(connect_fd < 0) {
        perror("shm_open"); 
        return 1; 
    } 
    else 
        printf("Connected to connect channel shared memory (fd = %d)\n", connect_fd); 
    
    if(ftruncate(connect_fd, SHM_SIZE) != 0) {
        perror("ftruncate"); 
        return 1; 
    }

    // mmaping the data for the connect channel shared mem
    connect_data = (struct connect_data*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, connect_fd, 0); 
    if(connect_data == MAP_FAILED) {
        perror("mmap"); 
        return 1; 
    }

    // uid registration loop starts here 
    int conn_flag = 0; 
    char channel_name[MAX_UID_LENGTH]; 
    while(!conn_flag) {
        printf("Enter your UID : "); 
        char uid[MAX_UID_LENGTH]; 
        scanf("%s", uid); 

        while(1) { // continuous attempt to wrlock
            int busy_server_flag = pthread_rwlock_trywrlock(&connect_data->lock); 

            if(!busy_server_flag) { // no other client waiting for response
                strcpy(connect_data->request, uid);
                printf("Sent registration request for \"%s\" from client\n", uid); 
                
                // wait for response from server
                connect_data->reg_flag = 0; 
                while(connect_data->reg_flag == 0) {
                    usleep(10000);
                    printf("Attempting to register to server...\n");
                }
                
                // checking uid uniqueness
                if(connect_data->response_code == 2) {
                    conn_flag = 0; 
                    printf("Entered UID already active in other client :(\n"); 
                }
                else if(connect_data->response_code == 1) {
                    conn_flag = 1; 
                    printf("Existing UID is reregistered successfully!\nRegistered channel name :: %s\n", connect_data->response); 
                    strcpy(channel_name, connect_data->response); 
                }
                else {
                    conn_flag = 1; 
                    printf("New UID is registered successfully!\nRegistered channel name :: %s\n", connect_data->response); 
                    strcpy(channel_name, connect_data->response); 
                }
                // wrlock not release until response
                pthread_rwlock_unlock(&connect_data->lock); 
                break;
            }
        }
    }

    // communication with channel
    int cli_fd = shm_open(channel_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(cli_fd < 0) {
        perror("shm_open"); 
        return 1; 
    } else printf("Connected to client shared memory (fd = %d)\n", cli_fd); 

    if(ftruncate(cli_fd, sizeof(struct client_data)) != 0) {
        perror("ftruncate"); 
        return 1; 
    }

    data = (struct client_data*) mmap(NULL, sizeof(struct client_data), PROT_READ | PROT_WRITE, MAP_SHARED, cli_fd, 0);
    if(data == MAP_FAILED) {
        perror("mmap"); 
        return 1; 
    }

    printf("Connected to communication channel successfully\n"); 
    int loop_state = 0;

    while(1) {
        while(!data->request_flag) {
            usleep(10000);
            printf("Preparing for sending action requests...");
        }
        
        int sc;
        
        if(data->times_action_requested == 0) { // for first time service
            action_menu();
            printf("Enter your choice : \n");
        }
        else 
            printf("Enter your next choice (7 for menu) : \n");
        
        scanf("%d", &sc); 

        switch(sc) {
            case 0 : 
                printf("Current Operation : Calculator\n");
                int n1, n2, op; 
                printf("Enter First Number : ");
                scanf("%d", &n1);
                printf("Enter Second Number : ");
                scanf("%d", &n2);
                printf("Choose an operator : 1 - Addition, 2 - Subtraction, 3 - Multiplication, 4 - Division\n"); 
                scanf("%d", &op);
                data->request.action_code = 0;
                data->request.n1 = n1; 
                data->request.n2 = n2; 
                data->request.operator = op; 
                break; 
            case 1 : 
                printf("Current Operation : Even/Odd Checker\n"); 
                int x; 
                printf("Number : "); 
                scanf("%d", &x); 
                putchar('\n'); 
                data->request.n1 = x; 
                data->request.action_code = 1;
                break; 
            case 2 : 
                printf("Current Operation : Prime Checker\n"); 
                printf("Number : "); 
                scanf("%d", &x); 
                putchar('\n'); 
                data->request.n1 = x; 
                data->request.action_code = 2;
                break; 
            case 3 : 
                printf("Current Operation : Negative Checker\n"); 
                printf("Number : "); 
                scanf("%d", &x); 
                putchar('\n'); 
                data->request.n1 = x; 
                data->request.action_code = 3;
                break; 
            case 6 : 
                printf("Number of actions performed by this client = %d\n",data->times_action_requested+1);
                loop_state = 1;
                break;
            case 7 : 
                action_menu();
                loop_state = 1;
                break;
            case 8 : 
                loop_state = resp_handler()?2:1;
                break;
            case 9 :
                loop_state = resp_handler()?3:1;
                break;
            default : 
                printf("Invalid Choice...\n");
                loop_state = 1;
        }

        // conditions to execute the client specific options
        if(loop_state == 1) {
            loop_state = 0;
            data->times_action_requested++; 
            continue;
        }
        else if(loop_state == 2) {
            data->active = 0;
            handle_signal(SIGINT);
            break;
        }
        else if(loop_state == 3) {
            data->registered = 0;
            data->active = 0;
            printf("Terminating and clearing all memory...\n"); 
            // memory clearing takes place in server
            handle_signal(SIGINT);
            break;
        }

        data->times_action_requested++; 
        data->request_flag = 0; 

        printf("Request sent to server\n"); 
        while(data->request_flag == 0) {
            usleep(10000); //wait until response 
            printf("Waiting for response...\n");
        }
        printf("Received response from server\n"); 
        printf("%s\n",data->response);
    } 
}

void action_menu()
{
    printf("****ACTION MENU****\nEnter any of the following numbers for desired operation\n");
    printf("0 : Simple Calculator\n");
    printf("1 : Even-Odd Checker\n");
    printf("2 : Check if Prime Number\n");
    printf("3 : Negative Number Detector\n");
    printf("6 : Print number of actions performed\n");
    printf("7 : Print Menu Again\n");
    printf("8 : Terminate Session\n");
    printf("9 : UNREGISTER\n");
}

int resp_handler()
{
    printf("Are you sure (y/n)? ");
    char resp; 
    scanf("%s", &resp); 
    putchar('\n'); 
    if(resp == 'y') {
        return 1;
    }
    return 0;
}

void handle_signal(int sig) {
    // ctrl+c should terminate the client but not unregister an active client
    if(data != NULL && data->active) {
        data->active = 0;
    }
    printf(" Exiting Client...\n");
    exit(EXIT_SUCCESS);
}