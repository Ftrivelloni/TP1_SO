CC = gcc
CFLAGS = -Wall -g -std=c99
LDFLAGS = -lrt -lpthread

all: master vista player player_simple

master: master.c sharedMem.c sharedMem.h
	$(CC) $(CFLAGS) -o master master.c sharedMem.c $(LDFLAGS)

vista: vista.c sharedMem.c sharedMem.h
	$(CC) $(CFLAGS) -o vista vista.c sharedMem.c $(LDFLAGS)

player: player.c sharedMem.c sharedMem.h
	$(CC) $(CFLAGS) -o player player.c sharedMem.c $(LDFLAGS)

player_simple: player_simple.c sharedMem.c sharedMem.h
	$(CC) $(CFLAGS) -o player_simple player_simple.c sharedMem.c $(LDFLAGS)

clean:
	rm -f master vista player player_simple *.o

run: all
	./master -w 15 -h 15 -d 200 -t 10 -v ./vista -p ./player ./player

# Run with the provided ChompChamps binary
run-provided: vista player_simple
	chmod +x ./ChompChamps
	./ChompChamps -w 10 -h 10 -d 200 -t 30 -v ./vista -p ./player_simple ./player_simple

# Docker targets (using the required image)
docker-build:
	./run_docker.sh make all

docker-clean:
	./run_docker.sh make clean

docker-run:
	./run_docker.sh make run

docker-run-provided:
	./run_docker.sh make run-provided

docker-shell:
	./run_docker.sh

.PHONY: all clean run run-provided docker-build docker-clean docker-run docker-run-provided docker-shell
