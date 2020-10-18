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

Packet* rcv_buffer[MAX_RVWD_SIZE] = {NULL};
Packet* wnd[INIT_WND_SIZE] = {NULL};

unsigned long rcv_next; //The sequence number of the next byte of data that is expected from the server
unsigned long send_next; //The sequence number of the next byte of data to be sent to the server
short usable_wnd; //The amount of packets that can be sent 
unsigned long last_ack_rcvd; // The sequence number of the last ack received (to fast retransmit)
int acks_count = 0; // counter for repeated acks (to fast retransmit packet).

Timeout time_temp;
timer_t timerid;

int sockfd;
struct sockaddr_in servaddr;

/*
 -----------------------------------------------------------------------------------------------------------------------------------------------
 RETRANSMISSION
 -----------------------------------------------------------------------------------------------------------------------------------------------
 */

void retransmission(){

    Packet* pk;
    pk = (Packet*)malloc(sizeof(Packet));
    if (pk == NULL) {
        failure("Error: couldn't malloc packet.");
    }  

    int i = 0;
    socklen_t addrlen = sizeof(servaddr);

    //Check if ACKs have been received    
    while(try_recv_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen, &time_temp) != -1){
        if((pk->type == ACK) && (pk->ack_num > wnd[i]->seq_num)){
            printf("Received ACK #%lu.\n", pk->ack_num);
            i++;

            if((i+1 < INIT_WND_SIZE) && (wnd[i+1] != NULL)){
                arm_timer(&time_temp, timerid, 0);
            }else{   
                break;
            }
        }
    }

    if(i == 0){
        pk = wnd[0];
        printf("Retransmitting packet #%lu\n", pk->seq_num);
        if (send_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen, &time_temp) == -1) {
            fprintf(stderr, "Error: couldn't send packet #%lu.\n", pk->seq_num);
            return;
        }
        printf("Packet #%lu correctly retransmitted.\n\n", pk->seq_num);

        arm_timer(&time_temp, timerid, 0);
    }else{
        refresh_window(wnd, i, &usable_wnd);
    }
    
    return;
}

/* 
 ------------------------------------------------------------------------------------------------------------------------------------------------
 LIST OPERATION
 ------------------------------------------------------------------------------------------------------------------------------------------------
 */
 
int request_list(int sd, struct sockaddr_in addr, Timeout* time_out)
{
    /**VARIABLE DEFINITIONS**/
    socklen_t addrlen = sizeof(addr);
    int i, j;
    Packet *pk;
    /**END**/

    //Prepare to receive packets
    printf("\033[1;34m--- FILELIST ---\033[0m\n");

    pk = (Packet*)malloc(sizeof(Packet));
    if (pk == NULL) {
    	failure("Error: couldn't malloc packet.");
    }   
 
    //Retreive packets
    while (recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen, time_out) != -1) {

        if(pk->seq_num == rcv_next){
    	//Packet received in order: gets read by the client
    	printf("Received packet #%lu in order.\n", pk->seq_num);

            //Check if transmission has ended.
            if(pk->data_size == 0){
                //Data transmission completed: check if the receive buffer is empty
                if(rcv_buffer[0] == NULL){
                    printf("Transmission has completed successfully.\n");
                    return 0;
                }

                //Send ACK for the last correctly received packet
                if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
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
                if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
                    return 1;
                }

                memset(pk->data, 0, PAYLOAD);
            }

        } else if(pk->seq_num > rcv_next){
            //Packet received out of order: store packet in the receive buffer
            printf("Received packet #%lu out of order.\n", pk->seq_num);
         	if(store_rwnd(pk, rcv_buffer, MAX_RVWD_SIZE)){
         		
                //Send ACK for the last correctly received packet
                if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
                    return 1;
                }

                memset(pk->data, 0, PAYLOAD);

            }else{
                printf("Buffer Overflow. Discarding the packet\n");
                //Send ACK for the last correctly received packet
                if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
                    return 1;
                }

                memset(pk->data, 0, PAYLOAD);
            }      	
        } else{
            //Received duplicated packet
            printf("Received duplicated packet #%lu.\n", pk->seq_num);
            //Send ACK for the last correctly received packet
            if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
                return 1;
            }

            memset(pk->data, 0, PAYLOAD);
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

int request_get(int sd, struct sockaddr_in addr, char* filename, Timeout* time_out)
{
    /**VARIABLE DEFINITIONS**/
    FILE *fp;
    socklen_t addrlen = sizeof(addr);
    ssize_t write_size = 0;
    int n, i, j;
    Packet *pk;
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
    } else {
        printf("File %s successfully opened!\n", filename);
    }   
    
    /* Receive data in chunks of 65000 bytes */
    while((n = recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen, time_out) != -1)){
        //DEBUG:
        print_packet(*pk);
        //Check if the received packet is in order with the stream of bytes
        if(pk->seq_num == rcv_next){
            //Check if transmission has ended.
            if(pk->data_size == 0){
                //Data transmission completed: check if the receive buffer is empty
                if(rcv_buffer[0] == NULL){
                    printf("Transmission has completed successfully.\n");
                    return 0;
                }

                //Send ACK for the last correctly received packet
                if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
                    return 1;
                }

            }else{
                printf("Received packet #%lu in order.\n", pk->seq_num);
                //Convert byte array to file
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

                print_rwnd(rcv_buffer);     

                //Send ACK for the last correctly received packet
                if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
                    return 1;
                }
            }
            
        }else if(pk->seq_num > rcv_next){
            //Packet received out of order: store packet in the receive buffer
            printf("Received packet #%lu out of order.\n", pk->seq_num);
            if(store_rwnd(pk, rcv_buffer, MAX_RVWD_SIZE)){
                
                //Send ACK for the last correctly received packet
                if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
                    return 1;
                }

            }else{
                printf("Buffer Overflow. Discarding the packet\n");
                //Send ACK for the last correctly received packet
                if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
                    return 1;
                }
            } 

        } else {
            //Received duplicated packet
            printf("Received duplicated packet #%lu.\n", pk->seq_num);
            //Send ACK for the last correctly received packet
            if(send_ack(sd, addr, addrlen, send_next, rcv_next, time_out)){
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

    if (n == -1){
	    perror("Error: couldn't receive packet from socket.\n");
	    free(pk);
	    fclose(fp);
	    return 1;
	}

    fclose(fp);
    return 0;
}

/* 
 --------------------------------------------------------------------------------------------------------------------------------
 PUT OPERATION
 --------------------------------------------------------------------------------------------------------------------------------
 */

int request_put(int sd, struct sockaddr_in addr, char *filename, Timeout* time_out, timer_t timerid)
{
    /**VARIABLE DEFINITIONS**/
    FILE *fp;
    socklen_t addrlen = sizeof(addr);
    ssize_t read_size;
    char* sendline;
    Packet *pk, *pack;
    int i;
    /**END**/
    
    //Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file %s.\n", filename);
        return 1;
    } else {
        printf("File %s successfully opened!\n", filename);
    }   
    
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
        printf("Sending packet #%lu.\n", send_next);          
        if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, time_out) == -1) {
            free(sendline);
            free(pk);
            fprintf(stderr, "Error: couldn't send data.\n");
            return 1;
        }
        printf("Packet #%lu correctly sent\n", send_next);
        send_next += (unsigned long)pk->data_size;

        update_window(pk, wnd, &usable_wnd);

        //If it's the the only packet in the window, start timer
        if(usable_wnd == INIT_WND_SIZE-1){
            arm_timer(time_out, timerid, 0);
        }

        while(usable_wnd == 0){
            //FULL WINDOW: wait for ACK until either it is received or the timer expires
            
            i = 0;  
            while(try_recv_packet(pack, sockfd, (struct sockaddr*)&servaddr, addrlen, &time_temp) != -1){

                if(pack->type == ACK){
                    printf("Received ACK #%lu.\n", pack->ack_num);
                    if (last_ack_rcvd != pack->ack_num) {
                        // New ack received
                        printf("\033[1;32mNew ack received.\033[0m\n"); //debug
                        last_ack_rcvd = pack->ack_num; 
                    } else {
                        // Increase acks counter
                        acks_count++;
                        if (acks_count == 3) {
                            // Fast retransmission
                            disarm_timer(timerid);
                            retransmission();
                        }
                    }
                    while((i < INIT_WND_SIZE) && (pack->ack_num > wnd[i]->seq_num)){
                        i++;
                    }
                }
            }

            if(i != 0){
                if((i+1 < INIT_WND_SIZE+1) && (wnd[i+1] != NULL)){
                    arm_timer(&time_temp, timerid, 0);
                }else{ 
                    disarm_timer(timerid);
                    timeout_interval(&time_temp);  
                }

                refresh_window(wnd, i, &usable_wnd);
            }

        }

        memset(sendline, 0, MAX_DGRAM_SIZE);
    }

    pk = create_packet(send_next, rcv_next, 0, NULL, DATA);
    printf("Sending packet #%lu.\n", send_next);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, time_out) == -1) {
        fprintf(stderr, "Error: couldn't send the filename.\n");
        return 1;
    } else {
        printf("last packet.\n");
    }

    update_window(pk, wnd, &usable_wnd);

    //Wait for ACK until every packet is correctly received
    while(usable_wnd != INIT_WND_SIZE-1){
        i = 0;  
        while(try_recv_packet(pack, sockfd, (struct sockaddr*)&servaddr, addrlen, &time_temp) != -1){

            if(pack->type == ACK){
                printf("Received ACK #%lu.\n", pack->ack_num);
                if (last_ack_rcvd != pack->ack_num) {
                        // New ack received
                        printf("\033[1;32mNew ack received.\033[0m\n"); //debug
                        last_ack_rcvd = pack->ack_num; 
                    } else {
                        // Increase acks counter
                        acks_count++;
                        if (acks_count == 3) {
                            // Fast retransmission
                            disarm_timer(timerid);
                            retransmission();
                        }
                    }
                while((i < INIT_WND_SIZE) && (pack->ack_num > wnd[i]->seq_num)){
                    i++;
                }
            }
        }

        if(i != 0){
            if((i+1 < INIT_WND_SIZE+1) && (wnd[i+1] != NULL)){
                arm_timer(&time_temp, timerid, 0);
            }else{ 
                disarm_timer(timerid);
                timeout_interval(&time_temp);  
            }

            refresh_window(wnd, i, &usable_wnd);
        }
    }
    
    free(sendline);
    free(pk);
    free(pack);
    fclose(fp);
    return 0;
}

/*
 ------------------------------------------------------------------------------------------------------------------------------------------------
 MAIN
 -------------------------------------------------------------------------------------------------------------------------------------------------
 */

int main(int argc, char* argv[])
{   
    //Check command line
    if (argc < 2) {
        failure("Utilization: ./client <server IP>");
    }

    /**VARIABLE DEFINITIONS**/   
    int res = 0;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    char buff[MAXLINE];
    char *tok, *filename;

    Packet* pk = NULL;
    
    struct sigevent sev;
    struct sigaction sa;
    /**END**/

    memset(&time_temp, 0, sizeof(time_temp));

    //Create socket
    while ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        if(check_failure("\033[0;31mSocket() failed\033[0m")){
            continue;
        }
    }
    
    //Initialize address values
    memset((void*)&servaddr, 0, addrlen);    
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

    //Create timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerid;
    if(timer_create(CLOCK_REALTIME, &sev, &timerid) == -1){
        failure("Timer creation failed.\n");
    }

    /** 3-WAY HANDSHAKE **/
    handshake(pk, &send_next, &rcv_next, sockfd, &servaddr, addrlen, &time_temp, timerid);

    //Read from socket until EOF is found   
    while (1) {

    	usable_wnd = INIT_WND_SIZE;

        //Send request
        #ifdef PERFORMANCE

        strcpy(buff, argv[2]);

        #else

        printf("\n\n\033[0;34mChoose an operation:\033[0m ");
        fgets(buff, sizeof(buff), stdin);

        #endif 
     
        /* CHOOSE OPERATION BY COMMAND */

        //Send request
        pk = create_packet(send_next, rcv_next, strlen(buff), buff, DATA);
        printf("Sending packet #%lu\n", send_next);  
        if (send_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen, &time_temp) == -1) {
            fprintf(stderr, "Error: couldn't send packet #%lu.\n", pk->seq_num);
            free(pk);
            return 1;
        }
        printf("Packet #%lu correctly sent\n", send_next);
        send_next += (unsigned long)pk->data_size + 1;

        update_window(pk, wnd, &usable_wnd);

        arm_timer(&time_temp, timerid, 0);

        //Wait for ACK
        while(1){
        	if(recv_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen, &time_temp) == -1) {
	            failure("Error: couldn't receive ack.");
	        } else {            
	            if (pk->type == ACK) {
	            	disarm_timer(timerid);
	                printf("Received ACK #%lu.\n", pk->ack_num);
	                wnd[0] = NULL;
	                usable_wnd++;
	                break;	                
	            }
	        }
        }        
        free(pk);

        //Compute the timeout interval for exchange: REQUEST, ACK
        timeout_interval(&time_temp);

        //Retrieve command       
        tok = strtok(buff, " \n");
        printf("\033[0;34mInput selected:\033[0m %s\n", tok);
        
        
        if (strcmp(tok, "list") == 0) {

            //Execute LIST
            res = request_list(sockfd, servaddr, &time_temp);
	    
        } else if (strcmp(tok, "get") == 0) {

            //Execute GET
            tok = strtok(NULL, " \n");
            if(tok == NULL){
            	printf("To correctly get a file, insert also the name of the file!\n");
            	continue;
            }
            filename = strdup(tok);
            printf("Filename: %s\n", filename);

            res = request_get(sockfd, servaddr, filename, &time_temp);

        } else if (strcmp(tok, "put") == 0) {

            //Execute PUT
            tok = strtok(NULL, " \n");
            if(tok == NULL){
            	printf("To correctly upload a file, insert also the name of the file!\n");
            	continue;
            }
            filename = strdup(tok);
            printf("Filename: %s\n", filename);

            res = request_put(sockfd, servaddr, filename, &time_temp, timerid);

        } else {
            failure("\033[0;31mError: couldn't retrieve input.\033[0m\n");
        }
        
        if (res == 1) {
            failure("\033[0;31mInput function error.\033[0m\n");
        } else {
            printf("\nOK.\n");
            demolition(send_next, rcv_next, sockfd, &servaddr, addrlen, &time_temp, timerid);
            break;
        }       
    }
    
    exit(0);
}
