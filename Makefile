CC = gcc
CFLAGS = -Wall -g -std=c99 -pedantic
LDFLAGS = -lrt -lpthread -lm


all: vista player_simple master

vista: vista.c sharedMem.c
	$(CC) $(CFLAGS) vista.c sharedMem.c -o vista $(LDFLAGS)

player_simple: player_simple.c sharedMem.c
	$(CC) $(CFLAGS) player_simple.c sharedMem.c -o player_simple $(LDFLAGS)

master: master.c sharedMem.c
	$(CC) $(CFLAGS) master.c sharedMem.c -o master $(LDFLAGS)

clean:
	rm -f vista player_simple master