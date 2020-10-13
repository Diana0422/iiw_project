#include "server.h"

int handshake(Packet* pk, unsigned long* init_seq, int sockfd, struct sockaddr_in* addr, socklen_t addrlen, Timeout* to, timer_t timerid)
{

    *init_seq = rand_lim(1000);

    pk = create_packet(*init_seq, 0, 0, (char*)NULL, SYNACK);

    //Send a SYNACK to connect to the client    
    printf("Server sends a SYNACK packet.\n");   
    while (send_packet(pk, sockfd, (struct sockaddr*)addr, addrlen, to) == -1) {
        if(errno != EINTR){
            fprintf(stderr, "Couldn't contact client.\n");
            exit(EXIT_FAILURE);
        }
    }

    arm_timer(to, timerid);

    //Wait for ACK
    printf("Server waiting for the ACK to initialize the connection.\n");
    while (recv_packet(pk, sockfd, (struct sockaddr*)addr, addrlen, to) == -1) {
        if(errno != EINTR){
            fprintf(stderr, "Couldn't contact client.\n");
            exit(EXIT_FAILURE);
        }
    }

    if(pk->type != ACK){
        free(pk);
        return -1;
    }

    disarm_timer(timerid);

    return 0;
}

int demolition(int sockfd, struct sockaddr_in* addr, socklen_t addrlen, Timeout* to, timer_t timerid){
   
    Packet *pk = (Packet*)malloc(sizeof(Packet));
    if(pk == NULL){
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }
   
    pk = create_packet(0, 0, 0, (char*)NULL, FINACK);
    
    //Send a SYNACK to connect to the client    
    printf("Server sends a FINACK packet.\n");   
    while (send_packet(pk, sockfd, (struct sockaddr*)addr, addrlen, to) == -1) {
        if(errno != EINTR){
            fprintf(stderr, "Couldn't contact client.\n");
            exit(EXIT_FAILURE);
        }
    }

    arm_timer(to, timerid);

    //Wait for ACK
    printf("Server waiting for the ACK to close the connection.\n");
    while (recv_packet(pk, sockfd, (struct sockaddr*)addr, addrlen, to) == -1) {
        if(errno != EINTR){
            fprintf(stderr, "Couldn't contact client.\n");
            exit(EXIT_FAILURE);
        }
    }

    if(pk->type != ACK){
        free(pk);
        return -1;
    }

    disarm_timer(timerid);

    return 0;
    
    free(pk);
}
