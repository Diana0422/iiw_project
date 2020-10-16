#include "server.h"

/* CREATE_PACKET
 * @brief Allocate space for a packet and fill fields with metadata.
 * @param type: message type; seq_num: sequence number of the packet; size: data size; data: pointer to data buffer
 * @return Packet*
 */
 
Packet* create_packet(unsigned long seq_num, unsigned long ack_num, size_t size, char* data, packet_type type)
{
	Packet* pack;
	
	pack = (Packet*)malloc(sizeof(Packet));
	if (pack == NULL) {
		fprintf(stderr, "Error: Malloc() packet #%lu.\n", seq_num);
		return NULL;
	}
	
	pack->seq_num = seq_num;
	pack->ack_num = ack_num;
	pack->data_size = size;

	memset(pack->data, 0, PAYLOAD);
	if(size == 0){
		strcpy(pack->data, "NULL");
	}else{
		memcpy(pack->data, data, size);
	}
	pack->type = type;

	return pack;
}

   
/* SERIALIZE_PACKET
 * @brief Serialize packet data into a buffer that can be sent through a socket
 * @param packet: packet to read
 * @return char* buffer: pointer to packet.
 */
    
char* serialize_packet(Packet* packet)
{
	char* buffer = (char*)malloc(MAX_DGRAM_SIZE);
	if(buffer == NULL){
		perror("Malloc failed");
		exit(EXIT_FAILURE);
	}
	memset(buffer, 0, MAX_DGRAM_SIZE);

	sprintf(buffer, "%lu %lu %ld %d ", packet->seq_num, packet->ack_num, packet->data_size, (int)packet->type);

	if(packet->data_size){
		memcpy(buffer+strlen(buffer), packet->data, packet->data_size);
	}else{
		strcat(buffer, packet->data);
	}

	return buffer;
}


/* UNSERIALIZE_PACKET
 * @brief Unserialize data from a memory area into a packet (struct)
 * @param buffer: pointer to source memory area, packet: pointer to destination packet.
 * @return Packet* packet: pointer to packet.
 */

Packet unserialize_packet(char* buffer)
{
	char seq[20], ack[20], size_str[7], type_str[2];
	int type, len;
	
	Packet* pk = (Packet*)malloc(sizeof(Packet));
	if (pk == NULL) {
		fprintf(stderr, "Error: couldn't malloc packet.\n");
		exit(EXIT_FAILURE);
	}

	sscanf(buffer, "%s %s %s %s ", seq, ack, size_str, type_str);
	len = strlen(seq)+strlen(ack)+strlen(size_str)+strlen(type_str)+4;

	pk->seq_num = (unsigned long)atol(seq);
	pk->ack_num = (unsigned long)atol(ack);
	pk->data_size = (size_t)(atoi(size_str));
	type = atoi(type_str);
	pk->type = (packet_type)type;

	memset(pk->data, 0, pk->data_size);
	if(pk->data_size){
		//memmove(buffer, buffer+len, pk->data_size);
		memcpy(pk->data, buffer+len, pk->data_size);
	}else{
		strcpy(pk->data, buffer+len);
	}

	return *pk;
}
    

/* SEND_PACKET
 * @brief Sends a struct through socket;
 * @param pkt = pointer to struct to send, socket = socket descriptor, addr = destination address, addrlen = destination address length
 * @return 1 = error, n = ok
 */
 
 int send_packet(Packet *pkt, int socket, struct sockaddr* addr, socklen_t addrlen, Timeout* t)
 {
 	
 	char* buffer = (char*)malloc(MAX_DGRAM_SIZE);
 	if(buffer == NULL){
 		perror("Malloc failed");
 		exit(EXIT_FAILURE);
 	}
 	memset(buffer, 0, MAX_DGRAM_SIZE);

 	int n;
 	//AUDIT
	//print_packet(*pkt);

 	memcpy(buffer, serialize_packet(pkt), MAX_DGRAM_SIZE);

	gettimeofday(&t->start, NULL);
	//printf("STARTING TIME COUNTER FOR SAMPLE RTT:\n");
	
	// Set socket option for transmission timer
	//setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &t->interval, sizeof(t->interval));
	//printf("TRANSMISSION TIMER: %ld secs and %ld usecs.\n", t->interval.tv_sec, t->interval.tv_usec);
	
 	if ((n = sendto(socket, buffer, MAX_DGRAM_SIZE, 0, (struct sockaddr*)addr, addrlen)) == -1) {
 		printf("\033[0;31mTRANSMISSION TIMEOUT: max wait time reached.\033[0m\n");
 		return -1;
	}

    return n;
 }
 

/* RECV_PACKET
 * @brief Receive a packet/struct through a socket
 * @param pkt: pointer to packet, socket: socket descriptor, addr: pointer to address struct, addlen: size of addr
 * @return error: -1, success: n
 */
 
 int recv_packet(Packet *pkt, int socket, struct sockaddr* addr, socklen_t addrlen, Timeout* t) 
 {
 	char buffer[MAX_DGRAM_SIZE];
 	int n;
 	
 	memset(buffer, 0, MAX_DGRAM_SIZE);
 	
 	if ((n = recvfrom(socket, buffer, MAX_DGRAM_SIZE, 0, (struct sockaddr*)addr, &addrlen)) == -1) {
 		return -1;
 	} else {
 		//printf("ENDING TIME COUNTER FOR SAMPLE RTT.\n");		
 		*pkt = unserialize_packet(buffer);
 		if(pkt->type != SYN){
 			gettimeofday(&t->end, NULL);
 			//timeout_interval(t);
 		}

 		//print_packet(*pkt);
	} 
	
	return n;
 }


/* PRINT_PACKET 
 * @brief printf the individual fields of a packet.
 * @param packet to print
 */
//USED FOR DEBUGGING

void print_packet(Packet pk){
	char type_str[7];
	int type_int = (int)(pk.type);

	printf("--PACKET--\n");

	switch(type_int){
		case 0:
			strcpy(type_str, "DATA");
			break;
		case 1:
			strcpy(type_str,"ACK");
			break;
		case 2:
			strcpy(type_str,"SYN");
			break;
		case 3:
			strcpy(type_str,"SYNACK");
			break;
		case 4:
			strcpy(type_str,"FIN");
			break;
		case 5:
			strcpy(type_str,"FINACK");
			break;
	}

	printf("Type: %s\nSeq: %lu\nAck: %lu\nData size: %ld\n\n", type_str, pk.seq_num, pk.ack_num, pk.data_size);
	/*int i, count = 1;
	for (i = 0; i < (int)pk.data_size; i++){
	    printf("%d: %02X\n", count, pk.data[i]);
	    count++;
	}
	printf("\n\n");*/
}

/* ORDER_BUFFER 
 * @brief store a packet in the correct position in the buffer
 * @param pk: packet to store;
 		  buff: pointer to the receive buffer
 		  pos: position in the buffer which contains a packet whose sequence number is bigger that the packet to store
 */

void order_buffer(Packet* pk, Packet* buff[], int pos){
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

/* STORE_PCK 
 * @brief store a packet out of order in the receive buffer and check for buffer overflow.
 * @param pk: packet to store;
 		  buff: pointer to the receive buffer
 		  size: total size of the buffer
   @return free space in the buffer; -1 for buffer overflow
 */

int store_pck(Packet* pk, Packet* buff[], int size) {

	int free, i=0;

	//If the buffer is empty, store the packet as first
	if(buff[i] == NULL){
		buff[i] = pk;
		free = size-1;
		return free;
	}

	//If the buffer is full, return an error
	if(buff[size-1] != NULL){
		return -1;
	}

	//In all other cases: store the packet in order
	while(buff[i] != NULL){
		if(pk->seq_num < buff[i]->seq_num){
			order_buffer(pk, buff, i);
			free = size-i-1;
			return free;
		}
	}

	buff[i] = pk;
	free = size-i-1;
	return free;
}

/* CHECK_BUFFER
 * @brief check if the receive buffer contains data in order with the last packet arrived and, if so, read the data and reorder the buffer
 * @param pk: packet to store;
 		  buff: pointer to the receive buffer
 */

int check_buffer(Packet* pk, Packet* buff[]){
	int i = 0;
	int res = 0;
	unsigned long seq_num = (pk->seq_num + (unsigned long)pk->data_size + 1);

	if(buff[0] == NULL){
		return res;
	}

	//Count the buffered packets that can be read
	while(seq_num == buff[i]->seq_num){
		res++;
		seq_num += buff[i]->data_size+1;
		i++;
	}

	return res;
}
