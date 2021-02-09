#include "server.h"

/* INSERT_CLIENT 
 * @brief Insert a client that has completed the handshaking into the client list.
 * @param h: head of the client list
          cli_addr: address of the new client
          packet: last packet received from the client during the handshake
 */

void insert_client(Client **h, struct sockaddr_in cli_addr, Packet* packet, Timeout to){
    
    Client *prev;
    Client *curr;
    Client *new;

    if((new = (Client*)malloc(sizeof(Client))) == NULL){
    	perror("Malloc() failed");
    	exit(-1);
    }
    
    if ((new->pack = (Packet*)malloc(sizeof(Packet))) == NULL) {
    	perror("Malloc() failed.\n");
    	exit(-1);
    }

    if ((new->rtx_pack = (Packet*)malloc(sizeof(Packet))) == NULL) {
        perror("Malloc() failed.\n");
    	exit(-1);
    }

    new->addr = cli_addr;
    new->pack = packet;     
    new->to_info = to; 
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

/* GET_CLIENT 
 * @brief get a client to serve in the list 
 * @param h: pointer to the head of the client list
          thread: id of the server thread
          client_info: pointer to the client struct of the client to acquire
 */

void get_client(Client **h, int thread, Client** client_info){

    Client *node = *h;

    while(node != NULL){
        if(node->server < 0){
            node->server = thread;
            *client_info = node;

            printf("New client acquired.\n");
            
            return;
        }
        node = node->next;
    }
}

/* REMOVE_CLIENT 
 * @brief remove a client from the list 
 * @param h: pointer to the head of the client list
          addr: socket address of the client
 */

void remove_client(Client **h, struct sockaddr_in addr){

    Client *prev;
    Client *curr;

    prev = NULL;
    curr = *h;

    while(curr != NULL){
        if((addr.sin_addr.s_addr == (curr->addr).sin_addr.s_addr) && (addr.sin_port == (curr->addr).sin_port)){
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
}

/* DISPATCH_CLIENT 
 * @brief dispatch new messages from the clients to the right server thread
 * @param h: pointer to the head of the client list
          address: socket address of the client
          server: pointer to the variable that will contain the server thread id
 */

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

