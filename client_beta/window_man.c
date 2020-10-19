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
 * @brief discard every packet sent that has been acknowledged, considering the last ack received, and slide the others still "in flight"
 		  at the initial positions
 * @param list: transmission window;
 		  index: index in the thread's transmission window of the acknowledged packet;
 		  free_space: usable window size
 */

void refresh_window(Packet** list, int index, short* free_space){
	int count = 0;
	int i = 0;

	if(index == INIT_WND_SIZE){
		memset(list, 0, INIT_WND_SIZE*sizeof(Packet*));
	}else{
		while((index+i < INIT_WND_SIZE) && (list[index+i] != NULL)){
			list[i] = list[index+i];
			memset(&list[index+i], 0, sizeof(Packet*));
			count++;
			i++;
		}

		while((i < INIT_WND_SIZE) && (list[i] != NULL)){
			memset(&list[i], 0, sizeof(Packet*));
			i++;
		}
	}

	*free_space = INIT_WND_SIZE - count;

	//print_wnd(list);
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