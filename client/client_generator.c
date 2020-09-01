#define _GNU_SOURCE

#include "client_test.h"
#include <sys/wait.h>

#define SERV_PORT 5193

void child(char* IP, int num){

	char* args[4];
	args[0] = "./client";
	args[1] = strdup(IP);

	int i = num%7;

	switch(i){
		case 0:
			args[2] = "list";
			break;
		case 1:
			args[2] = "get livia.txt";
			break;
		case 2:
			args[2] = "put alice29.txt";
			break;
		case 3:
			args[2] = "get murloc.jpg";
			break;
		case 4:
			args[2] = "put family.png";
			break;
		case 5:
			args[2] = "get ocean.mp3";
			break;
		case 6:
			args[2] = "put murloc_sound.mp3";
			break;
		default:
			return;
	}

	args[3] = (char*)0; 

	printf("Child starting a process client with: %s %s %s\n\n", args[0], args[1], args[2]);
	if(execve("./client", args, NULL) == -1){
		perror("execve() failed");
		exit(0);
	}

	exit(0);
}

int main(int argc, char* argv[]){

	int i, count = 0;

	if(argc != 3){
		printf("Utilization: <./a.out IP_SERVER NUM_CLIENTS>");
		exit(0);
	}

	int N = atoi(argv[2]);
	for(i=0; i<N; i++){
		if(fork()){
			count++;
		}else{
			child(argv[1], count);
		}
		sleep(1);
	}

	for(i=0; i<N; i++){
		wait(NULL);
	}

	exit(0);
}
