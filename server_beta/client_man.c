#include "server.h"

#define THREAD_POOL 10


/* INSERT_CLIENT 
 * @brief Insert a client that has completed the handshaking into the client list.
 * @param h: head of the client list
          cli_addr: address of the new client
          packet: last packet received from the client during the handshake
 */

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
    
    //printf("Insert client.\n"); 
    //print_packet(*packet);
    
    Client *prev;
    Client *curr;

    new->addr = cli_addr;
    new->pack = packet; 

    //printf("Packet copied in client info.\n");
    //print_packet(*pack);
    
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
    
    //AUDIT
    printf("Client inserted: ");
    print_list(*h);
}

void get_client(Client **h, int thread, Client** client_info){

    Client *node = *h;

    while(node != NULL){
        if(node->server < 0){
            node->server = thread;
            *client_info = node;

            //AUDIT
            printf("Client acquired: ");
            print_list(*h);
            
            return;
        }
        node = node->next;
    }
}


void remove_client(Client **h, int thread, pthread_mutex_t* mux){

    Client *prev;
    Client *curr;

    prev = NULL;
    curr = *h;

    pthread_mutex_lock(mux);
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
    pthread_mutex_unlock(mux);
    
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

void dispatch_client(Client* h, struct sockaddr_in address, int* server){

    Client *prev;
    Client *curr;

    prev = NULL;
    curr = h;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
        if((address.sin_addr.s_addr == (prev->addr).sin_addr.s_addr) && (address.sin_port == (prev->addr).sin_port)) {
            *server = prev->server;
            break;
        }
    }
}

/* UPDATE_PACKET
 * @brief Update the packet stored into the client node in the list of clients.
 * @param h: head of the list; 
          thread: worker thread dedicated to the client; 
          pk: new packet to upload;
 * @return Packet*
 */

void update_packet(Client* h, int thread, Packet* pk){
    Client *prev;
    Client *curr;

    prev = NULL;
    curr = h;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
        if(thread == (prev->server)) {
            prev->pack = pk;
            break;
        }
    }
}
