#ifndef MASTER_UTILS_H
#define MASTER_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <stdbool.h>
#include <math.h>
#include "sharedMem.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_PLAYERS 9
#define MIN_WIDTH 10
#define MIN_HEIGHT 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10
#define READ_END 0
#define WRITE_END 1
#define TO_MILI_SEC 1000
#define PLAYER_NAME_MAX_LENGTH 16

// Movement directions
extern const int movement[8][2];

/* This struct is used to store the process information
 * for each player, including pipes for communication.
 */
typedef struct {
    pid_t pid;
    int pipe_fd[2]; // pipe for receiving movement requests
    char* binary_path;
} PlayerProcess;

/* This struct is used to store the process information
 * for the view component.
 */
typedef struct {
    pid_t pid;
    char* binary_path;
} ViewProcess;

// External declarations for global variables (defined in master.c)
extern GameState* game_state;
extern GameSync* game_sync;
extern PlayerProcess players[MAX_PLAYERS];
extern ViewProcess view;
extern int player_count;
extern size_t game_state_size;

// Function prototypes

/**
 * @brief Parse command line arguments and set game parameters.
 * @param argc The number of command line arguments.
 * @param argv The array of command line argument strings.
 * @param width Pointer to store the board width.
 * @param height Pointer to store the board height.
 * @param delay Pointer to store the game delay in milliseconds.
 * @param timeout Pointer to store the game timeout in seconds.
 * @param seed Pointer to store the random seed for board generation.
 * @param view_path Pointer to store the path to the view binary.
 * @param player_paths Pointer to array of player binary paths.
 * @param player_count Pointer to store the number of players.
 */
void parse_args(int argc, char* argv[], int* width, int* height, int* delay, 
                int* timeout, unsigned int* seed, char** view_path, 
                char*** player_paths, int* player_count);

/**
 * @brief Initialize the game state with board and player information.
 * @param width The width of the game board.
 * @param height The height of the game board.
 * @param player_count The number of players in the game.
 * @param seed The random seed for board generation.
 */
void init_game_state(int width, int height, int player_count, unsigned int seed);

/**
 * @brief Initialize synchronization primitives for the game.
 * @param player_count The number of players to create semaphores for.
 */
void init_game_sync(int player_count);

/**
 * @brief Place all players on their initial positions on the board.
 */
void place_players_on_board(void);

/**
 * @brief Start all player processes and the view process.
 * @param width The width of the game board.
 * @param height The height of the game board.
 */
void start_players_and_view(int width, int height);

/**
 * @brief Main game loop that processes player movements and manages game flow.
 * @param delay The delay between moves in milliseconds.
 * @param timeout The timeout for player responses in seconds.
 */
void game_loop(int delay, int timeout);

/**
 * @brief Process a movement request from a player.
 * @param player_idx The index of the player making the move.
 * @param direction The direction of movement (0-7).
 * @return true if the move was valid, false otherwise.
 */
bool process_movement(int player_idx, unsigned char direction);

/**
 * @brief Check if a player can make any valid moves.
 * @param player_idx The index of the player to check.
 * @return true if the player can move, false if blocked.
 */
bool can_player_move(int player_idx);

/**
 * @brief Clean up resources including shared memory and semaphores.
 */
void cleanup(void);

/**
 * @brief Signal handler for graceful shutdown on SIGINT/SIGTERM.
 * @param signo The signal number received.
 */
void sig_handler(int signo);

/**
 * @brief Display the game results and winner information.
 */
void display_winner(void);

#endif // MASTER_UTILS_H