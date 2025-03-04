//
//  server.c
//  Created by Diana Pasquali on 20/07/2020.
//
#define _GNU_SOURCE

#include "server.h"

#define SERV_PORT 5193          

pthread_mutex_t list_mux;                       //Mutex: coordinate the access to the client list
pthread_mutex_t file_mux;                       //Mutex: coordinate the access to the same file
pthread_mutex_t mux_free[THREAD_POOL];          //Mutexes: signal the "all clear" to subscribe the packet in a client node
pthread_mutex_t mux_avb[THREAD_POOL];           //Mutexes: signal a new packet in a client node 
pthread_mutex_t wnd_lock[THREAD_POOL];          //Mutex: coordinate the access to the transmission window of every thread

unsigned long rcv_next[THREAD_POOL];            //The sequence number of the next byte of data that is expected from the client
short usable_wnd[THREAD_POOL];                  //The amount of bytes that can be sent   

int sem_client;                                 //Semaphore descriptor: signal an incoming client to serve
int listensd;                                   //Listening socket descriptor: transmission 

Client *cliaddr_head;                           //Pointer to Client structure: head of the client list

Packet* wnd[THREAD_POOL][INIT_WND_SIZE];        //Transmission windows for every thread
unsigned long rtx_packs[THREAD_POOL];           //Lost packets ids

timer_t timerid[THREAD_POOL];                   //Array of timer descriptors, assigned each one to a thread
Timeout to[THREAD_POOL];                        //Timeout structures related to each thread 

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 RETRANSMISSION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void retransmission(timer_t* ptr, bool fast_rtx, int thread){
    int id = 0;
    Packet* pk;
    Client* client_info = cliaddr_head;
    struct sockaddr_in address;
    socklen_t addrlen;

    if (fast_rtx == false) {  
        //Retransmission due to timeout  
        //Retrieve the thread for which the timer has expired
        for(int i=0; i<THREAD_POOL; i++){
            if(timerid+i == ptr){
                id = i;
                break;
            }
        }
    }else{
        id = thread;
    }

    //Fetch the client's information to proceed with the retransmission
    while(client_info != NULL){
        if(id == (client_info->server)) {
            address = client_info->addr;
            break;
        }
        client_info = client_info->next;
    }
    addrlen = sizeof(address);

    //Fetch the packet to retransmit
    pk = wnd[id][0];

    if (send_packet(pk, listensd, (struct sockaddr*)&address, addrlen, &to[id]) == -1) {
        fprintf(stderr, "\033[0;31mError: couldn't send packet #%lu.\033[0m\n", pk->seq_num);
        return;
    }

    arm_timer(&to[id], timerid[id], 0);

    return;
}

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 LIST OPERATION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void response_list(int sd, Client* cli_info)
{
    /**VARIABLE DEFINITIONS**/
    struct sockaddr_in addr = cli_info->addr;     //Client address
    socklen_t addrlen = sizeof(addr); 
    DIR* dirp;                                    
    struct dirent* pdirent;
    Packet *pk;
    char* buff;
    int payld = 0;    //Used to insert as much filenames as possibile into one packet (Transmission efficiency)
    unsigned long send_next = cli_info->pack->ack_num;
    int thread = cli_info->server;
    /**END**/

    rcv_next[thread] = cli_info->pack->seq_num + (int)cli_info->pack->data_size + 1;  //The sequence number of the next byte of data that is expected from the client
    usable_wnd[thread] = INIT_WND_SIZE;
    
    //Send ACK
    if(send_ack(sd, addr, addrlen, send_next, rcv_next[thread], &to[thread])){
        return;
    }

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

    //Process each entry in the directory
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
            pk = create_packet(send_next, rcv_next[thread], strlen(buff), buff, DATA);
            if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &to[thread]) == -1) {
                fprintf(stderr, "Error: couldn't send packet #%lu.\n", pk->seq_num);
                return;
            }
            send_next += (unsigned long)pk->data_size;

            pthread_mutex_lock(&wnd_lock[thread]);
            update_window(pk, wnd, thread, &usable_wnd[thread]);
            pthread_mutex_unlock(&wnd_lock[thread]);

            //If it's the first packet to be transmitted, start timer
            if(usable_wnd[thread] == INIT_WND_SIZE-1){
            	arm_timer(&to[thread], timerid[thread], 0);
            }

            while(usable_wnd[thread] == 0){
                //FULL WINDOW: wait for ACK until either it is received or the timer expires
                sleep(1);
            }

            payld = 0;
            memset(buff, 0, strlen(buff));
        }       
    }
    
    //Send last data packet with filenames (amount of data smaller than PAYLOAD)
    pk = create_packet(send_next, rcv_next[thread], strlen(buff), buff, DATA);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &to[thread]) == -1) {
        fprintf(stderr, "Error: couldn't send packet #%lu.\n", pk->seq_num);
        return;
    }
    send_next += (int)pk->data_size;

    pthread_mutex_lock(&wnd_lock[thread]);
    update_window(pk, wnd, thread, &usable_wnd[thread]);
    pthread_mutex_unlock(&wnd_lock[thread]);

    //If it's the first packet to be transmitted, start timer
    if(usable_wnd[thread] == INIT_WND_SIZE-1){
        arm_timer(&to[thread], timerid[thread], 0);
    } 

    //Wait for ACK until every packet is correctly received
    while(usable_wnd[thread] != INIT_WND_SIZE){
        sleep(1);
    }
       
    pk = create_packet(send_next, rcv_next[thread], 0, NULL, DATA);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &to[thread]) == -1) {
    	fprintf(stderr, "Error: couldn't send the filename.\n");
        return;
    }

    free(buff);
    free(pk);
}

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 GET OPERATION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void response_get(int sd, char* filename, Client* cli_info)
{
    /**VARIABLE DEFINITIONS**/
    struct sockaddr_in addr = cli_info->addr;
    socklen_t addrlen = sizeof(addr);
    char *sendline;
    FILE *fp;
    char path[strlen("files/")+strlen(filename)];
    ssize_t read_size;
    unsigned long send_next = cli_info->pack->ack_num;
    Packet *pk;
    int size;

    int thread = cli_info->server;
    /**END**/

    rcv_next[thread] = cli_info->pack->seq_num + (unsigned long)cli_info->pack->data_size + 1;    
    usable_wnd[thread] = INIT_WND_SIZE;  

    //Try to open the file and check if it exists
    memset(path, 0, strlen(path));
    strcat(path, "files/");
    strcat(path, filename);
    
    fp = fopen(path, "rb");
    if (fp == NULL) {
        if(errno == ENOENT){
            sendline = "The file does not exist.";
            printf("%s\n", sendline);
            pk = create_packet(send_next, rcv_next[thread], strlen(sendline), sendline, ERROR);     
            if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &to[thread]) == -1){
                free(pk);
                fprintf(stderr, "Error: couldn't send data.\n");
                return;
            }
            send_next += (unsigned long)pk->data_size;

            pthread_mutex_lock(&wnd_lock[thread]);
            update_window(pk, wnd, thread, &usable_wnd[thread]);       
            pthread_mutex_unlock(&wnd_lock[thread]);

            arm_timer(&to[thread], timerid[thread], 0);

            //Wait for the ACK
            while(usable_wnd[thread] != INIT_WND_SIZE){
                sleep(1);
            }

            return;

        }else{
            fprintf(stderr, "Error: couldn't open %s.", filename);
            return;
        }
        
    }

    //Send ACK for the request packet
    if(send_ack(sd, addr, addrlen, send_next, rcv_next[thread], &to[thread])){
        return;
    }
    
    //Open the file
    memset(path, 0, strlen(path));
    strcat(path, "files/");
    strcat(path, filename);
    
    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open %s.", filename);
        return;
    }

    //Allocate space
    if((sendline = (char*)malloc(MAX_DGRAM_SIZE)) == NULL){
    	perror("Malloc() failed.");
    	exit(-1);
    }
    memset(sendline, 0, MAX_DGRAM_SIZE);

    //Get file size
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    //Send file size
    sprintf(sendline, "%d", size);
    pk = create_packet(send_next, rcv_next[thread], strlen(sendline), sendline, DATA);    
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &to[thread]) == -1) {
        free(sendline);
        free(pk);
        fprintf(stderr, "Error: couldn't send data.\n");
        return;
    }
    send_next += (unsigned long)pk->data_size;

    pthread_mutex_lock(&wnd_lock[thread]);
    update_window(pk, wnd, thread, &usable_wnd[thread]);       
    pthread_mutex_unlock(&wnd_lock[thread]);

    arm_timer(&to[thread], timerid[thread], 0);

    //Wait for the ACK
    while(usable_wnd[thread] != INIT_WND_SIZE){
        sleep(1);
    }
  
    //Send the file to the client
    while(!feof(fp)) {       
        //Read from the file and send data   
        read_size = fread(sendline, 1, PAYLOAD, fp);
        pk = create_packet(send_next, rcv_next[thread], read_size, sendline, DATA);     
        if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &to[thread]) == -1) {
            free(sendline);
            free(pk);
            fprintf(stderr, "Error: couldn't send data.\n");
            return;
        }
        send_next += (unsigned long)pk->data_size;

        pthread_mutex_lock(&wnd_lock[thread]);
        update_window(pk, wnd, thread, &usable_wnd[thread]);       
        pthread_mutex_unlock(&wnd_lock[thread]);

        //If it's the the only packet in the window, start timer
        if(usable_wnd[thread] == INIT_WND_SIZE-1){
            arm_timer(&to[thread], timerid[thread], 0);
        }

        while(usable_wnd[thread] == 0){
            //FULL WINDOW: wait for ACK until either it is received or the timer expires
            sleep(1);
        }

        memset(sendline, 0, MAX_DGRAM_SIZE);
    }

    pk = create_packet(send_next, rcv_next[thread], 0, NULL, DATA);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &to[thread]) == -1) {
        fprintf(stderr, "Error: couldn't send the filename.\n");
        return;
    }

    pthread_mutex_lock(&wnd_lock[thread]);
    update_window(pk, wnd, thread, &usable_wnd[thread]);    
    pthread_mutex_unlock(&wnd_lock[thread]);

    //Wait for ACK until every packet is correctly received
    while(usable_wnd[thread] != INIT_WND_SIZE){
        sleep(1);
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


void response_put(int sd, char* filename, Client* cli_info)
{
    /**VARIABLE DEFINITIONS**/

    //ADDRESS
    struct sockaddr_in addr = cli_info->addr;     
    socklen_t addrlen = sizeof(addr);  

    //FILE    
    FILE *fp;                                       
    char path[strlen("files/")+strlen(filename)];                                       
    ssize_t write_size = 0;
    int i, j, size;

    //RECEPTION
    unsigned long send_next = cli_info->pack->ack_num;
    Packet* rcv_buffer[MAX_RVWD_SIZE] = {NULL}; 
    int thread = cli_info->server;  

    /**END**/

    rcv_next[thread] = cli_info->pack->seq_num + (unsigned long)cli_info->pack->data_size + 1; 

    //Send ACK for the request
    if(send_ack(sd, addr, addrlen, send_next, rcv_next[thread], &to[thread])){
        return;
    }
    
    //Open the file
    memset(path, 0, strlen(path));
    strcat(path, "files/");
    strcat(path, filename);

    fp = fopen(path, "wb+");
    if (fp == NULL) {
        perror("fopen() failed");
        return;
    }

    //Receive file size
    while(1){
        pthread_mutex_lock(&mux_avb[thread]);
        
        if(cli_info->pack->seq_num == rcv_next[thread]){
            sscanf(cli_info->pack->data, "%d", &size);
            rcv_next[thread] += (unsigned long)cli_info->pack->data_size;

            //Send ACK
            if(send_ack(sd, addr, addrlen, send_next, rcv_next[thread], &to[thread])){
                return;
            }
            pthread_mutex_unlock(&mux_free[thread]);
            break;
        }
        pthread_mutex_unlock(&mux_free[thread]);
    }
    
    /* Receive data in chunks of 65000 bytes */
    while(write_size != size){
        pthread_mutex_lock(&mux_avb[thread]);
  
        if(cli_info->pack->seq_num == rcv_next[thread]){
            //Check if transmission has ended.
            if(cli_info->pack->data_size == 0){
                //Data transmission completed: check if the receive buffer is empty
                if(rcv_buffer[0] == NULL){
                    printf("\rTransmission has completed successfully.\n");
                    fflush(stdout);
                    fclose(fp);
                    return;
                }

            } else {
                //PACKET IN ORDER
                if (cli_info->rtx_pack->seq_num == rcv_next[thread]) {
                    write_size += fwrite(cli_info->rtx_pack->data, 1, cli_info->rtx_pack->data_size, fp);

                    rcv_next[thread] += (unsigned long)cli_info->rtx_pack->data_size;

                    //Check if there are other buffered data to read
                    i=0;
                    j=0;
                    while((rcv_buffer[i] != NULL) && (rcv_buffer[i]->seq_num == cli_info->rtx_pack->seq_num + (unsigned long)cli_info->rtx_pack->data_size)){
                        write_size += fwrite(rcv_buffer[i]->data, 1, rcv_buffer[i]->data_size, fp);
                        rcv_next[thread] += (unsigned long)rcv_buffer[i]->data_size;
                
                        memset(cli_info->rtx_pack, 0, sizeof(Packet));                    
                        memcpy(cli_info->rtx_pack, rcv_buffer[i], sizeof(Packet));                    
                        memset(&rcv_buffer[i], 0, sizeof(Packet*));
                
                        i++;
                    }
                

                }else if (cli_info->pack->seq_num == rcv_next[thread]){
                    write_size += fwrite(cli_info->pack->data, 1, cli_info->pack->data_size, fp);

                    rcv_next[thread] += (unsigned long)cli_info->pack->data_size;

                    //Check if there are other buffered data to read
                    i=0;
                    j=0;
                    while((rcv_buffer[i] != NULL) && (rcv_buffer[i]->seq_num == cli_info->pack->seq_num + (unsigned long)cli_info->pack->data_size)){
                        write_size += fwrite(rcv_buffer[i]->data, 1, rcv_buffer[i]->data_size, fp);
                        rcv_next[thread] += (unsigned long)rcv_buffer[i]->data_size;
                
                        memset(cli_info->pack, 0, sizeof(Packet));                    
                        memcpy(cli_info->pack, rcv_buffer[i], sizeof(Packet));                    
                        memset(&rcv_buffer[i], 0, sizeof(Packet*));
                
                        i++;
                    }
                }

                //Reorder the buffer
                if(i != 0){
                    while((i+j < MAX_RVWD_SIZE) && (rcv_buffer[i+j] != NULL)){
                        rcv_buffer[j] = rcv_buffer[i+j];
                        memset(&rcv_buffer[i+j], 0, sizeof(Packet*));
                        j++;
                    }
                } 
            }           

        //PACKET OUT OF ORDER
        } else if (cli_info->pack->seq_num > rcv_next[thread]){
            //Store the packet
            store_rwnd(cli_info->pack, rcv_buffer, MAX_RVWD_SIZE);

        } 

        //Send ACK for the last correctly received packet
        if(send_ack(sd, addr, addrlen, send_next, rcv_next[thread], &to[thread])){
            return;
        }
        pthread_mutex_unlock(&mux_free[thread]);                   
    }
    fflush(stdout);
    fclose(fp);
}

/*
 ----------------------------------------------------------------------------------------------------------------------------------------------
 MAIN & THREAD HANDLER
 ----------------------------------------------------------------------------------------------------------------------------------------------
 */
    
void* thread_service(void* arg){

    /**VARIABLE DEFINITIONS**/

    //PARAMETER
    int id = *((int*)arg);

    //SERVICE
	struct sembuf oper;         
	Client* recipient;          
	char* cmd, *filename;       

    /**END**/

    //Semaphore's fixed settings
    oper.sem_num = 0;
    oper.sem_op = -1;
    oper.sem_flg = 0;
    
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
        to[id] = recipient->to_info;
        pthread_mutex_unlock(&list_mux);
		
	    //Retrieve request
        pthread_mutex_lock(&mux_avb[id]);
	    cmd = strtok(recipient->pack->data, " \n");
        pthread_mutex_unlock(&mux_free[id]);

	    printf("Selected request: %s\n", cmd);
	    
        //SERVE THE CLIENT
	    if (strcmp(cmd, "list") == 0) {
	        //Respond to LIST
	        response_list(listensd, recipient); 

	    } else if (strcmp(cmd, "get") == 0) {
	        //Respond to GET
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	continue;
	        }

	        filename = strdup(cmd);
	        response_get(listensd, filename, recipient);

	    } else if (strcmp(cmd, "put") == 0) {
	        //Respond to PUT
	        if((cmd = strtok(NULL, " \n")) == NULL){
	        	continue;
	        } 
	        filename = strdup(cmd);
	        response_put(listensd, filename, recipient);

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

    //CONCURRENCY
    key_t ksem_client = 324;    
    struct sembuf ops;          
    
    //ADDRESSES
    struct sockaddr_in cliaddr;
    struct sockaddr_in servaddr;
    socklen_t cliaddrlen = sizeof(cliaddr);
    socklen_t servaddrlen = sizeof(servaddr);

    //TIMER SIGNAL
    Timeout time_temp;		
    struct sigevent sev;
    timer_t main_timerid;   
    struct sigaction sa;
            
    //THREAD POOL   
    pthread_t tid[THREAD_POOL];   
    int thread, i; 
    int* indexes;

    //HANDSHAKE
    Packet* pk_hds = NULL;
    unsigned long init_seq;  

    //RECEPTION
    Packet* pk_rcv;      

    /**END**/

    /**INITIALIZAZION OF THE STRUCTURES**/

    //Create socket
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
    while(((sem_client = semget(ksem_client, 1, IPC_CREAT|IPC_PRIVATE|0600)) == -1) || (semctl(sem_client, 0, SETVAL, 0) == -1)){
        if(errno == EINTR){
            continue;
        }else{
            fprintf(stderr, "semget() failed");
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
        pthread_mutex_init(&wnd_lock[i], 0);
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

        if(pthread_create(&tid[i], 0, (void*)thread_service, (void*)(&indexes[i]))){
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
    
    //Initialize timeout values
    memset(&time_temp, 0, sizeof(time_temp));

    //Construct signal handlers
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timeout_handler;
    if(sigaction(SIGRTMIN, &sa, NULL) == -1){
        failure("Sigaction failed.\n");
    }

    //Create timers for workers and main thread
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    for(i=0; i<THREAD_POOL; i++){
        sev.sigev_value.sival_ptr = &timerid[i]; //Allows handler to get the ID of the timer that causes a timeout
        if(timer_create(CLOCK_REALTIME, &sev, &timerid[i]) == -1){
            failure("Timer creation failed.\n");
        }
    }
    sev.sigev_value.sival_ptr = main_timerid;
    if(timer_create(CLOCK_REALTIME, &sev, &main_timerid) == -1){
        failure("Timer creation failed.\n");
    }
    
    /**EXECUTING SERVER**/
    printf("\033[0;34mWaiting for a request...\033[0m\n");

    while(1) {
    
        //Receive request from client
        if (recv_packet(pk_rcv, listensd, (struct sockaddr*)&cliaddr, cliaddrlen, &time_temp) == -1) {
            fprintf(stderr, "\033[0;31mError: couldn't receive packet.\033[0m\n");
            free(pk_rcv);
            exit(EXIT_FAILURE);

        } else {
            if (pk_rcv->type == SYN) {
                //Received a new connection request
                if(handshake(pk_hds, &init_seq, listensd, &cliaddr, cliaddrlen, &time_temp, main_timerid) == -1){
                    fprintf(stderr, "\033[0;31mConnection failure.\033[0m\n");
                    continue;

                }else{
                    printf("Connection enstablished.\n");

                    //Add new client address to the list
                    pthread_mutex_lock(&list_mux);
                    insert_client(&cliaddr_head, cliaddr, pk_hds, time_temp);
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
                //New packet from a queued client
                dispatch_client(cliaddr_head, cliaddr, &thread);

                if(pk_rcv->type == ACK){
                    
                    //New ACK received
                    pthread_mutex_lock(&wnd_lock[thread]);
                    incoming_ack(thread, cliaddr_head, pk_rcv, wnd, &usable_wnd[thread], time_temp, timerid[thread], &to[thread]);
                    pthread_mutex_unlock(&wnd_lock[thread]);

                }else if(pk_rcv->type == FIN){
                    //Received a request for connection demolition
                    if(demolition(listensd, &cliaddr, cliaddrlen, &time_temp, timerid[thread]) == -1){
                        fprintf(stderr, "\033[0;31mDemolition failure.\033[0m\n");
                        continue; 
                    }else{
                        printf("Connection closed.\n");

                        //Remove client from the list
                        pthread_mutex_lock(&list_mux);
                        remove_client(&cliaddr_head, cliaddr);
                        pthread_mutex_unlock(&list_mux);
                    }

                    printf("\033[0;34mWaiting for a request...\033[0m\n");
                }else{
                    //New DATA packet
                    pthread_mutex_lock(&mux_free[thread]);               
                    update_packet(cliaddr_head, rtx_packs[thread], thread, pk_rcv, time_temp);
                    pthread_mutex_unlock(&mux_avb[thread]);
                }
            }   
        }

        pk_rcv = (Packet*)malloc(sizeof(Packet));
        if (pk_rcv == NULL) {
            fprintf(stderr, "Error: couldn't malloc packet.\n");
            exit(EXIT_FAILURE);
        }                                                                       
    }
    
    free(pk_rcv);
    exit(0);
}