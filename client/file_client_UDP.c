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

int request_list(int sd, struct sockaddr_in addr)
{
    socklen_t addrlen = sizeof(addr);
    //char buff[MAXLINE]; NO PACKETS
    int n;
    Packet* pk;
    
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
        if(n == 0){
            return 0;
        }else{
            printf("%s\n", pk->data);
        }
    }
    
    free(pk->type);
    free(pk->data);
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
    FILE *fp;
    socklen_t addrlen = sizeof(addr);
    size_t max_size;
    ssize_t write_size = 0;
    //char buff[MAXLINE]; NO PACKETS
    //char buff[MAX_DGRAM_SIZE];
    char* recvline;
    int n, sum = 0;
    Packet* pk;

    //Open the file to write on
    fp = fopen(filename, "w+");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file %s.\n", filename);
        return 1;
    } else {
        printf("File %s successfully opened!\n", filename);
    }   
    
    //Retrive the dimension of the file to allocate
    /*
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) > 0) {          NO PACKETS
        max_size = atoi(buff);
        printf("File size: %ld\n", max_size);
        if ((recvline = (char*)malloc(max_size)) == NULL) {
            perror("Malloc() failed.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Error: couldn't retrieve dimension of file %s", filename);
        return 1;
    } 
    */
    
    pk = (Packet*)malloc(sizeof(Packet));
    if (pk == NULL) {
    	fprintf(stderr, "Error: couldn't malloc packet.\n");
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
    }
    
    // Receive file content
    
    /*      NO PACKETS
    while(sum < (int)max_size){
        // Receive data in chunks of 1024 bytes 
        if ((n = recvfrom(sd, recvline, max_size, 0, (struct sockaddr*)&addr, &addrlen)) == -1) {
            perror("Errore: couldn't read file content.\n");
            exit(EXIT_FAILURE);
        } else {
            sum += n;
            printf("Bytes read: %d\n", sum);
        }

        // Convert byte array to image
        write_size += fwrite(recvline, 1, n, fp);
    }
    */
    
    while(sum < (int)max_size){
    	
        /* Receive data in chunks of 1024 bytes */
        if ((n = recv_packet(pk, sd, (struct sockaddr*)&addr, addrlen)) == -1){
            perror("Errore: couldn't receive packet from socket.\n");
            free(pk);
            return 0;
        } else {
            sum += n; 	
            printf("Bytes read: %d\n", sum);
        }

        // Convert byte array to image
        write_size += fwrite(pk->data, 1, pk->data_size, fp);
        printf("write_size = %ld/%ld.\n", write_size, max_size);
    }
      
    printf("Bytes written: %d\n", (int)max_size);
    free(pk->data);
    free(pk->type);
    free(pk);
    fclose(fp);
    free(recvline);
    printf("Done.\n"); 
    return 0;
}


/* 
 --------------------------------------------------------------------------------------------------------------------------------
 PUT OPERATION
 --------------------------------------------------------------------------------------------------------------------------------
 */

int request_put(int sd, struct sockaddr_in addr, char *filename)
{
    FILE *fp;
    socklen_t addrlen = sizeof(addr);
    size_t max_size;
    ssize_t read_size;
    char buff[MAXLINE];
    char* sendline;
    
    //Wait for the "all clear" of the server
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) == 0) {
        printf("A file with the same name already exists.\n");
        return 0;
    }

    //Open the file
    fp = fopen(filename, "r");
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
    if (sendto(sd, buff, strlen(buff), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the dimension to the server.");
        fclose(fp);
        return 1;
    } else {
        printf("File dimension correctly sent.\n");
    }
        
    //Allocate space
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
    
    //Check command line
    if (argc < 2) {
        failure("Utilization: client_test <server IP>");
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

        printf("\n\033[0;34mChoose an operation:\033[0m ");
        fgets(buff, sizeof(buff), stdin);

        #endif 
        
        while (sendto(sockfd, buff, strlen(buff), 0, (struct sockaddr*)&servaddr, addrlen) == -1) {
            if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
                continue;
            }
        }
            
        printf("\n\033[0;32mInput successfully sent.\033[0m\n");
        
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
            printf("OK.\n");
        }
        
        break;
    }
    
    exit(0);
}
