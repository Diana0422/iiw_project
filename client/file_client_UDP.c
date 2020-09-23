//
//  client_test.c
//  
//
//  Created by Diana Pasquali on 20/07/2020.

#define _GNU_SOURCE

#include "client_test.h"

#define SERV_PORT 5193
#define DIR_PATH files
#define MAX_DGRAM_SIZE 65505
#define PAYLOAD 65400

/* 
 ------------------------------------------------------------------------------------------------------------------------------------------------
 LIST OPERATION
 ------------------------------------------------------------------------------------------------------------------------------------------------
 */
 
 
/*int request_list(int sd, struct sockaddr_in addr)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *recvline;
    size_t max_size;
    int n;
    
    //Retrieving filelist.txt content dimension
    while (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) < 0) {
        if(check_failure("Error: couldn't retrieve length of filelist.txt.\n")){
            continue;
        }
    }

    max_size = atoi(buff);
    printf("\033[0;34mFile size:\033[0m %ld\n\n", max_size);
    if((recvline = (char*)malloc(max_size)) == NULL){
        failure("Malloc() failed");
    }
    
    printf("\033[1;34m--- filelist.txt ---\033[0m\n");
    if ((n = recvfrom(sd, recvline, max_size, 0, (struct sockaddr*)&addr, &addrlen)) != -1) {
        recvline[n] = 0;
        if (puts(recvline) == EOF){
            free(recvline);
            perror("puts() failed");
            return 1;
        }
    }
    
    free(recvline);
    buf_clear(buff);
    printf("\nrecvline freed.\n\n\n");
    
    if (n < 0) {
        perror("read() failed");
        return 1;
    }
    
    return 0;
}*/

int request_list(int sd, struct sockaddr_in addr, int sequence)
{
    socklen_t addrlen = sizeof(addr);
    //char buff[MAXLINE]; NO PACKETS
    int n;
    Packet* pk;
    int recv_base = sequence;
    int send_base = recv_base;
    int size;
    
    printf("\033[1;34m--- FILELIST ---\033[0m\n");
    /*
    while ((n = recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen)) != -1) { //NO PACKETS
        if(n == 0){
            return 0;
        }else{
            printf("%s\n", buff);
            buf_clear(buff);
        }
    }
    */
    pk = (Packet*)malloc(sizeof(Packet));
    if (pk == NULL) {
    	fprintf(stderr, "Error: couldn't malloc packet.\n");
    	return 0;
    }
  
    
    while ((n = recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen)) != -1) {
        if(pk->data_size == 0){
            return 0;
        }
     
        if (pk->seq == recv_base) {
        	//printf("%d.\n", pk->seq);
        	printf("%s\n", pk->data);
        	
            	recv_base++;
            	send_base = pk->seq;
            	
       	// Send ack ...
       	pk = create_packet("ack", send_base, strlen("ack"), "ack");
       	
       	/*printf("--PACKET #%d: \n", pk->seq);
		printf("  type:      %s \n", pk->type);
		printf("  seq:       %d  \n", pk->seq);
		printf("  data_size: %ld  \n", pk->data_size);
		printf("  data:      %s  \n\n\n", pk->data);*/
       	
       	if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
       		fprintf(stderr, "Error: couldn't send ack #%d.\n", pk->seq);
       		return 0;
       	}
       	
         } else {
         	// Store packet data ...
            	// Send last ackd
            	printf("Do something for reliability.\n");
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

int request_get(int sd, struct sockaddr_in addr, char* filename, int sequence)
{
    FILE *fp;
    socklen_t addrlen = sizeof(addr);
    size_t max_size;
    ssize_t write_size = 0;
    int n, sum = 0;
    Packet* pk;
    Packet* pack;
    int send_base = sequence+1;
    int size;

    //Open the file to write on
    fp = fopen(filename, "w+");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file %s.\n", filename);
        return 1;
    } else {
        printf("File %s successfully opened!\n", filename);
    }   
    
    //Retrive the dimension of the file to allocate    
    pk = (Packet*)malloc(sizeof(Packet));
    if (pk == NULL) {
    	fprintf(stderr, "Error: couldn't malloc packet.\n");
    	return 0;
    }
    
    pack = (Packet*)malloc(sizeof(Packet));
    if (pk == NULL) {
    	fprintf(stderr, "Error: couldn't malloc packet ack.\n");
    	return 0;
    }
    
    if (recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen) != -1) {
    	max_size = atoi(pk->data);
    	printf("File size: %ld\n", max_size);
        if ((recvline = (char*)malloc(max_size)) == NULL) {
            perror("Malloc() failed.\n");
            return 0;
        }
    } else {
    	fprintf(stderr, "Error: couldn't retrieve dimension of file %s", filename);
    	send_base = pk->seq;
    	memset(pk->data, 0, pk->data_size);
    	memset(pk->type, 0, strlen(pk->type));
    	free(pk->data);
    }
    
    // Send ack
    pack = create_packet("ack", send_base, strlen("ack"), "ack");
    if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
    	fprintf(stderr, "Error: couldn't send ack.\n");
    	return 0; 
    }
    
    while(sum < (int)max_size){
    	
        /* Receive data in chunks of 65000 bytes */
        if ((n = recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen)) == -1){
            perror("Errore: couldn't receive packet from socket.\n");
            free(pk);
            return 0;
        } else {
            sum += n; 	
            printf("Bytes read: %d\n", sum);
        }

	// Send ack
        pack = create_packet("ack", send_base, strlen("ack"), "ack");
        if (send_packet(pack, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
    	    fprintf(stderr, "Error: couldn't send ack.\n");
    	    return 0; 
        }

        // Convert byte array to image
        write_size += fwrite(pk->data, 1, pk->data_size, fp);
        printf("write_size = %ld/%ld.\n", write_size, max_size);
    }
      
    printf("Bytes written: %d\n", (int)max_size);
    free(pk);
    fclose(fp);
    printf("Done.\n"); 
    return 0;
}


/* 
 --------------------------------------------------------------------------------------------------------------------------------
 PUT OPERATION
 --------------------------------------------------------------------------------------------------------------------------------
 */

int request_put(int sd, struct sockaddr_in addr, char *filename, int sequence)
{
    FILE *fp;
    socklen_t addrlen = sizeof(addr);
    size_t max_size;
    ssize_t read_size;
    char buff[MAXLINE];
    char* sendline;
    Packet* pk;
    Packet* pack;
    int size;
    int i;
    int send_base = sequence;
    int rcvd = 0;
    
    // Allocate packet for packet.
    pack = (Packet*)malloc(sizeof(Packet));
    if (pack == NULL) {
    	fprintf(stderr, "Error: couldn't malloc packet ack.\n");
    	return 0;
    }
    
    //Wait for the "all clear" of the server
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) == 0) {
        printf("A file with the same name already exists.\n");
        return 0;
    }

    //Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file %s.\n", filename);
        return 1;
    } else {
        printf("File %s successfully opened!\n", filename);
    }   
    
    // Retrive the dimension of the file to allocate
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("File size = %ld\n", max_size);
        
    // Send the dimension to the server
    sprintf(buff, "%ld", max_size);
    /* NO PACKETS
    if (sendto(sd, buff, strlen(buff), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the dimension to the server.");
        fclose(fp);
        return 1;
    } else {
        printf("File dimension correctly sent.\n");
    }*/
    
    pk = create_packet("data", send_base, strlen(buff), buff);
    
    if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
    	fprintf(stderr, "Error: couldn't send the dimension packet to server.\n");
    	fclose(fp);
    	return 1;
    } else {
    	printf("File dimension correctly sent.\n");
    }
    
    // Receive ack
    if (recv_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
    	fprintf(stderr, "Error: couldn't receive ack packet.\n");
    	return 1;
    } else {
    	printf("--PACKET #%d: \n", pack->seq);
        printf("  type:      %s \n", pack->type);
        printf("  seq:       %d  \n", pack->seq);
        printf("  data_size: %ld  \n", pack->data_size);
        printf("  data:      %s  \n\n\n", pack->data);
        
        if ((pack->seq == send_base) && strcmp(pack->type, "ack") == 0) {
        	printf("Ack OK.\n");
        	send_base++;
        } else {
        	printf("Ack NO OK.\n");
        	return 1;
        }
    }
    
    
    // Send data content
    
    /*    
    //Allocate space                      NO PACKETS
    sendline = (char*)malloc(max_size);
    if(sendline == NULL){
        perror("Malloc() failed.");
        exit(EXIT_FAILURE);
    }
        
    while(!feof(fp)) {
        //Read from the file into our send buffer
        read_size = fread(sendline, 1, MAX_DGRAM_SIZE, fp);
        printf("File size read: %ld\n", read_size);
        //Send data through our socket 
        //printf("%d bytes sent to client.\n", n);
        if (sendto(sd, sendline, read_size, 0, (struct sockaddr*)&addr, addrlen) == -1) {
            free(sendline);
            fprintf(stderr, "Error: couldn't send data.\n");
            return 1;
        }
        sleep(1); //USED TO COORDINATE SENDER AND RECEIVER IN THE ABSENCE OF RELIABILITY 
    }
    */
    
    //Allocate space
    sendline = (char*)malloc(MAX_DGRAM_SIZE);
    if(sendline == NULL){
        perror("Malloc() failed.");
        exit(EXIT_FAILURE);
    }
    
    while(!feof(fp)) {
    
        //Read from the file into our send buffer
        read_size = fread(sendline, 1, PAYLOAD, fp);
        printf("File size read: %ld\n", read_size);
        printf("PAYLOAD = %d", PAYLOAD);
        printf("sendline = %s\n", sendline);
        
        rcvd += read_size;
        printf("Bytes sent: %d/%d.\n", rcvd, max_size);
        //Send data through our socket
        pk = create_packet("data", send_base, read_size, sendline);
         
        //printf("%d bytes sent to client.\n", n);
        if (send_packet(pk, sd, (struct sockaddr*)&addr, addrlen, &size) == -1) {
            free(sendline);
            fprintf(stderr, "Error: couldn't send data.\n");
            return 1;
        }
        
        // Receive ack
    	if (recv_packet(pack, sd, (struct sockaddr*)&addr, addrlen) == -1) {
    		fprintf(stderr, "Error: couldn't receive ack packet.\n");
    		return 1;
    	} else {
    		printf("--PACKET #%d: \n", pack->seq);
        	printf("  type:      %s \n", pack->type);
        	printf("  seq:       %d  \n", pack->seq);
        	printf("  data_size: %ld  \n", pack->data_size);
        	printf("  data:      %s  \n\n\n", pack->data);
        
        	if ((pack->seq == send_base) && strcmp(pack->type, "ack") == 0) {
        		printf("Ack OK.\n");
        		send_base++;
       	} else {
        		printf("Ack NO OK.\n");
        		return 1;
        	}
    	}
        
        
        free(pk);
        memset(sendline, 0, MAX_DGRAM_SIZE);
        sleep(1); //USED TO COORDINATE SENDER AND RECEIVER IN THE ABSENCE OF RELIABILITY 
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
    int sockfd;
    struct sockaddr_in servaddr;
    char buff[MAXLINE];
    socklen_t addrlen = sizeof(servaddr);
    int res = 0;
    char *tok, *filename;
    Packet* pk;
    int size;
    int init_seq;
    
    //Check command line
    if (argc < 2) {
        failure("Utilization: ./client <server IP>");
    }
    
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
    
    //Read from socket until EOF is found
    
    while (1) {

        //Send request
        #ifdef PERFORMANCE

        strcpy(buff, argv[2]);

        #else

        printf("\n\n\033[0;34mChoose an operation:\033[0m ");
        fgets(buff, sizeof(buff), stdin);

        #endif 
        
        /** INITIALIZE SEQUENCE NUMBER **/
        init_seq = rand_lim(50);
        
        pk = create_packet("conn", init_seq, strlen(buff), buff);
        
        // Send first packet to initialize sequence number
        
        while (send_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen, &size) == -1) {
        	if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
                	continue;
            	}
        }
        
        printf("\n\033[0;32mInput successfully sent.\033[0m\n");
        
        // Receive second packet to initialize connection with the same sequence number
        
        while (recv_packet(pk, sockfd, (struct sockaddr*)&servaddr, addrlen) == -1) {
        	if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
                	continue;
            	}
        }
        
        if (pk->seq != init_seq) {
        	fprintf(stderr,"Error: packet #%d lost.\n", init_seq);
        	exit(EXIT_FAILURE);
        }
        
        printf("INITIAL SEQUENCE NUMBER = %d.\n\n", init_seq);
      	/** END **/
        
        
        /* CHOOSE OPERATION BY COMMAND */
        
        //Retrieve command       
        tok = strtok(buff, " \n");
        printf("\033[0;34mInput selected:\033[0m %s\n", tok);
        
        
        if (strcmp(tok, "list") == 0) {

            //Execute LIST
            res = request_list(sockfd, servaddr, init_seq);
	    
        } else if (strcmp(tok, "get") == 0) {

            //Execute GET
            tok = strtok(NULL, " \n");
            if(tok == NULL){
            	printf("To correctly get a file, insert also the name of the file!\n");
            	continue;
            }
            filename = strdup(tok);
            printf("Filename: %s\n", filename);

            res = request_get(sockfd, servaddr, filename, init_seq);

        } else if (strcmp(tok, "put") == 0) {

            //Execute PUT
            tok = strtok(NULL, " \n");
            if(tok == NULL){
            	printf("To correctly upload a file, insert also the name of the file!\n");
            	continue;
            }
            filename = strdup(tok);
            printf("Filename: %s\n", filename);

            res = request_put(sockfd, servaddr, filename, init_seq);

        } else {
            failure("\033[0;31mError: couldn't retrieve input.\033[0m\n");
        }
        
        if (res == 1) {
            failure("\033[0;31mInput function error.\033[0m\n");
        } else {
            printf("OK.\n");
        }
        
    }
    
    free(pk);
    exit(0);
}
