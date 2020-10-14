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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>
#include <math.h>
#include <signal.h>

#include "packet.h"

#define MAX_DGRAM_SIZE 65507
#define THREAD_POOL	10

//extern void print_timeout(Timeout*);

typedef struct node{
	struct sockaddr_in addr;   //Client's address: used for contact
	Packet* pack;			   //Packet received from the client
	int server;				   //ID of the thread serving the client
	struct node* next;		   //Pointer to the next client in the list 
	Timeout to_info;           // Timeout values structure
}Client;

extern void insert_client(Client**, struct sockaddr_in, Packet*, Timeout);

extern void get_client(Client**, int, Client**);

extern void remove_client(Client**, struct sockaddr_in);

extern void dispatch_client(Client*, struct sockaddr_in, int*);

extern void update_packet(Client*, int, Packet*, Timeout);

extern void print_list(Client*);

extern int check_failure(const char*);
	
extern int failure(const char*);

extern unsigned long rand_lim(int);

extern char* read_file(FILE*, char*, size_t*);
 
extern FILE* write_file(FILE*, char*, char*, size_t);

#endif /* client_test_h */
