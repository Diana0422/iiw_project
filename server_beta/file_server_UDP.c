//
//  server.c
//
//
//  Created by Diana Pasquali on 20/07/2020.

#include "server.h"

#define SERV_PORT 5193          //Port number used by the server application
#define INIT_WND_SIZE 10        //Initial dimension of the transmission window
#define MAX_RVWD_SIZE 1000        //Dimension of the receive buffer

pthread_mutex_t list_mux;           //Mutex: coordinate the access to the client list
pthread_mutex_t file_mux;           //Mutex: coordinate the access to the same file
pthread_mutex_t mux_free[THREAD_POOL];  //Mutexes: signal the "all clear" to subscribe the packet in a client node
pthread_mutex_t mux_avb[THREAD_POOL];   //Mutexes: signal a new packet in a client node 

int sem_client;     //Semaphore descriptor: signal an incoming client to serve
int listensd;       //Listening socket descriptor: transmission 

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

    int rcv_next = cli_info->pack->seq_num + (int)cli_info->pack->data_size + 1;  //The sequence number of the next byte of data that is expected from the client
    int send_next = cli_info->pack->ack_num;     //The sequence number of the next byte of data to be sent to the client
    int send_una = send_next;                    //The sequence number of the first byte of data that has been sent but not yet acknowledged
    int send_wnd = INIT_WND_SIZE;                //The size of the send window. The window specifies the total number of bytes that any device may have unacknowledged at any one time
    int usable_wnd = send_wnd;                   //The amount of bytes that can be sent
    /**END**/
      
    //AUDIT
    printf("response_list started.\n");

    //Send ACK
    printf("Sending ACK.\n");
    pk = create_packet(send_next, rcv_next, 0, NULL, ACK);
    if (send_packet(pk, listensd, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send ack #%d.\n", pk->ack_num);
        free(pk);
        return;
    }
    printf("ACK #%d correctly sent.\n", pk->ack_num);
    free(pk);
    
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
        //Ignore the current and parent directory
        if(!(strcmp(pdirent->d_name, ".") && strcmp(pdirent->d_name, ".."))){ 
            continue;
        }

        strcat(buff, pdirent->d_name);
        strcat(buff, "\n");
        //Check if more data can be sent into one packet only
        if((payld += strlen(buff)) > PAYLOAD){
            //Send data packet with filenames
            pk = create_packet(send_next, rcv_next, strlen(buff), buff, DATA);
            if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                fprintf(stderr, "Error: couldn't send packet #%d.\n", pk->seq_num);
                return;
            }
            send_next += (int)pk->data_size;
            free(pk);

            while((usable_wnd = send_una + send_wnd - send_next) < 0){
                //FULL WINDOW: wait for ACK until either it is received (restart the timer and update window) or the timer expires (retransmit send_una packet)
                pthread_mutex_lock(&mux_avb[thread]);
                if((cli_info->pack->type == ACK) && (cli_info->pack->ack_num > send_una)){
                    printf("Ack #%d recevied.\n", cli_info->pack->ack_num);
                    send_una = cli_info->pack->ack_num;
                    //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS: send_una != send_next
                }
                pthread_mutex_unlock(&mux_free[thread]);
            }
            payld = 0;
            memset(buff, 0, strlen(buff));
        }       
    }

    //IMPLEMENTING THE RECEPTION OF AN ACK AS AN EVENT ON WHICH THE THREAD RESPONDS WITH UPDATING THE WINDOW WOULD BE AWSOME

    //Send last data packet with filenames (amount of data smaller than PAYLOAD)
    pk = create_packet(send_next, rcv_next, strlen(buff), buff, DATA);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send packet #%d.\n", pk->seq_num);
        return;
    }
    send_next += (int)pk->data_size;
    free(pk);

    //Wait for ACK until every packet is correctly received
    while(send_una != send_next){
        pthread_mutex_lock(&mux_avb[thread]);
        if((cli_info->pack->type == ACK) && (cli_info->pack->ack_num > send_una)){
            printf("Ack #%d recevied.\n", cli_info->pack->ack_num);
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

    char *sendline;

    FILE *fp;
    char path[strlen("files/")+strlen(filename)];
    ssize_t read_size;

    Packet *pk;

    int rcv_next = cli_info->pack->seq_num + (int)cli_info->pack->data_size + 1;    //The sequence number of the next byte of data that is expected from the client
    int send_next = cli_info->pack->ack_num;       //The sequence number of the next byte of data to be sent to the client
    int send_una = send_next;           //The sequence number of the first byte of data that has been sent but not yet acknowledged
    int send_wnd = INIT_WND_SIZE;       //The size of the send window. The window specifies the total number of bytes that any device may have unacknowledged at any one time
    int usable_wnd = send_wnd;          //The amount of bytes that can be sent
    /**END**/

    printf("response_get started.\n");

    //Send ACK
    printf("Sending ACK.\n");
    pk = create_packet(send_next, rcv_next, 0, NULL, ACK);
    if (send_packet(pk, listensd, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send ack #%d.\n", pk->ack_num);
        free(pk);
        return;
    }
    printf("ACK #%d correctly sent.\n", pk->ack_num);
    free(pk);
    
    //Open the file
    memset(path, 0, strlen(path));
    strcat(path, "files/");
    strcat(path, filename);
    
    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open %s.", filename);
        return;
    } else {
        printf("File %s correctly opened.\n", filename);
    }

    //Allocate space
    if((sendline = (char*)malloc(MAX_DGRAM_SIZE)) == NULL){
    	perror("Malloc() failed.");
    	exit(-1);
    }
  
    //Send the file to the client
    while(!feof(fp)) {       
        //Read from the file and send data   
        read_size = fread(sendline, 1, PAYLOAD, fp);
        printf("Send_next #%d: sendline %s\n", send_next, sendline); 
        pk = create_packet(send_next, rcv_next, read_size, sendline, DATA);  
        printf("Sending packet #%d: %s\n", pk->seq_num, pk->data);          
        if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
            free(sendline);
            free(pk);
            fprintf(stderr, "Error: couldn't send data.\n");
            return;
        }
        //printf("Packet #%d correctly sent\n", send_next);
        send_next += (int)pk->data_size;

        //Check if there's still usable space in the transmission window: if not, wait for an ACK
        while((usable_wnd = send_una + send_wnd - send_next) == 0){
            //FULL WINDOW: wait for ACK until either it is received (restart the timer and update window) or the timer expires (retransmit send_una packet)
            pthread_mutex_lock(&mux_avb[thread]);
            if((cli_info->pack->type == ACK) && (cli_info->pack->ack_num > send_una)){
                printf("Received ACK #%d.\n", cli_info->pack->ack_num);
                send_una = cli_info->pack->ack_num;
                //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS: send_una != send_next
            }
        }

        free(pk);
        memset(sendline, 0, MAX_DGRAM_SIZE);
        sleep(1);
    }

    pk = create_packet(send_next, rcv_next, 0, NULL, DATA);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the filename.\n");
        return;
    } else {
        printf("last packet.\n");
    }

    //Wait for ACK until every packet is correctly received
    while(send_una != send_next){
        pthread_mutex_lock(&mux_avb[thread]);
        if((cli_info->pack->type == ACK) && (cli_info->pack->ack_num > send_una)){
            printf("Received ACK #%d.\n", cli_info->pack->ack_num);
            send_una = cli_info->pack->ack_num;
            //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS: send_una != send_next
            //ELSE, STOP TIMER
        }
    }
    
    free(sendline);
    free(pk);
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
    ssize_t write_size = 0;

    Packet* pack;

    Packet* rcv_buffer[MAX_RVWD_SIZE] = {NULL};     //Receiver buffer: used to buffer incoming packets out of order. NEED FOR FLOW CONTROL
    int rcv_next = cli_info->pack->seq_num + (int)cli_info->pack->data_size;    //The sequence number of the next byte of data that is expected from the client
    int send_next = cli_info->pack->ack_num;       //The sequence number of the next byte of data to be sent to the client
    /**END**/

    printf("response_put started.\n\n");

    //Send ACK
    printf("Sending ACK.\n");
    pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
    if (send_packet(pack, listensd, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
        free(pack);
        return;
    }
    printf("ACK #%d correctly sent.\n\n", pack->ack_num);
    free(pack);
    
    //Open the file
    memset(path, 0, strlen(path));
    strcat(path, "files/");
    strcat(path, filename);

    //Open the file associated with the filename
    fp = fopen(path, "wb+");
    if (fp == NULL) {
        perror("fopen() failed");
        return;
    } else {
        printf("File %s correctly opened.\n", filename);
    }

    /* Receive data in chunks of 65000 bytes */
    while(1){
        pthread_mutex_lock(&mux_avb[thread]);
        if(cli_info->pack->seq_num == rcv_next){ //Packet received in order: gets read by the client
            //Check if transmission has ended.
            if(cli_info->pack->data_size == 0){
                //Data transmission completed: check if the receive buffer is empty
                if(rcv_buffer[0] == NULL){
                    printf("Transmission has completed successfully.\n");
                    pthread_mutex_unlock(&mux_free[thread]);
                    break;
                }

            }else{
                //Convert byte array to file
                write_size += fwrite(cli_info->pack->data, 1, cli_info->pack->data_size, fp);
                rcv_next += (int)cli_info->pack->data_size;
            }

            //Send ACK for the last correctly received packet
            pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
            printf("Sending ACK.\n");
            if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
                free(pack);
                return;
            }
            printf("ACK #%d correctly sent.\n\n", pack->ack_num); 
            free(pack); 

        } else {
            //Packet received out of order: store packet in the receive buffer
            if(store_pck(cli_info->pack, rcv_buffer, MAX_RVWD_SIZE)){

                //Send ACK for the last correctly received packet
                pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
                printf("Sending ACK.\n");
                if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                    fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
                    free(pack);
                    pthread_mutex_unlock(&mux_free[thread]); 
                    return;
                }
                printf("ACK #%d correctly sent.\n\n", pack->ack_num);
                //COUNT DUPLICATE ACK FOR A STORED OUT OF ORDER PACKET
                free(pack);

            }else{
                printf("Buffer Overflow. NEED FLOW CONTROL\n");
                pthread_mutex_unlock(&mux_free[thread]); 
                return;
            }       
        }
        pthread_mutex_unlock(&mux_free[thread]);                   
    }

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
        pthread_mutex_lock(&mux_avb[id]);
	    cmd = strtok(recipient->pack->data, " \n");
        pthread_mutex_unlock(&mux_free[id]);

	    printf("Selected request: %s\n", cmd);
	    
	    //SERVE THE CLIENT
	    if (strcmp(cmd, "list") == 0) {
	        //Respond to LIST
	        response_list(listensd, id, recipient); 

	    } else if (strcmp(cmd, "get") == 0) {
	        //Respond to GET
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	continue;
	        }

	        filename = strdup(cmd);
	        response_get(listensd, id, filename, recipient);

	    } else if (strcmp(cmd, "put") == 0) {
	        //Respond to PUT
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	continue;
	        } 
	        filename = strdup(cmd);
	        response_put(listensd, id, filename, recipient);

	    } else {
	        fprintf(stderr, "Error: couldn't retrieve the request.\n");
            continue;
	    }
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

            }else if(pk_rcv->type == FIN){
                printf("\033[0;34mReceived a request for connection demolition.\033[0m\n");
                
                //3-WAY HANDSHAKE
                if(demolition(listensd, &cliaddr, cliaddrlen) == -1){
                    fprintf(stderr, "\033[0;31mDemolition failure.\033[0m\n");
                    printf("\033[0;34mWaiting for a request...\033[0m\n");
                    continue;

                }else{
                    printf("Connection closed.\n");

                    //Add new client address to the list
                    pthread_mutex_lock(&list_mux);
                    remove_client(&cliaddr_head, cliaddr);
                    pthread_mutex_unlock(&list_mux);
                }

            }else{
                //Find out who's serving the client: to signal a new packet to the thread
                dispatch_client(cliaddr_head, cliaddr, &thread);
                //If the new packet contains an ACK then update instantly the packet: if the client is sending ACK, it is not sending data!
                if(pk_rcv->type == ACK){
                    update_packet(cliaddr_head, thread, pk_rcv);
                    pthread_mutex_unlock(&mux_avb[thread]);
                }else{
                    pthread_mutex_lock(&mux_free[thread]);                
                    //Update the new packet in the client node so that the thread can fetch new data
                    update_packet(cliaddr_head, thread, pk_rcv);
                    pthread_mutex_unlock(&mux_avb[thread]);
                }
            }   
        }
        printf("\033[0;34mWaiting for a request...\033[0m\n");                                                                           
    }
    
    free(pk_rcv);
    exit(0);
}

