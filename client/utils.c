/*UTILS.C*/

#include "client_test.h"
#define MAXSIZE 200000

/*typedef struct message {
	char* type;
	int seq;
	size_t data_size;
	char* data;
}Packet;*/


/* CREATE_PACKET
 * @brief Allocate space for a packet and fill fields with metadata.
 * @param type: message type; seq_num: sequence number of the packet; size: data size; data: pointer to data buffer
 * @return Packet*
 */
 
Packet* create_packet(char* type, int seq_num, size_t size, char* data)
{
	Packet* pack = NULL;
	
	pack = (Packet*)malloc(sizeof(Packet));
	if (pack == NULL) {
		fprintf(stderr, "Error: Malloc() packet #%d.\n", seq_num);
		return NULL;
	} else {
		printf("Packet #%d successfully allocated.\n", seq_num);
	}
	
	//pack->data = NULL;
	
	pack->type = type;
	pack->seq = seq_num;
	pack->data_size = size;
	
	pack->data = (char*)malloc(pack->data_size);
	if (pack->data == NULL) {
		fprintf(stderr, "Error: malloc() pack->data.\n");
		return NULL;
	} else {
		printf("pack->data allocated successfully.\n\n");
	}
	
	memcpy(pack->data, data, pack->data_size);
	
	printf("--PACKET: \n");
	printf("  type:      %s \n", pack->type);
	printf("  seq:       %d  \n", pack->seq);
	printf("  data_size: %ld  \n", pack->data_size);
	printf("  data:      %s  \n\n\n", pack->data);
	
	return pack;
}

   
/* SERIALIZE_PACKET
 * @brief Serialize packet data into a buffer that can be sent through a socket
 * @param buffer: pointer to memory area where data needs to be saved, packet: packet to read, buf_size: pointer to the size of buffer after serialization.
 * @return char* buffer: pointer to the buffer serialized
 */
    
char* serialize_packet(char* buffer, Packet* packet, int* buf_size)
{
	char seq_num[MAXLINE];
	char size[MAXLINE];
    	

	memcpy(buffer, packet->type, strlen(packet->type));

	memcpy(buffer+strlen(packet->type), " ", 1);

	sprintf(seq_num, "%d", packet->seq);
	memcpy(buffer+strlen(packet->type)+1, seq_num, strlen(seq_num));
	
	memcpy(buffer+strlen(packet->type)+1+strlen(seq_num), " ", 1);
	
	sprintf(size, "%ld", packet->data_size);
	memcpy(buffer+strlen(packet->type)+1+strlen(seq_num)+1, size, strlen(size));
	
	
	memcpy(buffer+strlen(packet->type)+1+strlen(seq_num)+1+strlen(size), " ", 1);

	memcpy(buffer+strlen(packet->type)+1+strlen(seq_num)+1+strlen(size)+1, packet->data, packet->data_size);   	
	    	
	printf("\nPacket #%d serialized.\n\n", packet->seq);
	printf("-- buffer = %s\n\n", buffer);
	*buf_size = strlen(packet->type)+1+strlen(seq_num)+1+strlen(size)+1+packet->data_size;
	printf("buffer size = %d.\n\n", *buf_size);
	
	return buffer;
}


/* UNSERIALIZE_PACKET
 * @brief Unserialize data from a memory area into a packet (struct)
 * @param buffer: pointer to source memory area, packet: pointer to destination packet.
 * @return Packet* packet: pointer to packet.
 */

Packet* unserialize_packet(char* buffer, Packet* packet)
{
	char type[5];
	char seq[5];
	char size[MAXLINE];
	char *data = NULL;
	char *buf_cpy;
	int hdr_size;
	int data_size;
	int max_size;
	
	//printf("Unserializing packet..\n\n");
	
	max_size = strlen(buffer)+1;
	
	buf_cpy = (char*)malloc(max_size);
	if (buf_cpy == NULL) {
		fprintf(stderr, "Error: malloc() buf_cpy.\n");
		return NULL;
	} else {
		//printf("malloc() buf_cpy successful.\n");
	}
	
	//printf("-- buffer = %s\n", buffer);
	strcpy(buf_cpy, buffer);
	//printf("-- buf_cpy = %s\n", buf_cpy);
	
	sscanf(buf_cpy, "%s %s %s ", type, seq, size);
	//printf("type = %s\n", type);
	//printf("seq = %s\n", seq);
	//printf("size = %s\n", size);
	
	data_size = atoi(size);
	
	//printf("boh.\n");
	data = (char*)malloc(data_size);
	if (data == NULL) {
		fprintf(stderr, "Error: malloc() data.\n");
		return NULL;
	} else {
		//printf("malloc() data successful.\n");
	}
	//printf("bruh.\n");
	
	hdr_size = strlen(type)+1+strlen(seq)+1+strlen(size)+1;
	
	memcpy(data, buffer+hdr_size, data_size);
	//printf("data = %s\n", data);
	
	
	packet->type = (char*)malloc(strlen(type)+1);
	if (packet->type == NULL) {
		fprintf(stderr, "Error: couldn't malloc packet->type.\n");
		return NULL;
	} else {
		//printf("packet->type successfully allocated.\n");
	}
	strcpy(packet->type , type);
	
	packet->seq = atoi(seq);
	packet->data_size = data_size;
	
	packet->data = (char*)malloc(packet->data_size);
	if (packet->data == NULL) {
		fprintf(stderr, "Error: malloc() packet->data.\n");
		return NULL;
	} else {
		//printf("packet->data malloc successful.\n");
	}
	
	memcpy(packet->data, data, packet->data_size);
	
	/*printf("\nPacket #%d unserialized.\n\n", packet->seq);
	printf("--PACKET #%d: \n", packet->seq);
	printf("  type:      %s \n", packet->type);
	printf("  seq:       %d  \n", packet->seq);
	printf("  data_size: %ld  \n", packet->data_size);
	printf("  data:      %s  \n\n\n", packet->data);
	*/
	free(buf_cpy);
	free(data);
	
	return packet;
}
    

/* SEND_PACKET
 * @brief Sends a struct through socket;
 * @param pkt = pointer to struct to send, socket = socket descriptor, addr = destination address, addrlen = destination address length
 * @return 1 = error, n = ok
 */
 
 int send_packet(Packet *pkt, int socket, struct sockaddr* addr, socklen_t addrlen, int* size)
 {
 	char buffer[MAX_DGRAM_SIZE];
 	int n;
 	int len;
 	
 	serialize_packet(buffer, pkt, size);
 	len = *size;
 	
 	if ((n = sendto(socket, buffer, len, 0, (struct sockaddr*)addr, addrlen)) == -1) {
 		return -1;
    	} else {
 		printf("Packet #%d sent successfully.\n\n", pkt->seq);
 		memset(buffer, 0, MAX_DGRAM_SIZE);
    	}
    	
    	return n;
 }
 

/* RECV_PACKET
 * @brief Receive a packet/struct through a socket
 * @param pkt: pointer to packet, socket: socket descriptor, addr: pointer to address struct, addlen: size of addr
 * @return error: -1, success: n
 */
 
 int recv_packet(Packet *pkt, int socket, struct sockaddr* addr, socklen_t addrlen) 
 {
 	char buffer[MAX_DGRAM_SIZE];
 	int n;
 	
 	memset(buffer, 0, MAX_DGRAM_SIZE);
 	
 	if ((n = recvfrom(socket, buffer, MAX_DGRAM_SIZE, 0, (struct sockaddr*)addr, &addrlen)) == -1) {
 		return -1;
 	} else {
 		//printf("Packet received successfully.\n");
 		unserialize_packet(buffer, pkt);
	} 
	
	return n;
 }
 
 
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
 	
 	/*if (write_size != file_dim) {
 		fprintf(stderr, "Error: couldn't write all bytes on file.\n");
 		return NULL;
	} else {
		printf("File written successfully.\n");
	}*/
 	
 	fseek(fp, 0, SEEK_END);
 	return fp;
 	
 }
 
 
 /*int main()
 {
 	Packet* pk = NULL;
 	Packet* pk2 = NULL;
 	FILE *fp;
 	//char* filename1 = "prova";
 	char* filename1 = "pika.jpeg";
 	//char* filename1 = "family.png";
 	FILE *cp;
 	char* filename2;
 	char *buffer = NULL;
 	char *content = NULL;
 	size_t write_size = 0;
 	size_t d_size = 0;
 	size_t read_size = 0;
 	int size_p;
	
	pk2 = (Packet*)malloc(sizeof(Packet));
	if (pk2 == NULL) {
		fprintf(stderr, "Error: Malloc() packet 2.\n");
		exit(EXIT_FAILURE);
	} else {
		printf("Packet 2 successfully allocated.\n");
	}
	
	content = read_file(fp, filename1, &d_size);
	pk = create_packet("data", 0, d_size, content);
	
	int size = MAXLINE + d_size;
	printf("Buffer size = %d.\n", size);
	
	
	buffer = (char*)malloc(size);
	if (buffer == NULL) {
		fprintf(stderr, "Error: Malloc() buffer.\n");
		exit(EXIT_FAILURE);
	} else {
		printf("Buffer allocated successfully.\n");
	}
	
	printf("--PACKET: \n");
	printf("  type:      %s \n", pk->type);
	printf("  seq:       %d  \n", pk->seq);
	printf("  data_size: %ld  \n", pk->data_size);
	printf("  data:      %s  \n", pk->data);
	
	
	serialize_packet(buffer, pk, &size_p);
	free(pk);
	printf("\n\n-- buffer = %s\n", buffer);
	
	unserialize_packet(buffer, pk2);
	
	free(buffer);
	
	printf("--PACKET COPY: \n");
	printf("  type:      %s \n", pk2->type);
	printf("  seq:       %d  \n", pk2->seq);
	printf("  data_size: %ld  \n", pk2->data_size);
	printf("  data:      %s  \n", pk2->data);

	
	//cp = write_file(cp, "copia.txt", pk2->data, pk2->data_size);
	//cp = write_file(cp, "copia.png", pk2->data, pk2->data_size);
	cp = write_file(cp, "copia.jpeg", pk2->data, pk2->data_size);
	
	fclose(cp);
	free(pk2);
 }*/
