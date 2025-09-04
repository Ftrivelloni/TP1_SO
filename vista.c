#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h> 
#include "sharedMem.h"

GameState* game_state = NULL;
GameSync* game_sync = NULL;
size_t game_state_size = 0;

void display_game_state();
void cleanup();
void sig_handler(int signo);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <width> <height>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    game_state_size = sizeof(GameState) + width * height * sizeof(int);
    
    int fd_state = shm_open(NAME_BOARD, O_RDONLY, 0666);
    if (fd_state == -1) {
        perror("shm_open state");
        exit(EXIT_FAILURE);
    }
    
    game_state = (GameState*)mmap(NULL, game_state_size, PROT_READ, MAP_SHARED, fd_state, 0);
    if (game_state == MAP_FAILED) {
        perror("mmap state");
        close(fd_state);
        exit(EXIT_FAILURE);
    }
    close(fd_state);
    
    int fd_sync = shm_open(NAME_SYNC, O_RDWR, 0666);
    if (fd_sync == -1) {
        perror("shm_open sync");
        munmap(game_state, game_state_size);
        exit(EXIT_FAILURE);
    }
    
    game_sync = (GameSync*)mmap(NULL, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd_sync, 0);
    if (game_sync == MAP_FAILED) {
        perror("mmap sync");
        close(fd_sync);
        munmap(game_state, game_state_size);
        exit(EXIT_FAILURE);
    }
    close(fd_sync);
    
    while (!game_state->game_over) {
        sem_wait(&game_sync->view_update_sem);
        display_game_state();
        sem_post(&game_sync->view_done_sem);
    }

    display_game_state();
    sem_post(&game_sync->view_done_sem);

    cleanup();
    return 0;
}

void display_game_state() {
    int width = game_state->width;
    int height = game_state->height;
    int player_count = game_state->player_count;
    
    printf("\033[2J\033[H");
    
    printf("===== ChompChamps =====\n");
    printf("Game Status: %s\n", game_state->game_over ? "GAME OVER" : "IN PROGRESS");
    printf("\n");
    
    printf("Players:\n");
    for (int i = 0; i < player_count; i++) {
        int color = 31 + (i % 7);
        printf("\033[%dm[%d] %s - Score: %u, Position: (%u,%u), Valid Moves: %u, Invalid Moves: %u, %s\033[0m\n",
              color, i, game_state->players[i].name, game_state->players[i].score,
              game_state->players[i].x, game_state->players[i].y,
              game_state->players[i].valid_moves, game_state->players[i].invalid_moves,
              game_state->players[i].is_blocked ? "BLOCKED" : "ACTIVE");
    }
    printf("\n");
    
    printf("Board:\n");
    printf("   ");
    for (int x = 0; x < width; x++) {
        printf("%2d ", x);
    }
    printf("\n");
    
    const char* player_colors[] = {
        "\033[31m",  // Red
        "\033[34m",  // Blue
        "\033[35m",  // Magenta
        "\033[36m",  // Cyan
        "\033[33m",  // Yellow
        "\033[97m",  // White
        "\033[37;44m", // White on blue
        "\033[37;45m", // White on magenta
        "\033[37;46m"  // White on cyan
    };
    
    for (int y = 0; y < height; y++) {
        printf("%2d ", y);
        for (int x = 0; x < width; x++) {
            int cell_value = game_state->board[y * width + x];
            
            bool player_present = false;
            for (int i = 0; i < player_count; i++) {
                if (game_state->players[i].x == x && game_state->players[i].y == y) {
                    player_present = true;
                    break;
                }
            }
            
            if (player_present) {
                printf("\033[1;33m # \033[0m");
            } else if (cell_value > 0) {
                printf("\033[32m%2d \033[0m", cell_value);
            } else {
                int owner = -cell_value - 1;
                
                if (owner >= 0 && owner < player_count) {
                    printf("%s%2d \033[0m", player_colors[owner % 9], owner);
                } else {
                    printf("\033[31m ? \033[0m");
                }
            }
        }
        printf("\n");
    }
   
    printf("\nLegend:\n");
    printf("\033[32m1-9\033[0m - Reward value\n");
    
    for (int i = 0; i < player_count && i < 9; i++) {
        printf("%s%2d \033[0m - Player %d's captured cells\n", 
               player_colors[i % 9], i, i);
    }
    
    printf("\033[1;33m # \033[0m - Player's current position\n");
    
    fflush(stdout);
}

void cleanup() {
    if (game_state != NULL) {
        munmap(game_state, game_state_size);
        game_state = NULL;
    }
    
    if (game_sync != NULL) {
        munmap(game_sync, sizeof(GameSync));
        game_sync = NULL;
    }
}

void sig_handler(int signo) {
    printf("View received signal %d. Cleaning up and exiting...\n", signo);
    cleanup();
    exit(EXIT_SUCCESS);
}