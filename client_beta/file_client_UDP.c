//
//  client.c
//  
//
//  Created by Diana Pasquali on 20/07/2020.

#define pragma once
#define _GNU_SOURCE

#include "client.h"

#define SERV_PORT 5193
#define DIR_PATH files
#define LOGDIM	50

Packet* wnd_base[MAX_CONC_REQ];     //Packet at the base of the transmission window (global for retransmission)

Timeout time_temp[MAX_CONC_REQ];
timer_t timerid[MAX_CONC_REQ];

int sockfd[MAX_CONC_REQ];
socklen_t addrlen = sizeof(struct sockaddr_in);
struct sockaddr_in servaddr;

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 LOG
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void* log_thread(void* p){
	char buff[MAXLINE];

	sprintf(buff, "mate-terminal -e 'tail -F %s --pid %d'", (char*)p, getpid());
    system(buff);

   	pthread_exit(0);
}

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 RETRANSMISSION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void retransmission(timer_t* ptr, bool fast_rtx, int thread){

    int id = 0;
    Packet* pk = NULL;

    if (fast_rtx == false) {    //Retransmission due to timeout
        //Retrieve the thread for which the timer has expired
        for(int i=0; i<MAX_CONC_REQ; i++){
            if(timerid+i == ptr){
                id = i;
                break;
            }
        }
    }else{
        id = thread;
    }

    //Fetch the packet to retransmit
    pk = wnd_base[id];

    //printf("Retransmitting packet #%lu\n", pk->seq_num);
    if (send_packet(pk, sockfd[thread], (struct sockaddr*)&servaddr, addrlen, &time_temp[id]) == -1) {
        fprintf(stderr, "\033[0;31mError: couldn't send packet #%lu.\033[0m\n", pk->seq_num);
        return;
    }
    //printf("\r ** Packet #%lu correctly retransmitted.        \t\t", pk->seq_num);
    fflush(stdout);

    arm_timer(&time_temp[id], timerid[id], 0);

    return;
}

/* 
 ------------------------------------------------------------------------------------------------------------------------------------------------
 LIST OPERATION
 ------------------------------------------------------------------------------------------------------------------------------------------------
 */
 
int request_list(int id, unsigned long send_next, unsigned long rcv_next)
{
    /**VARIABLE DEFINITIONS**/
    int i, j;
    Packet *pk;
    Packet* rcv_buffer[MAX_RVWD_SIZE] = {NULL};
    /**END**/

    //Prepare to receive packets
    printf("\033[1;34m--- FILELIST ---\033[0m\n");

    pk = (Packet*)malloc(sizeof(Packet));
    if (pk == NULL) {
    	failure("Error: couldn't malloc packet.");
    }   
 
    //Retreive packets
    while (recv_packet(pk, sockfd[id], (struct sockaddr*)&servaddr, addrlen, &time_temp[id]) != -1) {

        if(pk->seq_num == rcv_next){
    	//Packet received in order: gets read by the client
    	//printf("Received packet #%lu in order.\n", pk->seq_num);

            //Check if transmission has ended.
            if(pk->data_size == 0){
                //Data transmission completed: check if the receive buffer is empty
                if(rcv_buffer[0] == NULL){
                    printf("Transmission has completed successfully.\n");
                    fflush(stdout);
                    return 0;
                }

                //Send ACK for the last correctly received packet
                if(send_ack(sockfd[id], servaddr, addrlen, send_next, rcv_next, &time_temp[id])){
                    return 1;
                }

            }else{
                printf("%s\n", pk->data);
                rcv_next += (unsigned long)pk->data_size;

                //Check if there are other buffered data to read: if so, read it, reorder the buffer and update ack_num
                i=0;
                j=0;
                while((rcv_buffer[i] != NULL) && (rcv_buffer[i]->seq_num == pk->seq_num + (unsigned long)pk->data_size)){
                    printf("%s\n", rcv_buffer[i]->data);
                    rcv_next += (unsigned long)rcv_buffer[i]->data_size;
                    
                    memset(pk, 0, sizeof(Packet));                    
                    memcpy(pk, rcv_buffer[i], sizeof(Packet));                   
                    memset(&rcv_buffer[i], 0, sizeof(Packet*));
                    
                    i++;
                }

                if(i != 0){
                    while((i+j < MAX_RVWD_SIZE) && (rcv_buffer[i+j] != NULL)){
                        rcv_buffer[j] = rcv_buffer[i+j];
                        memset(&rcv_buffer[i+j], 0, sizeof(Packet*));
                        j++;
                    }
                }             

                //Send ACK for the last correctly received packet
                if(send_ack(sockfd[id], servaddr, addrlen, send_next, rcv_next, &time_temp[id])){
                    return 1;
                }

                memset(pk->data, 0, PAYLOAD);
            }

        } else if(pk->seq_num > rcv_next){
            //Packet received out of order: store packet in the receive buffer
            //printf("Received packet #%lu out of order.\n", pk->seq_num);
         	store_rwnd(pk, rcv_buffer, MAX_RVWD_SIZE);
         		
            //Send ACK for the last correctly received packet
            if(send_ack(sockfd[id], servaddr, addrlen, send_next, rcv_next, &time_temp[id])){
                return 1;
            }
  	
        } else{
            //Received duplicated packet
            //printf("Received duplicated packet #%lu.\n", pk->seq_num);

            //Send ACK for the last correctly received packet
            if(send_ack(sockfd[id], servaddr, addrlen, send_next, rcv_next, &time_temp[id])){
                return 1;
            }
        }

        //Change the pk address or the receive buffer will have all identical packets: waste of memory -> looking for alternatives
        pk = (Packet*)malloc(sizeof(Packet));
        if (pk == NULL) {
            fprintf(stderr, "Error: couldn't malloc packet.\n");
            return 0;
        }
    }

    free(pk);
    return 1;
}


/*
 --------------------------------------------------------------------------------------------------------------------------------
 GET OPERATION
 --------------------------------------------------------------------------------------------------------------------------------
 */

int request_get(int id, char* filename, unsigned long send_next, unsigned long rcv_next)
{
    /**VARIABLE DEFINITIONS**/
    FILE *fp;
    ssize_t write_size = 0;
    int i, j;
    Packet *pk;
    Packet *rcv_buffer[MAX_RVWD_SIZE] = {NULL};
    /**END**/

    //Initialize packet
    pk = (Packet*)malloc(sizeof(Packet));
    if (pk == NULL) {
        fprintf(stderr, "Error: couldn't malloc packet.\n");
        return 0;
    }

    //Open the file to write on
    fp = fopen(filename, "w+");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file %s.\n", filename);
        return 1;
    }
    
    /* Receive data in chunks of 65000 bytes */
    while(recv_packet(pk, sockfd[id], (struct sockaddr*)&servaddr, addrlen, &time_temp[id]) != -1){
        //Check if the received packet is in order with the stream of bytes
        if(pk->seq_num == rcv_next){
            //Check if transmission has ended.
            if(pk->data_size == 0){
                //Data transmission completed: check if the receive buffer is empty
                if(rcv_buffer[0] == NULL){
                    printf("\rTransmission has completed successfully.");
                    fflush(stdout);
                    fclose(fp);
                    return 0;
                }

                //Send ACK for the last correctly received packet
                if(send_ack(sockfd[id], servaddr, addrlen, send_next, rcv_next, &time_temp[id])){
                    return 1;
                }

            }else{
                //Packet receiver in order
                write_size += fwrite(pk->data, 1, pk->data_size, fp);
                rcv_next += (unsigned long)pk->data_size;

                //Check if there are other buffered data to read: if so, read it, reorder the buffer and update ack_num
                i=0;
                j=0;
                while((rcv_buffer[i] != NULL) && (rcv_buffer[i]->seq_num == pk->seq_num + (unsigned long)pk->data_size)){
                    //Convert byte array to file
                    write_size += fwrite(rcv_buffer[i]->data, 1, rcv_buffer[i]->data_size, fp);
                    rcv_next += (unsigned long)rcv_buffer[i]->data_size;
                    
                    memset(pk, 0, sizeof(Packet));                   
                    memcpy(pk, rcv_buffer[i], sizeof(Packet));                   
                    memset(&rcv_buffer[i], 0, sizeof(Packet*));
                    
                    i++;
                }

                if(i != 0){
                    while((i+j < MAX_RVWD_SIZE) && (rcv_buffer[i+j] != NULL)){
                        rcv_buffer[j] = rcv_buffer[i+j];
                        memset(&rcv_buffer[i+j], 0, sizeof(Packet*));
                        j++;
                    }
                }   
     
                //Send ACK for the last correctly received packet
                if(send_ack(sockfd[id], servaddr, addrlen, send_next, rcv_next, &time_temp[id])){
                    return 1;
                }
            }
            
        }else if(pk->seq_num > rcv_next){
            //Packet received out of order: store packet in the receive buffer
            store_rwnd(pk, rcv_buffer, MAX_RVWD_SIZE);
                
            //Send ACK for the last correctly received packet
            if(send_ack(sockfd[id], servaddr, addrlen, send_next, rcv_next, &time_temp[id])){
                return 1;
            }
    
        } else{
            //Received duplicated packet
            if(send_ack(sockfd[id], servaddr, addrlen, send_next, rcv_next, &time_temp[id])){
                return 1;
            }
        }

        //Reset the packet
        pk = (Packet*)malloc(sizeof(Packet));
        if (pk == NULL) {
            fprintf(stderr, "Error: couldn't malloc packet.\n");
            return 0;
        }

        //Print progress
        //printf("\033[1;34m%ld bytes downloaded.\033[0m\n", write_size);
    }

    return 0;
}

/* 
 --------------------------------------------------------------------------------------------------------------------------------
 PUT OPERATION
 --------------------------------------------------------------------------------------------------------------------------------
 */

int request_put(int id, char *filename, unsigned long send_next, unsigned long rcv_next, short usable_wnd)
{
    /**VARIABLE DEFINITIONS**/
    FILE *fp, *log_d;
    ssize_t read_size;
    char* sendline;

    char logname[LOGDIM];
    pthread_t logtid;

    Packet *pk, *pack;
    Packet* wnd[INIT_WND_SIZE] = {NULL};

    unsigned long last_ack_rcvd;        		//The sequence number of the last ack received (to fast retransmit)
    int done, i, acks_count = 0; 
    /**END**/

    printf("Uploading %s\n", filename);

    //Open the log
    sprintf(logname, "log/log%d.log", id);
    log_d = fopen(logname, "w+");
    if (log_d == NULL) {
        fprintf(stderr, "Error: cannot open file %s.\n", logname);
        return 1;
    }

    if(pthread_create(&logtid, 0, log_thread, logname)){
    	fprintf(stderr, "pthread_create() failed");
        return 1;
    }
    
    //Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file %s.\n", filename);
        return 1;
    }

    //Get file size to print progress
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    //Allocate space
    if((sendline = (char*)malloc(MAX_DGRAM_SIZE)) == NULL){
        perror("Malloc() failed.");
        exit(-1);
    }
    memset(sendline, 0, MAX_DGRAM_SIZE);

    pack = (Packet*)malloc(sizeof(Packet));
    if (pack == NULL) {
        fprintf(stderr, "Error: couldn't malloc packet.\n");
        return 0;
    }
  
    //Send the file to the client
    while(!feof(fp)) {       
        //Read from the file and send data   
        read_size = fread(sendline, 1, PAYLOAD, fp);

        pk = create_packet(send_next, rcv_next, read_size, sendline, DATA);    
        if (send_packet(pk, sockfd[id], (struct sockaddr*)&servaddr, addrlen, &time_temp[id]) == -1) {
            free(sendline);
            free(pk);
            fprintf(stderr, "Error: couldn't send data.\n");
            return 1;
        }
        send_next += (unsigned long)pk->data_size;

        //Print progress
        done += read_size;
        double percent = (double)done/size;
		print_progress(percent, log_d);

        update_window(pk, wnd, &usable_wnd);
        wnd_base[id] = wnd[0];

        //If it's the the only packet in the window, start timer
        if(usable_wnd == INIT_WND_SIZE-1){
            arm_timer(&time_temp[id], timerid[id], 0);
        }

        while(!usable_wnd){
            //FULL WINDOW: wait for ACK until either it is received or the timer expires           
            i = 0;  
            while(try_recv_packet(pack, sockfd[id], (struct sockaddr*)&servaddr, addrlen, &time_temp[id]) != -1){
                if(pack->type == ACK){
                    //printf("Received ACK #%lu.\n", pack->ack_num);
                    if (last_ack_rcvd != pack->ack_num) {
                        // New ack received
                        //printf("\033[1;32mNew ack received.\033[0m\n"); //debug
                        last_ack_rcvd = pack->ack_num; 
                    } else {
                        // Increase acks counter
                        acks_count++;
                        if (acks_count == 3) {
                            // Fast retransmission
                            disarm_timer(timerid[id]);
                            retransmission(&timerid[id], true, id);
                        }
                    }

                    //Count acknowledged packets
                    while((i < INIT_WND_SIZE) && (pack->ack_num > wnd[i]->seq_num)){ 
                        i++;
                    }
                }
            }

            if(i != 0){
                if((i+1 < INIT_WND_SIZE+1) && (wnd[i+1] != NULL)){
                	//Packets still in flight
                    arm_timer(&time_temp[id], timerid[id], 0);
                }else{ 
                	//No packet still in flight
                    disarm_timer(timerid[id]);
                    timeout_interval(&time_temp[id]);  
                }

                //Update the transmission window
                refresh_window(wnd, i, &usable_wnd);
                wnd_base[id] = wnd[0];
            }

        }

        memset(sendline, 0, MAX_DGRAM_SIZE);
    }

    pk = create_packet(send_next, rcv_next, 0, NULL, DATA);
    if (send_packet(pk, sockfd[id], (struct sockaddr*)&servaddr, addrlen, &time_temp[id]) == -1) {
        fprintf(stderr, "Error: couldn't send the filename.\n");
        return 1;
    }

    update_window(pk, wnd, &usable_wnd);
    wnd_base[id] = wnd[0];

    //Wait for ACK until every packet is correctly received
    while(usable_wnd != INIT_WND_SIZE-1){
        i = 0;  
        while(try_recv_packet(pack, sockfd[id], (struct sockaddr*)&servaddr, addrlen, &time_temp[id]) != -1){

            if(pack->type == ACK){
                //printf("Received ACK #%lu.\n", pack->ack_num);
                if (last_ack_rcvd != pack->ack_num) {
                        // New ack received
                       	//printf("\033[1;32mNew ack received.\033[0m\n"); //debug
                  		last_ack_rcvd = pack->ack_num; 
                    } else {
                        // Increase acks counter
                        acks_count++;
                        if (acks_count == 3) {
                            // Fast retransmission
                            disarm_timer(timerid[id]);
                            retransmission(&timerid[id], true, id);
                        }
                    }

                while((i < INIT_WND_SIZE) && (pack->ack_num > wnd[i]->seq_num)){
                    i++;
                }
            }
        }

        if(i != 0){
            if((i+1 < INIT_WND_SIZE+1) && (wnd[i+1] != NULL)){
                arm_timer(&time_temp[id], timerid[id], 0);
            }else{ 
                disarm_timer(timerid[id]);
                timeout_interval(&time_temp[id]);  
            }
            refresh_window(wnd, i, &usable_wnd);
            wnd_base[id] = wnd[0];
        }
	}
    
    free(sendline);
    free(pk);
    free(pack);
    fclose(fp);
    fclose(log_d);

    printf("Done uploaing %s\n", filename);

    return 0;
}

/*
 ------------------------------------------------------------------------------------------------------------------------------------------------
 MAIN & THREAD HANDLER
 -------------------------------------------------------------------------------------------------------------------------------------------------
 */

void* thread_function(void* arg){

    /**VARIABLE DEFINITIONS**/
    char cmd[MAXLINE]; 
    char filename[MAXLINE];
    char request[MAXLINE]; 
    char *tok; 

    unsigned long send_next;
    unsigned long rcv_next;

    int res, thread;
    Packet *pk = NULL;

    short usable_wnd = INIT_WND_SIZE; //The amount of packets that can be sent 
    /**END**/

    strcpy(cmd, (char*)arg);

    //Retrieve ID       
    tok = strtok(cmd, " \n");
    sscanf(tok, "%d", &thread);

    /** 3-WAY HANDSHAKE **/
    handshake(pk, &send_next, &rcv_next, sockfd[thread], &servaddr, addrlen, &time_temp[thread], timerid[thread]);
    //printf("Connection open\n");
    
    //Retrieve request
    tok = strtok(NULL, " \n");
    strcpy(request, tok);

    tok = strtok(NULL, "\n");
    if(tok != NULL){
        strcpy(filename, tok);
    }else{
        filename[0] = '\0';
    }

    strcpy(cmd, request);
    strcat(cmd, " ");
    strcat(cmd, filename);

    //Send request
    pk = create_packet(send_next, rcv_next, strlen(cmd), cmd, DATA);    
    if (send_packet(pk, sockfd[thread], (struct sockaddr*)&servaddr, addrlen, &time_temp[thread]) == -1) {
        fprintf(stderr, "Error: couldn't send packet #%lu.\n", pk->seq_num);
        free(pk);
        pthread_exit((void*)-1);
    }
    send_next += (unsigned long)pk->data_size + 1;

    wnd_base[thread] = pk;
    usable_wnd--;

    arm_timer(&time_temp[thread], timerid[thread], 0);

    //Wait for the ACK related to the request
    while(1){
        if(recv_packet(pk, sockfd[thread], (struct sockaddr*)&servaddr, addrlen, &time_temp[thread]) == -1) {
            failure("Error: couldn't receive ack.");
        } else {            
            if ((pk->type == ACK) && (pk->ack_num == send_next)) {
                disarm_timer(timerid[thread]);
                wnd_base[thread] = NULL;
                usable_wnd++;
                break;                  
            }
        }
    }        
    free(pk);

    //Compute the timeout interval for exchange: REQUEST, ACK
    timeout_interval(&time_temp[thread]);

    if (strcmp(request, "list") == 0) {
        //Execute LIST
        res = request_list(thread, send_next, rcv_next);
    
    } else if (strcmp(request, "get") == 0) {
        //Execute GET
        res = request_get(thread, filename, send_next, rcv_next);

    } else if (strcmp(request, "put") == 0) {
        //Execute PUT
        res = request_put(thread, filename, send_next, rcv_next, usable_wnd);

    } else {
        failure("\033[0;31mError: couldn't retrieve input.\033[0m\n");
    }

    if (res == 1) {
        failure("\033[0;31mInput function error.\033[0m\n");
        pthread_exit((void*)-1);

    } else { 
        wnd_base[thread] = NULL;

        demolition(send_next, rcv_next, sockfd[thread], &servaddr, addrlen, &time_temp[thread], timerid[thread]);
        printf("Connection closed\n");
        pthread_exit(0);      
    }
}

int main(int argc, char* argv[])
{   
    //Check command line
    if (argc < 2) {
        failure("Utilization: ./client <server IP>");
    }

    /**VARIABLE DEFINITIONS**/      
    char buff[MAXLINE];
    char filename[MAXLINE];
    
    struct sigevent sev;
    struct sigaction sa;

    pthread_t tid[MAX_CONC_REQ] = {0};

    int oper, i, num_threads = 0;
    /**END**/

    //Create socket
    for(i=0; i<MAX_CONC_REQ; i++){
        while ((sockfd[i] = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            if(check_failure("\033[0;31mSocket() failed\033[0m")){
                continue;
            }
        }
    }
    
    //Initialize address values
    memset((void*)&servaddr, 0, sizeof(struct sockaddr_in));    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
        failure("error in inet_pton.");
    }

    //Construct handlers
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timeout_handler;
    if(sigaction(SIGRTMIN, &sa, NULL) == -1){
        failure("Sigaction failed.\n");
    }

    memset(&time_temp, 0, sizeof(time_temp));

    //Create timers
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    for(i=0; i<MAX_CONC_REQ; i++){
        sev.sigev_value.sival_ptr = &timerid[i]; //Allows handler to get the ID of the timer that causes a timeout
        if(timer_create(CLOCK_REALTIME, &sev, &timerid[i]) == -1){
            failure("Timer creation failed.\n");
        }
    }

    //Read from socket until EOF is found   
    while (1) {
    	printf("\n\n\033[0;34mChoose an operation:\n1. List\n2. Get\n3. Put\n0. Quit\n\n\033[0m");

        while(scanf("%d", &oper) != 1);
        while(getchar()!='\n');

        switch(oper){
            case 1:
                sprintf(buff, "%d ", num_threads);
                strcat(buff, "list\n");

                if(pthread_create(&tid[num_threads], 0, (void*)thread_function, (void*)buff)){
                    fprintf(stderr, "pthread_create() failed");
                    for(i=0; i<MAX_CONC_REQ; i++){
                        close(sockfd[i]);
                    }
                    exit(-1);
                } 
                num_threads++;

                break;

            case 2:
                sprintf(buff, "%d ", num_threads);
                strcat(buff, "get ");

                printf("\033[0;34mWhich file do you want to download?\n\033[0m");
                scanf("%s", filename);

                strcat(buff, filename);

                if(pthread_create(&tid[num_threads], 0, (void*)thread_function, (void*)buff)){
                    fprintf(stderr, "pthread_create() failed");
                    for(i=0; i<MAX_CONC_REQ; i++){
                        close(sockfd[i]);
                    }
                    exit(-1);
                }   
                num_threads++;
                //printf("thread #%ld created.\n", tid[num_threads]);  //debug
                break;

            case 3:
                sprintf(buff, "%d ", num_threads);
                strcat(buff, "put ");

                printf("\033[0;34mWhich file do you want to upload?\n\033[0m");
                scanf("%s", filename);

                strcat(buff, filename);

                if(pthread_create(&tid[num_threads], 0, (void*)thread_function, (void*)buff)){
                    fprintf(stderr, "pthread_create() failed");
                    for(i=0; i<MAX_CONC_REQ; i++){
                        close(sockfd[i]);
                    }
                    exit(-1);
                }
                num_threads++;
                //printf("thread #%ld created.\n", tid[num_threads]); //debug
                break;

            case 0:
                //Wait for every active thread to end
                for(i=0; i<MAX_CONC_REQ; i++){
                    if(tid[i] != 0){
                        if(pthread_join(tid[i], NULL)){
                            fprintf(stderr, "pthread_join() failed");
                            for(i=0; i<MAX_CONC_REQ; i++){
                                close(sockfd[i]);
                            }
                            exit(-1);
                        }
                    }else{
                        break;
                    }
                }  

                exit(0);
            default:
                printf("Invalid operation.\n\n");
                break;
        }

        if(num_threads == MAX_CONC_REQ){
            printf("Saturated capacity. Please wait...\n");
            for(i=0; i<MAX_CONC_REQ; i++){
                if(tid[i] != 0){
                    if(pthread_join(tid[i], NULL)){
                        fprintf(stderr, "pthread_join() failed");
                        for(i=0; i<MAX_CONC_REQ; i++){
                            close(sockfd[i]);
                        }
                        exit(-1);
                    }
                    tid[i] = 0;
                    num_threads--;
                }else{
                    break;
                }
            }
            //printf("\n\n\033[0;34mChoose an operation:\n1. List\n2. Get\n3. Put\n0. Quit\n\n\033[0m");
        }


    }
    
    for(i=0; i<MAX_CONC_REQ; i++){
        close(sockfd[i]);
    }
    exit(0);
}
