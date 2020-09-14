//
//  server_test.c
//
//
//  Created by Diana Pasquali on 20/07/2020.

#include "server_test.h"

#define SERV_PORT 5193
#define MAXLINE 1024
#define PAYLOAD 65400
#define THREAD_POOL	10
#define MAX_DGRAM_SIZE 65505

pthread_mutex_t list_mux;
pthread_mutex_t file_mux;
pthread_mutex_t put_free[THREAD_POOL];
pthread_mutex_t put_avb[THREAD_POOL];

int sem_client;
int listensd;

char put_msg[THREAD_POOL][MAX_DGRAM_SIZE+1];
int seq[THREAD_POOL];


Client *cliaddr_head;

typedef struct thread_arg {
 	int thread;
 	size_t datalen;
 	Packet* pack;
 }thread_arg;

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 LIST OPERATION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

/*void response_list(int sd, struct sockaddr_in addr)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *sendline;
    FILE *fp;
    char *filename = "filelist.txt";
    size_t max_size;
    DIR* dirp;
    struct dirent* pdirent;
      
    printf("response_list started.\n");
    
    fp = fopen(filename, "w+");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open %s", filename);
        return;
    } else {
    	printf("%s correctly opened.\n", filename);
    }
    
    // Ensure we can open directory
    dirp = opendir("files");
    if (dirp == NULL) {
    	printf("cannot open directory %s.\n", "files");
    	exit(-1);
    }
    
    // Process each entry in the directory
    printf("******* FILES *******\n");
    while ((pdirent = readdir(dirp)) != NULL) {
    	printf("%s\n", pdirent->d_name);
    	fputs(pdirent->d_name, fp);
    	fputs("\n", fp);
    } 
    
    //Retrieve file dimension
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    
    //Send file dimension
    if((sendline = (char*) malloc(max_size)) == NULL){
        perror("Malloc() failed");
        exit(-1);
    }
   
    buf_clear(buff, MAXLINE);
    sprintf(buff, "%ld", max_size);
    printf("File size: %ld\n", max_size);
    
    if (sendto(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the dimension of %s\n", filename);
        return;
    } else {
        printf("Dimension of %s correctly sent.\n", filename);
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
    if (remove("filelist.txt") == 0) {
    	printf("File %s removed successfully!.\n", "filelist.txt");
    } else {
    	fprintf(stderr, "Error: unable to delete the file.\n");
    	return;
    }
    
    printf("sendline freed.\n\n"); 
}*/


void response_list(int sd, struct sockaddr_in addr, int sequence, int thread)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAX_DGRAM_SIZE];
    DIR* dirp;
    struct dirent* pdirent;
    Packet *pk;
    int size;
    int send_base = sequence;
      
    printf("response_list started.\n");
    memset(buff, 0, MAX_DGRAM_SIZE);
    
    // Ensure we can open directory
    dirp = opendir("files");
    if (dirp == NULL) {
        perror("Cannot open directory");
        exit(-1);
    }
    
    // Process each entry in the directory
    //printf("******* FILES *******\n");
    while ((pdirent = readdir(dirp)) != NULL) {
   
        printf("%s\n", pdirent->d_name);
        if(!(strcmp(pdirent->d_name, ".") && strcmp(pdirent->d_name, ".."))){ //Ignore the current and parent directory
            continue;
        }

        strcpy(buff, pdirent->d_name);
        
        // Send data packet with filename
        pk = create_packet("data", send_base, strlen(buff), buff);
        
        printf("Sending packet #%d...", send_base);
        printf("LIST - send name: ");
        if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
        	fprintf(stderr, "Error: couldn't send packet #%d.\n", pk->seq);
        	return;
        }
        
        // Wait for ack...
        printf("LIST - recv ack: ");
        int i;
        
        pthread_mutex_lock(&put_avb[thread]);
        i = seq[thread];
        pthread_mutex_unlock(&put_free[thread]);
        
        if (i == send_base) {
        	printf("Ack OK.\n");
        } else {
        	return;
        }
        
        send_base++;
        //buf_clear(buff, MAXLINE);
        memset(buff, 0, MAX_DGRAM_SIZE);
    } 
    
    pk = create_packet("data", send_base, 0, NULL);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
    	fprintf(stderr, "Error: couldn't send the filename.\n");
        return;
    } else {
    	printf("last packet.\n");
    }
}

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 GET OPERATION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */


void response_get(int sd, struct sockaddr_in addr, char* filename, int sequence, int thread)
{
    socklen_t addrlen = sizeof(addr);
    char mess[MAXLINE];
    char buff[MAX_DGRAM_SIZE];
    char *sendline;
    FILE *fp;
    size_t max_size;
    char path[strlen("files/")+strlen(filename)];
    int n, i;
    ssize_t read_size;
    Packet* pk;
    int size;
    int send_base = sequence;
    
    printf("response_get started.\n");
    
    strcat(path, "files/");
    strcat(path, filename);
    
    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open %s.", filename);
        return;
    } else {
        printf("File %s correctly opened.\n", filename);
    }
    
    //Retrieve the file dimension
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp); 
    fseek(fp, 0, SEEK_SET);
    
    sprintf(buff, "%ld", max_size); 
    
    /*  NO PACKETS - SEND FILE DIMENSION
    //Send the file dimension
    if (sendto(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the dimension.\n");
        return;
    } else {
        printf("File dimension correctly sent.\n");
    }
    
    */
    
    // Send file dimension
    pk = create_packet("data", send_base, strlen(buff), buff);
    size = strlen(buff);
    
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
    	fprintf(stderr, "Error: couldn't send packet #%d.\n", pk->seq);
    	return;
    } else {
    	printf("File dimension in packet #%d sent correctly.\n", pk->seq);
    }
    
    // Receive ack
    pthread_mutex_lock(&put_avb[thread]);
    i = seq[thread];
    memcpy(mess, put_msg[thread], strlen(put_msg[thread]));
    
    if ((i == send_base) && (strcmp(mess, "ack") == 0)) {
    	printf("Ack OK.\n");
    }
    pthread_mutex_unlock(&put_free[thread]);
    
    
    // Send file content
    /*   NO PACKETS
    //Allocate sufficient space to send the file
    if((sendline = (char*)malloc(max_size)) == NULL){
        perror("Malloc() failed.");
        exit(-1);
    }

    while(!feof(fp)) {
        //Read from the file into our send buffer
        read_size = fread(sendline, 1, MAX_DGRAM_SIZE, fp);
        printf("File size read: %ld\n", read_size);
        //Send data through our socket 
        //printf("%d bytes sent to client.\n", n);
        if ((n = sendto(sd, sendline, read_size, 0, (struct sockaddr*)&addr, addrlen)) == -1) {
            fprintf(stderr, "Error: couldn't send data.\n");
            return;
        }
        sleep(1); //USED TO COORDINATE SENDER AND RECEIVER IN THE ABSENCE OF RELIABILITY 
    }
    */
    
    free(pk);

    if((sendline = (char*)malloc(MAX_DGRAM_SIZE)) == NULL){
    	perror("Malloc() failed.");
    	exit(-1);
    }
    		
    while(!feof(fp)) {
    	send_base++;
        //Read from the file into our send buffer
        read_size = fread(sendline, 1, PAYLOAD, fp);
        printf("Image size read: %ld\n", read_size);
        printf("PAYLOAD = %d", PAYLOAD);
        printf("sendline = %s\n", sendline);
        		
        //Send data through our socket 
        pk = create_packet("data", send_base, read_size, sendline);
        		
        if ((n = send_packet(pk, listensd, (struct sockaddr*)&addr, addrlen, &size)) == -1) {
        	fprintf(stderr, "Error: couldn't send packet #%d.\n", pk->seq);
        	exit(EXIT_FAILURE);
	}
	
	// Receive ack
    	pthread_mutex_lock(&put_avb[thread]);
    	i = seq[thread];
    	memcpy(mess, put_msg[thread], strlen(put_msg[thread]));
    
    	if ((i == send_base) && (strcmp(mess, "ack") == 0)) {
    		printf("Ack OK.\n");
    	}
    	pthread_mutex_unlock(&put_free[thread]);
        		
        free(pk);
        memset(sendline, 0, MAX_DGRAM_SIZE);
        sleep(1); //USED TO COORDINATE SENDER AND RECEIVER IN THE ABSENCE OF RELIABILITY 
    }
    
    
    free(sendline);
    fclose(fp);
    printf("sendline freed.\n\n");
}

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 PUT OPERATION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */


void response_put(int sd, struct sockaddr_in addr, char* filename, int server, int sequence)
{
    socklen_t addrlen = sizeof(addr);
    FILE *fp;
    char path[strlen("files/")+strlen(filename)];
    size_t max_size;
    ssize_t write_size = 0;
    int dgrams, temp, i;
    DIR* dirp;
    struct dirent* pdirent;
    Packet* pack;
    int send_base = sequence;
    int recv_base;
    int size;
      
    printf("response_put started.\n");
    
    strcpy(path, "files/");
    strcat(path, filename);
    
    // Allocate packet for ack
    pack = (Packet*)malloc(sizeof(Packet));
    if (pack == NULL) {
    	fprintf(stderr, "Error: couldn't malloc packet for ack.\n");
    	return;
    }

    /*//Check if the file already exists
    dirp = opendir("files");
    if (dirp == NULL) {
        perror("Cannot open directory");
        exit(-1);
    }
    
    while ((pdirent = readdir(dirp)) != NULL) {
        if(!(strcmp(pdirent->d_name, filename))){
            printf("The selected file already exists.\n");
            //Sending an advertisement to client to abort the operation (maybe avoidable with reliability)
            if (sendto(sd, NULL, 0, 0, (struct sockaddr*)&addr, addrlen) == -1) {
		        fprintf(stderr, "Error: couldn't send the message.\n");
		        return;
		    }

		    printf("Advertisement correctly sent.\n");
		    return;
        }
    }*/

    //Sending an advertisement to client to continue with the operation (maybe avoidable with reliability)
    if (sendto(sd, "OK", 2, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the message.\n");
        return;
    }
    
    //Open the file associated with the filename
    fp = fopen(path, "wb+");
    if (fp == NULL) {
        perror("fopen() failed");
        return;
    } else {
        printf("File %s correctly opened.\n", filename);
    }

    //Retrieve file dimension from socket
    
    pthread_mutex_lock(&put_avb[server]);
    //printf("mutex put_avb[%d] locked.\n", server);
    max_size = atoi(put_msg[server]);
    recv_base = seq[server];
    printf("File size: %ld\n", max_size);
    printf("put_msg[%d] = %d\n", server, atoi(put_msg[server]));
    pthread_mutex_unlock(&put_free[server]);
    //printf("mutext put_free[%d] unlocked.\n", server);
    
   // Send ack...
    pack = create_packet("ack", recv_base, strlen("ack"), "ack");
    
    if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
    	fprintf(stderr, "Error: couldn't send ack packet.\n");
    	return;
    } else {
    	recv_base++;
    }
    
    
    //Retrieve file content from socket
    if((dgrams = max_size/MAX_DGRAM_SIZE)){ 
        for(i=0; i<dgrams; i++){
            pthread_mutex_lock(&put_avb[server]);
            printf("mutex put_avb[%d] locked.\n", server);
            
            // Convert byte array to image
            //write_size += fwrite(put_msg[server], 1, MAX_DGRAM_SIZE, fp);
            write_size += fwrite(put_msg[server], 1, PAYLOAD, fp);
            recv_base = seq[server];
            
            pthread_mutex_unlock(&put_free[server]);
            printf("mutex put_free[%d] unlocked.\n", server);
            
            // Send ack...
    	    pack = create_packet("ack", recv_base, strlen("ack"), "ack");
    
            if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
    	    	fprintf(stderr, "Error: couldn't send ack packet.\n");
    		return;
    	    } else {
    		recv_base++;
    	    }
            
    	    printf("retrieving another chunk...\n");
        }
    }
    

    if((temp = max_size%PAYLOAD)){
        pthread_mutex_lock(&put_avb[server]); 
        //printf("mutex put_avb[%d] locked.\n", server);       
        
        // Convert byte array to image
        write_size += fwrite(put_msg[server], 1, temp, fp);
        recv_base = seq[server];
     
        pthread_mutex_unlock(&put_free[server]);
        //printf("mutex put_free[%d] unlocked.\n", server);
        
        // Send ack...
    	pack = create_packet("ack", recv_base, strlen("ack"), "ack");
    
        if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
    	    	fprintf(stderr, "Error: couldn't send ack packet.\n");
    		return;
    	} else {
    		recv_base++;
    	}
    }
     
    printf("Bytes written: %d\n", (int)write_size);
    fclose(fp);
}

/*
 ----------------------------------------------------------------------------------------------------------------------------------------------
 MAIN & THREAD HANDLER
 ----------------------------------------------------------------------------------------------------------------------------------------------
 */
    
void* thread_service(void* arg){

	struct sembuf oper;
	struct sockaddr_in address;
	char* cmd, *filename;
	char buff[MAX_DGRAM_SIZE];
	//int tag = *((int*)p);
	thread_arg a = *((thread_arg*)arg);
	Packet* pk = a.pack;
	int num_seq;

    oper.sem_num = 0;
    oper.sem_op = -1;
    oper.sem_flg = 0;

	while(1){

		//buf_clear(buff, MAXLINE);
		memset(buff, 0, MAX_DGRAM_SIZE);

		//Waiting for a request to serve
		while(semop(sem_client, &oper, 1) == -1){
	    	if(errno == EINTR){
	    		continue;
	    	}else{
	    		fprintf(stderr, "semop() failed");
	    		pthread_exit((void*)-1);
	    	}
		}

		//Retrieve client address and request
		pthread_mutex_lock(&list_mux);
		//get_client(&cliaddr_head, tag, &address, buff);
		get_client(&cliaddr_head, a.thread, &address, pk);
		memcpy(buff, pk->data, pk->data_size);
		num_seq = pk->seq;
	    	pthread_mutex_unlock(&list_mux);

	    cmd = strtok(buff, " \n");
	    printf("Selected request: %s\n", cmd);
	    
	    //SERVE THE CLIENT
	    if (strcmp(cmd, "list") == 0) {
	        //Respond to LIST
	        response_list(listensd, address, num_seq, a.thread);
	    } else if (strcmp(cmd, "get") == 0) {
	        //Respond to GET
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	printf("Please, insert also the filename.\n");
	        	//clean_thread(&cliaddr_head, &address, &list_mux, tag);
	        	clean_thread(&cliaddr_head, &address, &list_mux, a.thread);
	        	continue;
	        }

	        filename = strdup(cmd);
	        response_get(listensd, address, filename, num_seq, a.thread);
	    } else if (strcmp(cmd, "put") == 0) {
	        //Respond to PUT
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	printf("Please, insert also the filename.\n");
	        	//clean_thread(&cliaddr_head, &address, &list_mux, tag);
	        	clean_thread(&cliaddr_head, &address, &list_mux, a.thread);
	        	continue;
	        } 
	        filename = strdup(cmd);
	        //response_put(listensd, address, filename, tag);
	        response_put(listensd, address, filename, a.thread, num_seq);

	    } else {
	        fprintf(stderr, "Error: couldn't retrieve the request.\n");
            //clean_thread(&cliaddr_head, &address, &list_mux, tag);
            clean_thread(&cliaddr_head, &address, &list_mux, a.thread);
            continue;
	    }

	    //clean_thread(&cliaddr_head, &address, &list_mux, tag);
	    clean_thread(&cliaddr_head, &address, &list_mux, a.thread);
	}
	pthread_exit(0);
}

int main(void)
{
    pthread_t tid;
    //int index[THREAD_POOL];
    thread_arg* arg[THREAD_POOL];
    int thread, i, n, size;
    Packet* pk_rcv = NULL;
    int init_seq;

    char buff[MAX_DGRAM_SIZE];

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

    	pthread_mutex_init(&list_mux, 0);
	pthread_mutex_init(&file_mux, 0);

	for(i=0; i<THREAD_POOL; i++){
		pthread_mutex_init(&put_free[i], 0);
		pthread_mutex_init(&put_avb[i], 0);
		pthread_mutex_lock(&put_avb[i]);
	}

    //Create thread pool
    for(i=0; i<THREAD_POOL; i++){
    	
    	arg[i] = (thread_arg*)malloc(sizeof(thread_arg));
    	arg[i]->pack = (Packet*)malloc(sizeof(Packet));
    	arg[i]->thread = i;
    	//index[i] = i;
    	/*if(pthread_create(&tid, 0, (void*)thread_service, (void*)&index[i])){
            fprintf(stderr, "pthread_create() failed");
            close(listensd);
            exit(-1);
        } */
        
        if(pthread_create(&tid, 0, (void*)thread_service, (void*)arg[i])){
            fprintf(stderr, "pthread_create() failed");
            close(listensd);
            exit(-1);
        }
        
    }
    
    printf("\033[0;34mWaiting for a request...\033[0m\n");

    while(1) {
    
    	/** INITIALIZE SEQUENCE NUMBER- SERVER **/
redo:
        memset(buff, 0, MAX_DGRAM_SIZE);
	 
	// Receive request from client and initialize sequence number
	pk_rcv = (Packet*)malloc(sizeof(Packet));
	if (pk_rcv == NULL) {
		fprintf(stderr, "Error: couldn't malloc packet.\n");
		exit(EXIT_FAILURE);
	}
	
	if ((n = recv_packet(pk_rcv, listensd, (struct sockaddr*)&cliaddr, cliaddrlen)) == -1) {
		fprintf(stderr, "Error: couldn't receive packet.\n");
		exit(EXIT_FAILURE);
	} else {
		//strcpy(buff, pk->data);
		printf("MAIN: Packet received...\n");
		printf("4.\n");
		memcpy(buff, pk_rcv->data, pk_rcv->data_size);
	}
	
	printf("--PACKET #%d: \n", pk_rcv->seq);
	printf("  type:      %s \n", pk_rcv->type);
	printf("  seq:       %d  \n", pk_rcv->seq);
	printf("  data_size: %ld  \n", pk_rcv->data_size);
	printf("  data:      %s  \n\n\n", pk_rcv->data);
	
	if (strcmp(pk_rcv->type, "conn") == 0) {
		// Resend the packet to the client to initialize connection
		if (send_packet(pk_rcv, listensd, (struct sockaddr*)&cliaddr, cliaddrlen, &size) == -1) {
			fprintf(stderr, "Error: couldn't initialize sequence number.\n");
			printf("\033[0;34mWaiting for a request...\033[0m\n");
			goto redo;
		}
		
		init_seq = pk_rcv->seq;
		printf("INITIAL SEQUENCE NUMBER = %d.\n\n", init_seq);
	}
	
	if (strcmp(pk_rcv->type, "ack") == 0) {
		printf("Do something for ack.\n");
	}
	
	/** END **/
	
	//Check if the client is still being served
	if(dispatch_client(cliaddr_head, cliaddr, &thread)){
	    printf("\033[0;32mClient already queued!\033[0m\n");
            
	    pthread_mutex_lock(&put_free[thread]);
	    memset(put_msg[thread], 0, MAX_DGRAM_SIZE);
	    memcpy(put_msg[thread], pk_rcv->data, pk_rcv->data_size);
	    seq[thread] = pk_rcv->seq;
	    printf("put_msg[%d] = %s.\n", thread, pk_rcv->data);
	    printf("seq[%d] = %d.\n", thread, seq[thread]);
	    pthread_mutex_unlock(&put_avb[thread]);

	}else{
	    printf("\033[0;34mReceived a new request.\033[0m\n");
		
	    //Add new client address to list
	    pthread_mutex_lock(&list_mux);
	    insert_client(&cliaddr_head, cliaddr, pk_rcv);
	    pthread_mutex_unlock(&list_mux);
	       
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
	
//}      
	memset(pk_rcv->data, 0, pk_rcv->data_size);
       printf("\033[0;34mWaiting for a request...\033[0m\n");																	         
    }
    

    exit(0);
}
