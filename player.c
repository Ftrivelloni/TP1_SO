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
#include "sharedMem.h"

// Global variables for cleanup
GameState* game_state = NULL;
GameSync* game_sync = NULL;
size_t game_state_size = 0;
int player_idx = -1;

// Function prototypes
unsigned char choose_movement();
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
    
    // Seed random number generator
    srand(time(NULL) ^ getpid());
    
    // Calculate size of game state (including the board)
    game_state_size = sizeof(GameState) + width * height * sizeof(int);
    
    // Open shared memory for game state and synchronization
    game_state = (GameState*)open_shared_memory(GAME_STATE_SHM, game_state_size);
    game_sync = (GameSync*)open_shared_memory(GAME_SYNC_SHM, sizeof(GameSync));
    
    // Find our player index based on process ID
    pid_t my_pid = getpid();
    fprintf(stderr, "Player PID: %d looking for index\n", my_pid);
    
    for (int i = 0; i < game_state->player_count; i++) {
        fprintf(stderr, "Checking player %d with PID %d\n", i, game_state->players[i].pid);
        if (game_state->players[i].pid == my_pid) {
            player_idx = i;
            fprintf(stderr, "Found my index: %d\n", player_idx);
            break;
        }
    }
    
    if (player_idx == -1) {
        fprintf(stderr, "Error: Could not find player index for PID %d\n", my_pid);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    // Main loop
    /*
    fprintf(stderr, "Player %d entering main loop\n", player_idx);
    */
    while (!game_state->game_over && !game_state->players[player_idx].is_blocked) {
        // Wait for our turn to move
        /*
        fprintf(stderr, "Player %d waiting for semaphore\n", player_idx);
        */
        sem_wait(&game_sync->player_move_sem[player_idx]);
        
        /*
        if (game_state->game_over) {
            fprintf(stderr, "Player %d detected game over\n", player_idx);
            break;
        }
        */
        
        /*
        fprintf(stderr, "Player %d about to make a move\n", player_idx);
        */
        
        // Implement the readers-writer problem (reader part)
        // Entry section
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->readers_count++;
        if (game_sync->readers_count == 1) {
            sem_wait(&game_sync->game_state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        // Critical section (reading)
        /* PARA DEBUGGEAR ? 
        fprintf(stderr, "Player %d choosing movement\n", player_idx);
        */
        unsigned char direction = choose_movement();
        /*
        fprintf(stderr, "Player %d chose direction %d\n", player_idx, direction);
        */
        
        // Exit section
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->readers_count--;
        if (game_sync->readers_count == 0) {
            sem_post(&game_sync->game_state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        // Send movement to master
        /*
        Para debuggear ? el jugador no tendia que imprimir nada durante la partida. 

        fprintf(stderr, "Player %d sending direction %d to master\n", player_idx, direction);
        */
        if (write(STDOUT_FILENO, &direction, 1) != 1) {
            perror("write");
            break;
        }
        /*
        fprintf(stderr, "Player %d sent direction successfully\n", player_idx);
        */
    }
    
    // Clean up
    cleanup();
    
    return 0;
}

unsigned char choose_movement() {
    int width = game_state->width;
    int height = game_state->height;
    int x = game_state->players[player_idx].x;
    int y = game_state->players[player_idx].y;
    
    // Check which directions are valid and their reward values
    typedef struct {
        unsigned char direction;
        int reward;
    } MoveOption;
    
    MoveOption valid_moves[8];
    int valid_count = 0;
    
    // Helper arrays for direction offsets
    int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    
    for (int dir = 0; dir < 8; dir++) {
        int new_x = x + dx[dir];
        int new_y = y + dy[dir];
        
        // Check if the move is valid
        if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
            int cell_value = game_state->board[new_y * width + new_x];
            
            if (cell_value > 0) {
                valid_moves[valid_count].direction = dir;
                valid_moves[valid_count].reward = cell_value;
                valid_count++;
            }
        }
    }
    
    // If no valid moves, pick a random direction
    if (valid_count == 0) {
        return rand() % 8;
    }
    
    // Simple strategy: prefer moves with higher rewards
    int best_idx = 0;
    int best_score = valid_moves[0].reward;
    
    for (int i = 1; i < valid_count; i++) {
        if (valid_moves[i].reward > best_score) {
            best_idx = i;
            best_score = valid_moves[i].reward;
        }
    }
    
    return valid_moves[best_idx].direction;
}

void cleanup() {
    // Close shared memory (don't unlink as the master will do that)
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
