//  Created by Diana Pasquali on 20/07/2020.
//
#define pragma once

#ifndef client_h
#define client_h
//#define MAXLINE 1024

#include <sys/sem.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>

#include "windows.h"

#define MAX_DGRAM_SIZE 65507
#define THREAD_POOL	10

//extern void print_timeout(Timeout*);

typedef struct node{
	struct sockaddr_in addr;   //Client's address: used for contact
	Packet* pack;			   //Packet received from the client
	Packet* rtx_pack;
	int server;				   //ID of the thread serving the client
	Timeout to_info;           // Timeout values structure
	int ack_counter;
	unsigned long last_ack_received;
	struct node* next;		   //Pointer to the next client in the list 
}Client;

extern void insert_client(Client**, struct sockaddr_in, Packet*, Timeout);

extern void get_client(Client**, int, Client**);

extern void remove_client(Client**, struct sockaddr_in);

extern void dispatch_client(Client*, struct sockaddr_in, int*);

extern void print_list(Client*);

extern unsigned long rand_lim(int);

extern int loss_with_prob(int);

extern void incoming_ack(int, Client*, Packet*, Packet*(*)[INIT_WND_SIZE], short*, Timeout, timer_t, Timeout*);

extern void update_packet(Client*, unsigned long, int, Packet*, Timeout);

#endif /* client_test_h */
