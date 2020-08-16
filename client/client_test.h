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

#define MAXLINE 5036

#endif /* client_test_h */

int check_failure(const char*);

int failure(const char*);

void buf_clear(char*);

