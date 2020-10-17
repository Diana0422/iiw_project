#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define TIMER_BASE 3
#define MINIMUM_RTO 1
#define ALPHA	0.125
#define BETA	0.25

typedef struct timeout_info {
	double estimated_rtt;
	double dev_rtt;
	struct timeval start;
	struct timeval end;
	struct timeval interval;
} Timeout;

extern void timeout_interval(Timeout*);

extern void arm_timer(Timeout*, timer_t, int);

extern void disarm_timer(timer_t);

extern void timeout_handler(int, siginfo_t*, void*);

extern int check_failure(const char*);
	
extern int failure(const char*);

extern void retransmission();