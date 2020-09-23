#include "client.h"

void handshake(Packet* pk, int* init_seq, int* init_ack, int sockfd, struct sockaddr_in* servaddr, socklen_t addrlen)
{
    pk = create_packet(0, 0, 0, NULL, SYN);

    //Send first packet to initialize the connection     
    printf("Client sends first SYN packet.\n");  
    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }

    //Wait for confirmation
    printf("Client waiting for SYNACK packet.\n");
    while (recv_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }

    if(pk->type != SYNACK){
        free(pk);
        failure("Connection failed.");
    }
   
    //Close the handshake
    *init_ack = pk->seq_num + 1;
    *init_seq = rand_lim(100000);

    free(pk);
    pk = create_packet(*init_seq, *init_ack, 0, NULL, ACK);    

    printf("Client initializes connection, sending last ACK and .\n");     
    while (send_packet(pk, sockfd, (struct sockaddr*)servaddr, addrlen) == -1) {
        if(check_failure("\033[0;31mError: couldn't contact server.\033[0m\n")){
            continue;
        }
    }
}

/*void demolition(){

}*/
