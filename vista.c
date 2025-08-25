#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include "sharedMem.h"

// Global variables for cleanup
GameState* game_state = NULL;
GameSync* game_sync = NULL;
size_t game_state_size = 0;

// Function prototypes
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
    
    // Set up signal handlers for clean termination
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    // Calculate size of game state (including the board)
    game_state_size = sizeof(GameState) + width * height * sizeof(int); // repetido en master  
    
    // Open shared memory for game state and synchronization
    game_state = (GameState*)open_shared_memory(GAME_STATE_SHM, game_state_size);
    game_sync = (GameSync*)open_shared_memory(GAME_SYNC_SHM, sizeof(GameSync));
    
    // Main loop
    while (!game_state->game_over) {
        // Wait for the master to signal that there are changes to display
        sem_wait(&game_sync->view_update_sem);
        
        // Display the game state
        display_game_state();
        
        // Signal the master that we're done displaying
        sem_post(&game_sync->view_done_sem);
    }

    // Impriir el estado de game over
    // display_game_state(); creo que no se nesceario


    // Clean up
    cleanup();
    
    return 0;
}

void display_game_state() {
    int width = game_state->width;
    int height = game_state->height;
    int player_count = game_state->player_count;
    
    // Clear screen
    printf("\033[2J\033[H");
    
    // Print game status
    printf("===== ChompChamps =====\n");
    printf("Game Status: %s\n", game_state->game_over ? "GAME OVER" : "IN PROGRESS");
    printf("\n");
    
    // Print player information
    printf("Players:\n");
    for (int i = 0; i < player_count; i++) {
        printf("[%d] %s - Score: %u, Position: (%u,%u), Valid Moves: %u, Invalid Moves: %u, %s\n",
              i, game_state->players[i].name, game_state->players[i].score,
              game_state->players[i].x, game_state->players[i].y,
              game_state->players[i].valid_moves, game_state->players[i].invalid_moves,
              game_state->players[i].is_blocked ? "BLOCKED" : "ACTIVE");
    }
    printf("\n");
    
    // Print the board
    printf("Board:\n");
    printf("   ");
    for (int x = 0; x < width; x++) {
        printf("%2d ", x);
    }
    printf("\n");
    
    for (int y = 0; y < height; y++) {
        printf("%2d ", y);
        for (int x = 0; x < width; x++) {
            int cell_value = game_state->board[y * width + x];
            
            if (cell_value > 0) {
                // Cell with reward
                printf("\033[32m%2d \033[0m", cell_value); // Green for rewards
            } else {
                // Cell captured by a player
                int player_idx = -cell_value;
                
                // Check if a player is currently on this cell
                bool player_present = false;
                for (int i = 0; i < player_count; i++) {
                    if (game_state->players[i].x == x && game_state->players[i].y == y) {
                        printf("\033[1;33m %d \033[0m", i); // Bright yellow for player position
                        player_present = true;
                        break;
                    }
                }
                
                if (!player_present) {
                    // Use different colors for different players
                    int color = 31 + (player_idx % 7); // 31-37 are ANSI colors
                    printf("\033[%dm %d \033[0m", color, player_idx);
                }
            }
        }
        printf("\n");
    }
    
    // Print legend
    printf("\nLegend:\n");
    printf("\033[32m1-9\033[0m - Reward value\n");
    for (int i = 0; i < player_count; i++) {
        int color = 31 + (i % 7);
        printf("\033[%dm %d \033[0m - %s's captured cells\n", color, i, game_state->players[i].name);
    }
    printf("\033[1;33m # \033[0m - Player's current position\n");
    
    // Flush output
    fflush(stdout);
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
    printf("View received signal %d. Cleaning up and exiting...\n", signo);
    cleanup();
    exit(EXIT_SUCCESS);
}
