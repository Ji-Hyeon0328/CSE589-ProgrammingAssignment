CC ?= cc
CFLAGS ?= -Wall -Wextra -O2 -std=c11

.PHONY: all clean

all: client server

client: client.c
	$(CC) $(CFLAGS) -o client client.c

server: server.c
	$(CC) $(CFLAGS) -o server server.c

clean:
	rm -f client server
