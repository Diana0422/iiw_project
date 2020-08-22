#define _GNU_SOURCE

#include "client_test.h"

#define SERV_PORT 5193

void child(char* IP, int num){

	char* args[4];
	args[0] = "./client";
	args[1] = strdup(IP);

	//int i = rand()%3;
	int i = 0;
	switch(i){
		case 0:
			args[2] = "list";
			break;
		case 1:
			args[2] = "get testo1.txt";
			break;
		case 2:
			args[2] = "put testo.txt";
			break;
		default:
			return;
	}

	args[3] = (char*)0; 

	printf("Child starting a process client with: %s %s %s\n", args[0], args[1], args[2]);
	if(execve("./client", args, NULL) == -1){
		perror("execve() failed");
		exit(0);
	}

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
			continue;
		}else{
			child(argv[1], count);
		}
		count++;
	}

	exit(0);
}
