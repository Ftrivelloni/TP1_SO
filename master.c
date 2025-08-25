#define _GNU_SOURCE // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include "sharedMem.h"

#define MAX_PLAYERS 9
#define MIN_WIDTH 10
#define MIN_HEIGHT 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10

typedef struct {
    pid_t pid;
    int pipe_fd[2]; // pipe for receiving movement requests
    char* binary_path;
} PlayerProcess;

typedef struct {
    pid_t pid;
    char* binary_path;
} ViewProcess;

// Global variables for cleanup
GameState* game_state = NULL;
GameSync* game_sync = NULL;
PlayerProcess players[MAX_PLAYERS];
ViewProcess view;
int player_count = 0;
size_t game_state_size = 0;

// Function prototypes
void parse_args(int argc, char* argv[], int* width, int* height, int* delay, 
                int* timeout, unsigned int* seed, char** view_path, 
                char*** player_paths, int* player_count);
void init_game_state(int width, int height, int player_count, unsigned int seed);
void init_game_sync(int player_count);
void place_players_on_board();
void start_players_and_view(int width, int height);
void game_loop(int delay, int timeout);
bool process_movement(int player_idx, unsigned char direction);
void cleanup();
void sig_handler(int signo);

int main(int argc, char* argv[]) {
    int width = MIN_WIDTH;
    int height = MIN_HEIGHT;
    int delay = DEFAULT_DELAY;
    int timeout = DEFAULT_TIMEOUT;
    unsigned int seed = time(NULL);
    char* view_path = NULL;
    char** player_paths = NULL;
    
    // Set up signal handlers for clean termination
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    // Parse command line arguments
    parse_args(argc, argv, &width, &height, &delay, &timeout, &seed, 
               &view_path, &player_paths, &player_count);
    
    if (player_count < 1) {
        fprintf(stderr, "Error: At least one player must be specified\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize game state and synchronization
    init_game_state(width, height, player_count, seed);
    init_game_sync(player_count);
    
    // Initialize player positions on the board
    place_players_on_board();
    
    // Start player and view processes
    view.binary_path = view_path;
    for (int i = 0; i < player_count; i++) {
        players[i].binary_path = player_paths[i];
    }
    start_players_and_view(width, height);
    
    // Run the game loop
    game_loop(delay, timeout);
    
    // Wait for all child processes and print results
    int status;
    char exit_info[128];
    
    for (int i = 0; i < player_count; i++) {
        waitpid(players[i].pid, &status, 0);
        if (WIFEXITED(status)) {
            sprintf(exit_info, "Player %d exited with code %d. Score: %u", 
                    i, WEXITSTATUS(status), game_state->players[i].score);
        } else if (WIFSIGNALED(status)) {
            sprintf(exit_info, "Player %d terminated by signal %d. Score: %u", 
                    i, WTERMSIG(status), game_state->players[i].score);
        }
        printf("%s\n", exit_info);
    }
    
    if (view_path != NULL) {
        waitpid(view.pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("View exited with code %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("View terminated by signal %d\n", WTERMSIG(status));
        }
    }
    
    // Clean up resources
    cleanup();
    
    free(view_path);
    for (int i = 0; i < player_count; i++) {
        free(player_paths[i]);
    }
    free(player_paths);
    
    return 0;
}

void parse_args(int argc, char* argv[], int* width, int* height, int* delay, 
                int* timeout, unsigned int* seed, char** view_path, 
                char*** player_paths, int* player_count) {
    int opt;
    bool p_flag = false;
    
    while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p:")) != -1) {
        switch (opt) {
            case 'w':
                *width = atoi(optarg);
                if (*width < MIN_WIDTH) *width = MIN_WIDTH;
                break;
            case 'h':
                *height = atoi(optarg);
                if (*height < MIN_HEIGHT) *height = MIN_HEIGHT;
                break;
            case 'd':
                *delay = atoi(optarg);
                break;
            case 't':
                *timeout = atoi(optarg);
                break;
            case 's':
                *seed = atoi(optarg);
                break;
            case 'v':
                *view_path = strdup(optarg);
                break;
            case 'p':
                p_flag = true;
                break;
            default:
                fprintf(stderr, "Usage: %s [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] -p player1 player2 ...\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    if (!p_flag) {
        fprintf(stderr, "Error: At least one player must be specified with -p\n");
        exit(EXIT_FAILURE);
    }
    
    // Get player binaries
    *player_count = argc - optind;
    if (*player_count > MAX_PLAYERS) {
        fprintf(stderr, "Error: Maximum number of players is %d\n", MAX_PLAYERS);
        exit(EXIT_FAILURE);
    }
    
    *player_paths = (char**)malloc(*player_count * sizeof(char*));
    if (*player_paths == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < *player_count; i++) {
        (*player_paths)[i] = strdup(argv[optind + i]);
    }
}

void init_game_state(int width, int height, int player_count, unsigned int seed) {
    // Calculate size of game state (including the board)
    game_state_size = sizeof(GameState) + width * height * sizeof(int); // repetido en vista
    
    // Create shared memory for game state
    game_state = (GameState*)create_shared_memory(GAME_STATE_SHM, game_state_size);
    
    // Initialize game state
    memset(game_state, 0, game_state_size);
    game_state->width = width;
    game_state->height = height;
    game_state->player_count = player_count;
    game_state->game_over = false;
    
    // Generate rewards for each cell (1-9)
    srand(seed);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            game_state->board[y * width + x] = (rand() % 9) + 1;
        }
    }
    
    // Initialize player names
    for (int i = 0; i < player_count; i++) {
        snprintf(game_state->players[i].name, 16, "Player %d", i + 1);
        game_state->players[i].score = 0;
        game_state->players[i].invalid_moves = 0;
        game_state->players[i].valid_moves = 0;
        game_state->players[i].is_blocked = false;
    }
}

void init_game_sync(int player_count) {
    // Create shared memory for synchronization
    game_sync = (GameSync*)create_shared_memory(GAME_SYNC_SHM, sizeof(GameSync));
    
    // Initialize semaphores
    sem_init(&game_sync->view_update_sem, 1, 0);
    sem_init(&game_sync->view_done_sem, 1, 0);
    sem_init(&game_sync->master_access_mutex, 1, 1);
    sem_init(&game_sync->game_state_mutex, 1, 1);
    sem_init(&game_sync->reader_count_mutex, 1, 1);
    
    game_sync->readers_count = 0;
    
    // Initialize semaphores for players
    for (int i = 0; i < player_count; i++) {
        sem_init(&game_sync->player_move_sem[i], 1, 0);
        // Post once to allow the first move
        sem_post(&game_sync->player_move_sem[i]);
    }
}

void place_players_on_board() {
    int width = game_state->width;
    int height = game_state->height;
    int player_count = game_state->player_count;
    
    // Distribute players on the board
    // Place players in different corners and sides of the board
    int positions[][2] = {
        {0, 0},                  // Top-left
        {width - 1, 0},          // Top-right
        {0, height - 1},         // Bottom-left
        {width - 1, height - 1}, // Bottom-right
        {width / 2, 0},          // Top-middle
        {0, height / 2},         // Middle-left
        {width - 1, height / 2}, // Middle-right
        {width / 2, height - 1}, // Bottom-middle
        {width / 2, height / 2}  // Center
    };
    
    for (int i = 0; i < player_count; i++) {
        int x = positions[i][0];
        int y = positions[i][1];
        
        game_state->players[i].x = x;
        game_state->players[i].y = y;
        
        // Mark the cell as captured by the player (no reward)
        game_state->board[y * width + x] = -(i);
    }
}

void start_players_and_view(int width, int height) {
    // Start view process if specified
    if (view.binary_path != NULL) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process (view)
            char width_str[16], height_str[16];
            sprintf(width_str, "%d", width);
            sprintf(height_str, "%d", height);
            
            execl(view.binary_path, view.binary_path, width_str, height_str, NULL);
            
            // If execl fails
            perror("execl view");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Parent process
            view.pid = pid;
        } else {
            perror("fork view");
            exit(EXIT_FAILURE);
        }
    }
    
    // Start player processes
    for (int i = 0; i < player_count; i++) {
        // Create pipe for receiving movement requests
        if (pipe(players[i].pipe_fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child process (player)
            close(players[i].pipe_fd[0]); // Close read end
            
            // Redirect stdout to pipe
            if (dup2(players[i].pipe_fd[1], STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            
            close(players[i].pipe_fd[1]); // Close original write end
            
            char width_str[16], height_str[16];
            sprintf(width_str, "%d", width);
            sprintf(height_str, "%d", height);
            
            execl(players[i].binary_path, players[i].binary_path, width_str, height_str, NULL);
            
            // If execl fails
            perror("execl player");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Parent process
            players[i].pid = pid;
            close(players[i].pipe_fd[1]); // Close write end
            
            // Make read end non-blocking
            int flags = fcntl(players[i].pipe_fd[0], F_GETFL, 0);
            fcntl(players[i].pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
            
            // Store the process ID in the game state
            game_state->players[i].pid = pid;
        } else {
            perror("fork player");
            exit(EXIT_FAILURE);
        }
    }
}

void game_loop(int delay, int timeout) {
    fd_set read_fds;
    struct timeval tv;
    int max_fd = -1;
    time_t last_valid_move_time = time(NULL);
    bool all_blocked = false;
    int current_player = 0; // For round-robin scheduling
    int no_moves_count = 0;  // Count iterations without valid moves
    const int MAX_NO_MOVES = 1000; // Maximum iterations without moves before forcing game end
    
    while (!game_state->game_over && !all_blocked) {
        // Check if timeout has been reached
        if (difftime(time(NULL), last_valid_move_time) >= timeout) {
            printf("Game over: Timeout reached\n");
            game_state->game_over = true;
            break;
        }
        
        // Force game end if we've gone too many iterations without moves
        if (no_moves_count > MAX_NO_MOVES) {
            printf("Game over: No valid moves for too long\n");
            game_state->game_over = true;
            break;
        }
        
        // Prepare the fd_set for select
        FD_ZERO(&read_fds);
        max_fd = -1;
        
        // Count players that can move
        int movable_players = 0;
        
        for (int i = 0; i < player_count; i++) {
            if (!game_state->players[i].is_blocked) {
                FD_SET(players[i].pipe_fd[0], &read_fds);
                if (players[i].pipe_fd[0] > max_fd) {
                    max_fd = players[i].pipe_fd[0];
                }
                movable_players++;
            }
        }
        
        // Check if all players are blocked
        if (movable_players == 0) {
            printf("Game over: All players are blocked\n"); // creo que nunca llega aca 
            game_state->game_over = true;
            break;
        }
        
        // Check if any player can make a valid move
        bool any_player_can_move = false;
        for (int i = 0; i < player_count; i++) {
            if (!game_state->players[i].is_blocked) {
                int x = game_state->players[i].x;
                int y = game_state->players[i].y;
                int width = game_state->width;
                int height = game_state->height;
                
                // Check all 8 directions
                int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
                int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
                
                for (int dir = 0; dir < 8; dir++) {
                    int new_x = x + dx[dir];
                    int new_y = y + dy[dir];
                    
                    if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
                        int cell_value = game_state->board[new_y * width + new_x];
                        if (cell_value > 0) {
                            any_player_can_move = true;
                            break;
                        }
                    }
                }
                
                if (any_player_can_move) {
                    break;
                }
            }
        }
        
        if (!any_player_can_move) {
            printf("Game over: No valid moves possible for any player\n");
            game_state->game_over = true;
            break;
        }
        
        // Set timeout for select
        tv.tv_sec = 0;
        tv.tv_usec = 1000; // 1ms
        
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ready == -1) {
            if (errno == EINTR) continue; // Interrupted by signal
            perror("select");
            break;
        }
        
        // Increment no_moves_count by default, will reset if we process a valid move
        no_moves_count++;
        
        // Round-robin processing of player requests
        if (ready > 0) {
            bool processed = false;
            int checked = 0;
            
            while (!processed && checked < player_count) {
                if (++current_player >= player_count) {
                    current_player = 0;
                }
                
                if (!game_state->players[current_player].is_blocked && 
                    FD_ISSET(players[current_player].pipe_fd[0], &read_fds)) {
                    
                    unsigned char direction;
                    ssize_t bytes_read = read(players[current_player].pipe_fd[0], &direction, 1);
                    
                    if (bytes_read > 0) {
                        // Process the movement
                        bool valid = process_movement(current_player, direction);
                        
                        // Signal the player that their move was processed
                        sem_post(&game_sync->player_move_sem[current_player]);
                        
                        // If valid movement, update last valid move time
                        if (valid) {
                            last_valid_move_time = time(NULL);
                            no_moves_count = 0; // Reset counter on valid move
                            
                            // Notify view of state change if available
                            if (view.binary_path != NULL) {
                                sem_post(&game_sync->view_update_sem);
                                sem_wait(&game_sync->view_done_sem);
                            }
                            
                            // Sleep for the specified delay
                            usleep(delay * 1000);
                        }
                        
                        processed = true;
                    } else if (bytes_read == 0) {
                        // End of file - player process has closed its pipe
                        printf("Player %d has closed its pipe\n", current_player);
                        game_state->players[current_player].is_blocked = true;
                    }
                }
                
                checked++;
            }
        }
        
        // Small delay to prevent CPU hogging
        usleep(1000);
    }
    
    // Game has ended, set game_over flag
    game_state->game_over = true;
    
    // tengo que hacer un sem post a la vista para que imprima el estado final

        if (view.binary_path != NULL) {
        sem_post(&game_sync->view_update_sem);
        sem_wait(&game_sync->view_done_sem);  // Espera a que la vista termine
    }

    // sem_post(&game_sync->view_update_sem); // Notifica a la vista que el juego terminó
    
    // Determine winner(s)
    int highest_score = -1;
    for (int i = 0; i < player_count; i++) {
        if ((int)game_state->players[i].score > highest_score) {
            highest_score = game_state->players[i].score;
        }
    }
    
    
    // First tiebreaker: minimum valid moves
    int min_valid_moves = INT_MAX;
    for (int i = 0; i < player_count; i++) {
        if (game_state->players[i].score == highest_score) {
            if (game_state->players[i].valid_moves < min_valid_moves) {
                min_valid_moves = game_state->players[i].valid_moves;
            }
        }
    }
    
    // Second tiebreaker: minimum invalid moves
    int min_invalid_moves = INT_MAX;
    for (int i = 0; i < player_count; i++) {
        if (game_state->players[i].score == highest_score && 
            game_state->players[i].valid_moves == min_valid_moves) {
            if (game_state->players[i].invalid_moves < min_invalid_moves) {
                min_invalid_moves = game_state->players[i].invalid_moves;
            }
        }
    }
    
    
    // esto lo imprime antes o sobre el game over
    // el master debe imprimir informacion de salida relacionada con la terminacion de procesos y puntajes de los jugadores

    // esto se tiene que imprimir despues de que se imprima la cista de GAME OVER
    printf("Game over! Highest score: %d\n", highest_score); 
    printf("Winners:\n");

    

    // Print the winners
    for (int i = 0; i < player_count; i++) {
        if (game_state->players[i].score == highest_score && 
            game_state->players[i].valid_moves == min_valid_moves &&
            game_state->players[i].invalid_moves == min_invalid_moves) {
            printf("- %s (Score: %u, Valid Moves: %u, Invalid Moves: %u)\n",
                  game_state->players[i].name, game_state->players[i].score,
                  game_state->players[i].valid_moves, game_state->players[i].invalid_moves);
        }
    }
}

bool process_movement(int player_idx, unsigned char direction) {
    if (direction > 7) {
        // Invalid direction
        game_state->players[player_idx].invalid_moves++;
        return false;
    }
    
    int width = game_state->width;
    int height = game_state->height;
    int x = game_state->players[player_idx].x;
    int y = game_state->players[player_idx].y;
    int new_x = x;
    int new_y = y;
    
    // Calculate new position based on direction
    switch (direction) {
        case DIRECTION_UP:
            new_y--;
            break;
        case DIRECTION_UP_RIGHT:
            new_y--;
            new_x++;
            break;
        case DIRECTION_RIGHT:
            new_x++;
            break;
        case DIRECTION_DOWN_RIGHT:
            new_y++;
            new_x++;
            break;
        case DIRECTION_DOWN:
            new_y++;
            break;
        case DIRECTION_DOWN_LEFT:
            new_y++;
            new_x--;
            break;
        case DIRECTION_LEFT:
            new_x--;
            break;
        case DIRECTION_UP_LEFT:
            new_y--;
            new_x--;
            break;
    }
    
    // Check if new position is valid
    if (new_x < 0 || new_x >= width || new_y < 0 || new_y >= height) {
        // Out of bounds
        game_state->players[player_idx].invalid_moves++;
        return false;
    }
    
    int cell_value = game_state->board[new_y * width + new_x];
    if (cell_value <= 0) {
        // Cell already captured
        game_state->players[player_idx].invalid_moves++;
        return false;
    }
    
    // Valid move
    game_state->players[player_idx].valid_moves++;
    game_state->players[player_idx].score += cell_value;
    game_state->players[player_idx].x = new_x;
    game_state->players[player_idx].y = new_y;
    
    // Mark the cell as captured by the player
    game_state->board[new_y * width + new_x] = -(player_idx);
    
    return true;
}

void cleanup() {
    // Close all pipes
    for (int i = 0; i < player_count; i++) {
        if (players[i].pipe_fd[0] > 0) {
            close(players[i].pipe_fd[0]);
            players[i].pipe_fd[0] = -1;
        }
    }
    
    // Signal all players one last time to prevent deadlocks
    if (game_sync != NULL) {
        for (int i = 0; i < player_count; i++) {
            sem_post(&game_sync->player_move_sem[i]);
        }
    }
    
    // Destroy semaphores
    if (game_sync != NULL) {
        sem_destroy(&game_sync->view_update_sem);
        sem_destroy(&game_sync->view_done_sem);
        sem_destroy(&game_sync->master_access_mutex);
        sem_destroy(&game_sync->game_state_mutex);
        sem_destroy(&game_sync->reader_count_mutex);
        
        for (int i = 0; i < player_count; i++) {
            sem_destroy(&game_sync->player_move_sem[i]);
        }
    }
    
    // Unmap and unlink shared memory
    if (game_state != NULL) {
        close_shared_memory(game_state, GAME_STATE_SHM, game_state_size);
        game_state = NULL;
    }
    
    if (game_sync != NULL) {
        close_shared_memory(game_sync, GAME_SYNC_SHM, sizeof(GameSync));
        game_sync = NULL;
    }
}

void sig_handler(int signo) {
    printf("Received signal %d. Cleaning up and exiting...\n", signo);
    
    // Set game_over flag first to notify all processes
    if (game_state != NULL) {
        game_state->game_over = true;
    }
    
    // Signal all players to prevent deadlocks
    if (game_sync != NULL) {
        for (int i = 0; i < player_count; i++) {
            sem_post(&game_sync->player_move_sem[i]);
        }
    }
    
    // Signal view to prevent deadlocks
    if (game_sync != NULL && view.binary_path != NULL) {
        sem_post(&game_sync->view_update_sem);
    }
    
    // Give processes a chance to observe the game_over flag
    usleep(100000); // 100ms
    
    // Send termination signal to all children
    if (view.binary_path != NULL && view.pid > 0) {
        kill(view.pid, SIGTERM);
    }
    
    for (int i = 0; i < player_count; i++) {
        if (players[i].pid > 0) {
            kill(players[i].pid, SIGTERM);
        }
    }
    
    // Wait a bit for processes to terminate gracefully
    usleep(100000); // 100ms
    
    // Clean up resources
    cleanup();
    
    exit(EXIT_SUCCESS);
}
