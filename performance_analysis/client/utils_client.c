#include "client_test.h"

int check_failure(const char* msg){
	if(errno == EINTR){
        return 1;
    }else{
        failure(msg);
    }

    return 0;
}

int failure(const char* msg){
	fprintf(stderr, "%s", msg);
	exit(-1);
}

void buf_clear(char *buffer)
{
    for (int i=0; i<MAXLINE; i++) buffer[i] = '\0';
}