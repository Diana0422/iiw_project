#include "client.h"

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

	//printf("packet %p\n", packet);
	
	/*printf("seq_num %lu\n", packet->seq_num);
	printf("ack_num %lu\n", packet->ack_num);
	printf("data_size %ld\n", packet->data_size);
	printf("type %d\n", (int)packet->type);*/

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

 	//printf("Sending packet #%lu\n", pkt->seq_num);

 	memcpy(buffer, serialize_packet(pkt), MAX_DGRAM_SIZE);
 	
	gettimeofday(&t->start, NULL);
	
 	if ((n = sendto(socket, buffer, MAX_DGRAM_SIZE, 0, (struct sockaddr*)addr, addrlen)) == -1) {
 		printf("\033[0;31mTRANSMISSION TIMEOUT: max wait time reached.\033[0m\n");
 		return -1;
	}

	//printf("Packet #%lu correctly sent\n", pkt->seq_num);

    return n;
 }

 /* SEND_ACK
 * @brief Sends an ACK through socket;
 * @param 
 * @return 
 */
 
 int send_ack(int socket, struct sockaddr_in addr, socklen_t addrlen, unsigned long seq, unsigned long ack, Timeout* to)
 {
 	Packet* pack;

 	//printf("Sending ACK #%lu.\n", ack);
    pack = create_packet(seq, ack, 0, NULL, ACK);
    if (send_packet(pack, socket, (struct sockaddr*)&addr, addrlen, to) == -1) {
        fprintf(stderr, "Error: couldn't send ack #%lu.\n", pack->ack_num);
        free(pack);
        return 1;
    } 
    //printf("ACK #%lu correctly sent.\n", pack->ack_num);
    free(pack); 

    return 0;
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

 	while ((n = recvfrom(socket, buffer, MAX_DGRAM_SIZE, 0, (struct sockaddr*)addr, &addrlen)) == -1) {
 		if(check_failure("Error: couldn't receive packet\n")){
            continue;
        }
 	} 

	*pkt = unserialize_packet(buffer);
 	gettimeofday(&t->end, NULL);
	
	return n;
 }

 /* TRY_RECV_PACKET
 * @brief Non blocking version of recv_packet
 */
 
 int try_recv_packet(Packet *pkt, int socket, struct sockaddr* addr, socklen_t addrlen, Timeout* t) 
 {
 	char buffer[MAX_DGRAM_SIZE];
 	int n;
 	
 	//memset(buffer, 0, MAX_DGRAM_SIZE);

 	while ((n = recvfrom(socket, buffer, MAX_DGRAM_SIZE, MSG_DONTWAIT, (struct sockaddr*)addr, &addrlen)) == -1) {
 		if(errno == EWOULDBLOCK){
            return -1;
        }else{
        	if(check_failure("Error: couldn't receive packet\n")){
	            continue;
	        }
        }
 	} 

	*pkt = unserialize_packet(buffer);
 	gettimeofday(&t->end, NULL);
	
	return n;
 }

//DEBUG

/* PRINT_PACKET 
 * @brief printf the individual fields of a packet.
 * @param packet to print
 */

/*void print_packet(Packet pk){
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
	int i, count = 1;
	for (i = 0; i < (int)pk.data_size; i++){
	    printf("%d: %02X\n", count, pk.data[i]);
	    count++;
	}
	printf("\n\n");
}*/