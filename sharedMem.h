#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdbool.h>
#include <semaphore.h>
#include <sys/types.h>

// Structure for player information
typedef struct {
    char name[16];                // Player name
    unsigned int score;           // Player score
    unsigned int invalid_moves;   // Number of invalid movement requests
    unsigned int valid_moves;     // Number of valid movement requests
    unsigned short x, y;          // Player coordinates on the board
    pid_t pid;                    // Process identifier
    bool is_blocked;              // Indicates if the player is blocked
} Player;

// Structure for game state
typedef struct {
    unsigned short width;         // Board width
    unsigned short height;        // Board height
    unsigned int player_count;    // Number of players
    Player players[9];            // List of players
    bool game_over;               // Indicates if the game has ended
    int board[];                  // Pointer to the beginning of the board
} GameState;

// Structure for synchronization
typedef struct {
    sem_t view_update_sem;        // Master signals view that there are changes to print
    sem_t view_done_sem;          // View signals master that it finished printing
    sem_t master_access_mutex;    // Mutex to prevent master starvation when accessing state
    sem_t game_state_mutex;       // Mutex for the game state
    sem_t reader_count_mutex;     // Mutex for the next variable
    unsigned int readers_count;   // Number of players reading the state
    sem_t player_move_sem[9];     // Signal each player that they can send 1 movement
} GameSync;

// Functions for shared memory operations
void* create_shared_memory(const char* name, size_t size);
void* open_shared_memory(const char* name, size_t size);
void close_shared_memory(void* ptr, const char* name, size_t size);

// Direction constants
#define DIRECTION_UP 0
#define DIRECTION_UP_RIGHT 1
#define DIRECTION_RIGHT 2
#define DIRECTION_DOWN_RIGHT 3
#define DIRECTION_DOWN 4
#define DIRECTION_DOWN_LEFT 5
#define DIRECTION_LEFT 6
#define DIRECTION_UP_LEFT 7

// Shared memory names
#define GAME_STATE_SHM "/game_state"
#define GAME_SYNC_SHM "/game_sync"

#endif // SHARED_MEM_H
