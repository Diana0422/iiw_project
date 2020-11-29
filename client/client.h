//
//  client.h
//  
//
//  Created by Diana Pasquali on 20/07/2020.
//

#define pragma once

#ifndef client_h
#define client_h

#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>

#include "windows.h"

#define MAXLINE 1024
#define MAX_DGRAM_SIZE 65507
#define MAX_CONC_REQ 3

typedef struct thread_node{
	pthread_t tid;
	struct thread_node* next;
}Thread_node;

typedef struct sock_node{
	int sockfd;
	struct sock_node* next;
}Socket_node;

extern unsigned long rand_lim(int limit);

extern Thread_node* insert_thread(Thread_node**);

extern Socket_node* insert_socket(Socket_node**);

extern void print_progress(double, FILE*);

extern int loss_with_prob(int);
 
#endif /* client_h */

