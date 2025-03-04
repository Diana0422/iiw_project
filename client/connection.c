#include "client.h"

void handshake(Packet* pk, unsigned long* init_seq, unsigned long* init_ack, int sockfd, struct sockaddr_in* servaddr, socklen_t addrlen, Timer_node* timer)
{
    pk = create_packet(0, 0, 0, NULL, SYN);

    //Send first packet to initialize the connection
    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, &(timer->to)) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }

    //Start timer with base value of 3000 msec
    arm_timer(timer, 1);

    //Wait for confirmation
    while (recv_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, &(timer->to)) != -1) {        
        if(pk->type != SYNACK){
            continue;
        }else{
            disarm_timer(timer->timerid);
            break;
        }       
    }

    //Compute the timeout interval for exchange: SYN, SYNACK
    timeout_interval(&(timer->to));

    //Close the handshake
    *init_ack = pk->seq_num + 1;
    *init_seq = rand_lim(1000);

    free(pk);
    
    pk = create_packet(*init_seq, *init_ack, 0, NULL, ACK);    

    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, &(timer->to)) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }
}

void demolition(unsigned long sequence, unsigned long ack, int sockfd, struct sockaddr_in* servaddr, socklen_t addrlen, Packet_node* base, Timer_node* timer){
  
    Packet *pk = (Packet*)malloc(sizeof(Packet));
    if(pk == NULL){
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }

    pk = create_packet(sequence, ack, 0, NULL, FIN);

    //Send FIN packet to close the connection   
    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, &(timer->to)) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }
    base->pk = pk;
    arm_timer(timer, 0);

    //Wait for confirmation
    while (recv_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, &(timer->to)) != -1) {        
        if(pk->type != FINACK){
            continue;
        }else{
            disarm_timer(timer->timerid);
            break;
        }       
    }
 
    //Close the connection
    free(pk);

    pk = create_packet(sequence, ack, 0, NULL, ACK);    
    
    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen, &(timer->to)) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }

    free(pk);
}
