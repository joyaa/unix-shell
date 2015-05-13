FLAGS  += -g -pedantic -ansi -Wall -Werror -Wextra -O3
LDFLAGS += -lreadline

shell: shell.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm shell

.PHONY: clean

-include config.mk
