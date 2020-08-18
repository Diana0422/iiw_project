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
pthread_mutex_t file_mux = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t put_free[THREAD_POOL];
pthread_mutex_t put_avb[THREAD_POOL];

int sem_client;
int listensd;

char* put_msg[THREAD_POOL] = {"\0"};

Client *cliaddr_head;

int filelist_ctrl(char* filename)
{
    FILE *fp;
    char s[MAXLINE];
    
    fp = fopen("filelist.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open filelist.txt");
        return 0;
    } else {
        printf("filelist.txt correctly opened.\n");
    }
    printf("\n");
    while (fgets(s, strlen(filename)+1, fp)) {
        //printf("Data from filelist.txt: %s\n", s);
        //printf("strcmp = %d\n", strncmp(filename, s, strlen(filename)));
        if (strncmp(filename, s, strlen(filename)) == 0) {
            return 0;
        }
    }
    fclose(fp);
    
    return 1;
}

void response_list(int sd, struct sockaddr_in addr)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *sendline;
    FILE *fp;
    char *filename = "filelist.txt";
    size_t max_size;
      
    printf("response_list started.\n"); 
    
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
    buf_clear(buff, MAXLINE);
    sprintf(buff, "%ld", max_size);
    printf("File size: %ld\n", max_size);
    
    if (sendto(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the dimension of %s\n", filename);
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


void response_get(int sd, struct sockaddr_in addr, char* filename)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *sendline;
    FILE *fp;
    size_t max_size;
    
    printf("response_get started.\n");
    
    printf("1\n");
    
    fp = fopen(filename, "r");
    printf("2\n");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open %s.", filename);
        return;
    } else {
        printf("File %s correctly opened.\n", filename);
    }
    printf("3\n");
    
    //Retrieve the file dimension
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
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

void response_put(char* filename, int server)
{
    char buff[MAXLINE];
    FILE *fp, *flp;
    char *filelist = "filelist.txt";
    size_t file_size;
    char *recvline;
    
    printf("response_put started.\n");
    
    //Open the file associated with the filename
    fp = fopen(filename, "w+");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open file %s", filename);
        return;
    } else {
        printf("File %s correctly opened.\n", filename);
    }
    
    //Check if the file already exists in the server and if not create it
    pthread_mutex_lock(&file_mux);
    printf("mutex file_mux locked. (3)\n");
    if (filelist_ctrl(filename)) {
        flp = fopen(filelist, "a+");
        if (flp == NULL) {
            fprintf(stderr, "Error: couldn't open filelist.");
            return;
        } else {
            printf("Filelist correctly opened.\n");
        }
        
        fputs(filename, flp);
        fputs("\n", flp);
        fclose(flp);       
    } 
    pthread_mutex_unlock(&file_mux);
    printf("mutex file_mux unlocked. (3)\n");
    
    //Retrieve file dimension from socket
    buf_clear(buff, MAXLINE);
  	
    pthread_mutex_lock(&put_avb[server]);
    printf("mutex put_avb[%d] locked.\n", server);
    strcpy(buff, put_msg[server]);
    buf_clear(put_msg[server], strlen(put_msg[server]));
    pthread_mutex_unlock(&put_free[server]);
    printf("mutext put_free[%d] unlocked.\n", server);
    
    file_size = atoi(buff);
    if((recvline = (char*)malloc(file_size)) == NULL){
        perror("Malloc() failed.");
        exit(EXIT_FAILURE);
    }

    //Retrieve file content from socket

    pthread_mutex_lock(&put_avb[server]);
    printf("mutex put_avb[%d] locked.\n", server);
    recvline = strdup(put_msg[server]);
    pthread_mutex_unlock(&put_free[server]);
    printf("mutex put_free[%d] unlocked.\n", server);

    //Write content on file
    printf("\n\n");
    puts(recvline);
    printf("\n");
    fputs(recvline, fp);
    fclose(fp);
    
    free(recvline);
    printf("recvline freed.\n");
}
   
void* thread_service(void* p){

	struct sembuf oper;
	struct sockaddr_in address;
	char* cmd, *filename;
	char buff[MAXLINE];
	int tag = *((int*)p);

    oper.sem_num = 0;
    oper.sem_op = -1;
    oper.sem_flg = 0;

	while(1){

		buf_clear(buff, MAXLINE);

		//Waiting for a request to serve
		while(semop(sem_client, &oper, 1) == -1){
	    	if(errno == EINTR){
	    		continue;
	    	}else{
	    		fprintf(stderr, "semop() failed");
	    		exit(EXIT_FAILURE);
	    	}
		}

		//Retrieve client address and request
		pthread_mutex_lock(&list_mux);
		printf("mutex list_mux locked (2).\n");
		get_client(&cliaddr_head, tag, &address, buff);
	    pthread_mutex_unlock(&list_mux);
	    printf("mutex list_mux unlocked (2).\n");

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
	        	clean_thread(&cliaddr_head, &address, &list_mux, tag);
	        	continue;
	        }

	        filename = strdup(cmd);
	        response_get(listensd, address, filename);
	    } else if (strcmp(cmd, "put") == 0) {
	        //Respond to PUT
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	printf("Please, insert also the filename.\n");
	        	clean_thread(&cliaddr_head, &address, &list_mux, tag);
	        	continue;
	        } 
	        filename = strdup(cmd);
	        response_put(filename, tag);

	        buf_clear(put_msg[tag], strlen(put_msg[tag]));
	    } else {
	        fprintf(stderr, "Error: couldn't retrieve the request.\n");
	        exit(EXIT_FAILURE);
	    }

	    clean_thread(&cliaddr_head, &address, &list_mux, tag);
	}
	exit(0);
}

int main(void)
{
    pthread_t tid;
    int index[THREAD_POOL];
    int thread, i;

    char buff[MAXLINE];

    key_t ksem_client = 324;

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

    //Initialize semaphores
    while((sem_client = semget(ksem_client, 1, IPC_CREAT|IPC_PRIVATE|0600)) == -1){
    	if(errno == EINTR){
    		continue;
    	}else{
    		fprintf(stderr, "semget() failed");
    		exit(-1);
    	}
    }

    while(semctl(sem_client, 0, SETVAL, 0) == -1){
    	if(errno == EINTR){
    		continue;
    	}else{
    		fprintf(stderr, "semctl() failed");
    		exit(-1);
    	}
	}

	ops.sem_num = 0;
	ops.sem_op = 1;
	ops.sem_flg = 0;

	//Initialize client address list
	cliaddr_head = NULL;

	for(i=0; i<THREAD_POOL; i++){
		pthread_mutex_init(&put_free[i], 0);
		printf("mutex put_free[%d] initialized.\n", i);
		pthread_mutex_init(&put_avb[i], 0);
		printf("mutex put_avb[%d] initialized.\n", i);
		pthread_mutex_lock(&put_avb[i]);
		printf("mutex put_avb[%d] locked.\n", i);
	}

    //Create thread pool
    for(i=0; i<THREAD_POOL; i++){
    	index[i] = i;
    	if(pthread_create(&tid, 0, (void*)thread_service, (void*)&index[i])){
            fprintf(stderr, "pthread_create() failed");
            close(listensd);
            exit(-1);
        } 
    }
    
    printf("\033[0;34mWaiting for a request...\033[0m\n");

    while(1) {

        buf_clear(buff, MAXLINE);

        if (recvfrom(listensd, buff, MAXLINE, 0, (struct sockaddr*)&cliaddr, &cliaddrlen) == -1) {
        	perror("recvfrom() failed");
        	close(listensd);
	        exit(-1);
	    }

	    //Check if the client is still being served
	    if(dispatch_client(cliaddr_head, cliaddr, &thread)){
	    	printf("Client already queued.\n");
	    	pthread_mutex_lock(&put_free[thread]);
	    	printf("mutex put_free[%d] locked.\n", thread);
	    	put_msg[thread] = strdup(buff);
	    	pthread_mutex_unlock(&put_avb[thread]); // put_avb??
	    	printf("mutex put_avb[%d] unlocked.\n", thread);

	    }else{
	    	printf("\033[0;34mReceived a new request.\033[0m\n");

	        //Add new client address to list
	        pthread_mutex_lock(&list_mux);
	        printf("mutex list_mux locked.\n");
	        insert_client(&cliaddr_head, cliaddr, buff);
	        pthread_mutex_unlock(&list_mux);
	        printf("mutex list_mux unlocked.\n");
	       
	        //Signal the serving threads 
	        while(semop(sem_client, &ops, 1) == -1){
		    	if(errno == EINTR){
		    		continue;
		    	}else{
		    		fprintf(stderr, "semop() failed");
		    		exit(-1);
		    	}
			}
		}
		buf_clear(buff, MAXLINE);	
		
		printf("\033[0;34mWaiting for a request...\033[0m\n");																	         
    }
    exit(0);
}
