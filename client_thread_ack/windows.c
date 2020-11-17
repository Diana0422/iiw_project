#include "client.h"

//DEBUG

void print_wnd(Packet** list){
	int i;
	printf("\nTransmission window: {");
	for(i=0; i<INIT_WND_SIZE-1; i++){
		if(list[i] != NULL){
			printf("%lu, ", list[i]->seq_num);
		}else{
			printf("NULL, ");
		}
		
	}
	if(list[INIT_WND_SIZE-1] != NULL){
		printf("%lu}\n\n", list[INIT_WND_SIZE-1]->seq_num);
	}else{
		printf("NULL}\n\n");
	}
	
}

void print_rwnd(Packet** rwnd){
	int i;
	printf("\nReceive window: {");
	for(i=0; i<MAX_RVWD_SIZE-1; i++){
		if(rwnd[i] != NULL){
			printf("%lu, ", rwnd[i]->seq_num);
		}else{
			printf("NULL, ");
		}
		
	}
	if(rwnd[MAX_RVWD_SIZE-1] != NULL){
		printf("%lu}\n\n", rwnd[MAX_RVWD_SIZE-1]->seq_num);
	}else{
		printf("NULL}\n\n");
	}
}

/* UPDATE_WINDOW 
 * @brief add a new transmitted packet to the transmission window
 * @param sent: transmitted packet;
 		  list: transmission window;
 */

void update_window(Packet* sent, Packet** list, short* free_space){

	list[INIT_WND_SIZE - *free_space] = sent;
	*free_space = *free_space - 1;
	//print_wnd(list);
}

/* REFRESH_WINDOW 
 * @brief discard every packet sent that has been acknowledged and slide the window
 * @param wnd: transmission window;
 		  index: index of the acknowledged packet;
 		  free_space: usable window size;
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
	//print_wnd(wnd);
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

	//print_rwnd(buff);
}

/* STORE_RWND 
 * @brief store a packet out of order in the receive buffer and check for buffer overflow.
 * @param pk: packet to store;
 		  buff: pointer to the receive buffer
 		  size: total size of the buffer
 */

void store_rwnd(Packet* pk, Packet* buff[], int size) {

	int i=0;

	if(buff[i] == NULL){ 	//If the buffer is empty, store the packet as first
		buff[i] = pk;
		
	}else if(buff[size-1] != NULL){		//If the buffer is full don't do anything
		//printf("Buffer overflow\n");
	}else{									//In all other cases: store the packet in order
		while(buff[i] != NULL){
			if(pk->seq_num < buff[i]->seq_num){
				order_rwnd(pk, buff, i);
			}
			i++;
		}
		buff[i] = pk;
	}

	//print_rwnd(buff);
}

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
 * @brief react to an ack reception: 
                find the packet that the ack acknowledges;
                check if there are still in flight packets and, if so, restart the timer and refresh the window;
                if there are no more packets in flight, match the ack with the packet stop the timer and recompute the timeout interval
 * @param 
          ack: ack packet;
          base: transmission window;
          free_space: usable window size
          ack_to: timeout structure in the ack packet;
 */

void incoming_ack(unsigned long ack_num, int* num_dup, unsigned long* last_received, Packet** base, short* free_space, Timeout ack_to, Timer_node* timer)
{
    int i; 

    //Find the packet in the transmission window that the ACK received ackowledges
    for(i=0; i<(INIT_WND_SIZE - *free_space); i++){
        if(base[i]->seq_num == ack_num){
            break;
        }
    }

    //Fast retransmission handling
    if (ack_num != *last_received) {
        //New ACK received
        *last_received = ack_num;
        *num_dup = 0;

        if(i != 0){
		    refresh_window(base, i, free_space);
		    if(base == NULL){           
		        (timer->to).end = ack_to.end;
		        timeout_interval(&(timer->to));
			}
		}

    } else {
    	//Duplicated ACK
        *num_dup += 1;
        //Check for the number of duplicates
        if (*num_dup == 3) {
            *num_dup = 0;   
            retransmission(&(timer->timerid));
        }
    }  
}