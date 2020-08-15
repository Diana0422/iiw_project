#include "server_test.h"

void buf_clear(char *buffer)
{
    for (int i=0; i<MAXLINE; i++) buffer[i] = '\0';
}

void insert_node(Client **h, struct sockaddr_in cli_addr, char* message){
    
    Client *new;
    if((new = (Client*)malloc(sizeof(Client))) == NULL){
    	perror("Malloc() failed");
    	exit(-1);
    }
    
    Client *prev;
    Client *curr;

    new->addr = cli_addr;
    strcpy(new->buff, message);
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

void get_and_remove(Client **h, struct sockaddr_in *address, char* message){

    Client *del = *h;
    Client *temp;

    *address = (*h)->addr;

    strcpy(message, (*h)->buff);
    
    temp = del->next;
    free(del);

   	h = &temp;
}