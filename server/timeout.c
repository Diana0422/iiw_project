#include "timeout.h"

/* TIMEOUT_INTERVAL
 * @brief returns a timeout interval to wait for ack to be received
 * @param time: pointer to Timeout struct
 * @return double interval
 */
 
void timeout_interval(Timeout* time) 
{
 	double sample_rtt, interval;
 	long long usec;
 	
 	sample_rtt = (double)((time->end.tv_sec - time->start.tv_sec)*1000000L + (time->end.tv_usec - time->start.tv_usec))/1000; // sample rtt in msec

 	time->estimated_rtt = (1-ALPHA) * time->estimated_rtt + ALPHA * sample_rtt;
 	time->dev_rtt = (1-BETA) * time->dev_rtt + BETA * fabs(sample_rtt - time->estimated_rtt);
	
	interval = (time->estimated_rtt + 4 * (time->dev_rtt));
	
	//Convert interval double to timeval structure values
	usec = interval * 1000;	
	time->interval.tv_sec = usec /1000000;
	time->interval.tv_usec = usec % 1000000;
}

/* ARM_TIMER
 * @brief starts timer for transmission
 * @param to: pointer to Timeout struct
 		  id: id of the timer
 		  first: 1 if timer is not set
 */

void arm_timer(Timeout* to, timer_t id, int first){
	struct itimerspec its;
	memset(&its, 0, sizeof(its));

	if(first){
		its.it_value.tv_sec = TIMER_BASE;
		its.it_value.tv_nsec = 0;
	}else{
		if((to->interval.tv_sec + (double)to->interval.tv_usec/1000000) < MINIMUM_RTO){
			its.it_value.tv_sec = 1;
			its.it_value.tv_nsec = 0;
		}else{
			its.it_value.tv_sec = to->interval.tv_sec;
			its.it_value.tv_nsec = to->interval.tv_usec*1000;
		}		
	}

	if(timer_settime(id, 0, &its, NULL) == -1){
		failure("Timer updating failed.\n");
	}
}

/* DISARM_TIMER
 * @brief stops timer for transmission
 * @param id: timer id
 */

void disarm_timer(timer_t id){
	struct itimerspec its;
	memset(&its, 0, sizeof(its));

	if(timer_settime(id, 0, &its, NULL) == -1){
		failure("Timer updating failed.\n");
	}
}

/* TIMEOUT_HANDLER
 * @brief event handler for timeout
 * @param sig: signal number
 		  si: pointer to signal info struct
 */

void timeout_handler(int sig, siginfo_t* si, void* uc){

	timer_t* tmptr;		//Pointer to the timer that caused a timeout
	tmptr = si->si_value.sival_ptr;

	retransmission(tmptr, false, 0);

	return;
}