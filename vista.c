#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h> 
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
    game_state_size = sizeof(GameState) + width * height * sizeof(int);
    
    // Open shared memory for game state (read-only) and synchronization
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
    
    // Main loop
    while (!game_state->game_over) {
        // Wait for the master to signal that there are changes to display
        sem_wait(&game_sync->view_update_sem);
        
        // Display the game state
        display_game_state();
        
        // Signal the master that we're done displaying
        sem_post(&game_sync->view_done_sem);
    }

    // Final display at game over (only once)
    display_game_state();
    sem_post(&game_sync->view_done_sem);

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
        // Use a distinct color for each player's information
        int color = 31 + (i % 7); // 31-37 are ANSI colors
        printf("\033[%dm[%d] %s - Score: %u, Position: (%u,%u), Valid Moves: %u, Invalid Moves: %u, %s\033[0m\n",
              color, i, game_state->players[i].name, game_state->players[i].score,
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
    
    // Define a wider range of colors for players (beyond the standard 7)
    // We'll use both foreground and background colors to get more combinations
    const char* player_colors[] = {
        "\033[31m",  // Red
        "\033[34m",  // Blue
        "\033[35m",  // Magenta
        "\033[36m",  // Cyan
        "\033[33m",  // Yellow
        "\033[32m",  // Green
        "\033[37;44m", // White on blue
        "\033[37;45m", // White on magenta
        "\033[37;46m"  // White on cyan
    };
    
    for (int y = 0; y < height; y++) {
        printf("%2d ", y);
        for (int x = 0; x < width; x++) {
            int cell_value = game_state->board[y * width + x];
            
            // Check if a player is currently on this cell
            bool player_present = false;
            int present_player = -1;
            for (int i = 0; i < player_count; i++) {
                if (game_state->players[i].x == x && game_state->players[i].y == y) {
                    player_present = true;
                    present_player = i;
                    break;
                }
            }
            
            if (player_present) {
                // Player is on this cell - show player number in bright yellow
                printf("\033[1;33m%2d \033[0m", present_player);
            } else if (cell_value > 0) {
                // Free cell with reward - show in green
                printf("\033[32m%2d \033[0m", cell_value);
            } else if (cell_value <= 0) {
                // Captured cell - show owner with their distinct color
                int owner = -cell_value;  // Convert -id to id
                
                if (owner >= 0 && owner < player_count) {
                    // Use the player's color from our array
                    printf("%s%2d \033[0m", player_colors[owner % 9], owner);
                } else {
                    // In case of invalid cell value
                    printf(" ? ");
                }
            }
        }
        printf("\n");
    }
    
    // Print legend with player-specific colors
    printf("\nLegend:\n");
    printf("\033[32m1-9\033[0m - Reward value\n");
    
    for (int i = 0; i < player_count && i < 9; i++) {
        printf("%s %d \033[0m - Player %d's captured cells\n", 
               player_colors[i], i, i);
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