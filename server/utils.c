#include "server.h"

/* CHECK_FAILURE
 * @brief checks if an error occured because of an interruption of a syscall
 * @param msg: error message
 * @return 1 if error caused by EINTR
 */

int check_failure(const char* msg){
	if(errno == EINTR){
        return 1;
    }else{
        failure(msg);
    }

    return 0;
}

/* FAILURE
 * @brief error handler
 * @param msg: error message
 * @return exit code
 */

int failure(const char* msg){
	fprintf(stderr, "%s", msg);
	exit(-1);
}

/* LOSS_WITH_PROB
 * @brief simulates a packet loss with given probability
 * @param prob: probability
 * @return 0 if packet is lost, else 1
 */

int loss_with_prob(int prob){
    int random = rand();

    if(!(random % prob)){
        return 1;
    }

    return 0;
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


