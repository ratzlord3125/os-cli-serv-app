/*
Team members:
Aditya Kumar
Aditya Seth
Ayush Mishra
Anmol Goyal
Puneet Agarwal
Kaushik Chetluri
*/

// run here : gcc A2_25_client.c -o A2_25_client -lpthread -lrt && ./A2_25_client

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
    int request_flag;
    int times_action_requested; 
    int active; // state to check if client thread active or not
    int registered;
};

struct connect_data* connect_data_ptr; // for shm of connect channel
struct client_data* client_data_ptr; // for shm of comm channel
void action_menu(); // for printing the menu
int resp_handler(); // for confirmation dialogues

void handle_signal(int sig); // handling SIGINT

int main() {
    printf("Client has been started!!\n");

    signal(SIGINT,handle_signal);

    // creating connect channel shared memory
    int connect_fd = shm_open(SHM_NAME, O_RDWR, S_IRUSR | S_IWUSR);
    if(connect_fd < 0) {
        perror("shm_open"); 
        exit(EXIT_FAILURE);
    } 
    else 
        printf("Connected to connect channel shared memory (fd = %d)\n", connect_fd); 
    
    if(ftruncate(connect_fd, SHM_SIZE) != 0) {
        perror("ftruncate"); 
        exit(EXIT_FAILURE);
    }

    // mmaping the data for the connect channel shared mem
    connect_data_ptr = (struct connect_data*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, connect_fd, 0); 
    if(connect_data_ptr == MAP_FAILED) {
        perror("mmap"); 
        exit(EXIT_FAILURE);
    }

    // uid registration loop starts here 
    int conn_flag = 0; 
    char client_channel_name[MAX_UID_LENGTH]; 
    while(!conn_flag) {
        printf("Enter your UID : "); 
        char uid[MAX_UID_LENGTH]; 
        scanf("%s", uid); 

        while(1) { // continuous attempt to wrlock
            int busy_server_flag = pthread_rwlock_trywrlock(&connect_data_ptr->lock); 

            if(!busy_server_flag) { // no other client waiting for response
                printf("Sending UID \"%s\" to server\n",uid);
                strcpy(connect_data_ptr->request, uid);
                printf("Sent registration request for \"%s\" from client\n", connect_data_ptr->request); 
                
                // wait for response from server
                connect_data_ptr->reg_flag = 0; 
                while(connect_data_ptr->reg_flag == 0) {
                    usleep(500000); // sleep 0.5 sec
                    printf("Attempting to register to server...\n");
                }
                
                // checking uid uniqueness
                if(connect_data_ptr->response_code == 0) { // new uid
                    conn_flag = 1; 
                    printf("New UID is registered successfully!\nRegistered channel name :: %s\n", connect_data_ptr->response); 
                    strcpy(client_channel_name, connect_data_ptr->response); 
                }
                else if(connect_data_ptr->response_code == 1) { // inactive uid
                    conn_flag = 1; 
                    printf("Existing UID is re-registered successfully!\nRegistered channel name :: %s\n", connect_data_ptr->response); 
                    strcpy(client_channel_name, connect_data_ptr->response); 
                }
                else if(connect_data_ptr->response_code == 2) { // active uid
                    conn_flag = 0; 
                    printf("Entered UID already active in other client :(\n"); 
                }
                // rwlock not release until response
                pthread_rwlock_unlock(&connect_data_ptr->lock); 
                break;
            }
        }
    }

    // communication with channel
    int cli_fd = shm_open(client_channel_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(cli_fd < 0) {
        perror("shm_open"); 
        exit(EXIT_FAILURE);
    } else printf("Connected to client shared memory (fd = %d)\n", cli_fd); 

    if(ftruncate(cli_fd, sizeof(struct client_data)) != 0) {
        perror("ftruncate"); 
        exit(EXIT_FAILURE);
    }

    client_data_ptr = (struct client_data*) mmap(NULL, sizeof(struct client_data), PROT_READ | PROT_WRITE, MAP_SHARED, cli_fd, 0);
    if(client_data_ptr == MAP_FAILED) {
        perror("mmap"); 
        return 1; 
    }

    printf("Connected to communication channel successfully\n"); 
    int loop_state = 0;

    while(1) {
        while(!client_data_ptr->request_flag) {
            usleep(500000);
            printf("Preparing for sending action requests...");
        }
        
        if(client_data_ptr->request_flag) {
            int sc;
            
            if(client_data_ptr->times_action_requested == 0) { // for first time service
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
                    printf("Choose an operator : 1-Addition | 2-Subtraction | 3-Multiplication | 4-Division\n"); 
                    scanf("%d", &op);
                    client_data_ptr->request.action_code = 0;
                    client_data_ptr->request.n1 = n1; 
                    client_data_ptr->request.n2 = n2; 
                    client_data_ptr->request.operator = op; 
                    break; 
                case 1 : 
                    printf("Current Operation : Even/Odd Checker\n"); 
                    int x; 
                    printf("Number : "); 
                    scanf("%d", &x); 
                    putchar('\n'); 
                    client_data_ptr->request.n1 = x; 
                    client_data_ptr->request.action_code = 1;
                    break; 
                case 2 : 
                    printf("Current Operation : Prime Checker\n"); 
                    printf("Number : "); 
                    scanf("%d", &x); 
                    putchar('\n'); 
                    client_data_ptr->request.n1 = x; 
                    client_data_ptr->request.action_code = 2;
                    break; 
                case 3 : 
                    printf("Current Operation : Negative Checker\n"); 
                    printf("Number : "); 
                    scanf("%d", &x); 
                    putchar('\n'); 
                    client_data_ptr->request.n1 = x; 
                    client_data_ptr->request.action_code = 3;
                    break; 
                case 6 : 
                    printf("Number of actions performed by this client = %d\n",client_data_ptr->times_action_requested+1);
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
                client_data_ptr->times_action_requested++; 
                continue;
            }
            else if(loop_state == 2) {
                client_data_ptr->active = 0;
                handle_signal(SIGINT);
                break;
            }
            else if(loop_state == 3) {
                client_data_ptr->registered = 0;
                client_data_ptr->active = 0;
                printf("Terminating and clearing all memory...\n"); 
                // memory clearing takes place in server
                handle_signal(SIGINT);
                break;
            }
        }

        client_data_ptr->request_flag = 0; 

        printf("Request sent to server\n"); 
        while(client_data_ptr->request_flag == 0) {
            usleep(500000); // wait until response is received
            printf("Waiting for response...\n");
        }
        printf("Received response from server\n"); 
        printf("%s\n",client_data_ptr->response);

        client_data_ptr->times_action_requested++; 
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
    if(client_data_ptr != NULL && client_data_ptr->active) {
        client_data_ptr->active = 0;
    }
    printf(" Exiting Client...\n");
    exit(EXIT_SUCCESS);
}