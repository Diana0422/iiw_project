//
//  client_test.h
//  
//
//  Created by Diana Pasquali on 20/07/2020.
//
#define pragma once

#ifndef client_test_h
#define client_test_h

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#define MAXLINE 5036
#define MAX_DGRAM_SIZE 65505
#define PAYLOAD 65400

typedef struct message {
	char type[5];
	int seq;
	size_t data_size;
	char data[PAYLOAD];
}Packet;

#endif /* client_test_h */

int check_failure(const char*);

int failure(const char*);

void buf_clear(char*);

extern Packet* create_packet(char*, int, size_t, char*);

extern char* serialize_packet(char*, Packet*, int*);

extern Packet* unserialize_packet(char*, Packet*);

extern int send_packet(Packet*, int, struct sockaddr*, socklen_t, int*);

extern int recv_packet(Packet*, int, struct sockaddr*, socklen_t);

extern char* read_file(FILE*, char*, size_t*);

extern FILE* write_file(FILE*, char*, char*, size_t);

