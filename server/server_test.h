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

typedef struct node{
	struct sockaddr_in addr;
	char buff[MAXLINE];
	int server;
	struct node* next;
}Client;

extern void buf_clear(char*, int);

extern void insert_client(Client**, struct sockaddr_in, char*);

extern void get_client(Client**, int, struct sockaddr_in*, char*);

extern void remove_client(Client**, int);

extern int dispatch_client(Client*, struct sockaddr_in, int*);

#endif /* client_test_h */
