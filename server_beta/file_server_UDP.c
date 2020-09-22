//
//  server.c
//
//
//  Created by Diana Pasquali on 20/07/2020.

#include "server.h"

#define SERV_PORT 5193          //Port number used by the server application
#define MAXLINE 1024            //Maximum dimension of the temporary buffer
#define PAYLOAD 65400           //Maximum amount of data bytes in a single packet
#define THREAD_POOL	10          //Number of working thread
#define MAX_DGRAM_SIZE 65505    //Maximum dimension of a datagram that can be sent through a socket
#define INIT_WND_SIZE 10        //Initial dimension of the transmission window
#define MAX_RVWD_SIZE 10        //Dimension of the receive buffer

pthread_mutex_t list_mux;           //Mutex: coordinate the access to the client list
pthread_mutex_t file_mux;           //Mutex: coordinate the access to the same file
pthread_mutex_t mux_free[THREAD_POOL];  //Mutexes: signal the "all clear" to subscribe the packet in a client node
pthread_mutex_t mux_avb[THREAD_POOL];   //Mutexes: signal a new packet in a client node 

int sem_client;     //Semaphore descriptor: signal an incoming client to serve
int listensd;       //Listening socket descriptor: transmission 

//int seq[THREAD_POOL];

Client *cliaddr_head;   //Pointer to Client structure: head of the client list

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 LIST OPERATION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void response_list(int sd, int thread, Client* cli_info)
{
    /**VARIABLE DEFINITIONS**/
    struct sockaddr_in addr = cli_info->addr;     //Client address
    socklen_t addrlen = sizeof(addr);            
    DIR* dirp;                                    
    struct dirent* pdirent;
    Packet *pk;
    char* buff;
    int payld = 0;    //Used to insert as much filenames as possibile into one packet (Transmission efficiency)

    int rcv_next = cli_info->pack->seq_num + 1;  //The sequence number of the next byte of data that is expected from the client
    int send_next = cli_info->pack->ack_num;     //The sequence number of the next byte of data to be sent to the client
    int send_una = send_next;                    //The sequence number of the first byte of data that has been sent but not yet acknowledged
    int send_wnd = INIT_WND_SIZE;                //The size of the send window. The window specifies the total number of bytes that any device may have unacknowledged at any one time
    int usable_wnd = send_wnd;                   //The amount of bytes that can be sent
    /**END**/
      
    printf("response_list started.\n");
    
    // Ensure we can open directory
    dirp = opendir("files");
    if (dirp == NULL) {
        perror("Cannot open directory");
        exit(-1);
    }
    
    buff = (char*)malloc(PAYLOAD);
    if(buff == NULL){
        perror("Malloc failed.");
        exit(EXIT_FAILURE);
    }
    // Process each entry in the directory
    //printf("******* FILES *******\n");
    while((pdirent = readdir(dirp)) != NULL){
        printf("%s\n", pdirent->d_name);
        //Ignore the current and parent directory
        if(!(strcmp(pdirent->d_name, ".") && strcmp(pdirent->d_name, ".."))){ 
            continue;
        }

        strcat(buff, pdirent->d_name);
        strcat(buff, "\n");
        //Check if more data can be sent into one packet only
        while((payld += strlen(buff)) < PAYLOAD){
            strcat(buff, pdirent->d_name);
            strcat(buff, "\n");
        }

        //Send data packet with filenames
        pk = create_packet(send_next, rcv_next, strlen(buff), buff, DATA);
        
        printf("Sending packet #%d...", send_next);
        if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
            fprintf(stderr, "Error: couldn't send packet #%d.\n", pk->seq_num);
            return;
        }
        send_next += (int)pk->data_size;
        payld = 0;
        memset(buff, 0, strlen(buff));
        while((usable_wnd = send_una + send_wnd - send_next) < 0){
            //FULL WINDOW: wait for ACK until either it is received (restart the timer and update window) or the timer expires (retransmit send_una packet)
            pthread_mutex_lock(&mux_avb[thread]);
            if((cli_info->pack->type == ACK) && (cli_info->pack->ack_num > send_una)){
                printf("Ack OK.\n");
                send_una = cli_info->pack->ack_num;
                //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS: send_una != send_next
            }
            pthread_mutex_unlock(&mux_free[thread]);
        }
    }

    //IMPLEMENTING THE RECEPTION OF AN ACK AS AN EVENT ON WHICH THE THREAD RESPONDS WITH UPDATING THE WINDOW WOULD BE AWSOME

    //Send last data packet with filenames (amount of data smaller than PAYLOAD)
    pk = create_packet(send_next, rcv_next, strlen(buff), buff, DATA);
    
    printf("Sending packet #%d...", send_next);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send packet #%d.\n", pk->seq_num);
        return;
    }

    //Wait for ACK until every packet is correctly received
    while(send_una != send_next){
        pthread_mutex_lock(&mux_avb[thread]);
        if((cli_info->pack->type == ACK) && (cli_info->pack->ack_num > send_una)){
            printf("Ack OK.\n");
            send_una = cli_info->pack->ack_num;
            //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS: send_una != send_next
            //ELSE, STOP TIMER
        }
        pthread_mutex_unlock(&mux_free[thread]);
    }
    
    
    pk = create_packet(send_next, rcv_next, 0, NULL, DATA);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
    	fprintf(stderr, "Error: couldn't send the filename.\n");
        return;
    } else {
    	printf("last packet.\n");
    }

    free(buff);
    free(pk);
}

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 GET OPERATION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void response_get(int sd, int thread, char* filename, Client* cli_info)
{
    /**VARIABLE DEFINITIONS**/
    struct sockaddr_in addr = cli_info->addr;     //Client address
    socklen_t addrlen = sizeof(addr);

    char buff[MAX_DGRAM_SIZE];
    char *sendline;

    FILE *fp;
    size_t max_size;
    char path[strlen("files/")+strlen(filename)];
    ssize_t read_size;

    Packet* pk;

    int rcv_next = cli_info->pack->seq_num + 1;    //The sequence number of the next byte of data that is expected from the client
    int send_next = cli_info->pack->ack_num;       //The sequence number of the next byte of data to be sent to the client
    int send_una = send_next;           //The sequence number of the first byte of data that has been sent but not yet acknowledged
    int send_wnd = INIT_WND_SIZE;       //The size of the send window. The window specifies the total number of bytes that any device may have unacknowledged at any one time
    int usable_wnd = send_wnd;          //The amount of bytes that can be sent
    /**END**/

    printf("response_get started.\n");
    
    //Open the file
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
    printf("File size = %ld\n", max_size);
        
    //Send the dimension to the server and wait for it to be correctly received
    sprintf(buff, "%ld", max_size);    
    pk = create_packet(send_next, rcv_next, strlen(buff), buff, DATA);    
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the dimension packet to server.\n");
        fclose(fp);
        return;
    }
    send_next += (int)pk->data_size;
    usable_wnd = send_una + send_wnd - send_next;
    
    //Wait for ACK
    pthread_mutex_lock(&mux_avb[thread]);
    if((cli_info->pack->type == ACK) && (cli_info->pack->ack_num > send_una)){
        printf("Ack OK.\n");
        send_una = cli_info->pack->ack_num;
        //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS: send_una != send_next
        //ELSE, STOP TIMER
    }
    pthread_mutex_unlock(&mux_free[thread]);
    
    //Allocate space
    if((sendline = (char*)malloc(MAX_DGRAM_SIZE)) == NULL){
    	perror("Malloc() failed.");
    	exit(-1);
    }
    	
    //Send the file to the client
    while(!feof(fp)) {       
        //Read from the file and send data
        read_size = fread(sendline, 1, PAYLOAD, fp);
        
        pk = create_packet(send_next, rcv_next, read_size, sendline, DATA);          
        if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
            free(sendline);
            fprintf(stderr, "Error: couldn't send data.\n");
            return;
        }
        send_next += (int)pk->data_size;

        //Check if there's still usable space in the transmission window: if not, wait for an ACK
        while((usable_wnd = send_una + send_wnd - send_next) < 0){
            //FULL WINDOW: wait for ACK until either it is received (restart the timer and update window) or the timer expires (retransmit send_una packet)
            pthread_mutex_lock(&mux_avb[thread]);
            if((cli_info->pack->type == ACK) && (cli_info->pack->ack_num > send_una)){
                printf("Ack OK.\n");
                send_una = cli_info->pack->ack_num;
                //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS: send_una != send_next
            }
            pthread_mutex_unlock(&mux_free[thread]);
        }

        free(pk);
        memset(sendline, 0, MAX_DGRAM_SIZE);
    }

    //Wait for ACK until every packet is correctly received
    while(send_una != send_next){
        pthread_mutex_lock(&mux_avb[thread]);
        if((cli_info->pack->type == ACK) && (cli_info->pack->ack_num > send_una)){
            printf("Ack OK.\n");
            send_una = cli_info->pack->ack_num;
            //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS: send_una != send_next
            //ELSE, STOP TIMER
        }
        pthread_mutex_unlock(&mux_free[thread]);
    }
    
    free(sendline);
    fclose(fp);
}

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 PUT OPERATION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */


void response_put(int sd, int thread, char* filename, Client* cli_info)
{
    /**VARIABLE DEFINITIONS**/
    struct sockaddr_in addr = cli_info->addr;       //Client address
    socklen_t addrlen = sizeof(addr);      

    FILE *fp;                                       //File descriptor
    char path[strlen("files/")+strlen(filename)];   //File path
    size_t max_size;                                         
    ssize_t write_size = 0;

    int dgrams, temp, i, j;

    //DIR* dirp;
    //struct dirent* pdirent;

    Packet* pack;

    Packet* rcv_buffer[MAX_RVWD_SIZE] = {NULL};     //Receiver buffer: used to buffer incoming packets out of order. NEED FOR FLOW CONTROL
    int rcv_next = cli_info->pack->seq_num + 1;    //The sequence number of the next byte of data that is expected from the client
    //int rcv_wnd;                                   //The size of the receive window “advertised” to the client
    int send_next = cli_info->pack->ack_num;       //The sequence number of the next byte of data to be sent to the client
    int interv;
    /**END**/

    printf("response_put started.\n");

    //Open the file associated with the filename
    strcpy(path, "files/");
    strcat(path, filename);
    fp = fopen(path, "wb+");
    if (fp == NULL) {
        perror("fopen() failed");
        return;
    } else {
        printf("File %s correctly opened.\n", filename);
    }

    //Retrieve file dimension from socket     
    pthread_mutex_lock(&mux_avb[thread]);
    while(cli_info->pack->seq_num != rcv_next){
        pthread_mutex_unlock(&mux_free[thread]);
        pthread_mutex_lock(&mux_avb[thread]);
    }
    
    max_size = atoi(cli_info->pack->data);
    rcv_next += (int)cli_info->pack->data_size;

    //Send ACK: ack_num updated to the byte the client is waiting to receive                        
    pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
    if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
        free(pack);
        return;
    }
    free(pack);
    
    //Retrieve file content from socket in chunks of PAYLOAD bytes
    if((dgrams = max_size/PAYLOAD)){ 
        for(i = 0; i < dgrams; i++){
            pthread_mutex_lock(&mux_avb[thread]);
            //Packet received in order: gets read by the client
            if (cli_info->pack->seq_num == rcv_next) {
                
                //Convert byte array to file
                write_size += fwrite(cli_info->pack->data, 1, cli_info->pack->data_size, fp);
                printf("write_size = %ld/%ld.\n", write_size, max_size);
                rcv_next += cli_info->pack->data_size;

                //Check if there are other buffered data to read: if so, read it, reorder the buffer and update ack_num
                interv = check_buffer(pack, rcv_buffer);
                for(i = 0; i < interv; i++){
                    //Read every packet in order: everytime from the head of the buffer to avoid gaps in the buffer
            
                    //Convert byte array to file
                    write_size += fwrite(rcv_buffer[0]->data, 1, rcv_buffer[0]->data_size, fp);
                    printf("write_size = %ld/%ld.\n", write_size, max_size);
                    rcv_next += rcv_buffer[0]->data_size;
                    //Slide the buffered packets to the head of the buffer
                    while(rcv_buffer[j] != NULL){
                        rcv_buffer[j-1] = rcv_buffer[j];
                        j++;
                    }
                    rcv_buffer[j-1] = NULL;
                    j=1;
                }
                
                //Send ACK: ack_num updated to the byte the client is waiting to receive                        
                pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
                if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                    fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
                    free(pack);
                    return;
                }
                free(pack);
            
            } else {
                //Packet received out of order: store packet in the receive buffer
                if(store_pck(pack, rcv_buffer, MAX_RVWD_SIZE)){

                    //Send ACK for the last correctly received packet
                    pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
                    if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                        fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
                        free(pack);
                        return;
                    }
                    //COUNT DUPLICATE ACK FOR A STORED OUT OF ORDER PACKET
                    free(pack);

                }else{
                    printf("Buffer Overflow. NEED FLOW CONTROL\n");
                    return;
                }       
            }
            pthread_mutex_unlock(&mux_free[thread]);           
        }
            
        printf("Retrieving another chunk...\n");
    }
    
    //Retrieve file content in an amount of bytes less than PAYLOAD
    if((temp = max_size%PAYLOAD)){
        pthread_mutex_lock(&mux_avb[thread]);        
        
        //Convert byte array to image
        write_size += fwrite(cli_info->pack->data, 1, temp, fp);
        rcv_next += (int)cli_info->pack->data_size;

        pthread_mutex_unlock(&mux_free[thread]);
        
        //Send ACK
    	pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
        if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
            fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
            free(pack);
            return;
        }
        free(pack);
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

    /**VARIABLE DEFINITIONS**/
	struct sembuf oper;         //Access to client semaphore: set to fetch 1 token when possible.
    oper.sem_num = 0;
    oper.sem_op = -1;
    oper.sem_flg = 0;

	Client* recipient;          //Variable filled by the client's informations, everytime a client is acquired

	char* cmd, *filename;       //Used to distinguish requests and files

	int id = *((int*)arg);      //Thread ID: used to assign a client to a working thread  
    /**END**/
    
	while(1){

		//Waiting for a request to serve
		while(semop(sem_client, &oper, 1) == -1){
	    	if(errno == EINTR){
	    		continue;
	    	}else{
	    		fprintf(stderr, "semop() failed");
	    		pthread_exit((void*)-1);
	    	}
		}

		//Acquire client
		pthread_mutex_lock(&list_mux);
		get_client(&cliaddr_head, id, &recipient);
	    pthread_mutex_unlock(&list_mux);

	    //Retrieve request
	    cmd = strtok(recipient->pack->data, " \n");
	    printf("Selected request: %s\n", cmd);
	    
	    //SERVE THE CLIENT
	    if (strcmp(cmd, "list") == 0) {
	        //Respond to LIST
	        response_list(listensd, id, recipient); 

	    } else if (strcmp(cmd, "get") == 0) {
	        //Respond to GET
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	printf("Please, insert also the filename.\n");
	        	remove_client(&cliaddr_head, id, &list_mux);
	        	continue;
	        }

	        filename = strdup(cmd);
	        response_get(listensd, id, filename, recipient);

	    } else if (strcmp(cmd, "put") == 0) {
	        //Respond to PUT
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	printf("Please, insert also the filename.\n");
	        	remove_client(&cliaddr_head, id, &list_mux);
	        	continue;
	        } 
	        filename = strdup(cmd);
	        response_put(listensd, id, filename, recipient);

	    } else {
	        fprintf(stderr, "Error: couldn't retrieve the request.\n");
            remove_client(&cliaddr_head, id, &list_mux);
            continue;
	    }

	    remove_client(&cliaddr_head, id, &list_mux);
	}
	pthread_exit(0);
}

int main(void)
{
    /**VARIABLE DEFINITIONS**/
    struct sockaddr_in cliaddr;
    struct sockaddr_in servaddr;
    socklen_t cliaddrlen = sizeof(cliaddr);
    socklen_t servaddrlen = sizeof(servaddr);

    key_t ksem_client = 324;    //Semaphore key to initialize a semaphore
    struct sembuf ops;          //Structure for semaphore operations

    pthread_t tid;              
    int thread, i, init_seq;     //Supporting variables for different kind of task: indexing, returning values or temporary storage
    Packet* pk_rcv;      //Packet used to receive from socket
    Packet* pk_hds = NULL;             //Packet used for the handshake and as basic informations for a new client who's just completed the handshake
    int* indexes;    
    /**END**/

    /**INITIALIZAZION OF THE STRUCTURES**/

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

    //Initialize mutexes
    pthread_mutex_init(&list_mux, 0);
	pthread_mutex_init(&file_mux, 0);

	for(i=0; i<THREAD_POOL; i++){
		pthread_mutex_init(&mux_free[i], 0);
		pthread_mutex_init(&mux_avb[i], 0);
		pthread_mutex_lock(&mux_avb[i]);
	}

    //Initialize working threads
    indexes = (int*)malloc(sizeof(int)*THREAD_POOL);
    if(indexes == NULL){
        fprintf(stderr, "Error: couldn't malloc indexes.\n");
        exit(EXIT_FAILURE);
    }

    //Create thread pool
    for(i=0; i<THREAD_POOL; i++){
    	indexes[i] = i;       
        if(pthread_create(&tid, 0, (void*)thread_service, (void*)(&indexes[i]))){
            fprintf(stderr, "pthread_create() failed");
            close(listensd);
            exit(-1);
        }
    }

    free(indexes);

    pk_rcv = (Packet*)malloc(sizeof(Packet));
    if (pk_rcv == NULL) {
        fprintf(stderr, "Error: couldn't malloc packet.\n");
        exit(EXIT_FAILURE);
    }
    
    /**EXECUTING SERVER**/
    printf("\033[0;34mWaiting for a request...\033[0m\n");

    while(1) {

    	//Receive request from client
		if (recv_packet(pk_rcv, listensd, (struct sockaddr*)&cliaddr, cliaddrlen) == -1) {
			fprintf(stderr, "Error: couldn't receive packet.\n");
            free(pk_rcv);
			exit(EXIT_FAILURE);

		} else {
            //If the packet carries a request for a new connection, enstablish a connection
            if (pk_rcv->type == SYN) {
                printf("\033[0;34mReceived a new connection request.\033[0m\n");
                
                //3-WAY HANDSHAKE
                if(handshake(pk_hds, &init_seq, listensd, &cliaddr, cliaddrlen) == -1){
                    fprintf(stderr, "\033[0;31mConnection failure.\033[0m\n");
                    //memset(pk_rcv->data, 0, pk_rcv->data_size);
                    printf("\033[0;34mWaiting for a request...\033[0m\n");
                    continue;

                }else{
                    printf("Connection enstablished.\n");

                    //Add new client address to the list
                    pthread_mutex_lock(&list_mux);
                    insert_client(&cliaddr_head, cliaddr, pk_hds);
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

            }else{
                printf("MAIN: Packet received...\n");

                //Find out who's serving the client: to signal a new packet to the thread
                dispatch_client(cliaddr_head, cliaddr, &thread);
                //If the new packet contains an ACK then update instantly the packet: if the client is sending ACK, it is not sending data!
                if(pk_rcv->type == ACK){
                    update_packet(cliaddr_head, thread, pk_rcv);
                }else{
                    pthread_mutex_lock(&mux_free[thread]);                
                    //Update the new packet in the client node so that the thread can fetch new data
                    update_packet(cliaddr_head, thread, pk_rcv);
                    pthread_mutex_unlock(&mux_avb[thread]);
                }
            }
			
		}
		
		//print_packet(*pk_rcv);

        printf("\033[0;34mWaiting for a request...\033[0m\n");																	         
    }
    
    free(pk_rcv);
    exit(0);
}
