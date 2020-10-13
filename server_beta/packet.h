//PACKET.H
//
//Header file related to the definition of a packet and the prototypes of the functions concerning packet management
//

#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "server.h"

#define PAYLOAD 65467

#define ALPHA 0.125 // for timeout
#define BETA 0.25

typedef enum type {DATA, ACK, SYN, SYNACK, FIN, FINACK} packet_type;

typedef struct message {
	unsigned long seq_num;
	unsigned long ack_num;
	size_t data_size;
	char data[PAYLOAD+1];
	packet_type type;
}Packet;


typedef struct timeout_info {
	double estimated_rtt;
	double dev_rtt;
	struct timeval start;
	struct timeval end;
	struct timeval interval;
} Timeout;

extern struct timeval timeout_interval(Timeout*);

extern Packet* create_packet(unsigned long, unsigned long, size_t, char*, packet_type);

extern char* serialize_packet(Packet*);

extern Packet unserialize_packet(char*);

extern int send_packet(Packet*, int, struct sockaddr*, socklen_t, Timeout*);

extern int recv_packet(Packet*, int, struct sockaddr*, socklen_t, Timeout*);

extern int handshake(Packet*, unsigned long*, int, struct sockaddr_in*, socklen_t, Timeout*);

extern int demolition(int, struct sockaddr_in*, socklen_t, Timeout*);

extern void print_packet(Packet);

extern int check_buffer(Packet*, Packet**);

extern int store_pck(Packet*, Packet**, int); 
