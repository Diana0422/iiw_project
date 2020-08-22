#include "server_test.h"

#define THREAD_POOL 10

void buf_clear(char *buffer, int dim)
{
    for (int i=0; i<dim; i++) buffer[i] = '\0';
}

void insert_client(Client **h, struct sockaddr_in cli_addr, char* message){
    
    Client *new;
    if((new = (Client*)malloc(sizeof(Client))) == NULL){
    	perror("Malloc() failed");
    	exit(-1);
    }
    
    Client *prev;
    Client *curr;

    new->addr = cli_addr;
    strcpy(new->buff, message);
    new->server = -1;
    new->next = NULL;

    prev = NULL;
    curr = *h;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
    }

    if(prev == NULL){
        new->next = *h;
        *h = new;
    }else{
        prev->next = new;
        new->next = curr;
    }
    
    printf("Client inserted: ");
    print_list(*h);
}	

void get_client(Client **h, int thread, struct sockaddr_in *address, char* message){
    Client *prev;
    Client *curr;

    prev = NULL;
    curr = *h;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
        if(prev->server < 0){
            prev->server = thread;
            *address = prev->addr;
            strcpy(message, prev->buff);

            printf("Client acquired: ");
            print_list(*h);
            
            return;
        }
    }
}

void remove_client(Client **h, int thread){

    Client *prev;
    Client *curr;

    prev = NULL;
    curr = *h;

    while(curr != NULL){
        if(curr->server == thread){
            if(prev == NULL){
                *h = curr->next;
            }else{
                prev->next = curr->next;
            }

            free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    
    printf("Client removed: ");
    print_list(*h);
    printf("\n");
}

void print_list(Client *h){
    printf("[");
    while(h != NULL){
        printf("T%d -> ", h->server);
        h = h->next;
    }
    printf("NULL]\n");
}

int dispatch_client(Client* h, struct sockaddr_in address, int* server){

    Client *prev;
    Client *curr;

    prev = NULL;
    curr = h;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
        if((address.sin_addr.s_addr == (prev->addr).sin_addr.s_addr) && (address.sin_port == (prev->addr).sin_port)) {
            *server = prev->server;
            return 1;
        }
    }

    return 0;
}

void clean_thread(Client **h, struct sockaddr_in *address, pthread_mutex_t* mux, int thread){

    printf("cleaning thread %d.\n", thread);

    pthread_mutex_lock(mux);
    remove_client(h, thread);
    pthread_mutex_unlock(mux);

    
    memset((void*)address, 0, sizeof(*address));
    
    printf("thread %d clean.\n", thread);
}
