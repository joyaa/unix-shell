FLAGS  += -g -pedantic -ansi -Wall -O4
LDFLAGS += -lreadline

shell: shell.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm shell
