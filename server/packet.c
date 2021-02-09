#include "server.h"

#define LOSS_PROB 50

/* CREATE_PACKET
 * @brief Allocate space for a packet and fill fields with metadata.
 * @param seq_num: sequence number of the packet;
 	      ack_num: ack number of the packet; 
 	      size: data size; 
 	      data: pointer to data buffer;
 	      type: message type; 
 * @return pointer to the new packet
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
 * @param packet: pointer to the packet to read;
 * @return char* buffer: pointer to the serialized packet.
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
 * @param buffer: pointer to source memory area;
 * @return unserialized packet.
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
		memcpy(pk->data, buffer+len, pk->data_size);
	}else{
		strcpy(pk->data, buffer+len);
	}

	return *pk;
}
    

/* SEND_PACKET
 * @brief Sends a packet through socket;
 * @param pkt = pointer to the packet to send;
 		  socket = socket descriptor;
 		  addr = destination address;
 		  addrlen = destination address length;
 		  t = pointer to the timeout struct assigned to the packet;
 * @return -1 = error, else ok
 */
 
 int send_packet(Packet *pkt, int socket, struct sockaddr* addr, socklen_t addrlen, Timeout* t)
 {
 	
 	char buffer[MAX_DGRAM_SIZE];
 	int n;
 	
 	memset(buffer, 0, MAX_DGRAM_SIZE);
 	memcpy(buffer, serialize_packet(pkt), MAX_DGRAM_SIZE);

	gettimeofday(&t->start, NULL);

	if(loss_with_prob(LOSS_PROB)){
		n = 1;
	}else{
	 	if ((n = sendto(socket, buffer, MAX_DGRAM_SIZE, MSG_DONTWAIT, (struct sockaddr*)addr, addrlen)) == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				return -1;
			}

			fprintf(stderr, "\033[0;31mCouldn't send packet.\033[0m\n");
	 		return -1;
		}
	}

    return n;
 }
 
/* SEND_ACK
 * @brief Sends an ACK packet through socket;
 * @param socket = socket descriptor;
 		  addr = destination address;
 		  addrlen = destination address length;
 		  seq = sequence number of the ack;
 		  ack = ack number of the ack;
 		  to = pointer to the timeout struct assigned to the ack;
 * @return 1 for error, 0 for success
 */
 
 int send_ack(int socket, struct sockaddr_in addr, socklen_t addrlen, unsigned long seq, unsigned long ack, Timeout* to)
 {
 	Packet* pack;

    pack = create_packet(seq, ack, 0, NULL, ACK);
    if (send_packet(pack, socket, (struct sockaddr*)&addr, addrlen, to) == -1) {
        fprintf(stderr, "Error: couldn't send ack #%lu.\n", pack->ack_num);
        free(pack);
        return 1;
    } 
    
    free(pack); 

    return 0;
 }

/* RECV_PACKET
 * @brief Receive a packet through a socket
 * @param pkt: pointer to packet; 
 		  socket: socket descriptor;
 		  addr: pointer to address struct;
 		  addlen: size of addr
 		  t = pointer to the timeout struct assigned to the packet;
 * @return error: -1, success: n
 */
 
 int recv_packet(Packet *pkt, int socket, struct sockaddr* addr, socklen_t addrlen, Timeout* t) 
 {
 	char buffer[MAX_DGRAM_SIZE];
 	int n;
 	
 	memset(buffer, 0, MAX_DGRAM_SIZE);
 	
 	while ((n = recvfrom(socket, buffer, MAX_DGRAM_SIZE, 0, (struct sockaddr*)addr, &addrlen)) == -1) {
 		if(errno == EINTR){
	        continue;
	    }else{
	        return -1;
	    }
 	}

 	*pkt = unserialize_packet(buffer);
	if(pkt->type != SYN){
		gettimeofday(&t->end, NULL);
	}
	
	return n;
 }

 /* UPDATE_PACKET
 * @brief Update the packet stored into the client node in the list of clients.
 * @param h: head of the list; 
          thread: worker thread dedicated to the client; 
          pk: new packet to upload;
 * @return Packet*
 */

void update_packet(Client* h, unsigned long lost_id, int thread, Packet *pk, Timeout to){
    Client *prev;
    Client *curr;

    prev = NULL;
    curr = h;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
        if(thread == (prev->server)) {
            if (lost_id == pk->seq_num) {
                prev->rtx_pack = pk;

            } else {
                prev->pack = pk;
            }
            prev->to_info = to;
            break;
        }
    }
}