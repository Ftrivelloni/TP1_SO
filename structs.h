#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdlib.h>
#include <semaphore.h>
#include <stdbool.h>

/* Shared memory segment names for game state and synchronization */
#define NAME_BOARD "/game_state"
#define NAME_SYNC "/game_sync"


/* This struct is used to store information about each player
 * including their state, position, and statistics.
 */
typedef struct {
    char name[16];                // Player name
    unsigned int score;           // Player score
    unsigned int invalid_moves;   // Number of invalid movement requests
    unsigned int valid_moves;     // Number of valid movement requests
    unsigned short x, y;          // Player coordinates on the board
    pid_t pid;                    // Process identifier
    bool is_blocked;              // Indicates if the player is blocked
} Player;

/* This struct represents the complete game state including
 * board dimensions, players, and the game board itself.
 */
typedef struct {
    unsigned short width;         // Board width
    unsigned short height;        // Board height
    unsigned int player_count;    // Number of players
    Player players[9];            // List of players
    bool game_over;               // Indicates if the game has ended
    int board[];                  // Pointer to the beginning of the board
} GameState;

/* This struct contains all synchronization primitives needed
 * for coordinating between master, players, and view processes.
 */
typedef struct {
    sem_t view_update_sem;        // Master signals view that there are changes to print
    sem_t view_done_sem;          // View signals master that it finished printing
    sem_t master_access_mutex;    // Mutex to prevent master starvation when accessing state
    sem_t game_state_mutex;       // Mutex for the game state
    sem_t reader_count_mutex;     // Mutex for the next variable
    unsigned int readers_count;   // Number of players reading the state
    sem_t player_move_sem[9];     // Signal each player that they can send 1 movement
} GameSync;

#endif // STRUCTS_H