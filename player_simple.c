//This version direcly uses POSIX calls

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include "sharedMem.h"

// Global variables
GameState* game_state = NULL;
GameSync* game_sync = NULL;
size_t game_state_size = 0;
int player_idx = -1;

// Function prototypes
void cleanup();
void sig_handler(int signo);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <width> <height>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    
    // Set up signal handlers
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    // Seed random number generator
    srand(time(NULL) ^ getpid());
    
    // Calculate size of game state
    game_state_size = sizeof(GameState) + width * height * sizeof(int);
    
    // Open shared memory for game state
    int fd = shm_open(GAME_STATE_SHM, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open game_state");
        exit(EXIT_FAILURE);
    }
    
    game_state = mmap(NULL, game_state_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (game_state == MAP_FAILED) {
        perror("mmap game_state");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    
    // Open shared memory for synchronization
    fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open game_sync");
        munmap(game_state, game_state_size);
        exit(EXIT_FAILURE);
    }
    
    game_sync = mmap(NULL, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (game_sync == MAP_FAILED) {
        perror("mmap game_sync");
        close(fd);
        munmap(game_state, game_state_size);
        exit(EXIT_FAILURE);
    }
    close(fd);
    
    // Find our player index
    pid_t my_pid = getpid();
    for (int i = 0; i < game_state->player_count; i++) {
        if (game_state->players[i].pid == my_pid) {
            player_idx = i;
            break;
        }
    }
    
    if (player_idx == -1) {
        fprintf(stderr, "Could not find player index for PID %d\n", my_pid);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    // Main game loop
    while (!game_state->game_over) {
        // Wait for our turn
        sem_wait(&game_sync->player_move_sem[player_idx]);
        
        if (game_state->game_over) {
            break;
        }
        
        // Reader lock
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->readers_count++;
        if (game_sync->readers_count == 1) {
            sem_wait(&game_sync->game_state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        // Choose direction (simple strategy: look for highest reward)
        unsigned char direction = 0;
        int best_reward = 0;
        int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
        int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
        int x = game_state->players[player_idx].x;
        int y = game_state->players[player_idx].y;
        
        for (int dir = 0; dir < 8; dir++) {
            int new_x = x + dx[dir];
            int new_y = y + dy[dir];
            
            if (new_x >= 0 && new_x < game_state->width && 
                new_y >= 0 && new_y < game_state->height) {
                
                int reward = game_state->board[new_y * game_state->width + new_x];
                if (reward > 0 && reward > best_reward) {
                    best_reward = reward;
                    direction = dir;
                }
            }
        }
        
        // If no valid move found, just pick a random direction
        if (best_reward == 0) {
            direction = rand() % 8;
        }
        
        // Release reader lock
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->readers_count--;
        if (game_sync->readers_count == 0) {
            sem_post(&game_sync->game_state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        // Send move to master
        write(STDOUT_FILENO, &direction, 1);
    }
    
    cleanup();
    return 0;
}

void cleanup() {
    if (game_state != NULL) {
        munmap(game_state, game_state_size);
    }
    
    if (game_sync != NULL) {
        munmap(game_sync, sizeof(GameSync));
    }
}

void sig_handler(int signo) {
    cleanup();
    exit(EXIT_SUCCESS);
}
