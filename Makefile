CC = gcc
CFLAGS = -Wall -g -std=c99 -pedantic
LDFLAGS = -lrt -lpthread -lm

all: vista player player_simple

vista: vista.c sharedMem.c sharedMem.h
    $(CC) $(CFLAGS) -o vista vista.c sharedMem.c $(LDFLAGS)

player: player.c sharedMem.c sharedMem.h
    $(CC) $(CFLAGS) -o player player.c sharedMem.c $(LDFLAGS)

player_simple: player_simple.c sharedMem.c sharedMem.h
    $(CC) $(CFLAGS) -o player_simple player_simple.c sharedMem.c $(LDFLAGS)

clean:
    rm -f vista player player_simple

# Run with the provided ChompChamps binary
run-provided: vista player
    chmod +x ./ChompChamps
    ./ChompChamps -w 10 -h 10 -d 200 -t 30 -v ./vista -p ./player ./player

# New targets
player-only: vista player
    @echo "Compiled vista and player only"

run-chomper: vista player
    chmod +x ./ChompChamps
    ./ChompChamps -w 10 -h 10 -d 200 -t 30 -v ./vista -p ./player ./player

.PHONY: all clean run-provided player-only run-chomper
