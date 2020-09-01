mono:
	gcc -g file_client_UDP.c utils_client.c -Wall -Wextra -o client
	gcc -g client_generator.c -Wall -Wextra

multi:    
	gcc -g file_client_UDP.c utils_client.c -Wall -Wextra -DPERFORMANCE -o client
	gcc -g client_generator.c -Wall -Wextra

clean:
	-rm client
	-rm a.out
