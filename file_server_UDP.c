//
//  server_test.c
//
//
//  Created by Diana Pasquali on 20/07/2020.
//

#include "server_test.h"

#define SERV_PORT 5193
#define BACKLOG 10
#define MAXLINE 1024

void buf_clear(char *buffer)
{
    for (int i=0; i<MAXLINE; i++) buffer[i] = '\0';
}

int filelist_ctrl(char* filename) // control file già presente in filelist.
{
    FILE *fp;
    int ret = 1;
    char s[MAXLINE];
    
    fp = fopen("filelist.txt", "r");
    if (fp == NULL) {
        fprintf(stderr, "errore: impossibile aprire filelist.txt");
        ret = 0;
    } else {
        printf("filelist.txt aperto correttamente.\n");
    }
    printf("\n");
    while (fgets(s, strlen(filename)+1, fp)) {
        printf("data from filelist.txt: %s\n", s);
        printf("strcmp = %d\n", strncmp(filename, s, strlen(filename)));
        if (strncmp(filename, s, strlen(filename)) == 0) {
            ret = 0;
            break;
        }
    }
    fclose(fp);
    
    return ret;
}



void response_list(int sd, struct sockaddr_in servaddr)
{
    socklen_t addrlen = sizeof(servaddr);
    char buff[MAXLINE];
    char *sendline;
    FILE *fp;
    char *filename = "filelist.txt";
    size_t max_size;
    int count;
    
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "errore: impossibile aprire %s", filename);
        exit(-1);
    } else {
        printf("%s aperto correttamente.\n", filename);
    }
    
    // prelevo la lunghezza del contenuto del file
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    //invio la lunghezza del contenuto del file
    buf_clear(buff);
    sprintf(buff, "%ld", max_size);
    printf("file size: %ld\n", max_size);
    
    if (sendto(sd, buff, MAXLINE, 0, (struct sockaddr*)&servaddr, addrlen) == -1) {
        fprintf(stderr, "errore inviando la dimensione del file %s.", filename);
        exit(-1);
    } else {
        printf("dimensione di %s inviata correttamente.\n", filename);
    }
    
    /* invio il contenuto del file */
    sendline = (char*) malloc(max_size);
    
    // scrivo il contenuto del file sul buffer
    char ch;
    for (int i=0; i<max_size; i++) {
        ch = fgetc(fp);
        sendline[i] = ch;
        if (ch == EOF) break;
    }
    
    // invio il contenuto del file al client
    if(sendto(sd, sendline, max_size, 0, (struct sockaddr*)&servaddr, addrlen) == -1) {
        fprintf(stderr, "errore inviando il file al client.");
        exit(-1);
    }
    
    free(sendline);
    printf("sendline freed.\n\n");
    
}


void response_get(int sd, struct sockaddr_in servaddr)
{
    socklen_t addrlen = sizeof(servaddr);
    char buff[MAXLINE];
    char *sendline;
    FILE *fp;
    size_t max_size;
    
    // attende il nome del file
    printf("attendo il nome del file...\n");
    buf_clear(buff);
    recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&servaddr, &addrlen);
    
    fp = fopen(buff, "r");
    if (fp == NULL) {
        fprintf(stderr, "errore: impossibile aprire il file %s.", buff);
        exit(-1);
    } else {
        printf("il file %s è stato aperto correttamente.\n", buff);
    }
    
    // prelevo la dimensione del file
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    buf_clear(buff);
    sprintf(buff, "%ld", max_size);
    
    //invio la dimensione del file
    if (sendto(sd, buff, MAXLINE, 0, (struct sockaddr*)&servaddr, addrlen) == -1) {
        fprintf(stderr, "errore: impossibile inviare la dimensione del file.\n");
        exit(-1);
    } else {
        printf("dimensione del file inviata correttamente.\n");
    }
    
    //alloco spazio sufficiente per inviare il file
    sendline = (char*)malloc(max_size);
    
    char ch;
    for (int i=0; i<max_size; i++) {
        ch = fgetc(fp);
        sendline[i] = ch;
        if (ch == EOF) break;
    }
    
    // invio il file al client
    if (sendto(sd, sendline, max_size, 0, (struct sockaddr*)&servaddr, addrlen) == -1) {
        fprintf(stderr, "errore: impossibile inviare il contenuto del file richiesto.\n");
        exit(-1);
    } else {
        printf("contenuto del file inviato correttamente.\n");
        free(sendline);
        printf("sendline freed.\n\n");
    }
}

void response_put(int sd, struct sockaddr_in addr)
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    FILE *fp, *flp;
    char *filename;
    char *filelist = "filelist.txt";
    size_t file_size;
    char *recvline;
    
    //prelevo dal socket il nome del file da aggiungere
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) == -1) {
        fprintf(stderr, "errore: impossibile prelevare il nome del file.\n");
        exit(-1);
    } else {
        filename = strdup(buff);
        printf("filename: %s\n", filename);
    }
    
    // apro il file in scrittura
    fp = fopen(buff, "w+");
    if (fp == NULL) {
        fprintf(stderr, "errore: impossibile aprire il file %s", buff);
        exit(-1);
    } else {
        printf("file %s aperto correttamente.\n", filename);
    }
    
    if (filelist_ctrl(filename)) { // controllo se il file è già presente sul server
        flp = fopen(filelist, "a+");
        if (flp == NULL) {
            fprintf(stderr, "impossibile aprire filelist");
            exit(-1);
        } else {
            printf("filename aperto correttamente.\n");
        }
        
        fputs(filename, flp); // aggiungo il file al filelist
        fputs("\n", flp);
        fclose(flp);
    }
    
    
    // prelevo dal socket la dimensione del file
    buf_clear(buff);
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) == -1) {
        fprintf(stderr, "errore: impossibile ricevere la dimensione del file dal client.");
        exit(-1);
    } else {
        file_size = atoi(buff);
        printf("file size: %ld\n", file_size);
    }
    
    // ricevi il contenuto del file dal client
    recvline = (char*)malloc(file_size);
    if (recvfrom(sd, recvline, file_size, 0, (struct sockaddr*)&addr, &addrlen) == -1) {
        fprintf(stderr, "errore: impossibile prelevare il contenuto del file.");
        exit(-1);
    } else {
        printf("contenuto del file prelevato correttamente.\n");
    }
    // scrivo il contenuto sul file
    printf("\n\n");
    puts(recvline);
    printf("\n");
    fputs(recvline, fp);
    fflush(fp);
    fclose(fp);
    
    free(recvline);
    printf("recvline freed.\n");
}


int main(int argc, char* argv[])
{
    int listensd, connsd, nbytes;
    long file_size;
    struct sockaddr_in servaddr;
    char buff[MAXLINE];
    char *sendline;
    FILE *fp;
    socklen_t addrlen = sizeof(servaddr);
    char *cmd;
    
    // crea il listening socket
    if ((listensd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "errore in socket");
        exit(-1);
    }
    
    // inizalizzo l'indirizzo ip
    memset((void*)&servaddr, 0, sizeof(servaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // il server accetta connessioni su una qualunque delle sue interfacce di rete
    servaddr.sin_port = htons(SERV_PORT);
    
    // Assegna l'indirizzo al socket
    if (bind(listensd, (struct sockaddr*)&servaddr, addrlen) < 0) {
        fprintf(stderr, "errore in bind.");
        exit(-1);
    } else {
        printf("\033[0;32mbinding eseguito correttamente.\033[0m\n");
    }


    while(1) {
        printf("\033[0;34mattendo un comando...\033[0m\n");
        buf_clear(buff);
        recvfrom(listensd, buff, MAXLINE, 0, (struct sockaddr*)&servaddr, &addrlen);
        
        // Prelevo il tipo di comando
        cmd = strtok(buff, " \n");
        printf("comando selezionato: %s\n", cmd);
        
        if (strcmp(cmd, "list") == 0) {
            // effettuo il comando list
            response_list(listensd, servaddr);
        } else if (strcmp(cmd, "get") == 0) {
            // effettuo comando get
            response_get(listensd, servaddr);
        } else if (strcmp(cmd, "put") == 0) {
            // effettuo comando put
            response_put(listensd, servaddr);
        } else {
            fprintf(stderr, "errore: impossibile rilevare il comando.\n");
            exit(-1);
        }
    
    }
    exit(0);
}
