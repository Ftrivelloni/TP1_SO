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

// Directions: UP, UP-RIGHT, RIGHT, DOWN-RIGHT, DOWN, DOWN-LEFT, LEFT, UP-LEFT
int vector[][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}; 

// Global variables for cleanup
GameState* game_state = NULL;
GameSync* game_sync = NULL;
size_t game_state_size = 0;
int player_idx = -1;

// Function prototypes
unsigned char best_move(GameState* game_state, Player* player);
void cleanup();
void sig_handler(int signo);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <width> <height>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    
    // Set up signal handlers for clean termination
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    // Calculate size of game state
    game_state_size = sizeof(GameState) + width * height * sizeof(int);
    
    // Open shared memory for game state (read-only)
    game_state = (GameState*)open_shared_memory(GAME_STATE_SHM, game_state_size, O_RDONLY);
    
    // Open shared memory for synchronization (read-write)
    game_sync = (GameSync*)open_shared_memory(GAME_SYNC_SHM, sizeof(GameSync), O_RDWR);
    
    // Find our player index
    pid_t pid = getpid();
    player_idx = 0;
    
    while(player_idx < game_state->player_count) {
        if(game_state->players[player_idx].pid == pid) {
            break;
        }
        player_idx++;
    }
    
    if (player_idx >= game_state->player_count) {
        fprintf(stderr, "Could not find player index for PID %d\n", pid);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    // Seed random number generator
    srand(time(NULL) ^ getpid());
    
    // Main game loop
    while(!game_state->players[player_idx].is_blocked && !game_state->game_over) {
        // Wait for our turn (sem_wait on player_move_sem will be posted by master after processing a move)
        sem_wait(&game_sync->player_move_sem[player_idx]);
        
        if (game_state->game_over) {
            break;
        }
        
        // Implement readers-writer pattern: Reader lock
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->readers_count++;
        if(game_sync->readers_count == 1) {
            sem_wait(&game_sync->game_state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        // Choose the best move
        unsigned char move = best_move(game_state, &game_state->players[player_idx]);
        
        // Release reader lock
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->readers_count--;
        if(game_sync->readers_count == 0) {
            sem_post(&game_sync->game_state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        // Send move to master through stdout (which is connected to pipe)
        if (write(STDOUT_FILENO, &move, sizeof(unsigned char)) == -1) {
            perror("write");
            cleanup();
            exit(EXIT_FAILURE);
        }
        
        // No need to sem_wait on player_move_sem here - master will post after processing the move
    }
    
    // Clean up
    cleanup();
    return 0;
}

// Implementation of the best_move function
unsigned char best_move(GameState* game_state, Player* player) {
    int max_reward = -1;
    unsigned char best_direction = 0;
    
    // Check all 8 directions
    for (unsigned char dir = 0; dir < 8; dir++) {
        int new_x = player->x + vector[dir][0];
        int new_y = player->y + vector[dir][1];
        
        // Check if new position is within bounds
        if (new_x >= 0 && new_x < game_state->width && 
            new_y >= 0 && new_y < game_state->height) {
            
            // Calculate the index in the board array
            int index = new_y * game_state->width + new_x;
            
            // Check if the cell has a reward (positive value)
            int reward = game_state->board[index];
            if (reward > 0 && reward > max_reward) {
                max_reward = reward;
                best_direction = dir;
            }
        }
    }
    
    // If no valid move found, choose a random direction
    if (max_reward == -1) {
        best_direction = rand() % 8;
    }
    
    return best_direction;
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
    printf("Player received signal %d. Cleaning up and exiting...\n", signo);
    cleanup();
    exit(EXIT_SUCCESS);
}