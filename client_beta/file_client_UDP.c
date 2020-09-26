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
#define INIT_WIN_SIZE  10
#define MAX_BUFF_SIZE	10

Packet* rcv_buffer[MAX_BUFF_SIZE] = {NULL};
unsigned long rcv_next; //The sequence number of the next byte of data that is expected from the server
unsigned long rcv_wnd; //The size of the receive window “advertised” to the server.

unsigned long send_una; //The sequence number of the first byte of data that has been sent but not yet acknowledged
unsigned long send_next; //The sequence number of the next byte of data to be sent to the server
unsigned long send_wnd; //The size of the send window. The window specifies the total number of bytes that any device may have unacknowledged at any one time
unsigned long usable_wnd; //The amount of bytes that can be sent 

/* 
 ------------------------------------------------------------------------------------------------------------------------------------------------
 LIST OPERATION
 ------------------------------------------------------------------------------------------------------------------------------------------------
 */
 
int request_list(int sd, struct sockaddr_in addr)
{
    /**VARIABLE DEFINITIONS**/
    socklen_t addrlen = sizeof(addr);
    int interv, i, j=1;
    Packet *pk;
    /**END**/

    //Prepare to receive packets
    printf("\033[1;34m--- FILELIST ---\033[0m\n");

    pk = (Packet*)malloc(sizeof(Packet));
    if (pk == NULL) {
    	failure("Error: couldn't malloc packet.");
    }   
 
    //Retreive packets
    while (recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen) != -1) {

        if(pk->data_size == 0){
            //Data transmission completed: check if the receive buffer is empty
            if(rcv_buffer[0] == NULL){
                printf("Transmission has completed successfully.\n");
                return 0;
            }

            //Send ACK for the last correctly received packet
            pk = create_packet(send_next, rcv_next, 0, NULL, ACK);
            if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                fprintf(stderr, "Error: couldn't send ack #%lu.\n", pk->ack_num);
                free(pk);
                return 1;
            } 
            free(pk);          
        }

        //Packet received in order: gets read by the client
        if (pk->seq_num == rcv_next) {
            printf("%s\n", pk->data);
            rcv_next += (int)pk->data_size;

            //Check if there are other buffered data to read: if so, read it, reorder the buffer and update ack_num
            interv = check_buffer(pk, rcv_buffer);
            for(i=0; i<interv; i++){
                //Read every packet in order: everytime from the head of the buffer to avoid gaps in the buffer
                printf("%s\n", rcv_buffer[0]->data);
                rcv_next += (int)rcv_buffer[0]->data_size;
                //Slide the buffered packets to the head of the buffer
                while(rcv_buffer[j] != NULL){
                    rcv_buffer[j-1] = rcv_buffer[j];
                    j++;
                }
                rcv_buffer[j-1] = NULL;
                j=1;
            }
        	
            //Send ACK: ack_num updated to the byte the client is waiting to receive                       	
       	    pk = create_packet(send_next, rcv_next, 0, NULL, ACK);
           	if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
           		fprintf(stderr, "Error: couldn't send ack #%lu.\n", pk->ack_num);
                free(pk);
           		return 1;
           	}
            free(pk);
        } else {
            //Packet received out of order: store packet in the receive buffer
         	if(store_pck(pk, rcv_buffer, MAX_BUFF_SIZE)){

                //Send ACK for the last correctly received packet
            	pk = create_packet(send_next, rcv_next, 0, NULL, ACK);
                if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                    fprintf(stderr, "Error: couldn't send ack #%lu.\n", pk->ack_num);
                    free(pk);
                    return 1;
                }
                free(pk);

            }else{
                printf("Buffer Overflow. NEED FLOW CONTROL\n");
                free(pk);
                return 1;
            }      	
        }
    }

    //Retrieving error
    free(pk);
    return 1;
}


/*
 --------------------------------------------------------------------------------------------------------------------------------
 GET OPERATION
 --------------------------------------------------------------------------------------------------------------------------------
 */

int request_get(int sd, struct sockaddr_in addr, char* filename)
{
    /**VARIABLE DEFINITIONS**/
    FILE *fp;
    socklen_t addrlen = sizeof(addr);
    ssize_t write_size = 0;
    int n, interv, i, j=1;
    Packet *pk, *pack;
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
    while((n = recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen) != -1)){
    	//Check if transmission has ended.
    	if(pk->data_size == 0){
            //Data transmission completed: check if the receive buffer is empty
            if(rcv_buffer[0] == NULL){
                printf("Transmission has completed successfully.\n");
                break;
            }

            //Send ACK for the last correctly received packet
            pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
            if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                fprintf(stderr, "Error: couldn't send ack #%lu.\n", pack->ack_num);
                free(pack);
                return 1;
            } 
            free(pack); 

        //Check if the received packet is in order with the stream of bytes
        }else if(pk->seq_num == rcv_next){

        	printf("Received packet #%lu\n", pk->seq_num);
            //Convert byte array to file
            write_size += fwrite(pk->data, 1, pk->data_size, fp);
            rcv_next += (unsigned long)pk->data_size;

            //Check if there are other buffered data to read: if so, read it, reorder the buffer and update ack_num
            interv = check_buffer(pk, rcv_buffer);
            printf("Buffered packets that can be read: %d\n", interv);
            for(i=0; i<interv; i++){
                //Read every packet in order: everytime from the head of the buffer to avoid gaps in the buffer
            
                //Convert byte array to file
                write_size += fwrite(rcv_buffer[0]->data, 1, rcv_buffer[0]->data_size, fp);
                rcv_next += (unsigned long)rcv_buffer[0]->data_size;

                //Slide the buffered packets to the head of the buffer
                while(rcv_buffer[j] != NULL){
                    rcv_buffer[j-1] = rcv_buffer[j];
                    j++;
                }
                rcv_buffer[j-1] = NULL;
                j=1;
            }

            memset(pk->data, 0, PAYLOAD);
            
            //Send ACK: ack_num updated to the byte the client is waiting to receive                        
            pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
            printf("Sending ACK #%lu.\n", rcv_next);
            if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                fprintf(stderr, "Error: couldn't send ack #%lu.\n", pack->ack_num);
                free(pk);
                free(pack);
                return 1;
            }
            printf("ACK #%lu correctly sent.\n", pack->ack_num);
            free(pack);
        
        }else{
            //Packet received out of order: store packet in the receive buffer
            //AUDIT
            printf("Received packet #%lu out of order.\n", pk->seq_num);
            //
            if(store_pck(pk, rcv_buffer, MAX_BUFF_SIZE)){
                printf("RECEIVE BUFFER: \n");
                i=0;
                while(rcv_buffer[i] != NULL){
                    print_packet(*rcv_buffer[i]);
                    i++;
                }
                printf("RECEIVE BUFFER\n");
                //Send ACK for the last correctly received packet
                printf("Sending ACK #%lu.\n", rcv_next);
                pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
                if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                    fprintf(stderr, "Error: couldn't send ack #%lu.\n", pack->ack_num);
                    free(pk);
                    free(pack);
                    return 1;
                }
                printf("ACK #%lu correctly sent.\n", pack->ack_num);
                free(pack);

                memset(pk->data, 0, PAYLOAD);
            }else{
                printf("Buffer Overflow. NEED FLOW CONTROL\n");
                free(pk);
                return 1;
            }       
        }           
    }

    if (n == -1){
	    perror("Error: couldn't receive packet from socket.\n");
	    free(pk);
	    fclose(fp);
	    return 1;
	}

      
    //free(pk);
    fclose(fp);
    return 0;
}


/* 
 --------------------------------------------------------------------------------------------------------------------------------
 PUT OPERATION
 --------------------------------------------------------------------------------------------------------------------------------
 */

int request_put(int sd, struct sockaddr_in addr, char *filename)
{
	/**VARIABLE DEFINITIONS**/
    FILE *fp;
    socklen_t addrlen = sizeof(addr);
    ssize_t read_size;
    char* sendline;
    Packet *pk;
    /**END**/
    
    //Wait for the "all clear" of the server
    /*if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) == 0) {
        printf("A file with the same name already exists.\n");
        return 0;
    }*/

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
  
    //Send the file to the client
    while(!feof(fp)) {       
        //Read from the file and send data   
        read_size = fread(sendline, 1, PAYLOAD, fp);

        pk = create_packet(send_next, rcv_next, read_size, sendline, DATA);  
        printf("Sending packet #%lu\n", send_next);          
        if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
            free(sendline);
            free(pk);
            fprintf(stderr, "Error: couldn't send data.\n");
            return 1;
        }
        printf("Packet #%lu correctly sent\n", send_next);
        send_next += (unsigned long)pk->data_size;

        //Check if there's still usable space in the transmission window: if not, wait for an ACK
        while((usable_wnd = send_una + send_wnd - send_next) <= 0){
            //FULL WINDOW: wait for ACK until either it is received (restart the timer and update window) or the timer expires (retransmit send_una packet)
            if(recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1){
                perror("Error: couldn't receive packet from socket.\n");
                free(pk);
                fclose(fp);
                return 1;
            }

            if((pk->type == ACK) && (pk->ack_num > send_una)){
                printf("Received ACK #%lu.\n", pk->ack_num);
                send_una = pk->ack_num;
                //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS: send_una != send_next
            }
        }

        free(pk);
        memset(sendline, 0, MAX_DGRAM_SIZE);
    }

    pk = create_packet(send_next, rcv_next, 0, NULL, DATA);
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the filename.\n");
        return 1;
    } else {
        printf("last packet.\n");
    }

    //Wait for ACK until every packet is correctly received
    while(send_una != send_next){
        if(recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1){
            perror("Error: couldn't receive packet from socket.\n");
            free(pk);
            fclose(fp);
            return 1;
        }

        if((pk->type == ACK) && (pk->ack_num > send_una)){
            printf("Received ACK #%lu.\n", pk->ack_num);
            send_una = pk->ack_num;
            //RESTART TIMER IF THERE ARE STILL UNACKNOWLEDGED PACKETS, ELSE STOP
        }
    }
    
    free(sendline);
    free(pk);
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
    int sockfd, res = 0;

	struct sockaddr_in servaddr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    char buff[MAXLINE];
    char *tok, *filename;

    Packet* pk = NULL;
    /**END**/

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

    /** 3-WAY HANDSHAKE **/
    handshake(pk, &send_next, &rcv_next, sockfd, &servaddr, addrlen);

    //Initialize windows and stream variables
    send_una = send_next;
    send_wnd = INIT_WIN_SIZE*MAX_DGRAM_SIZE;
    
    //Read from socket until EOF is found   
    while (1) {

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
        printf("Sending packet %lu\n", send_next);  
        if (send_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen) == -1) {
            fprintf(stderr, "Error: couldn't send packet #%lu.\n", pk->seq_num);
            free(pk);
            return 1;
        }
        printf("Packet #%lu correctly sent\n", send_next);
        send_next += (unsigned long)pk->data_size;

        //Wait for ACK
        if (recv_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen) == -1) {
            failure("Error: couldn't receive ack.");
        } else {               
            if ((pk->type == ACK) && (pk->ack_num > send_una)) {
                printf("Received ACK #%lu.\n", pk->ack_num);
                send_una = (int)pk->ack_num;
                //RESTART TIMER IF send_una < send_next 
            } else {
                printf("Ack NOT OK.\n");
                return 1;
            }
        }
        free(pk);

        //Retrieve command       
        tok = strtok(buff, " \n");
        printf("\033[0;34mInput selected:\033[0m %s\n", tok);
        
        
        if (strcmp(tok, "list") == 0) {

            //Execute LIST
            res = request_list(sockfd, servaddr);
	    
        } else if (strcmp(tok, "get") == 0) {

            //Execute GET
            tok = strtok(NULL, " \n");
            if(tok == NULL){
            	printf("To correctly get a file, insert also the name of the file!\n");
            	continue;
            }
            filename = strdup(tok);
            printf("Filename: %s\n", filename);

            res = request_get(sockfd, servaddr, filename);

        } else if (strcmp(tok, "put") == 0) {

            //Execute PUT
            tok = strtok(NULL, " \n");
            if(tok == NULL){
            	printf("To correctly upload a file, insert also the name of the file!\n");
            	continue;
            }
            filename = strdup(tok);
            printf("Filename: %s\n", filename);

            res = request_put(sockfd, servaddr, filename);

        } else {
            failure("\033[0;31mError: couldn't retrieve input.\033[0m\n");
        }
        
        if (res == 1) {
            failure("\033[0;31mInput function error.\033[0m\n");
        } else {
            printf("\nOK.\n");
            demolition(send_next, rcv_next, sockfd, &servaddr, addrlen);
            break;
        }       
    }
    
    exit(0);
}
