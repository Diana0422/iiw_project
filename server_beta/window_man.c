#include "windows.h"

void update_window(Packet** list, int pos, Packet* sent){
	int i = 0;

	while(&list[pos][i] != NULL){
		i++;
	}

	list[pos][i] = *sent;
}

void refresh_window(Packet* list, int pos){
	Packet* new_first = &list[pos+1];

	int i = 0;

	while(new_first+i != NULL){
		list[i] = *(new_first+i);
		memset(&list[pos+1+i], 0, sizeof(list[pos+1+i]));
		i++;
	}

	while(&list[i] != NULL){
		memset(&list[i], 0, sizeof(list[i]));
		i++;
	}
}