#include "server.h"

#define DELIMITER "|"

/* CREATE_PACKET
 * @brief Allocate space for a packet and fill fields with metadata.
 * @param type: message type; seq_num: sequence number of the packet; size: data size; data: pointer to data buffer
 * @return Packet*
 */
 
Packet* create_packet(int seq_num, int ack_num, size_t size, char* data, packet_type type)
{
	Packet* pack;
	
	pack = (Packet*)malloc(sizeof(Packet));
	if (pack == NULL) {
		fprintf(stderr, "Error: Malloc() packet #%d.\n", seq_num);
		return NULL;
	}
	
	pack->seq_num = seq_num;
	pack->ack_num = ack_num;
	pack->data_size = size;
	memset(pack->data, 0, PAYLOAD);
	if(size == 0){
		strcpy(pack->data, "NULL");
	}else{
		strcpy(pack->data, data);
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
	char *buffer = (char*)malloc(MAX_DGRAM_SIZE);
	if(buffer == NULL){
		perror("Malloc failed.");
		exit(EXIT_FAILURE);
	}

	char seq[7], ack[7], size[7], type_str[2];

	sprintf(seq, "%d", packet->seq_num);
	sprintf(ack, "%d", packet->ack_num);
	sprintf(size, "%ld", packet->data_size);
	sprintf(type_str, "%d", (int)packet->type);

	strcat(buffer, seq);
	strcat(buffer, DELIMITER);

	strcat(buffer, ack);
	strcat(buffer, DELIMITER);

	strcat(buffer, size);
	strcat(buffer, DELIMITER);

	strcat(buffer, packet->data);
	strcat(buffer, DELIMITER);

	strcat(buffer, type_str);
	strcat(buffer, DELIMITER);

	return buffer;
}


/* UNSERIALIZE_PACKET
 * @brief Unserialize data from a memory area into a packet (struct)
 * @param buffer: pointer to source memory area, packet: pointer to destination packet.
 * @return Packet* packet: pointer to packet.
 */

Packet unserialize_packet(char* buffer)
{
	char token[MAX_DGRAM_SIZE];
	int size, type;
	
	Packet* pk = (Packet*)malloc(sizeof(Packet));
	if (pk == NULL) {
		fprintf(stderr, "Error: couldn't malloc packet.\n");
		exit(EXIT_FAILURE);
	}

	strcpy(token, strtok(buffer, DELIMITER));
	pk->seq_num = atoi(token);
	memset(token, 0, strlen(token));

	strcpy(token, strtok(NULL, DELIMITER));
	pk->ack_num = atoi(token);
	memset(token, 0, strlen(token));

	strcpy(token, strtok(NULL, DELIMITER));
	size = atoi(token);
	pk->data_size = (size_t)size;
	memset(token, 0, strlen(token));

	strcpy(token, strtok(NULL, DELIMITER));
	strcpy(pk->data, token);
	memset(token, 0, strlen(token));

	strcpy(token, strtok(NULL, DELIMITER));
	type = atoi(token);
	pk->type = (packet_type)type;

	return *pk;
}
    

/* SEND_PACKET
 * @brief Sends a struct through socket;
 * @param pkt = pointer to struct to send, socket = socket descriptor, addr = destination address, addrlen = destination address length
 * @return 1 = error, n = ok
 */
 
 int send_packet(Packet *pkt, int socket, struct sockaddr* addr, socklen_t addrlen)
 {
 	char buffer[MAX_DGRAM_SIZE];
 	int n;
 	
 	//AUDIT
	print_packet(*pkt);

 	strcpy(buffer, serialize_packet(pkt));
 	
 	if ((n = sendto(socket, buffer, strlen(buffer), 0, (struct sockaddr*)addr, addrlen)) == -1) {
 		return -1;
	}

    return n;
 }
 

/* RECV_PACKET
 * @brief Receive a packet/struct through a socket
 * @param pkt: pointer to packet, socket: socket descriptor, addr: pointer to address struct, addlen: size of addr
 * @return error: -1, success: n
 */
 
 int recv_packet(Packet *pkt, int socket, struct sockaddr* addr, socklen_t addrlen) 
 {
 	char buffer[MAX_DGRAM_SIZE];
 	int n;
 	
 	memset(buffer, 0, MAX_DGRAM_SIZE);
 	if ((n = recvfrom(socket, buffer, MAX_DGRAM_SIZE, 0, (struct sockaddr*)addr, &addrlen)) == -1) {
 		return -1;
 	} else {
 		*pkt = unserialize_packet(buffer);

 		//AUDIT
		print_packet(*pkt);

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

	printf("Type: %s\nSeq: %d\nAck: %d\nData size: %ld\nData: %s\n\n", type_str, pk.seq_num, pk.ack_num, pk.data_size, pk.data);
	/*int i;
	for (i = 0; i < PAYLOAD; i++){
		if(pk.data[i] == '\0'){
			break;
		}
	    printf("%02X", pk.data[i]);
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
//USED ALSO IN FLOW CONTROL: retrieve the dimension of the free space in the buffer and send it to the server! (NEED TO ADD ANOTHER FIELD IN THE PACKET)

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
	int seq_num = (pk->seq_num + pk->data_size + 1);

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