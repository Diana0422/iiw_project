//PACKET.H
//
//Header file related to the definition of a packet and the prototypes of the functions concerning packet management
//

#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PAYLOAD 1024

typedef enum type {DATA, ACK, SYN, SYNACK, FIN, FINACK} packet_type;

typedef struct message {
	int seq_num;
	int ack_num;
	size_t data_size;
	char data[PAYLOAD+1];
	packet_type type;
}Packet;

extern Packet* create_packet(int, int, size_t, char*, packet_type);

extern char* serialize_packet(Packet*);

extern Packet unserialize_packet(char*);

extern int send_packet(Packet*, int, struct sockaddr*, socklen_t);

extern int recv_packet(Packet*, int, struct sockaddr*, socklen_t);

extern int rand_lim(int);

extern int handshake(Packet*, int*, int, struct sockaddr_in*, socklen_t);

extern void print_packet(Packet);

extern int check_buffer(Packet*, Packet**);

extern int store_pck(Packet*, Packet**, int); 