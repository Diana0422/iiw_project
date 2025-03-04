#include "client.h"

/* TIMEOUT_INTERVAL
 * @brief calculates a timeout interval to wait for ack to be received
 * @param time: pointer to Timeout struct
 */

void timeout_interval(Timeout* time) 
{
 	double sample_rtt, interval;
 	long long usec;
 	
 	sample_rtt = (double)((time->end.tv_sec - time->start.tv_sec)*1000000L + (time->end.tv_usec - time->start.tv_usec))/1000; // sample rtt in msec

 	time->estimated_rtt = (1-ALPHA) * time->estimated_rtt + ALPHA * sample_rtt;
 	time->dev_rtt = (1-BETA) * time->dev_rtt + BETA * fabs(sample_rtt - time->estimated_rtt);
	
	interval = (time->estimated_rtt + 4 * (time->dev_rtt));
	
	//convert interval double to timeval structure values
	usec = interval * 1000;	
	time->interval.tv_sec = usec /1000000;
	time->interval.tv_usec = usec % 1000000;
}

/* ARM_TIMER
 * @brief starts timer for transmission
 * @param timer: pointer to Timer_node struct
 		  first: 1 if timer is not set
 */

void arm_timer(Timer_node* timer, int first){
	struct itimerspec its;
	memset(&its, 0, sizeof(its));

	if(first){
		its.it_value.tv_sec = TIMER_BASE;
		its.it_value.tv_nsec = 0;
	}else{
		if(((timer->to).interval.tv_sec + (double)(timer->to).interval.tv_usec/1000000) < MINIMUM_RTO){
			its.it_value.tv_sec = 1;
			its.it_value.tv_nsec = 0;
		}else{
			its.it_value.tv_sec = (timer->to).interval.tv_sec;
			its.it_value.tv_nsec = (timer->to).interval.tv_usec*1000;
		}		
	}

	if(timer_settime(timer->timerid, 0, &its, NULL) == -1){
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
	retransmission(tmptr);
}

/* INSERT_TIMER
 * @brief inserts a new timer in the queue
 * @param head: pointer to the head node of the queue
 */

Timer_node* insert_timer(Timer_node** head){
    Timer_node *prev;
    Timer_node *curr;
    Timer_node *new;

    if((new = (Timer_node*)malloc(sizeof(Timer_node))) == NULL){
        perror("Malloc() failed");
        exit(-1);
    }

    memset(&(new->to), 0, sizeof(Timeout));
    new->timerid = 0;
    new->next = NULL;

    prev = NULL;
    curr = *head;

    while(curr != NULL){
        prev = curr;
        curr = curr->next;
    }

    if(prev == NULL){
        new->next = *head;
        *head = new;
    }else{
        prev->next = new;
        new->next = curr;
    }
    
    return new;
}
