#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include "sharedMem.h"

// Direction mapping array (matching the example)
const int directions[8][2] = {
    {0, -1},  // Up
    {1, -1},  // Up-Right
    {1, 0},   // Right
    {1, 1},   // Down-Right
    {0, 1},   // Down
    {-1, 1},  // Down-Left
    {-1, 0},  // Left
    {-1, -1}, // Up-Left
};

// Function to find the best move (similar to the example)
int find_best_path(GameState *game_state, int player_idx) {
    int max_points = -1;
    int best_move = -1;
    int width = game_state->width;
    int height = game_state->height;
    
    int player_x = game_state->players[player_idx].x;
    int player_y = game_state->players[player_idx].y;
    
    for (unsigned char i = 0; i < 8; i++) {
        int new_x = player_x + directions[i][0];
        int new_y = player_y + directions[i][1];
        
        if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
            // Calculate the index of the cell in the board
            int cell_index = (new_y * width) + new_x;
            
            if (cell_index < 0 || cell_index >= width * height) {
                fprintf(stderr, "Error: cell_index out of bounds (%d)\n", cell_index);
                continue;
            }
            
            int points = game_state->board[cell_index];
            
            if (points > max_points) {
                max_points = points;
                best_move = i;
            }
        }
    }
    
    return best_move;
}

int main(int argc, char *argv[]) {
    unsigned char move;
    
    // Check command-line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <width> <height>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int width = atoi(argv[1]);
    
    // Open shared memory for game state (read-only)
    GameState *game_state = (GameState *)open_shared_memory(GAME_STATE_SHM, sizeof(GameState) + width * atoi(argv[2]) * sizeof(int));
    
    // Open shared memory for synchronization (read-write)
    GameSync *game_sync = (GameSync *)open_shared_memory(GAME_SYNC_SHM, sizeof(GameSync));
    
    // Find our player index
    pid_t pid = getpid();
    int player_idx = 0;
    
    while (player_idx < game_state->player_count) {
        if (game_state->players[player_idx].pid == pid) {
            break;
        }
        player_idx++;
    }
    
    // Main game loop - keep playing until blocked or game ends
    while (!game_state->players[player_idx].is_blocked && !game_state->game_over) {
        // Wait for master to signal it's our turn
        sem_wait(&game_sync->player_move_sem[player_idx]);
        
        if (game_state->game_over) {
            break;
        }
        
        // Implement readers-writer pattern: Reader lock
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->readers_count++;
        if (game_sync->readers_count == 1) {
            sem_wait(&game_sync->game_state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        // Choose the best move
        move = find_best_path(game_state, player_idx);
        
        // Get current position for waiting later
        int x = game_state->players[player_idx].x;
        int y = game_state->players[player_idx].y;
        
        // Release reader lock
        sem_wait(&game_sync->reader_count_mutex);
        game_sync->readers_count--;
        if (game_sync->readers_count == 0) {
            sem_post(&game_sync->game_state_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);
        
        // Send move to master through stdout
        if (write(STDOUT_FILENO, &move, sizeof(unsigned char)) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        
        // Wait until the move is processed
        // This busy-wait ensures we don't proceed until our position changes
        int new_x = x + directions[move][0];
        int new_y = y + directions[move][1];
        int target_cell = (new_y * width) + new_x;
        
        while (game_state->board[target_cell] > 0 && 
               !game_state->players[player_idx].is_blocked && 
               !game_state->game_over);
    }
    
    // Clean up
    munmap(game_state, sizeof(GameState) + width * atoi(argv[2]) * sizeof(int));
    munmap(game_sync, sizeof(GameSync));
    
    return 0;
}
