#include "client.h"

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

/* TIMEOUT_INTERVAL
 * @brief returns a timeout interval to wait for ack to be received
 * @param time: pointer to Timeout struct
 * @return double interval
 */
 
 struct timeval timeout_interval(Timeout* time) 
 {
 	double sample_rtt, interval;
 	long long usec;
 	struct timeval final;
 	
 	
 	printf("\033[0;35m** time->estimated_rtt = %.4f\n", (double)time->estimated_rtt);
 	printf("\033[0;35m** time->dev_rtt = %.4f\n", (double)time->dev_rtt);
 	
 	sample_rtt = (float)((time->end.tv_sec - time->start.tv_sec)*1000000L + (time->end.tv_usec - time->start.tv_usec))/1000; // sample rtt in msec
 	
 	printf("** sample rtt (msec): %f\n", sample_rtt);
 	time->estimated_rtt = (1-ALPHA) * time->estimated_rtt + ALPHA * sample_rtt;
 	time->dev_rtt = (1-BETA) * time->dev_rtt + BETA * fabs(sample_rtt - time->estimated_rtt);
	
	interval = (time->estimated_rtt + 4 * (time->dev_rtt));
	printf("** timeout interval (msec): %f\n", interval);
	
	// convert interval double to timeval structure values
	usec = interval * 1000000;
	final.tv_sec = usec /1000000;
	final.tv_usec = usec % 1000000;
	printf("** final.tv_sec = %ld\n", final.tv_sec);
	printf("** final.tv_usec = %ld\033[0m\n", final.tv_usec);
	time->interval = final;
	 
	return final; // returns sender's timeout interval in micros 
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
