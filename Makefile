FLAGS  += -g -pedantic -ansi -Wall -O4

shell: shell.c
	$(CC) $(CFLAGS) $^ -o $@ 

clean:
	rm shell
