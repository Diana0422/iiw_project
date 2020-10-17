#include "packet.h"

#define INIT_WND_SIZE 10       //Initial dimension of the transmission window
#define MAX_RVWD_SIZE 10        //Dimension of the receive buffer

extern void update_window(Packet*, Packet**, short*);

extern void refresh_window(Packet**, int, short*);

extern void order_rwnd(Packet*, Packet** buff, int);

extern int store_rwnd(Packet*, Packet**, int);

extern void print_wnd(Packet**);

extern void print_rwnd(Packet**);