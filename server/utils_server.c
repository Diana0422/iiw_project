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
            return;
        }
    }
}

void remove_client(Client **h, int thread){

    Client *prev;
    Client *curr;
    Client *temp;

    prev = NULL;
    curr = *h;

    while(curr != NULL){
        if(curr->server == thread){
            if(prev == NULL){
                temp = curr->next;
                h = &temp;
            }else{
                prev->next = curr->next;
            }

            free(curr);
            break;
        }
    }
}

int dispatch_client(Client* h, struct sockaddr_in address, int* server){

    Client *prev;
    Client *curr;

    int count = 0;

    prev = NULL;
    curr = h;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
        if(address.sin_addr.s_addr == (prev->addr).sin_addr.s_addr){
            *server = count;
            return 1;
        }
        count++;
    }

    return 0;
}