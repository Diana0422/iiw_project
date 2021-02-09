#include "client.h"

/* UPDATE_WINDOW 
 * @brief add a new transmitted packet to the transmission window
 * @param sent: transmitted packet;
 		  list: transmission window;
 		  free_space: pointer to the variable containing the space left in the window;
 */

void update_window(Packet* sent, Packet** list, short* free_space){

	list[INIT_WND_SIZE - *free_space] = sent;
	*free_space = *free_space - 1;
}

/* REFRESH_WINDOW 
 * @brief discard every packet sent that has been acknowledged and slide the window
 * @param wnd: transmission window;
 		  index: index of the acknowledged packet;
 		  free_space: pointer to the variable containing the space left in the window;
 */

void refresh_window(Packet** wnd, int index, short* free_space){
	int count = 0;
	int i = 0;

	//Slide the window till the end or the last transmitted packet
	while((index+i < INIT_WND_SIZE) && (wnd[index+i] != NULL)){
		wnd[i] = wnd[index+i];
		memset(&wnd[index+i], 0, sizeof(Packet*));
		count++;
		i++;
	}

	while((i < INIT_WND_SIZE) && (wnd[i] != NULL)){
		memset(&wnd[i], 0, sizeof(Packet*));
		i++;
	}

	*free_space = INIT_WND_SIZE - count;
}

/* ORDER_RWND 
 * @brief store a packet in the correct position in the buffer
 * @param pk: packet to store;
 		  buff: pointer to the receive buffer
 		  pos: position in the buffer which contains a packet whose sequence number is bigger that the packet to store
 */

void order_rwnd(Packet* pk, Packet* buff[], int pos){
	int i = 1;
	Packet* temp;


	//Slide the buffer from pos to the end to make room for the new packet
	while(buff[pos+i] != NULL){
		temp = buff[pos+1];
		buff[pos+i] = buff[pos];
		buff[pos] = temp;
		i++;
	}
	buff[pos+i] = buff[pos];

	//Fill the gap
	buff[pos] = pk;
}

/* STORE_RWND 
 * @brief store a packet out of order in the receive buffer and check for buffer overflow.
 * @param pk: packet to store;
 		  buff: pointer to the receive buffer
 		  size: total size of the buffer
 */

void store_rwnd(Packet* pk, Packet* buff[], int size) {

	int i=0;

	if(buff[i] == NULL){ 
		//If the buffer is empty, store the packet as first	
		buff[i] = pk;
		
	}else if(buff[size-1] != NULL){		
		//If the buffer is full don't do anything

	}else{
		//In all other cases: store the packet in order									
		while(buff[i] != NULL){
			if(pk->seq_num < buff[i]->seq_num){
				order_rwnd(pk, buff, i);
			}
			i++;
		}
		buff[i] = pk;
	}
}

/* INSERT_BASE 
 * @brief insert the first packet in the base of the transmission window
 * @param head: pointer to the head of the window
 * @return pointer to the new packet
 */

Packet_node* insert_base(Packet_node** head){
	Packet_node *prev;
    Packet_node *curr;
    Packet_node *new;

    if((new = (Packet_node*)malloc(sizeof(Packet_node))) == NULL){
        perror("Malloc() failed");
        exit(-1);
    }

    if ((new->pk = (Packet*)malloc(sizeof(Packet))) == NULL) {
    	perror("Malloc() failed.\n");
    	exit(-1);
    }

    memset(new->pk, 0, sizeof(Packet));
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

/* INCOMING_ACK 
 * @brief react to an ack reception;
 * @param ack_num: ack number of the received packet;
          num_dup: pointer to the variable containing the number of duplicated ack that were received;
          last_received: pointer to the variable containing the ack number of the last received ack;
          wnd: pointer to the head node of the window;
          free_space: space left in the receive buffer;
          ack_to: timeout structure in the ack packet;
          timer: timer_node related to the transmission;
 */

void incoming_ack(unsigned long ack_num, int* num_dup, unsigned long* last_received, Packet** wnd, short* free_space, Timeout ack_to, Timer_node* timer)
{
    int i; 

    //Find the packet in the transmission window that the ACK received ackowledges
    for(i=0; i<(INIT_WND_SIZE - *free_space); i++){
        if(wnd[i]->seq_num == ack_num){
            break;
        }
    }

    //Fast retransmission handling
    if (ack_num != *last_received) {
        //New ACK received
        *last_received = ack_num;
        *num_dup = 0;

    } else {
    	//Duplicated ACK
        *num_dup += 1;
        //Check for the number of duplicates
        if (*num_dup == 3) {
            *num_dup = 0; 
            disarm_timer(timer->timerid);  
            retransmission(&(timer->timerid));
        }
    } 

    if(i != 0){
    	disarm_timer(timer->timerid);  
	    refresh_window(wnd, i, free_space);
	    if(wnd[0] != NULL){           
	        arm_timer(timer, 0);
		}else{
			(timer->to).end = ack_to.end;
	        timeout_interval(&(timer->to));
		}
	} 
}