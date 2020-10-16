#include "server.h"

/* INSERT_CLIENT 
 * @brief Insert a client that has completed the handshaking into the client list.
 * @param h: head of the client list
          cli_addr: address of the new client
          packet: last packet received from the client during the handshake
 */

void insert_client(Client **h, struct sockaddr_in cli_addr, Packet* packet, Timeout to){
    
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
    
    new->to_info = to;  // Copy timeout info in client data
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

void incoming_ack(int thread, Packet *ack, Packet** list, pthread_mutex_t* lock, Timeout ack_to, timer_t timerid, Timeout* thread_to){
    int i = 0;
    
    //Accedi a list[thread][] e scorri fino a trovare il pacchetto tale per cui seq_num+data_size = ack->ack_num - 1;
    while(&list[thread][i] != NULL){
        if((list[thread][i].seq_num + (int)list[thread][i].data_size) == (ack->ack_num - 1)){
            break;
        }
        i++;
    }

    pthread_mutex_lock(lock);
    //Se vi sono altri pacchetti più nuovi in list[thread][] restart timer, else stop timer
    if(&list[thread][i+1] != NULL){
        arm_timer(thread_to, timerid, 0);
        refresh_window(list[thread], i);
    }else{
        //The ACK is related to the last packet sent: thread_to contains the start time and ack_to contains the end time, so a new RTT can be computed
        thread_to->end = ack_to.end;
        timeout_interval(thread_to);
        disarm_timer(timerid);
    }
    pthread_mutex_unlock(lock);
}
