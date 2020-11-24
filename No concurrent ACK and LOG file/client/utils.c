#include "client.h"
#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 40

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

/* RAND_LIM 
 * @brief return a random number between 0 and limit inclusive.
 * @param limit: limit to the integer randomization
 * @return retval: random integer generated.
 */

unsigned long rand_lim(int limit) {

    unsigned long divisor = RAND_MAX/(limit+1);
    unsigned long retval;

    do { 
        retval = rand() / divisor;
    } while (retval > (unsigned long)limit);

    return retval;
}


void print_progress(double percentage, FILE* des) {
    int val = (int) (percentage * 100);             
    int lpad = (int) (percentage * PBWIDTH);        
    int rpad = PBWIDTH - lpad;
    char buff[MAXLINE];

    sprintf(buff, "\033[1;34m%3d%% [%.*s%*s]\033[0m\r", val, lpad, PBSTR, rpad, "");

    fwrite(buff, 1, strlen(buff), des);
}