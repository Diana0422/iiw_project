#include "client.h"

void handshake(Packet* pk, unsigned long* init_seq, unsigned long* init_ack, int sockfd, struct sockaddr_in* servaddr, socklen_t addrlen, Timeout* to, timer_t timerid)
{
    pk = create_packet(0, 0, 0, NULL, SYN);

    //Send first packet to initialize the connection     
    printf("Client sends first SYN packet.\n");  
    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, to) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }

    //Start timer with base value of 3000 msec
    arm_timer(to, timerid, 1);

    //Wait for confirmation
    printf("Client waiting for SYNACK packet.\n");
    while (recv_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, to) != -1) {        
        if(pk->type != SYNACK){
            continue;
        }else{
            disarm_timer(timerid);
            break;
        }       
    }

    //Compute the timeout interval for exchange: SYN, SYNACK
    timeout_interval(to);

    //Close the handshake
    *init_ack = pk->seq_num + 1;
    *init_seq = rand_lim(1000);

    free(pk);
    
    pk = create_packet(*init_seq, *init_ack, 0, NULL, ACK);    

    printf("Client initializes connection, sending the last ACK.\n");     
    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, to) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }
}

void demolition(unsigned long sequence, unsigned long ack, int sockfd, struct sockaddr_in* servaddr, socklen_t addrlen, Timeout* to, timer_t timerid){
  
    Packet *pk = (Packet*)malloc(sizeof(Packet));
    if(pk == NULL){
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }

    pk = create_packet(sequence, ack, 0, NULL, FIN);

    //Send FIN packet to close the connection     
    printf("Client sends FIN packet.\n");  
    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, to) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }

    arm_timer(to, timerid, 0);

    //Wait for confirmation
    printf("Client waiting for FINACK packet.\n");
    while (recv_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, to) != -1) {        
        if(pk->type != FINACK){
            continue;
        }else{
            disarm_timer(timerid);
            break;
        }       
    }
 
    //Close the connection
    free(pk);

    pk = create_packet(sequence, ack, 0, NULL, ACK);    

    printf("Client terminates connection, sending the last ACK.\n");     
    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, to) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }

    free(pk);
}
