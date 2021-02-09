#include "server.h"

/* UPDATE_WINDOW 
 * @brief add a new transmitted packet to the transmission window
 * @param sent: transmitted packet;
 		  list: transmission windows;
 		  pos: id of the thread that has transmitted the packet;
 		  free_space: pointer to the variable containing the space left in the window;
 */

void update_window(Packet* sent, Packet* list[][INIT_WND_SIZE], int pos, short* free_space){

	list[pos][INIT_WND_SIZE - *free_space] = sent;
	*free_space = *free_space - 1;
}

/* REFRESH_WINDOW 
 * @brief discard every packet sent that has been acknowledged and slide the window
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


/* INCOMING_ACK 
 * @brief react to an ack reception;
 * @param thread: server thread id;
 		  cli: client that sent the ack;
 		  ack: pointer to the ack;
 		  list: receive windows;
 		  free_space: space left in the receive buffer;
 		  ack_to: Timeout struct associated with the ack;
 		  timerid: id of the timer;
 		  thread_to: Timeout struct associated with the thread
 */

void incoming_ack(int thread, Client* cli, Packet *ack, Packet* list[][INIT_WND_SIZE], short* free_space, Timeout ack_to, timer_t timerid, Timeout* thread_to){
    int i = 0;

    //Find the packet in the transmission window that the ACK received ackowledges
    for(i=0; i<(INIT_WND_SIZE - *free_space); i++){
        if(list[thread][i]->seq_num == ack->ack_num){
            break;
        }
    }

    //Fast retransmission handling
    if (ack->ack_num != cli->last_ack_received) {
        cli->last_ack_received = ack->ack_num;
        cli->ack_counter = 0;
    } else {
        cli->ack_counter++;
        if (cli->ack_counter == 3) {
            cli->ack_counter = 0;   // reset ack counter for this client
            disarm_timer(timerid);
            retransmission(&timerid, true, thread);   //retransmit
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