.PHONY: all clean run run-4 docker-run docker-run-4

# Docker commands
DOCKER_IMG = agodio/itba-so-multi-platform:3.0
DOCKER_RUN = docker run --privileged --rm -v $(CURDIR):/home/frant/TP1_SO $(DOCKER_IMG)

# Regular targets for inside container
all:
    gcc -Wall -g -std=c99 -pedantic -o vista vista.c sharedMem.c -lrt -lpthread -lm
    gcc -Wall -g -std=c99 -pedantic -o player_simple player_simple.c sharedMem.c -lrt -lpthread -lm

clean:
    rm -f vista player_simple

run:
    gcc -Wall -g -std=c99 -pedantic -o vista vista.c sharedMem.c -lrt -lpthread -lm
    gcc -Wall -g -std=c99 -pedantic -o player_simple player_simple.c sharedMem.c -lrt -lpthread -lm
    chmod +x ./ChompChamps
    ./ChompChamps -w 10 -h 10 -d 200 -t 30 -v ./vista -p ./player_simple ./player_simple

run-4:
    gcc -Wall -g -std=c99 -pedantic -o vista vista.c sharedMem.c -lrt -lpthread -lm
    gcc -Wall -g -std=c99 -pedantic -o player_simple player_simple.c sharedMem.c -lrt -lpthread -lm
    chmod +x ./ChompChamps
    ./ChompChamps -w 15 -h 15 -d 200 -t 30 -v ./vista -p ./player_simple ./player_simple ./player_simple ./player_simple

# Docker targets that run from host machine
docker-run:
    $(DOCKER_RUN) bash -c "cd /home/frant/TP1_SO && \
    gcc -Wall -g -std=c99 -pedantic -o vista vista.c sharedMem.c -lrt -lpthread -lm && \
    gcc -Wall -g -std=c99 -pedantic -o player_simple player_simple.c sharedMem.c -lrt -lpthread -lm && \
    chmod +x ./ChompChamps && \
    ./ChompChamps -w 10 -h 10 -d 200 -t 30 -v ./vista -p ./player_simple ./player_simple"

docker-run-4:
    $(DOCKER_RUN) bash -c "cd /home/frant/TP1_SO && \
    gcc -Wall -g -std=c99 -pedantic -o vista vista.c sharedMem.c -lrt -lpthread -lm && \
    gcc -Wall -g -std=c99 -pedantic -o player_simple player_simple.c sharedMem.c -lrt -lpthread -lm && \
    chmod +x ./ChompChamps && \
    ./ChompChamps -w 15 -h 15 -d 200 -t 30 -v ./vista -p ./player_simple ./player_simple ./player_simple ./player_simple"