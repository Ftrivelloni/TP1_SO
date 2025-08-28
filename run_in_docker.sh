#!/bin/bash
# filepath: /home/frant/TP1_SO/run_in_docker.sh

docker run --privileged --rm -v $(pwd):/home/frant/TP1_SO agodio/itba-so-multi-platform:3.0 bash -c "
cd /home/frant/TP1_SO && 
gcc -Wall -g -std=c99 -pedantic -o vista vista.c sharedMem.c -lrt -lpthread -lm &&
gcc -Wall -g -std=c99 -pedantic -o player_simple player_simple.c sharedMem.c -lrt -lpthread -lm &&
chmod +x ./ChompChamps &&
./ChompChamps -w 10 -h 10 -d 200 -t 30 -v ./vista -p ./player_simple ./player_simple
"