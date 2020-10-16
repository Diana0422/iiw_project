#include "packet.h"

#define INIT_WND_SIZE 10       //Initial dimension of the transmission window
#define MAX_RVWD_SIZE 1000        //Dimension of the receive buffer

extern void update_window(Packet**, int, Packet*);

extern void refresh_window(Packet*, int);