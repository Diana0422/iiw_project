//
//  client_test.h
//  
//
//  Created by Diana Pasquali on 20/07/2020.
//
#pragma once

#ifndef client_test_h
#define client_test_h
#define MAXLINE 1024
#define MAX_DGRAM_SIZE 65505

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>

typedef struct node{
	struct sockaddr_in addr;
	char buff[MAX_DGRAM_SIZE];
	int server;
	struct node* next;
}Client;

typedef struct message {
	char* type;
	int seq;
	size_t data_size;
	char* data;
}Packet;

extern void buf_clear(char*, int);

extern void insert_client(Client**, struct sockaddr_in, char*);

extern void get_client(Client**, int, struct sockaddr_in*, char*);

extern void remove_client(Client**, int);

extern int dispatch_client(Client*, struct sockaddr_in, int*);

extern void clean_thread(Client**, struct sockaddr_in*, pthread_mutex_t*, int);

extern void print_list(Client*);

extern int filelist_ctrl(char*);


extern Packet* create_packet(char*, int, size_t, char*);

extern char* serialize_packet(char*, Packet*, int*);

extern Packet* unserialize_packet(char*, Packet*);

extern int send_packet(Packet*, int, struct sockaddr*, socklen_t, int*);

extern int recv_packet(Packet*, int, struct sockaddr*, socklen_t);

extern char* read_file(FILE*, char*, size_t*);

extern FILE* write_file(FILE*, char*, char*, size_t);

#endif /* client_test_h */
