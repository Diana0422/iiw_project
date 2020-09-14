#include "server_test.h"

#define THREAD_POOL 10

void buf_clear(char *buffer, int dim)
{
    for (int i=0; i<dim; i++) buffer[i] = '\0';
}

/*void insert_client(Client **h, struct sockaddr_in cli_addr, char* message, size_t mess_size){
    
    Client *new;
    if((new = (Client*)malloc(sizeof(Client))) == NULL){
    	perror("Malloc() failed");
    	exit(-1);
    }
    
    Client *prev;
    Client *curr;

    new->addr = cli_addr;
    //strcpy(new->buff, message);
    memcpy(new->buff, message, mess_size);
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
}*/	


void insert_client(Client **h, struct sockaddr_in cli_addr, Packet* packet){
    
    Client *new;
    if((new = (Client*)malloc(sizeof(Client))) == NULL){
    	perror("Malloc() failed");
    	exit(-1);
    }
    
    if ((new->pack = (Packet*)malloc(sizeof(Packet))) == NULL) {
    	perror("Malloc() failed.\n");
    	exit(-1);
    }
    
    printf("Insert client.\n");
    
    printf("--PACKET #%d: \n", packet->seq);
    printf("  type:      %s \n", packet->type);
    printf("  seq:       %d  \n", packet->seq);
    printf("  data_size: %ld  \n", packet->data_size);
    printf("  data:      %s  \n\n\n", packet->data);
    
    Client *prev;
    Client *curr;

    new->addr = cli_addr;
    //strcpy(new->buff, message);
    /*memcpy(new->buff, message, mess_size);*/
    new->pack->seq = packet->seq;
    /*new->pack.type = (char*)malloc(strlen(packet->type)+1);
    if (new->pack.type == NULL) {
    	fprintf(stderr, "Error: couldn't malloc new->pack.type.\n");
    	exit(-1);
    }*/
    strcpy(new->pack->type, packet->type);
    new->pack->data_size = packet->data_size;
    /*new->pack.data = (char*)malloc(packet->data_size);
    if (new->pack.data == NULL) {
    	fprintf(stderr, "Error: couldn't malloc new->pack.data.\n");
    	exit(-1);
    }*/
    memcpy(new->pack->data, packet->data, packet->data_size);
    
    printf("Packet copied in client info.\n");
    printf("--PACKET #%d: \n", new->pack->seq);
    printf("  type:      %s \n", new->pack->type);
    printf("  seq:       %d  \n", new->pack->seq);
    printf("  data_size: %ld  \n", new->pack->data_size);
    printf("  data:      %s  \n\n\n", new->pack->data);
    
    memcpy(new->buff, packet->data, packet->data_size);
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


/*void get_client(Client **h, int thread, struct sockaddr_in *address, char* message, size_t size){
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
            //memcpy(message, prev->buff, size);

            printf("Client acquired: ");
            print_list(*h);
            
            return;
        }
    }
}
*/


void get_client(Client **h, int thread, struct sockaddr_in *address, Packet* packet){
    Client *prev;
    Client *curr;

    prev = NULL;
    curr = *h;
    
    printf("Get client.\n");

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
        if(prev->server < 0){
            prev->server = thread;
            *address = prev->addr;
            
            /*printf("--PACKET #%d: \n", prev->pack->seq);
    	    printf("  type:      %s \n", prev->pack->type);
            printf("  seq:       %d  \n", prev->pack->seq);
            printf("  data_size: %ld  \n", prev->pack->data_size);
            printf("  data:      %s  \n\n\n", prev->pack->data);*/
            
            memset(packet->type, 0, 5);
            strcpy(packet->type, prev->pack->type);
            packet->seq = prev->pack->seq;
            packet->data_size = prev->pack->data_size;
            memset(packet->data, 0, PAYLOAD);
            memcpy(packet->data, prev->pack->data, packet->data_size);
            
            /*printf("--PACKET #%d: \n", packet->seq);
    	    printf("  type:      %s \n", packet->type);
            printf("  seq:       %d  \n", packet->seq);
            printf("  data_size: %ld  \n", packet->data_size);
            printf("  data:      %s  \n\n\n", packet->data);
            //memcpy(message, prev->buff, size);*/

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
	    
	    free(curr->pack);
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
