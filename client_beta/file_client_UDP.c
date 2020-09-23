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
#define INIT_WIN_SIZE 1000000	//Test
#define MAX_BUFF_SIZE	10

Packet* rcv_buffer[MAX_BUFF_SIZE] = {NULL};
int rcv_next; //The sequence number of the next byte of data that is expected from the server
int rcv_wnd; //The size of the receive window “advertised” to the server.

int send_una; //The sequence number of the first byte of data that has been sent but not yet acknowledged
int send_next; //The sequence number of the next byte of data to be sent to the server
int send_wnd; //The size of the send window. The window specifies the total number of bytes that any device may have unacknowledged at any one time
int usable_wnd; //The amount of bytes that can be sent 

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
                fprintf(stderr, "Error: couldn't send ack #%d.\n", pk->ack_num);
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
           		fprintf(stderr, "Error: couldn't send ack #%d.\n", pk->ack_num);
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
                    fprintf(stderr, "Error: couldn't send ack #%d.\n", pk->ack_num);
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
                fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
                free(pack);
                return 1;
            } 
            free(pack); 

        //Check if the received packet is in order with the stream of bytes
        }else if(pk->seq_num == rcv_next){

        	printf("Received packet #%d.\n", pk->seq_num);
            //Convert byte array to file
            write_size += fwrite(pk->data, 1, pk->data_size, fp);
            rcv_next += (int)pk->data_size;

            //Check if there are other buffered data to read: if so, read it, reorder the buffer and update ack_num
            interv = check_buffer(pk, rcv_buffer);
            for(i=0; i<interv; i++){
                //Read every packet in order: everytime from the head of the buffer to avoid gaps in the buffer
            
                //Convert byte array to file
                write_size += fwrite(rcv_buffer[0]->data, 1, rcv_buffer[0]->data_size, fp);
                rcv_next += (int)rcv_buffer[0]->data_size;

                //Slide the buffered packets to the head of the buffer
                while(rcv_buffer[j] != NULL){
                    rcv_buffer[j-1] = rcv_buffer[j];
                    j++;
                }
                rcv_buffer[j-1] = NULL;
                j=1;
            }

            memset(pk->data, 0, pk->data_size);
            
            //Send ACK: ack_num updated to the byte the client is waiting to receive                        
            pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
            printf("Sending ACK.\n");
            if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
                free(pk);
                free(pack);
                return 1;
            }
            printf("ACK #%d correctly sent.\n", pack->ack_num);
            free(pack);
        
        }else{
            //Packet received out of order: store packet in the receive buffer
            //AUDIT
            printf("Received packet #%d out of order.\n", pk->seq_num);
            //
            if(store_pck(pk, rcv_buffer, MAX_BUFF_SIZE)){

                //Send ACK for the last correctly received packet
                printf("Sending ACK.\n");
                pack = create_packet(send_next, rcv_next, 0, NULL, ACK);
                if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                    fprintf(stderr, "Error: couldn't send ack #%d.\n", pack->ack_num);
                    free(pk);
                    free(pack);
                    return 1;
                }
                printf("ACK #%d correctly sent.\n", pack->ack_num);
                free(pack);

                memset(pk->data, 0, pk->data_size);
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
    size_t max_size;
    ssize_t read_size;
    char buff[MAXLINE];
    char* sendline;
    Packet *pk;
    int rcvd = 0; 
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
    
    //Retrive the dimension of the file to allocate
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("File size = %ld\n", max_size);
        
    //Send the dimension to the server
    sprintf(buff, "%ld", max_size);    
    pk = create_packet(send_next, rcv_next, strlen(buff), buff, DATA);    
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
    	fprintf(stderr, "Error: couldn't send the dimension packet to server.\n");
    	fclose(fp);
    	return 1;
    } else {
    	printf("File dimension correctly sent.\n");
    	send_next += (int)pk->data_size;
    }
       
    //Receive ack
    if (recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
    	fprintf(stderr, "Error: couldn't receive ack packet.\n");
    	return 1;
    } else {
    	//If the received ACK has ack number bigger than the first unacknowledged byte, update window
        if ((pk->type == ACK) && (pk->ack_num > send_una)) {
        	printf("Ack OK.\n");
        	send_una = pk->ack_num;
            usable_wnd = send_una + send_wnd - send_next;
        	//RESTART TIMER IF send_una < send_next        	
        } else {
        	printf("Ack NOT OK.\n");
        	return 1;
        }
    }
    
    //Allocate space
    sendline = (char*)malloc(MAX_DGRAM_SIZE);
    if(sendline == NULL){
        perror("Malloc() failed.");
        exit(EXIT_FAILURE);
    }
    
    while(!feof(fp)) {
    	//While there's space in the transmission window read from the file
        read_size = fread(sendline, 1, PAYLOAD, fp);
        rcvd += read_size;
        printf("Bytes sent: %d/%ld.\n", rcvd, max_size);

        //Send data through our socket
        pk = create_packet(send_next, rcv_next, read_size, sendline, DATA);	         
        if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
            free(sendline);
            fprintf(stderr, "Error: couldn't send data.\n");
            return 1;
        }
        
        send_next += (int)pk->data_size;
        while((usable_wnd = send_una + send_wnd - send_next) <= 0){
            //Wait for ACK
            if (recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen) == -1) {
                failure("Error: couldn't receive ack packet.");
            } else {               
                if ((pk->type == ACK) && (pk->ack_num > send_una)) {
                    printf("Ack OK.\n");
                    send_una = (int)pk->ack_num;
                    //RESTART TIMER IF send_una < send_next 
                } else {
                    printf("Ack NOT OK.\n");
                    return 1;
                }
            }
        }
        free(pk);
        memset(sendline, 0, MAX_DGRAM_SIZE);
    }

    free(sendline);
    fclose(fp);
    printf("Done.\n"); 
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
    socklen_t addrlen = sizeof(servaddr);

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
    send_wnd = INIT_WIN_SIZE;
    
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
        printf("Sending packet %d\n", send_next);  
        if (send_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen) == -1) {
            fprintf(stderr, "Error: couldn't send packet #%d.\n", pk->seq_num);
            free(pk);
            return 1;
        }
        printf("Packet #%d correctly sent\n", send_next);
        send_next += (int)pk->data_size;

        //Wait for ACK
        if (recv_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen) == -1) {
            failure("Error: couldn't receive ack packet.");
        } else {               
            if ((pk->type == ACK) && (pk->ack_num > send_una)) {
                printf("Received ACK #%d.\n", pk->ack_num);
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
            //TERMINATE CONNECTION: hadler for SIGINT -> terminate connection sending FIN!
        }       
    }
    
    exit(0);
}
