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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include "packet.h"

#define MAXLINE 5036
#define MAX_DGRAM_SIZE 65505

extern int check_failure(const char*);

extern int failure(const char*);

extern int rand_lim(int limit);

extern char* read_file(FILE*, char*, size_t*);

extern FILE* write_file(FILE*, char*, char*, size_t);
 
#endif /* client_h */

