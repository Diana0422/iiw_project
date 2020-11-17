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

socklen_t addrlen;
struct sockaddr_in servaddr;

Timer_node* timer_head;
Packet_node* wnd_head;
Socket_node* sock_head;

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 RETRANSMISSION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void retransmission(timer_t* ptr){

    int i = 0;
    Packet_node* pk = wnd_head;
    Timer_node* tm = timer_head;
    Socket_node* sock = sock_head;

    while(tm->next != NULL){
        if(tm->timerid == ptr){
            break;
        }
        tm = tm->next;
        pk = pk->next;
        sock = sock->next;
        i++;
    }

    //printf("Retransmitting packet #%lu\n", pk->pk->seq_num);
    if (send_packet(pk->pk, sock->sockfd, (struct sockaddr*)&servaddr, addrlen, &(tm->to)) == -1) {
        fprintf(stderr, "\033[0;31mError: couldn't send packet #%lu.\033[0m\n", (pk->pk)->seq_num);
        return;
    }
}

/* 
 ------------------------------------------------------------------------------------------------------------------------------------------------
 ACK THREAD
 ------------------------------------------------------------------------------------------------------------------------------------------------
 */

void* ack_thread(void* arg){

    //Variables
    char* tok;
    char cmd[MAXLINE];
    int sock;
    Timer_node* timer;
    Packet** wnd;
    Packet_node* base;
    short* usable;
    Packet* pk_rcv;
    int acks = 0;
    unsigned long last_ack = 0;
    Timeout ack_to;
    pthread_mutex_t* mux;
    //End

    strcpy(cmd, (char*)arg);
    //Retrieve socket
    tok = strtok(cmd, " \n");
    sscanf(tok, "%d", &sock);
    //Retrieve window base
    tok = strtok(NULL, " \n");
    sscanf(tok, "%p", &usable);
    //Retrieve window
    tok = strtok(NULL, " \n");
    sscanf(tok, "%p", &wnd);
    //Retrieve window base
    tok = strtok(NULL, " \n");
    sscanf(tok, "%p", &base);
    //Retrieve mutex
    tok = strtok(NULL, " \n");
    sscanf(tok, "%p", &mux);
    //Retrieve timer
    tok = strtok(NULL, " \n");
    sscanf(tok, "%p", &timer);

    pk_rcv = (Packet*)malloc(sizeof(Packet));
    if(pk_rcv == NULL){
        perror("malloc failed");
        exit(-1);
    }

    while(1) {
        memset(pk_rcv, 0, sizeof(Packet));
    
        //Receive ack for the client
        if (recv_packet(pk_rcv, sock, (struct sockaddr*)&servaddr, addrlen, &ack_to) == -1) {
            fprintf(stderr, "Error: couldn't receive packet.\n");
            free(pk_rcv);
            exit(EXIT_FAILURE);
        }else{
            if(pk_rcv->type == ACK){ 
            	disarm_timer(timer->timerid);
                pthread_mutex_lock(mux);
                incoming_ack(pk_rcv->ack_num, &acks, &last_ack, wnd, usable, ack_to, timer); 
                //printf("Usable ref = %d\n", *usable); 
                base->pk = wnd[0];  
                pthread_mutex_unlock(mux);  
                arm_timer(timer, 0);         
            }
        }                                                                    
    }

    free(pk_rcv);
    pthread_exit(0);
}

/* 
 ------------------------------------------------------------------------------------------------------------------------------------------------
 LIST OPERATION
 ------------------------------------------------------------------------------------------------------------------------------------------------
 */
 
int request_list(int sock, unsigned long send_next, unsigned long rcv_next, Timer_node* timer)
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
    while (recv_packet(pk, sock, (struct sockaddr*)&servaddr, addrlen, &(timer->to)) != -1) {

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
                if(send_ack(sock, servaddr, addrlen, send_next, rcv_next, &(timer->to))){
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
                if(send_ack(sock, servaddr, addrlen, send_next, rcv_next, &(timer->to))){
                    return 1;
                }

                memset(pk->data, 0, PAYLOAD);
            }

        } else if(pk->seq_num > rcv_next){
            //Packet received out of order: store packet in the receive buffer
            //printf("Received packet #%lu out of order.\n", pk->seq_num);
         	store_rwnd(pk, rcv_buffer, MAX_RVWD_SIZE);
         		
            //Send ACK for the last correctly received packet
            if(send_ack(sock, servaddr, addrlen, send_next, rcv_next, &(timer->to))){
                return 1;
            }
  	
        } else{
            //Received duplicated packet
            //printf("Received duplicated packet #%lu.\n", pk->seq_num);

            //Send ACK for the last correctly received packet
            if(send_ack(sock, servaddr, addrlen, send_next, rcv_next, &(timer->to))){
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

int request_get(int sock, char* filename, unsigned long send_next, unsigned long rcv_next, Timer_node* timer)
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
    while(recv_packet(pk, sock, (struct sockaddr*)&servaddr, addrlen, &(timer->to)) != -1){
        //Check if the received packet is in order with the stream of bytes
        if(pk->seq_num == rcv_next){
            //Check if transmission has ended.
            if(pk->data_size == 0){
                //Data transmission completed: check if the receive buffer is empty
                if(rcv_buffer[0] == NULL){
                    printf("\rTransmission has completed successfully.\n");
                    fflush(stdout);
                    fclose(fp);
                    return 0;
                }

                //Send ACK for the last correctly received packet
                if(send_ack(sock, servaddr, addrlen, send_next, rcv_next, &(timer->to))){
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
                if(send_ack(sock, servaddr, addrlen, send_next, rcv_next, &(timer->to))){
                    return 1;
                }
            }
            
        }else if(pk->seq_num > rcv_next){
            //Packet received out of order: store packet in the receive buffer
            store_rwnd(pk, rcv_buffer, MAX_RVWD_SIZE);
                
            //Send ACK for the last correctly received packet
            if(send_ack(sock, servaddr, addrlen, send_next, rcv_next, &(timer->to))){
                return 1;
            }
    
        } else{
            //Received duplicated packet
            if(send_ack(sock, servaddr, addrlen, send_next, rcv_next, &(timer->to))){
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

int request_put(int sock, char *filename, unsigned long send_next, unsigned long rcv_next, short usable_wnd, Packet_node* base, Timer_node* timer)
{
    /**VARIABLE DEFINITIONS**/
    FILE *fp;
    ssize_t read_size;
    char* sendline;
    Packet *pk, *pack;
    Packet* wnd[INIT_WND_SIZE] = {NULL};
    int done; 
    pthread_t acktid;
    pthread_mutex_t wnd_mux;
    char buff[MAXLINE];
    /**END**/

    printf("Uploading %s\n", filename);

    /***TEMPORARY USE OF A WINDOW INSTEAD OF A TERMINAL***/
    initscr();
    WINDOW* win = newwin(0,0,0,0);
    wrefresh(win);
    endwin();

    pthread_mutex_init(&wnd_mux, 0);

    sprintf(buff, "%d %p %p %p %p %p\n", sock, &usable_wnd, wnd, base, &wnd_mux, timer);

    if(pthread_create(&acktid, 0, ack_thread, (void*)buff)){
        fprintf(stderr, "pthread_create() failed");
        exit(-1);
    }

    //Open file
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
  
    //Send the file to the server
    while(!feof(fp)) {       
          
        read_size = fread(sendline, 1, PAYLOAD, fp);
        pk = create_packet(send_next, rcv_next, read_size, sendline, DATA);    
        if (send_packet(pk, sock, (struct sockaddr*)&servaddr, addrlen, &(timer->to)) == -1) {
            free(sendline);
            free(pk);
            fprintf(stderr, "Error: couldn't send data.\n");
            return 1;
        }
        send_next += (unsigned long)pk->data_size;

        //Print progress
        done += read_size;
        double percent = (double)done/size;        
        print_progress(percent, win);

        pthread_mutex_lock(&wnd_mux);
        update_window(pk, wnd, &usable_wnd);
        //printf("Usable upd = %d\n", usable_wnd);
        base->pk = wnd[0];
        //If it's the the only packet in the window, start timer
        if(usable_wnd == INIT_WND_SIZE-1){
            arm_timer(timer, 0);
        }
        pthread_mutex_unlock(&wnd_mux);

        //Check for window limits
        while(usable_wnd == 0){
            sleep(1);
        }

        memset(sendline, 0, MAX_DGRAM_SIZE);
    }

    //Transmit the ending packet
    pk = create_packet(send_next, rcv_next, 0, NULL, DATA);
    if (send_packet(pk, sock, (struct sockaddr*)&servaddr, addrlen, &(timer->to)) == -1) {
        fprintf(stderr, "Error: couldn't send the filename.\n");
        return 1;
    }

    //Wait until every packet is correctly received
    while(usable_wnd != INIT_WND_SIZE){
    	sleep(1);
    }
    
    pthread_cancel(acktid);

    free(sendline);
    free(pk);
    free(pack);
    fclose(fp);

    printf("Done uploading %s\n", filename);

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
    int res, thread, sock;
    Packet *pk = NULL;
    Packet_node* trans_base;
    Timer_node* timer;
    short usable_wnd = INIT_WND_SIZE; //The amount of packets that can be sent 
    /**END**/

    strcpy(cmd, (char*)arg);

    //Retrieve ID       
    tok = strtok(cmd, " \n");
    sscanf(tok, "%d", &thread);

    //Retrieve socket
    tok = strtok(NULL, " \n");
    sscanf(tok, "%d", &sock);

    //Retrieve timer
    tok = strtok(NULL, " \n");
    sscanf(tok, "%p", &timer);

    //Retrieve window base
    tok = strtok(NULL, " \n");
    sscanf(tok, "%p", &trans_base);

    /** 3-WAY HANDSHAKE **/
    handshake(pk, &send_next, &rcv_next, sock, &servaddr, addrlen, timer);
    //printf("Connection opened\n");
    
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
    if (send_packet(pk, sock, (struct sockaddr*)&servaddr, addrlen, &(timer->to)) == -1) {
        fprintf(stderr, "Error: couldn't send packet #%lu.\n", pk->seq_num);
        free(pk);
        pthread_exit((void*)-1);
    }
    send_next += (unsigned long)pk->data_size + 1;

    trans_base->pk = pk;
    usable_wnd--;

    arm_timer(timer, 0);

    //Wait for the ACK related to the request
    while(1){
        if(recv_packet(pk, sock, (struct sockaddr*)&servaddr, addrlen, &(timer->to)) == -1) {
            failure("Error: couldn't receive ack.");
        } else {            
            if ((pk->type == ACK) && (pk->ack_num == send_next)) {
                disarm_timer(timer->timerid);
                usable_wnd++;
                break;                  
            }
        }
    }        
    free(pk);

    //Compute the timeout interval for exchange: REQUEST, ACK
    timeout_interval(&(timer->to));

    if (strcmp(request, "list") == 0) {
        //Execute LIST
        res = request_list(sock, send_next, rcv_next, timer);
    
    } else if (strcmp(request, "get") == 0) {
        //Execute GET
        res = request_get(sock, filename, send_next, rcv_next, timer);

    } else if (strcmp(request, "put") == 0) {
        //Execute PUT
        res = request_put(sock, filename, send_next, rcv_next, usable_wnd, trans_base, timer);

    } else {
        failure("\033[0;31mError: couldn't retrieve input.\033[0m\n");
    }

    if (res == 1) {
        failure("\033[0;31mInput function error.\033[0m\n");
        pthread_exit((void*)-1);

    } else { 
        demolition(send_next, rcv_next, sock, &servaddr, addrlen, timer);
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
    Thread_node* thread_head = NULL;	
    Thread_node* new_thread;
    Timer_node* new_timer;
    Packet_node* new_wnd;
    Socket_node* new_sock;
    int oper, num_threads = 0;
    /**END**/

    //Initializa global variables
	timer_head = NULL;
    wnd_head = NULL;
    sock_head = NULL;
    addrlen = sizeof(struct sockaddr_in);
   
    //Initialize address values
    memset((void*)&servaddr, 0, addrlen);    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
        failure("error in inet_pton.");
    }

    //Timer setup
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timeout_handler;
    if(sigaction(SIGRTMIN, &sa, NULL) == -1){
        failure("Sigaction failed.\n");
    }
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;

    while (1) {
    	printf("\n\n\033[0;34mChoose an operation:\n1. List\n2. Get\n3. Put\n0. Quit\n\n\033[0m");

        while(scanf("%d", &oper) != 1);
        while(getchar()!='\n');

        //Create socket for the new thread
        new_sock = insert_socket(&sock_head);
        if(((new_sock->sockfd) = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
	    	if(check_failure("\033[0;31mSocket() failed\033[0m")){
	            continue;
	        }
	    }

	    //Set up the timer for the new thread
	    new_timer = insert_timer(&timer_head);
	    sev.sigev_value.sival_ptr = new_timer->timerid; 
        if(timer_create(CLOCK_REALTIME, &sev, &(new_timer->timerid)) == -1){
            failure("Timer creation failed.\n");
        }

        //Find a spot in the queue for the new thread
        new_thread = insert_thread(&thread_head);

        //Create the base packet for transmission
        new_wnd = insert_base(&wnd_head);

        switch(oper){
            case 1:
                sprintf(buff, "%d %d %p %p ", num_threads, new_sock->sockfd, new_timer, new_wnd);
                strcat(buff, "list\n");

                if(pthread_create(&(new_thread->tid), 0, (void*)thread_function, (void*)buff)){
                    fprintf(stderr, "pthread_create() failed");
                    close(new_sock->sockfd);
                    exit(-1);
                }

                num_threads++;
                break;

            case 2:
                sprintf(buff, "%d %d %p %p ", num_threads, new_sock->sockfd, new_timer, new_wnd);
                strcat(buff, "get ");

                printf("\033[0;34mWhich file do you want to download?\n\033[0m");
                scanf("%s", filename);

                strcat(buff, filename);

                if(pthread_create(&(new_thread->tid), 0, (void*)thread_function, (void*)buff)){
                    fprintf(stderr, "pthread_create() failed");
                    close(new_sock->sockfd);
                    exit(-1);
                }  

                num_threads++;
                break;

            case 3:
                sprintf(buff, "%d %d %p %p ", num_threads, new_sock->sockfd, new_timer, new_wnd);
                strcat(buff, "put ");

                printf("\033[0;34mWhich file do you want to upload?\n\033[0m");
                scanf("%s", filename);

                strcat(buff, filename);

                if(pthread_create(&(new_thread->tid), 0, (void*)thread_function, (void*)buff)){
                    fprintf(stderr, "pthread_create() failed");
                    close(new_sock->sockfd);
                    exit(-1);
                }

                num_threads++;
                break;

            case 0:
                //Wait for every active thread to end
                
                while(num_threads && thread_head != NULL){                   
                    if(pthread_join(thread_head->tid, NULL)){
                        fprintf(stderr, "pthread_join() failed");
                        exit(-1);
                    }
                    num_threads--;
                    thread_head = thread_head->next;
                }  
                exit(0);

            default:
                printf("Invalid operation.\n\n");
                break;
        }
    }
    
    exit(0);
}
