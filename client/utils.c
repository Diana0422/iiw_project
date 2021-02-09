#include "client.h"
#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 40

/* CHECK_FAILURE
 * @brief checks if an error occured because of an interruption of a syscall
 * @param msg: error message
 * @return 1 if error caused by EINTR
 */

int check_failure(const char* msg){
	if(errno == EINTR){
        return 1;
    }else{
        failure(msg);
    }

    return 0;
}

/* FAILURE
 * @brief error handler
 * @param msg: error message
 * @return exit code
 */

int failure(const char* msg){
	fprintf(stderr, "%s", msg);
	exit(-1);
}

/* LOSS_WITH_PROB
 * @brief simulates a packet loss with given probability
 * @param prob: probability
 * @return 0 if packet is lost, else 1
 */

int loss_with_prob(int prob){
    int random = rand();

    if(!(random % prob)){
        return 1;
    }

    return 0;
}

/* RAND_LIM 
 * @brief return a random number between 0 and limit inclusive.
 * @param limit: limit to the integer randomization
 * @return retval: random integer generated.
 */

unsigned long rand_lim(int limit) {

    unsigned long divisor = RAND_MAX/(limit+1);
    unsigned long retval;

    do { 
        retval = rand() / divisor;
    } while (retval > (unsigned long)limit);

    return retval;
}

/* PRINT_PROGRESS
 * @brief prints the progress bar;
 * @param percentage: percentage of the file transmitted;
          des: log file descriptor.
 */

void print_progress(double percentage, FILE* des) {
    int val = (int) (percentage * 100);             
    int lpad = (int) (percentage * PBWIDTH);        
    int rpad = PBWIDTH - lpad;
    char buff[MAXLINE];


    sprintf(buff, "%3d%% [%.*s%*s]\r", val, lpad, PBSTR, rpad, "");

    fwrite(buff, 1, strlen(buff), des);
}

/* INSERT_THREAD
 * @brief insert a new thread in the queue;
 * @param head: pointer to the head node of the queue;
 * @return pointer to the thread node struct
 */

Thread_node* insert_thread(Thread_node** head){
    Thread_node *prev;
    Thread_node *curr;
    Thread_node *new;

    if((new = (Thread_node*)malloc(sizeof(Thread_node))) == NULL){
        perror("Malloc() failed");
        exit(-1);
    }

    new->tid = 0;
    new->next = NULL;

    prev = NULL;
    curr = *head;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
    }

    if(prev == NULL){
        new->next = *head;
        *head = new;
    }else{
        prev->next = new;
        new->next = curr;
    }
    
    return new;
}

/* INSERT_SOCKET
 * @brief insert a new socket in the queue related to a new request
 * @param head: pointer to the head node of the queue;
 * @return pointer to the socket node struct
 */

Socket_node* insert_socket(Socket_node** head){
    Socket_node *prev;
    Socket_node *curr;
    Socket_node *new;

    if((new = (Socket_node*)malloc(sizeof(Socket_node))) == NULL){
        perror("Malloc() failed");
        exit(-1);
    }

    new->sockfd = 0;
    new->next = NULL;

    prev = NULL;
    curr = *head;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
    }

    if(prev == NULL){
        new->next = *head;
        *head = new;
    }else{
        prev->next = new;
        new->next = curr;
    }
    
    return new;
}