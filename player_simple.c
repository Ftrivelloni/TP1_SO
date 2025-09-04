#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include "sharedMem.h"

// Directions: UP, UP-RIGHT, RIGHT, DOWN-RIGHT, DOWN, DOWN-LEFT, LEFT, UP-LEFT
int vector[][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}; 

// Global variables for cleanup
GameState* game_state = NULL;
GameSync* game_sync = NULL;
size_t game_state_size = 0;
int player_idx = -1;

// Function prototypes
unsigned char choose_best_move();
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
    
    // Try to open shared memory (may fail due to permissions)
    int fd_state = -1, fd_sync = -1;
    
    // Try to open game state (read-only) - FIXED: Use NAME_BOARD constant from structs.h
    fd_state = shm_open(NAME_BOARD, O_RDONLY, 0666);
    if (fd_state != -1) {
        game_state = (GameState*)mmap(NULL, game_state_size, PROT_READ, MAP_SHARED, fd_state, 0);
        close(fd_state);
        
        if (game_state == MAP_FAILED) {
            fprintf(stderr, "Warning: Could not map game state\n");
            game_state = NULL;
        }
    }
    
    // Try to open game sync (read-write) - FIXED: Use NAME_SYNC constant from structs.h
    fd_sync = shm_open(NAME_SYNC, O_RDWR, 0666);
    if (fd_sync != -1) {
        game_sync = (GameSync*)mmap(NULL, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd_sync, 0);
        close(fd_sync);
        
        if (game_sync == MAP_FAILED) {
            fprintf(stderr, "Warning: Could not map game sync\n");
            game_sync = NULL;
        }
    }
    
    // Find our player index
    if (game_state != NULL) {
        pid_t pid = getpid();
        for (player_idx = 0; player_idx < game_state->player_count; player_idx++) {
            if (game_state->players[player_idx].pid == pid) {
                break;
            }
        }
        
        if (player_idx >= game_state->player_count) {
            fprintf(stderr, "Warning: Could not find player index for PID %d\n", pid);
            player_idx = 0;  // Default to player 0
        }
    } else {
        // If we can't access game state, just use pid modulo to get an index
        player_idx = getpid() % 9;
    }
    
    // Seed random number generator
    srand(time(NULL) ^ getpid());
    
    // Main game loop
    while (1) {

        if (game_state != NULL && game_state->game_over) {
            break;
        }
        
        // Wait for our turn if we have access to the sync structure
        if (game_sync != NULL) {
            sem_wait(&game_sync->player_move_sem[player_idx]);
        }
        
        // Check if game is over (if we have access to the state)
        if (game_state != NULL && game_state->game_over) {
            break;
        }
        
        // Choose the best move based on board state
        unsigned char move = choose_best_move();
        
        // Send move to master through stdout (which is connected to pipe)
        if (write(STDOUT_FILENO, &move, sizeof(unsigned char)) != 1) {
            // Error or EOF, exit
            break;
        }
        
        // If we don't have access to the sync structure, add a small delay
        if (game_sync == NULL) {
            usleep(100000);  // 100ms
        }
    }
    
    // Clean up
    cleanup();
    return 0;
}

// Implementation of the choose_best_move function with smarter logic
unsigned char choose_best_move() {
    // If we can't access the game state, just return a random move
    if (game_state == NULL) {
        return rand() % 8;
    }
    
    int width = game_state->width;
    int height = game_state->height;
    int player_x = game_state->players[player_idx].x;
    int player_y = game_state->players[player_idx].y;
    
    // First, look for the highest reward in adjacent cells
    int max_reward = -1;
    unsigned char best_dir = 0;
    
    for (unsigned char dir = 0; dir < 8; dir++) {
        int new_x = player_x + vector[dir][0];
        int new_y = player_y + vector[dir][1];
        
        // Check if the new position is valid (within bounds)
        if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
            int cell_value = game_state->board[new_y * width + new_x];
            
            // If cell is free and has a reward
            if (cell_value > 0) {
                if (cell_value > max_reward) {
                    max_reward = cell_value;
                    best_dir = dir;
                }
            }
        }
    }
    
    // If we found a valid move with a reward, return it
    if (max_reward > 0) {
        return best_dir;
    }
    
    // If no good moves in adjacent cells, look further (up to 3 steps away)
    max_reward = -1;
    
    for (int distance = 2; distance <= 3; distance++) {
        for (unsigned char dir = 0; dir < 8; dir++) {
            int new_x = player_x + (vector[dir][0] * distance);
            int new_y = player_y + (vector[dir][1] * distance);
            
            // Check if the new position is valid
            if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
                int cell_value = game_state->board[new_y * width + new_x];
                
                // If cell is free and has a reward, consider the direction to move
                if (cell_value > 0) {
                    // Discount rewards by distance
                    int adjusted_reward = cell_value / distance;
                    
                    if (adjusted_reward > max_reward) {
                        max_reward = adjusted_reward;
                        // Still move in the original direction
                        best_dir = dir;
                    }
                }
            }
        }
    }
    
    // If we found a decent move in the extended search, use it
    if (max_reward > 0) {
        return best_dir;
    }
    
    // Last resort: choose a random direction
    unsigned char random_dir = rand() % 8;
    
    // Try to avoid moving outside the board if possible
    for (int i = 0; i < 8; i++) {
        unsigned char dir = (random_dir + i) % 8;
        int new_x = player_x + vector[dir][0];
        int new_y = player_y + vector[dir][1];
        
        if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
            return dir;
        }
    }
    
    // If all else fails, return the original random direction
    return random_dir;
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
    cleanup();
    exit(EXIT_SUCCESS);
}