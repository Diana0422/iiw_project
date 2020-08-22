//
//  client_test.c
//  
//
//  Created by Diana Pasquali on 20/07/2020.
//

#define _GNU_SOURCE

#include "client_test.h"

#define SERV_PORT 5193
#define DIR_PATH files

/* 
 ------------------------------------------------------------------------------------------------------------------------------------------------
 LIST OPERATION
 ------------------------------------------------------------------------------------------------------------------------------------------------
 */
 
 
int request_list(int sd, struct sockaddr_in addr) // OK
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
}


/*
 --------------------------------------------------------------------------------------------------------------------------------
 GET OPERATION
 --------------------------------------------------------------------------------------------------------------------------------
 */
 

int get_textfile(int sd, struct sockaddr_in addr, socklen_t addrlen, char* buff, char* recvline, char* filename) // OK
{
    size_t max_size;
    FILE* fp;
    int n;
    
    //Retrieve the dimension of the space to allocate
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) > 0) {
        max_size = atoi(buff);
        if((recvline = (char*)malloc(max_size)) == NULL){
        	perror("Malloc() failed.");
        	exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Error: couldn't retrieve the dimension of %s.\n", filename);
        return 1;
    }
    
    //Create and open the file to write on
    fp = fopen(filename, "w+");
    if (fp == NULL) {
        fprintf(stderr, "Error: couldn't open %s", filename);
        free(recvline);
        return 1;
    } else {
        printf("File %s correctly opened.\n", filename);
    }
    
    //Retrieve data to write on the file
    if ((n = recvfrom(sd, recvline, max_size, 0, (struct sockaddr*)&addr, &addrlen)) != -1) {
        recvline[n] = 0;
        if (fputs(recvline, fp) == EOF){
            fprintf(stderr, "fputs() failed.");
            return 1;
        }
        //write_file(recvline, fp, max_size); //wrap with semaphore or mutex?
        
    }
    
    free(recvline);
    printf("recvline freed.\n");
    return 0;
}


int get_image(int sd, struct sockaddr_in addr, socklen_t addrlen, char* buff, char* recvline, char* filename)
{
	FILE *picp;
	size_t max_size;
	ssize_t write_size;
	int n;

	//Open the file to write the image on
	picp = fopen(filename, "w+");
	if (picp == NULL) {
		fprintf(stderr, "Error: cannot open file %s.\n", filename);
		return 1;
	} else {
		printf("File %s successfully opened!\n", filename);
	}	
	
	// Retrive the dimension of the file to allocate
	if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) > 0) {
		max_size = atoi(buff);
		printf("Image size: %ld\n", max_size);
		if ((recvline = (char*)malloc(max_size)) == NULL) {
			perror("Malloc() failed.\n");
			exit(EXIT_FAILURE);
		}
	} else {
		fprintf(stderr, "Error: couldn't retrieve dimension of file %s", filename);
		return 1;
	} 
	
	// Read the bytes received from the socket (byte array)
	if ((n = recvfrom(sd, recvline, max_size, 0, (struct sockaddr*)&addr, &addrlen)) == -1) {
		perror("Errore: couldn't read image content.\n");
		exit(EXIT_FAILURE);
	}
	
	// Convert byte array to image
	
	write_size = fwrite(recvline, sizeof(char), max_size, picp);
	printf("Written image size: %d\n", (int)max_size);
	fclose(picp);
	printf("Done.\n"); 
	
	
	//Insert content in "sendline" variable
    	/*char ch;
    	int bytes = 0;
    	for (int i=0; i<(int)max_size; i++) {
        	ch = recvline[i];
        	bytes += putc(ch, picp);
        	if (ch == EOF) break;
    	}
	printf("bytes written: %d\n", bytes);*/
	
	if (write_size != (int)max_size) {
		printf("Something's wrong.\n");
		return 1;
	}
	
	//printf("Done.\n");
	return 0;
}

int request_get(int sd, struct sockaddr_in addr, char *filename, char* extension) // OK
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *recvline = NULL;
    int type;
    
    if ((strcmp(extension, "txt") == 0) | (strcmp(extension, "png") == 0)) {
    	type = 0;
    } else if ((strcmp(extension, "jpg") == 0) /*| (strcmp(extension, "png") == 0)*/) {
    	type = 1;
    } else if (strcmp(extension, "mp3") == 0) {
    	type = 2;
    } else {
    	printf("File extension not expected.\n");
	return 1;
    }
    
    switch(type){

    	case 0:
    		get_textfile(sd, addr, addrlen, buff, recvline, filename);
    		break;
    	case 1:
		get_image(sd, addr, addrlen, buff, recvline, filename);
		break;
    	case 2:
		//get_audio();
		break;
	default:
		printf("File extension not recognized.\n");
		return 1;
    }
    
    return 0;
}




/* 
 --------------------------------------------------------------------------------------------------------------------------------
 PUT OPERATION
 --------------------------------------------------------------------------------------------------------------------------------
 */

int put_textfile(int sd, struct sockaddr_in addr, socklen_t addrlen, char* buff, char* sendline, char* filename) 
{
	size_t max_size;
	FILE* fp;	
	
	//Get the file dimension
    	fp = fopen(filename, "r");
    	if (fp == NULL) {
        	fprintf(stderr, "Error: couldn't open %s.", filename);
        	return 1;
    	} else {
        	printf("File %s correctly opened.\n", filename);
    	}
    
    	fseek(fp, 0, SEEK_END);
    	max_size = ftell(fp);
    	fseek(fp, 0, SEEK_SET);
    	printf("File size = %ld\n", max_size);
    	
    	//Send the file dimension
    	sprintf(buff, "%ld", max_size);
    	if (sendto(sd, buff, strlen(buff), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        	fprintf(stderr, "Error: couldn't send the dimension to the server.");
        	fclose(fp);
        	return 1;
    	} else {
        	printf("File dimension correctly sent.\n");
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
    	//read_file(sendline, fp, max_size);
    
    	printf("\n\n");
    	puts(sendline);
    	printf("\n");
    
    	//Send content to server
    	if (sendto(sd, sendline, max_size, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        	fprintf(stderr, "Error: couldn't send file content to server.");
        	free(sendline);
        	fclose(fp);
        	return 1;
    	} else {
        	printf("File %s correctly sent to the server.\n", filename);
    	}
    
    	fclose(fp);
    	free(sendline);
    	printf("sendline freed.\n");
    	return 0;
}


int put_image(int sd, struct sockaddr_in addr, socklen_t addrlen, char* buff, char* sendline, char* filename)
{
	FILE *picp;
	size_t max_size;
	ssize_t read_size;
	int n;

	//Open the image file
	picp = fopen(filename, "r");
	if (picp == NULL) {
		fprintf(stderr, "Error: cannot open file %s.\n", filename);
		return 1;
	} else {
		printf("File %s successfully opened!\n", filename);
	}	
	
	// Retrive the dimension of the file to allocate
	fseek(picp, 0, SEEK_END);
    	max_size = ftell(picp);
    	fseek(picp, 0, SEEK_SET);
    	printf("File size = %ld\n", max_size);
    	
    	// Send the dimension to the server
    	sprintf(buff, "%ld", max_size);
    	if (sendto(sd, buff, strlen(buff), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        	fprintf(stderr, "Error: couldn't send the dimension to the server.");
        	fclose(picp);
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
    	
    	// Get the image content
    	while(!feof(picp)) {
      		//Read from the file into our send buffer
      		read_size = fread(sendline, 1, max_size-1, picp);
      		printf("Image size read: %ld\n", read_size);
      		//Send data through our socket 
      		do{
        		n = sendto(sd, sendline, read_size, 0, (struct sockaddr*)&addr, addrlen);  
        		printf("%d bytes sent to client.\n", n);
        		if (n == -1) {
        			fprintf(stderr, "Error: couldn't send data.\n");
        			return 1;
        		}
      		}while (n < 0);
   	 }

	free(sendline);
	fclose(picp);
	printf("Done.\n"); 
	return 0;
	
}


int request_put(int sd, struct sockaddr_in addr, char *filename, char* extension)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *sendline = NULL;
    int type;
    
    //Send the filename to add to the server
    if (sendto(sd, filename, strlen(filename), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "Error: couldn't send the filename to the server.\n");
        return 1;
    } else {
        printf("Filename correctly sent.\n");
    }
    
    if (strcmp(extension, "txt") == 0) {
    	type = 0;
    } else if ((strcmp(extension, "jpg") == 0) | (strcmp(extension, "png") == 0)) {
    	type = 1;
    } else if (strcmp(extension, "mp3") == 0) {
    	type = 2;
    } else {
    	printf("File extension not expected.\n");
	return 1;
    }
    
    switch(type){

    	case 0:
    			put_textfile(sd, addr, addrlen, buff, sendline, filename);
    			break;
    	case 1:
			put_image(sd, addr, addrlen, buff, sendline, filename);
			break;
    	case 2:
			//put_audio();
			break;
	default:
			printf("File extension not recognized.\n");
			return 1;
    }

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
    char *tok, *filename, *ext;
    
    //Check command line
    if (argc != 2) {
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
        printf("\n\033[0;34mChoose an operation:\033[0m ");
        fgets(buff, sizeof(buff), stdin);
        
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

            //res = request_get(sockfd, servaddr, filename);

            //Find the extension of the file
            ext = strrchr(filename, '.');
	    if (!ext) {
	    	res = request_get(sockfd, servaddr, filename, ext); //no extension
	    } else {
            	res = request_get(sockfd, servaddr, filename, ext+1);
	    }

        } else if (strcmp(tok, "put") == 0) {

            //Execute PUT
            tok = strtok(NULL, " \n");
            if(tok == NULL){
            	printf("To correctly upload a file, insert also the name of the file!\n");
            	continue;
            }
            filename = strdup(tok);
            printf("Filename: %s\n", filename);

            //res = request_put(sockfd, servaddr, filename);

            //Find the extension of the file
            ext = strrchr(filename, '.');
	    if (!ext) {
	    	res = request_put(sockfd, servaddr, filename, ext); //no extension
	    } else {
	    	res = request_put(sockfd, servaddr, filename, ext+1);
	    }
	    

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
