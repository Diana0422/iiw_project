//
//  server_test.c
//
//
//  Created by Diana Pasquali on 20/07/2020.
//

#include "server_test.h"

#define SERV_PORT 5193
#define MAXLINE 1024
#define THREAD_POOL	10

pthread_mutex_t list_mux = PTHREAD_MUTEX_INITIALIZER;

int sem_avb;
int listensd;

Client *cliaddr_head;

int filelist_ctrl(char* filename)
{
    FILE *fp;
    int ret = 1;
    char s[MAXLINE];
    
    fp = fopen("filelist.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open filelist.txt");
        ret = 0;
    } else {
        printf("filelist.txt correctly opened.\n");
    }
    printf("\n");
    while (fgets(s, strlen(filename)+1, fp)) {
        printf("Data from filelist.txt: %s\n", s);
        printf("strcmp = %d\n", strncmp(filename, s, strlen(filename)));
        if (strncmp(filename, s, strlen(filename)) == 0) {
            ret = 0;
            break;
        }
    }
    fclose(fp);
    
    return ret;
}


void response_list(int sd, struct sockaddr_in addr)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *sendline;
    FILE *fp;
    char *filename = "filelist.txt";
    size_t max_size;
      
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open %s", filename);
        return;
    } else {
        printf("%s correctly opened.\n", filename);
    }
    
    //Retrieve file dimension
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    //Send file dimension
    buf_clear(buff);
    sprintf(buff, "%ld", max_size);
    printf("File size: %ld\n", max_size);
    
    if (sendto(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        //fprintf(stderr, "Error: couldn't send the dimension of %s\n", filename);
        perror("sendto(dimension)");
        return;
    } else {
        printf("Dimension of %s correctly sent.\n", filename);
    }
    
    /* Send file content */
    if((sendline = (char*) malloc(max_size)) == NULL){
        perror("Malloc() failed");
        exit(-1);
    }
    
    //Write content on a buffer
    char ch;
    for (int i=0; i<(int)max_size; i++) {
        ch = fgetc(fp);
        sendline[i] = ch;
        if (ch == EOF) break;
    }
    
    //Send the content to the client
    if(sendto(sd, sendline, max_size, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send content to client.");
        free(sendline);
        return;
    } else {
        printf("%s correctly sent.\n", filename);
    }
    
    free(sendline);
    printf("sendline freed.\n\n"); 
}


void response_get(int sd, struct sockaddr_in addr)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *sendline;
    FILE *fp;
    size_t max_size;
    
    //Retrieve filename from socket
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) == -1) {
        fprintf(stderr, "Error: couldn't retrieve filename.\n");
        return;
    }

    fp = fopen(buff, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open %s.", buff);
        return;
    } else {
        printf("File %s correctly opened.\n", buff);
    }
    
    //Retrieve the file dimension
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    buf_clear(buff);
    sprintf(buff, "%ld", max_size);
    
    //Send the file dimension
    if (sendto(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the dimension.\n");
        return;
    } else {
        printf("File dimension correctly sent.\n");
    }
    
    //Allocate sufficient space to send the file
    if((sendline = (char*)malloc(max_size)) == NULL){
        perror("Malloc() failed.");
        exit(-1);
    }
    
    char ch;
    for (int i=0; i<(int)max_size; i++) {
        ch = fgetc(fp);
        sendline[i] = ch;
        if (ch == EOF) break;
    }
    
    //Send the file to the client
    if (sendto(sd, sendline, max_size, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the file content.\n");
        free(sendline);
        return;
    } else {
        printf("File content correctly sent.\n");
    }

    free(sendline);
    printf("sendline freed.\n\n");
}

void response_put(int sd, struct sockaddr_in addr)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    FILE *fp, *flp;
    char *filename;
    char *filelist = "filelist.txt";
    size_t file_size;
    char *recvline;
    
    //Retrieve filename from socket
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) == -1) {
        fprintf(stderr, "Error: couldn't retrieve filename.\n");
        return;
    } else {
        filename = strdup(buff);
        printf("Filename: %s\n", filename);
    }
    
    //Open the file associated with the filename
    fp = fopen(filename, "w+");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open file %s", filename);
        return;
    } else {
        printf("File %s correctly opened.\n", filename);
    }
    
    if (filelist_ctrl(filename)) { //check if the file already exists in the server and if not create it
        flp = fopen(filelist, "a+");
        if (flp == NULL) {
            fprintf(stderr, "Error: couldn't open filelist.");
            return;
        } else {
            printf("Filelist correctly opened.\n");
        }
        
        fputs(filename, flp); //Add the new filename to the list
        fputs("\n", flp);
        fclose(flp);
    } 
    
    //Retrieve file dimension from socket
    buf_clear(buff);
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) == -1) {
        fprintf(stderr, "Error: couldn't retrieve the file dimension.");
        return;
    } else {
        file_size = atoi(buff);
        printf("File size: %ld\n", file_size);
    }
    
    //Retrieve file content from socket
    if((recvline = (char*)malloc(file_size)) == NULL){
        perror("Malloc() failed.");
        exit(EXIT_FAILURE);
    }

    if (recvfrom(sd, recvline, file_size, 0, (struct sockaddr*)&addr, &addrlen) == -1) {
        fprintf(stderr, "Error: couldn't retrieve file content.");
        free(recvline);
        return;
    } else {
        printf("File content correclty retrieved.\n");
    }

    //Writing content on file
    printf("\n\n");
    puts(recvline);
    printf("\n");
    fputs(recvline, fp);
    fclose(fp);
    
    free(recvline);
    printf("recvline freed.\n");
}
   
void* thread_service(void){

	struct sembuf oper;
	struct sockaddr_in address;
	char* cmd;
	char buff[MAXLINE];

	while(1){
		
		oper.sem_num = 0;
		oper.sem_op = -1;
		oper.sem_flg = 0;

		//Waiting for a request to serve
		while(semop(sem_avb, &oper, 1) == -1){
	    	if(errno == EINTR){
	    		continue;
	    	}else{
	    		fprintf(stderr, "semop() failed");
	    		exit(EXIT_FAILURE);
	    	}
		}

		//Retrieve client address and request
		pthread_mutex_lock(&list_mux);

		get_and_remove(&cliaddr_head, &address, buff);

	    pthread_mutex_unlock(&list_mux);

	    cmd = strtok(buff, " \n");
	    printf("Selected request: %s\n", cmd);
	    
	    //SERVE THE CLIENT
	    if (strcmp(cmd, "list") == 0) {
	        //Respond to LIST
	        response_list(listensd, address);
	    } else if (strcmp(cmd, "get") == 0) {
	        //Respond to GET
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	printf("Please, insert also the filename.\n");
	        	break;
	        } 
	        response_get(listensd, address);
	    } else if (strcmp(cmd, "put") == 0) {
	        //Respond to PUT
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	printf("Please, insert also the filename.\n");
	        	break;
	        } 
	        response_put(listensd, address);
	    } else {
	        fprintf(stderr, "Error: couldn't retrieve the request.\n");
	        exit(EXIT_FAILURE);
	    }
	}
}

int main(void)
{
    pthread_t tid[THREAD_POOL];

    char buff[MAXLINE];

    key_t ksem_avb = 324;
    struct sembuf ops;

    struct sockaddr_in cliaddr;
    struct sockaddr_in servaddr;
    socklen_t cliaddrlen = sizeof(cliaddr);
    socklen_t servaddrlen = sizeof(servaddr);
      
    //Create listening socket
    if ((listensd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "socket() failed");
        exit(-1);
    }
    
    //Initialize server IP address
    memset((void*)&servaddr, 0, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    
    //Assign server address to the socket
    if (bind(listensd, (struct sockaddr*)&servaddr, servaddrlen) < 0) {
        fprintf(stderr, "bind() failed");
        exit(-1);
    } else {
        printf("\033[0;32mBinding completed successfully.\033[0m\n");
    }

    //Initialize semaphore
    while((sem_avb = semget(ksem_avb, 1, IPC_CREAT|IPC_PRIVATE|0600)) == -1){
    	if(errno == EINTR){
    		continue;
    	}else{
    		//fprintf(stderr, "semget() failed");
            perror("semget() failed");
    		exit(-1);
    	}
    }

    while(semctl(sem_avb, 0, SETVAL, 0) == -1){
    	if(errno == EINTR){
    		continue;
    	}else{
    		//fprintf(stderr, "semctl() failed");
            perror("semctl() failed");
    		exit(-1);
    	}
	}

	ops.sem_num = 0;
	ops.sem_op = 1;
	ops.sem_flg = 0;

	//Initialize client address list
	cliaddr_head = NULL;
    
    //Create thread pool
    for(int i=0; i<THREAD_POOL; i++){
    	if(pthread_create(&tid[i], 0, (void*)thread_service, NULL)){
            fprintf(stderr, "pthread_create() failed");
            close(listensd);
            exit(-1);
        } 
    }

    while(1) {

        printf("\033[0;34mWaiting for a request...\033[0m\n");

        buf_clear(buff);

        if (recvfrom(listensd, buff, MAXLINE, 0, (struct sockaddr*)&cliaddr, &cliaddrlen) == -1) {
        	perror("recvfrom() failed");
        	close(listensd);
	        exit(-1);
	    }

        printf("\033[0;34mReceived a new request.\033[0m\n");

        //Add new client address to list
        pthread_mutex_lock(&list_mux);
        insert_node(&cliaddr_head, cliaddr, buff);
        pthread_mutex_unlock(&list_mux);

        //Signal the serving threads 
        while(semop(sem_avb, &ops, 1) == -1){
	    	if(errno == EINTR){
	    		continue;
	    	}else{
	    		fprintf(stderr, "semop() failed");
	    		exit(-1);
	    	}
		}
    }
    exit(0);
}
