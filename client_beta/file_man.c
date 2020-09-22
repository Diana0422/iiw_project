/*UTILS.C - SERVER SIDE*/

#include "client.h"
 
/* READ_FILE
 * @brief Read file and save content into a buffer
 * @param fp: pointer to file, filename: name of the file to read, size: pointer to size of read data
 * @return buffer: buffer containing file content
 */
 
 char* read_file(FILE *fp, char* filename, size_t *size)
 {
 	char* buffer = NULL;
 	size_t file_dim;
 	size_t read_size;
 	
 	// Open the file to read
 	fp = fopen(filename, "rb+");
 	if (fp == NULL) {
 		fprintf(stderr, "Error: couldn't open file %s.\n", filename);
 		return NULL;
	} else {
		printf("File %s opened successfully.\n", filename);
	}
	
	// Get file dimension
 	fseek(fp, 0, SEEK_END);
 	file_dim = ftell(fp);
 	fseek(fp, 0, SEEK_SET);
 	
 	*size = file_dim;
 	printf("size = %ld.\n", *size);
 	
 	// Allocate sufficient memory in buffer to contain file content
 	buffer = (char*)malloc(file_dim);
 	if (buffer == NULL) {
 		fprintf(stderr, "Error: couldn't malloc() buffer in read_file().\n");
 		return NULL;
 	} else {
 		printf("Buffer in read_file() allocated successfully.\n");
	}
	
	read_size = fread(buffer, sizeof(char), file_dim, fp);
	printf("Bytes read %ld/%ld\n", read_size, file_dim);
	
	if (read_size != file_dim) {
		fprintf(stderr, "Error: couldn't read all bytes from the file.\n");
		return NULL;
	} else {
		printf("File read correctly.\n");
	}
 
 	fclose(fp);
 	return buffer;
 }
 
 /* WRITE_FILE
  * @brief Read buffer and write content into a file
  * @param fp: pointer to file, filename: name of the file to read, file_dim: pointer to size of read data
  * @return fp: pointer to file
  */
 
 FILE* write_file(FILE* fp, char* filename, char* buffer, size_t file_dim)
 {
 	size_t write_size = 0;
 	
 	fp = fopen(filename, "a+");
 	if (fp == NULL) {
 		fprintf(stderr, "Error: couldn't open file %s.\n", filename);
 		return NULL;
 	} else {
 		printf("File %s opened successfully.\n", filename);
 	}
 	
 	write_size = fwrite(buffer, 1, file_dim, fp);
 	printf("Bytes written %ld/%ld\n", write_size, file_dim); 
 	
 	fseek(fp, 0, SEEK_END);
 	return fp;
 	
 }
 
 
 