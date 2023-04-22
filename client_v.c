#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

#define SHM_NAME "/myshm" 
#define SHM_SIZE sizeof(struct connect_data)

#define MAX_CLIENTS 10 
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

void action_menu()
{
    printf("****ACTION MENU****\nEnter any of the following numbers for desired operation\n");
    printf("0 : Simple Calculator\n");
    printf("1 : Even-Odd Checker\n");
    printf("2 : Check if Prime Number\n");
    printf("3 : Negative Number Detector\n");

    printf("7 : Print Menu Again\n");
    printf("8 : Terminate Session\n");
    printf("9 : UNREGISTER\n");
}
int resp_handler()
{
    printf("Are you sure?(y/n)");
    char resp; 
    scanf("%s", &resp); 
    putchar('\n'); 
    if(resp == 'y') {
        return 1;
    }
    return 0;
}


int main() {
    //CREATING CONNECTION SHM

    int connect_fd = shm_open(SHM_NAME, O_RDWR, S_IRUSR | S_IWUSR);
    if(connect_fd < 0) {
        perror("shm_open"); 
        return 1; 
    } else printf("Succesfully connected to 'connect' shm. fd :: %d\n", connect_fd); 
    
    if(ftruncate(connect_fd, SHM_SIZE) != 0) {
        perror("ftruncate"); 
        return 1; 
    }

    struct connect_data* connect_data = (struct connect_data*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, connect_fd, 0); 
    if(connect_data == MAP_FAILED) {
        perror("mmap"); 
        return 1; 
    }


    //REGISTRATION LOOP 
    int satisfied = 0; 
    char channel_name[MAX_USERNAME_LENGTH*2]; 
    while(!satisfied) {
        printf("Enter your username :: "); 
        char username[MAX_USERNAME_LENGTH]; 
        scanf("%s", username); 

        while(1) {
            int busy_server = pthread_rwlock_trywrlock(&connect_data->lock); 
            if(!busy_server) {
                //no other client is waiting for response
                //will not release rdlock until response
                strcpy(connect_data->request, username);
                printf("Registration request sent from user :: %s\n", username); 
                
                //wait for the response from server
                connect_data->served_registration_request = 0; 
                while(connect_data->served_registration_request == 0); 
                
                if(connect_data->response_code) {
                    satisfied = 0; 
                    printf("Username already in use\n"); 
                }
                else {
                    satisfied = 1; 
                    printf("Response 'ok' recieved from server\nChannel name :: %s\n", connect_data->response); 
                    strcpy(channel_name, connect_data->response); 
                }
                pthread_rwlock_unlock(&connect_data->lock); 
                break;
            }

        }
    }

    //CHANNEL COMMUNICATION LOOP
    int client_fd = shm_open(channel_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(client_fd < 0) {
        perror("shm_open"); 
        return 1; 
    } else printf("Succesfully connected to client shm. fd :: %d\n", client_fd); 

    if(ftruncate(client_fd, sizeof(struct client_data)) != 0) {
        perror("ftruncate"); 
        return 1; 
    }

    struct client_data* data = (struct client_data*) mmap(NULL, sizeof(struct client_data), PROT_READ | PROT_WRITE, MAP_SHARED, client_fd, 0);
    if(data == MAP_FAILED) {
        perror("mmap"); 
        return 1; 
    }

    printf("~Connection to server has been made\n"); 
    int loop_state = 0;

    while(1) {
        if(data->served_registration_request == 0) continue; 
        int sc;
        if(data->times_serviced == 0) {
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
                scanf("%d", &n1); putchar('\n');  
                printf("Enter Second Number : ");
                scanf( "%d", &n2); putchar('\n');  
                printf("Choose an operator : 0 - Addition, 1 - Subtraction, 2 - Multiplication, 3 - Division\n"); 
                scanf("%d", &op); putchar('\n'); 
                data->request.service_code = 0;
                data->request.n1 = n1; data->request.n2 = n2; data->request.op_type = op; 
                break; 
            case 1 : 
                printf("Current Operation : Even / Odd\n"); 
                int x; 
                printf("Number : "); 
                scanf("%d", &x); 
                putchar('\n'); 
                data->request.evenOdd = x; 
                data->request.service_code = 1;
                break; 
            case 2 : 
                printf("Current Operation : Is Prime\n"); 
                printf("Number : "); 
                scanf("%d", &x); 
                putchar('\n'); 
                data->request.isPrime = x; 
                data->request.service_code = 2;
                break; 
            case 3 : 
                printf("Current Operation : Is Negative \n"); 
                printf("Number : "); 
                scanf("%d", &x); 
                putchar('\n'); 
                data->request.isNegative = x; 
                data->request.service_code = 3;
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
        // conditions to check the last 3 options
        if(loop_state == 1)
        {
            loop_state = 0;
            continue;
        }
        else if(loop_state == 2)
        {
            printf("Stopping Client... \n");
            //data->active = 0;
            break;
        }
        else if(loop_state == 3)
        {
            printf("Stopping Client... \n");
            data->active = 0;
            printf("Terminating...\n"); 
            break;
        }

        data->times_serviced++; 
        data->served_registration_request = 0; 

        printf("Request sent to server\n"); 
        while(data->served_registration_request == 0) {
            usleep(500); //wait until response 
            printf("Waiting for response...\n");
        }
        printf("Received response from server!\n"); 

        switch (sc) {
            case 0: 
                printf("Answer : %d\n", data->response.ans); 
                break; 
            case 1: 
                printf("Even : %d | Odd : %d \n", data->response.even, data->response.odd); 
                break; 
            case 2: 
                printf("Is Prime : %d \n", data->response.isPrime); 
                break; 
            case 3: 
                printf("Is Negative : %d\n", data->response.isNegative); 
        }
    }
 
}