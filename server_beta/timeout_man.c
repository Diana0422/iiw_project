#include "server.h"

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

 void arm_timer(Timeout* to, timer_t id, int first){
	struct itimerspec its;
	memset(&its, 0, sizeof(its));

	if(first){
		its.it_value.tv_sec = TIMER_BASE;
		its.it_value.tv_nsec = TIMER_BASE % 1000000000;
	}else{
		its.it_value.tv_sec = to->interval.tv_sec;
		its.it_value.tv_nsec = to->interval.tv_sec % 1000000000;
	}

	if(timer_settime(id, 0, &its, NULL) == -1){
		failure("Timer updating failed.\n");
	}

	printf("Timer armed for %ld seconds\n", its.it_value.tv_sec);
}

void disarm_timer(timer_t id){
	struct itimerspec its;
	memset(&its, 0, sizeof(its));

	if(timer_settime(id, 0, &its, NULL) == -1){
		failure("Timer updating failed.\n");
	}

	printf("Timer disarmed.\n");
}

void timeout_handler(int sig, siginfo_t* si, void* uc){

	//Temporary behaviour
	printf("Timer expired. Terminating program.\n");
	exit(0);
}