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

void update_packet(Client* h, int thread, Packet *pk, Timeout to){
    Client *prev;
    Client *curr;

    prev = NULL;
    curr = h;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
        if(thread == (prev->server)) {
            prev->pack = pk;
            prev->to_info = to;
            break;
        }
    }
}

/* incoming_ack 
 * @brief react to an ack reception: 
                find the packet that the ack acknowledges;
                check if there are still in flight packets and, if so, restart the timer and refresh the window;
                if there are no more packets in flight, match the ack with the packet stop the timer and recompute the timeout interval
 * @param thread: id of the thread that has received the ack;
          ack: ack packet;
          list: transmission windows;
          lock: transmission window mutex;
          free_space: usable window size
          ack_to: timeout structure in the ack packet;
          timerid: id of the timer related to the thread;
          thread_to: timeout structure of the thread:
 */

void incoming_ack(int thread, Packet *ack, Packet* list[][INIT_WND_SIZE], short* free_space, Timeout ack_to, timer_t timerid, Timeout* thread_to){
    int i = 0;

    for(i=0; i<(INIT_WND_SIZE - *free_space); i++){
        if(list[thread][i]->seq_num == ack->ack_num){
            printf("Received ACK related to packet #%lu.\n", list[thread][i]->seq_num);
            break;
        }
    }

    if(i == 0){
        //print_wnd(list[thread]);
    }else{
        disarm_timer(timerid);
        refresh_window(list, thread, i, free_space);
        if(list[thread][0] != NULL){           
            arm_timer(thread_to, timerid, 0);
        }else{ 
            thread_to->end = ack_to.end;
            timeout_interval(thread_to);     
        }
    }
}
