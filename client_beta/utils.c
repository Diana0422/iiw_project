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


void print_progress(double percentage) {
    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\033[1;34m\r%3d%% [%.*s%*s]\033[0m", val, lpad, PBSTR, rpad, "");
    fflush(stdout);
    printf("\r");
}


void open_shell() {
    /*Spawn a child to run the program.*/
    pid_t pid=fork();
    if (pid==0) { /* child process */
        static char *argv[]={"echo","Foo is my name",NULL};
        execv("/bin/echo",argv);
        exit(127); /* only if execv fails */
    }
    else { /* pid!=0; parent process */
        waitpid(pid,0,0); /* wait for child to exit */
    }
}