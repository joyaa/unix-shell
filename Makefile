FLAGS  += -g -pedantic -ansi -Wall -O4

shell: shell.c
	$(CC) $(FLAGS) $(CFLAGS) $^ -o $@ 

clean:
	rm shell
