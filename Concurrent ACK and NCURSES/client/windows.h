#include "packet.h"

#define INIT_WND_SIZE 10       //Initial dimension of the transmission window
#define MAX_RVWD_SIZE 10        //Dimension of the receive buffer

extern void update_window(Packet*, Packet**, short*);

extern void refresh_window(Packet**, int, short*);

extern void order_rwnd(Packet*, Packet** buff, int);

extern void store_rwnd(Packet*, Packet**, int);

extern Packet_node* insert_base(Packet_node**);

extern void print_wnd(Packet**);

extern void print_rwnd(Packet**);

extern void incoming_ack(unsigned long, int*, unsigned long*, Packet**, short*, Timeout, Timer_node*);