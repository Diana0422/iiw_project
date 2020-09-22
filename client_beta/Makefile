mono:
	gcc -g file_client_UDP.c conn_man.c file_man.c pck_man.c utils.c -Wall -Wextra -o client
	gcc -g client_generator.c -Wall -Wextra

multi:    
	gcc -g file_client_UDP.c conn_man.c file_man.c pck_man.c utils.c -Wall -Wextra -DPERFORMANCE -o client
	gcc -g client_generator.c -Wall -Wextra

clean:
	-rm client
	-rm a.out
