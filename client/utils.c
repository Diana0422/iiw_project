/*UTILS.C*/

#include "client_test.h"
/* 
-------------------------------------------------------------------------------------------------------------------------------------------------
	IO FUNCTIONS
---------------------------------------------------------------------------------------------------------------------------------------------------
*/

/*
 * @brief prints a red error message
 * @param "flag": flag that enables perror, "string": message to print
 * @return void
 */
 void print_error(int flag,char *string)
 {

	printf("\033[1;31m\n");

	if(flag == 1){
		perror(string);
	}
	else{
		printf("%s\n",string);
	}

	printf("\033[0m\n");
 }


/*
 * @brief prints a message of successful operation (in green)
 * @param "string": message to print
 * @return void
 */
 
 void print_success(char* string)
 {
 	printf("\033[1;32m\n\n");
 	
 	printf("%s", string);
 	
 	printf("\033[0m\n\n");
 
 }
 
/*
 * @brief reads data from a file writes them on a buffer
 * @param "buffer": buffer on which the function writes data, "fp": pointer to the file, "filesize": file dimension
 * @return int: byte read
 */
 
 
 int read_file(char* buffer, FILE* fp, size_t filesize)
 {
 	char ch;
 	int bytes = 0;
 	
	for (int i=0; i<(int)filesize; i++) {
		ch = fgetc(fp);
		buffer[i] = ch;
		bytes++;
        	if (ch == EOF) break;
    	}
    	
    	return bytes;
 }
 
 
 /*
  * @brief reads data from a buffer and writes them on a file
  * @param "buffer": buffer to read, "fp": pointer to the file to write, "size": data size 
  * @return int: number of bytes written
  */
  
  int write_file(char* buffer, FILE* fp, size_t size)
  {
  	char ch;
  	char bytes = 0;
  	
  	for (int i=0; i<(int)size; i++) {
  		ch = buffer[i];
  		if (fputc(ch, fp) == EOF) break;
  		bytes++;
  	}	
  	
  	return bytes;
  }
  
  
  
 


