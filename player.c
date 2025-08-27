#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include "sharedMem.h"

int vector[][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}; 

int best_move(GameState * gameState, Player * player);


int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <width> <height>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);

    GameState * gameState = (GameState * ) open_shared_memory(NAME_BOARD, sizeof(GameState), O_RDONLY);
    GameSync * gameSync = (GameSync * ) open_shared_memory(NAME_SYNC, sizeof(GameSync), O_RDWR);

    pid_t pid = getpid();
    int playerCount = 0;

    while(playerCount < gameState->player_count){
        if(gameState->players[playerCount].pid == pid){
            break;
        }
        playerCount++;
    }

    while(!gameState->players[playerCount].is_blocked && !gameState->game_over){
        sem_wait(&gameSync->master_access_mutex);
        sem_post(&gameSync->master_access_mutex);

        sem_wait(&gameSync->reader_count_mutex);
        gameSync->readers_count++;
        if(gameSync->readers_count == 1){
            sem_wait(&gameSync->game_state_mutex);
        }
        sem_post(&gameSync->reader_count_mutex);

        unsigned int move;

        move = best_move(gameState, &gameState->players[playerCount]);

        int x = gameState->players[playerCount].x;
        int y = gameState->players[playerCount].y;

        sem_wait(&gameSync->reader_count_mutex);

        if (write(STDOUT_FILENO, &move, sizeof(unsigned int)) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        } 

        sem_wait(&gameSync->player_move_sem[playerCount]);
    }

    close_shared_memory(gameState, NAME_BOARD, sizeof(GameState) + sizeof(int) * gameState->width * gameState->height);
    close_shared_memory(gameSync, NAME_SYNC, sizeof(GameSync));

    return 0;
}