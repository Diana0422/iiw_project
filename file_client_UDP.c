//
//  client_test.c
//  
//
//  Created by Diana Pasquali on 20/07/2020.
//

#include "client_test.h"

#define SERV_PORT 5193
#define MAXLINE 5036

void buf_clear(char *buffer)
{
    for (int i=0; i<MAXLINE; i++) buffer[i] = '\0';
}


int request_list(int sd, struct sockaddr_in addr) // OK
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *recvline;
    size_t max_size;
    int err = 0;
    char *filename;
    FILE *f;
    int n, count = 0;
    
    // ricevo la lunghezza del contenuto del filelist.txt
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) > 0) {
        max_size = atoi(buff);
        printf("\033[0;34mfile size:\033[0m %ld\n\n", max_size);
        recvline = (char*)malloc(max_size);
    } else {
        fprintf(stderr, "errore: impossibile prelevare la lunghezza del filelist.txt.\n");
        err = 1;
    }
    
    printf("\033[1;34m--- filelist.txt ---\033[0m\n");
    if ((n = recvfrom(sd, recvline, max_size, 0, (struct sockaddr*)&addr, &addrlen)) != -1) {
        recvline[n] = 0;
        if (puts(recvline) == EOF){
            fprintf(stderr, "errore in puts");
            err = 1;
        }
    }
    
    free(recvline);
    buf_clear(buff);
    printf("\nrecvline freed.\n\n\n");
    
    if (n < 0) {
        fprintf(stderr, "errore in read");
        err = 1;
    }
    
    return err;
}

int request_get(int sd, struct sockaddr_in addr, char *filename) // OK
{
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    char *recvline;
    size_t max_size;
    FILE *fp;
    char *s = " ";
    int err = 0;
    int n, i;
    
    // invia il filename al server
    if (sendto(sd, filename, strlen(filename), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "errore: impossibile inviare il filename al server");
        err = 1;
    } else {
        printf("filename inviato correttamente.\n");
    }
    
    // prelevo la dimensione dello spazio da allocare
    if (recvfrom(sd, buff, MAXLINE, 0, (struct sockaddr*)&addr, &addrlen) > 0) {
        max_size = atoi(buff);
        recvline = (char*)malloc(max_size);
    } else {
        fprintf(stderr, "errore: impossibile prelevare la dimensione del file %s.\n", filename);
        err = 1;
    }
    
    // Creo apro il file in scrittura
    fp = fopen(filename, "w+");
    if (fp == NULL) {
        fprintf(stderr, "errore: impossibile aprire il file %s", filename);
        err = 1;
    } else {
        printf("file %s aperto correttamente.\n", filename);
    }
    
    // Ricevo i dati da inserire nel file
    if ((n = recvfrom(sd, recvline, max_size, 0, (struct sockaddr*)&addr, &addrlen)) != -1) {
        recvline[n] = 0;
        if (fputs(recvline, fp) == EOF){
            fprintf(stderr, "errore in fputs");
            err = 1;
        }
    }
    
    free(recvline);
    printf("recvline free.\n");
    err = 0;
    return err;
    
}

int request_put(int sd, struct sockaddr_in addr, char *filename)
{
    int err = 0;
    socklen_t addrlen = sizeof(addr);
    char buff[MAXLINE];
    FILE *fp;
    size_t max_size;
    char *sendline;
    
    // Invio il nome del file da aggiungere sul server
    if (sendto(sd, filename, strlen(filename), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "errore: impossibile inviare il filename al server.\n");
        err = 1;
    } else {
        printf("filename inviato correttamente al server.\n");
    }
    
    // Apro il file e ne prelevo la dimensione
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "errore: impossibile aprire file %s.", filename);
        err = 1;
    } else {
        printf("file %s aperto correttamente.\n", filename);
    }
    
    fseek(fp, 0, SEEK_END);
    max_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("file_size = %ld\n", max_size);
    
    // Invio la dimensione del contenuto del file
    sprintf(buff, "%ld", max_size);
    if (sendto(sd, buff, strlen(buff), 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "errore: impossibile inviare la dimensione del file.");
        err = 1;
    } else {
        printf("dimensione del file inviata correttamente.\n");
    }
    
    /* invio il contenuto del file */
    //alloco lo spazio necessario in sendline
    sendline = (char*)malloc(max_size);
    
    //inserisco il contenuto del file in sendline
    char ch;
    for (int i=0; i<max_size; i++) {
        ch = fgetc(fp);
        sendline[i] = ch;
        if (ch == EOF) break;
    }
    
    printf("\n\n");
    puts(sendline);
    printf("\n");
    
    //invio al server
    if (sendto(sd, sendline, max_size, 0, (struct sockaddr*)&addr, addrlen) == -1) {
        fprintf(stderr, "errore: impossibile inviare il contenuto al server.");
        err = 1;
    } else {
        printf("contenuto del file %s inviato correttamente al server.\n", filename);
    }
    
    fclose(fp);
    free(sendline);
    printf("sendline freed.\n");
    return err;
}

int main(int argc, char* argv[])
{
    int sockfd, n, idx;
    struct sockaddr_in servaddr;
    char buff[MAXLINE];
    socklen_t addrlen = sizeof(servaddr);
    int res1 = 0;
    int res2 = 0;
    int res3 = 0;
    char *cmd, *tok;
    char *filename;
    
    // controllo sui parametri inseriti in linea di comando
    if (argc != 2) {
        fprintf(stderr, "utilizzo: client_test <indirizzo ip server>");
        exit(-1);
    }
    
    // crea il socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "\033[0;31merrore in socket\033[0m");
        exit(-1);
    }
    
    // inizializzo i valori di indirizzo
    memset((void*)&servaddr, 0, addrlen);
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT); // assegna la porta del server
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "errore in inet_pton.");
        exit(-1);
    }
    
    // Legge dal socket fino a che non trova EOF
    
    while (1) {
        // scegliere un comando:
        printf("\n\033[0;34mInserire un comando:\033[0m ");
        fgets(buff, sizeof(buff), stdin);
        
        // inviare il comando al server
        if (sendto(sockfd, buff, strlen(buff), 0, (struct sockaddr*)&servaddr, addrlen) == -1) {
            fprintf(stderr, "\033[0;31mimpossibile inviare il comando al server.\033[0m\n");
            exit(-1);
        } else {
            printf("\n\033[0;32mcomando inviato con successo.\033[0m\n");
        }
        
        // prelevo il tipo di comando
        tok = strtok(buff, " \n");
        cmd = strdup(tok);
        printf("\033[0;34mcomando selezionato:\033[0m %s\n", cmd);
        
        
        if (strcmp(cmd, "list") == 0) {
            // eseguo comando list
            res1 = request_list(sockfd, servaddr);
        } else if (strcmp(cmd, "get") == 0) {
            // eseguo comando get
            tok = strtok(NULL, " \n");
            filename = strdup(tok);
            printf("filename: %s\n", filename);
            res2 = request_get(sockfd, servaddr, filename);
        } else if (strcmp(cmd, "put") == 0) {
            // effettuo comando put
            tok = strtok(NULL, " \n");
            filename = strdup(tok);
            printf("filename: %s\n", filename);
            res3 = request_put(sockfd, servaddr, filename);
        } else {
            fprintf(stderr, "\033[0;31merrore: impossibile rilevare il comando.\033[0m\n");
            exit(-1);
        }
        
        if (res1 == 1) {
            fprintf(stderr, "\033[0;31merrore in client_list.\033[0m\n");
            exit(-1);
        } else if (res2 == 1) {
            fprintf(stderr, "\033[0;31merrore in client_get.\033[0m\n");
            exit(-1);
        } else if (res3 == 1) {
            fprintf(stderr, "\033[0;31merrore in client_put.\033[0m\n");
            exit(-1);
        } else {
            printf("tutto ok.\n");
        }
        
        break;
    }
    
    exit(0);
}
