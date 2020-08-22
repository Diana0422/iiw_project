//
//  client_test.c
//  
//
//  Created by Diana Pasquali on 20/07/2020.
//

#define _GNU_SOURCE

#include "client_test.h"

#define SERV_PORT 5193

int request_list(int sd, struct sockaddr_in addr) 
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
    if((recvline = (char*)malloc(max_size)) == NULL){
        failure("Malloc() failed");
    }
    
    printf("\033[1;34m--- filelist.txt ---\033[0m\n");
    if ((n = recvfrom(sd, recvline, max_size, 0, (struct sockaddr*)&addr, &addrlen)) != -1) {
        recvline[n] = 0;
        if (puts(recvline) == EOF){
            perror("puts() failed");
            return 1;
        }
    }
    
    free(recvline);
    buf_clear(buff);

    if (n < 0) {
        perror("read() failed");
        return 1;
    }
    
    return 0;
}

int request_get(int sd, struct sockaddr_in addr, char *filename/*, char* extension*/) // OK
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *recvline;
    size_t max_size;
    FILE *fp;
    int n;
    
    //Send the filename to get from the server
    /*if (sendto(sd, filename, strlen(filename), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        perror("sendto() failed");
        return 1;
    }*/
    
    //Retrieve the dimension of the space to allocate
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) > 0) {
        max_size = atoi(buff);
        if((recvline = (char*)malloc(max_size)) == NULL){
        	perror("Malloc() failed.");
        	exit(EXIT_FAILURE);
        }
    } else {
        perror("recvfrom() failed");
        return 1;
    }
    
    //Create and open the file to write on
    fp = fopen(filename, "w+");
    if (fp == NULL) {
        perror("fopen() failed");
        free(recvline);
        return 1;
    }
    
    //Retrieve data to write on the file
    if ((n = recvfrom(sd, recvline, max_size, 0, (struct sockaddr*)&addr, &addrlen)) != -1) {
        recvline[n] = 0;
        if (fputs(recvline, fp) == EOF){
            fprintf(stderr, "fputs() failed.");
            return 1;
        }
    }
    
    free(recvline);
    return 0;
    
}

int request_put(int sd, struct sockaddr_in addr, char *filename)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    FILE *fp;
    size_t max_size;
    char *sendline;

    //Get the file dimension
    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Efopen() failed");
        return 1;
    } 

    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    //Send the file dimension
    sprintf(buff, "%ld", max_size);
    if (sendto(sd, buff, strlen(buff), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        perror("sendto() failed");
        fclose(fp);
        return 1;
    }
    
    /* SENDING FILE CONTENT */
    //Allocate space
    sendline = (char*)malloc(max_size);
    if(sendline == NULL){
    	perror("Malloc() failed.");
    	exit(EXIT_FAILURE);
    }
    
    //Insert content in "sendline" variable
    char ch;
    for (int i=0; i<(int)max_size; i++) {
        ch = fgetc(fp);
        sendline[i] = ch;
        if (ch == EOF) break;
    }
    
    //Send content to server
    if (sendto(sd, sendline, max_size, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        perror("sendto() failed");
        free(sendline);
        fclose(fp);
        return 1;
    }
    
    fclose(fp);
    free(sendline);
    return 0;
}

int main(int argc, char* argv[])
{
    int sockfd;
    struct sockaddr_in servaddr;
    char buff[MAXLINE];
    socklen_t addrlen = sizeof(servaddr);
    int res = 0;
    char *tok, *filename/*, *ext*/;
    
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
        strcpy(buff, argv[2]);
        
        while (sendto(sockfd, buff, strlen(buff), 0, (struct sockaddr*)&servaddr, addrlen) == -1) {
            if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
                continue;
            }
        }
        
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
            res = request_get(sockfd, servaddr, filename);

        } else if (strcmp(tok, "put") == 0) {

            //Execute PUT
            tok = strtok(NULL, " \n");
            if(tok == NULL){
            	printf("To correctly upload a file, insert also the name of the file!\n");
            	continue;
            }
            filename = strdup(tok);
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
