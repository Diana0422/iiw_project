#include "server.h"

//DEBUG
void print_wnd(Packet* list[]){
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
	for(i=0; i<INIT_WND_SIZE-1; i++){
		if(rwnd[i] != NULL){
			printf("%lu, ", rwnd[i]->seq_num);
		}else{
			printf("NULL, ");
		}
		
	}
	if(rwnd[INIT_WND_SIZE-1] != NULL){
		printf("%lu}\n", rwnd[INIT_WND_SIZE-1]->seq_num);
	}else{
		printf("NULL}\n");
	}
}

/* UPDATE_WINDOW 
 * @brief add a new transmitted packet to the transmission window
 * @param sent: transmitted packet;
 		  list: transmission windows;
 		  pos: id of the thread that has transmitted the packet;
 */

void update_window(Packet* sent, Packet* list[][INIT_WND_SIZE], int pos, short* free_space){

	list[pos][INIT_WND_SIZE - *free_space] = sent;
	*free_space = *free_space - 1;
	//print_wnd(list[pos]);
}

/* REFRESH_WINDOW 
 * @brief discard every packet sent that has been acknowledged considering the last ack received and slide the others still "in flight"
 		  at the initial positions
 * @param list: transmission windows;
 		  pos: id of the thread that has received the ack;
 		  index: index in the thread's transmission window of the acknowledged packet;
 		  free_space: usable window size
 */

void refresh_window(Packet* list[][INIT_WND_SIZE], int pos, int index, short* free_space){
	int count = 0;
	int i = 0;

	while((index+i < INIT_WND_SIZE) && (list[pos][index+i] != NULL)){
		list[pos][i] = list[pos][index+i];
		memset(&list[pos][index+i], 0, sizeof(Packet*));
		count++;
		i++;
	}

	while((i < INIT_WND_SIZE) && (list[pos][i] != NULL)){
		memset(&list[pos][i], 0, sizeof(Packet*));
		i++;
	}

	*free_space = INIT_WND_SIZE - count;

	//print_wnd(list[pos]);
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

	if(buff[i] == NULL){ 	//If the buffer is empty, store the packet as first
		buff[i] = pk;
		
	}else if(buff[size-1] != NULL){		//If the buffer is full don't do anything
		printf("Buffer overflow\n");
	}else{									//In all other cases: store the packet in order
		while(buff[i] != NULL){
			if(pk->seq_num < buff[i]->seq_num){
				order_rwnd(pk, buff, i);
			}
			i++;
		}

		buff[i] = pk;
	}

	print_rwnd(buff);
}

/* INCOMING ACK 
 * @brief react to an ack reception: 
                find the packet that the ack acknowledges;
                check if there are still in flight packets and, if so, restart the timer and refresh the window;
                if there are no more packets in flight, match the ack with the packet stop the timer and recompute the timeout interval
 * @param thread: id of the thread that has received the ack;
          ack: ack packet;
          list: transmission windows;
          free_space: usable window size
          ack_to: timeout structure in the ack packet;
          timerid: id of the timer related to the thread;
          thread_to: timeout structure of the thread:
 */

void incoming_ack(int thread, Client* cli, Packet *ack, Packet* list[][INIT_WND_SIZE], short* free_space, Timeout ack_to, timer_t timerid, Timeout* thread_to){
    int i = 0;

    //Find the packet in the transmission window that the ACK received ackowledges
    for(i=0; i<(INIT_WND_SIZE - *free_space); i++){
        if(list[thread][i]->seq_num == ack->ack_num){
            //printf("Received ACK related to packet #%lu.\n", list[thread][i]->seq_num);
            break;
        }
    }

    //Fast retransmission handling
    //printf("\033[1;32mFAST RETRANSMISSION HANDLING STARTS.\033[0m\n");
    if (ack->ack_num != cli->last_ack_received) {
        //printf("\033[1;32mNew ack received.\033[0m");
        cli->last_ack_received = ack->ack_num;
        cli->ack_counter = 0;
    } else {
        cli->ack_counter++;
        if (cli->ack_counter == 3) {
            cli->ack_counter = 0;   // reset ack counter for this client
            //printf("\033[1;32mFast retransmitting packet #%lu\n", list[thread][i]->seq_num);
            disarm_timer(timerid);
            retransmission(&timerid, true, thread);   //retransmit
            printf("Fast retransmitted packet #%lu\n", list[thread][i]->seq_num);
        }
    }

    if(i != 0){
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