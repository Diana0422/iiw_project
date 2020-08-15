all:
	gcc -g *.c -Wall -Wextra -o client
    #gcc -g -O *.c -Wall -Wextra -Wno-return-type -Wno-unused-but-set-parameter -o server -lpthread
	
clean:
	-rm client
    #-rm server
