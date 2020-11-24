#include "packet.h"
#include <stdlib.h>

#define INIT_WND_SIZE 10       //Initial dimension of the transmission window
#define MAX_RVWD_SIZE 10        //Dimension of the receive buffer

extern void update_window(Packet*, Packet*(*)[INIT_WND_SIZE], int, short*);

extern void refresh_window(Packet*(*)[INIT_WND_SIZE], int, int, short*);

extern void order_rwnd(Packet*, Packet** buff, int);

extern void store_rwnd(Packet*, Packet**, int);

extern void print_wnd(Packet**);

extern void print_rwnd(Packet**);