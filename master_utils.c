#define _GNU_SOURCE // For strdup
#include "master_utils.h"

// Directions for movement
const int movement[8][2] = {
    {0, -1},  // Up
    {1, -1},  // Up-Right
    {1, 0},   // Right
    {1, 1},   // Down-Right
    {0, 1},   // Down
    {-1, 1},  // Down-Left
    {-1, 0},  // Left
    {-1, -1}  // Up-Left
};

void display_winner(void) {
    int highest_score = -1;
    int winner_count = 0;
    int winner_indices[MAX_PLAYERS];
    
    for (int i = 0; i < player_count; i++) {
        if ((int)game_state->players[i].score > highest_score) {
            highest_score = game_state->players[i].score;
            winner_count = 1;
            winner_indices[0] = i;
        } else if ((int)game_state->players[i].score == highest_score) {
            winner_indices[winner_count] = i;
            winner_count++;
        }
    }
    
    printf("\nGame over! Final scores:\n");
    for (int i = 0; i < player_count; i++) {
        printf("%s: %u points (%u valid moves, %u invalid moves)\n",
               game_state->players[i].name, game_state->players[i].score,
               game_state->players[i].valid_moves, game_state->players[i].invalid_moves);
    }
    
    printf("\n\033[1;32m"); // Bold green text
    printf("=================================\n");
    if (winner_count == 1) {
        printf("WINNER: %s with %d points!\n", 
            game_state->players[winner_indices[0]].name, 
            highest_score);
    } else if (winner_count > 1) {
        printf("TIE GAME! WINNERS:\n");
        for (int i = 0; i < winner_count; i++) {
            int idx = winner_indices[i];
            printf("- %s with %d points\n", 
                game_state->players[idx].name, 
                highest_score);
        }
    }
    printf("=================================\n");
    printf("\033[0m"); // Reset text formatting

    printf("\nWinner%s:\n", winner_count > 1 ? "s" : "");
    for (int i = 0; i < player_count; i++) {
        if (game_state->players[i].score == highest_score) {
            printf("- %s (Score: %u, Valid Moves: %u, Invalid Moves: %u)\n",
                  game_state->players[i].name, game_state->players[i].score,
                  game_state->players[i].valid_moves, game_state->players[i].invalid_moves);
        }
    }
}

void parse_args(int argc, char* argv[], int* width, int* height, int* delay, 
                int* timeout, unsigned int* seed, char** view_path, 
                char*** player_paths, int* player_count) {
    int opt;
    bool p_flag = false;
    
    while ((opt = getopt(argc, argv, "w:h:d:t:s:v:p")) != -1) {
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
        fprintf(stderr, "Error: -p flag is required\n");
        exit(EXIT_FAILURE);
    }
    
    *player_count = argc - optind;
    if (*player_count < 1) {
        fprintf(stderr, "Error: At least one player must be specified\n");
        exit(EXIT_FAILURE);
    }
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
    game_state_size = sizeof(GameState) + width * height * sizeof(int);
    game_state = (GameState*)create_shared_memory(NAME_BOARD, game_state_size);
    
    memset(game_state, 0, game_state_size);
    game_state->width = width;
    game_state->height = height;
    game_state->player_count = player_count;
    game_state->game_over = false;
    
    srand(seed);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            game_state->board[y * width + x] = (rand() % 9) + 1;
        }
    }
    
    for (int i = 0; i < player_count; i++) {
        snprintf(game_state->players[i].name, PLAYER_NAME_MAX_LENGTH, "Player %d", i + 1);
        game_state->players[i].score = 0;
        game_state->players[i].invalid_moves = 0;
        game_state->players[i].valid_moves = 0;
        game_state->players[i].is_blocked = false;
    }
}

void init_game_sync(int player_count) {
    game_sync = (GameSync*)create_shared_memory(NAME_SYNC, sizeof(GameSync));
    
    sem_init(&game_sync->view_update_sem, 1, 0);
    sem_init(&game_sync->view_done_sem, 1, 0);
    sem_init(&game_sync->master_access_mutex, 1, 1);
    sem_init(&game_sync->game_state_mutex, 1, 1);
    sem_init(&game_sync->reader_count_mutex, 1, 1);
    
    game_sync->readers_count = 0;
    
    for (int i = 0; i < player_count; i++) {
        sem_init(&game_sync->player_move_sem[i], 1, 0);
        sem_post(&game_sync->player_move_sem[i]); // Allow first move
    }
}

void place_players_on_board(void) {
    int width = game_state->width;
    int height = game_state->height;
    int player_count = game_state->player_count;
    int center_x = width / 2;
    int center_y = height / 2;
    
    if (player_count == 1) {
        game_state->players[0].x = center_x;
        game_state->players[0].y = center_y;

        game_state->board[center_y * width + center_x] = -1; // Marked as captured by player 0
    } else {
        double radius_x = width / 3.0;
        double radius_y = height / 3.0;
        
        for (int i = 0; i < player_count; i++) {
            double angle = 2.0 * M_PI * i / player_count;
            int x = (int)(center_x + radius_x * cos(angle));
            int y = (int)(center_y + radius_y * sin(angle));
            
            // Ensure coordinates are within bounds
            x = (x < 0) ? 0 : ((x >= width) ? width - 1 : x);
            y = (y < 0) ? 0 : ((y >= height) ? height - 1 : y);
            
            game_state->players[i].x = x;
            game_state->players[i].y = y;
            
            // Mark as captured by player i
            game_state->board[y * width + x] = -(i );
        }
    }
}

void start_players_and_view(int width, int height) {
    if (view.binary_path != NULL) {
        pid_t pid = fork();
        if (pid == 0) {
            char width_str[16], height_str[16];
            sprintf(width_str, "%d", width);
            sprintf(height_str, "%d", height);
            
            execl(view.binary_path, view.binary_path, width_str, height_str, NULL);
            
            perror("execl view");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            view.pid = pid;
        } else {
            perror("fork view");
            exit(EXIT_FAILURE);
        }
    }
    
    for (int i = 0; i < player_count; i++) {
        if (pipe(players[i].pipe_fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        
        pid_t pid = fork();
        if (pid == 0) {
            close(players[i].pipe_fd[READ_END]);
            
            for (int j = 0; j < player_count; j++) {
                if (j != i) {
                    close(players[j].pipe_fd[READ_END]);
                    close(players[j].pipe_fd[WRITE_END]);
                }
            }
            
            if (dup2(players[i].pipe_fd[WRITE_END], STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            
            close(players[i].pipe_fd[WRITE_END]);
            
            char width_str[16], height_str[16];
            sprintf(width_str, "%d", width);
            sprintf(height_str, "%d", height);
            
            execl(players[i].binary_path, players[i].binary_path, width_str, height_str, NULL);
            
            perror("execl player");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            players[i].pid = pid;
            close(players[i].pipe_fd[WRITE_END]);
            
            game_state->players[i].pid = pid;
        } else {
            perror("fork player");
            exit(EXIT_FAILURE);
        }
    }
}

void game_loop(int delay, int timeout) {
    fd_set read_fds;
    struct timeval tv, current_time, last_valid_move_time;
    int max_fd = -1;
    
    gettimeofday(&last_valid_move_time, NULL);
    
    int start_index = 0; // For round-robin player processing
    
    while (!game_state->game_over) {
        // Check if all players are blocked
        int blocked_players = 0;
        for (int i = 0; i < player_count; i++) {
            if (game_state->players[i].is_blocked) {
                blocked_players++;
            } else if (!can_player_move(i)) {
                game_state->players[i].is_blocked = true;
                blocked_players++;
            }
        }
        
        if (blocked_players == player_count) {
            printf("Game over: All players are blocked\n");
            game_state->game_over = true;
            break;
        }
        
        // Calculate max file descriptor for select
        max_fd = -1;
        for (int i = 0; i < player_count; i++) {
            if (players[i].pipe_fd[READ_END] > max_fd) {
                max_fd = players[i].pipe_fd[READ_END];
            }
        }
        
        // Set up select timeout
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        
        // Prepare fd_set for select
        FD_ZERO(&read_fds);
        int active_players = 0;
        for (int i = 0; i < player_count; i++) {
            if (!game_state->players[i].is_blocked) {
                FD_SET(players[i].pipe_fd[READ_END], &read_fds);
                active_players++;
            }
        }
        
        if (active_players == 0) {
            printf("Game over: No active players remain\n");
            game_state->game_over = true;
            break;
        }
        
        // Check for timeout
        gettimeofday(&current_time, NULL);
        long elapsed_time = (current_time.tv_sec - last_valid_move_time.tv_sec) * 1000 +
                           (current_time.tv_usec - last_valid_move_time.tv_usec) / 1000;
        
        if (elapsed_time > timeout * TO_MILI_SEC) {
            printf("Game over: Timeout reached (%d seconds without valid moves)\n", timeout);
            game_state->game_over = true;
            break;
        }
        
        // Wait for player input
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ready == -1) {
            if (errno == EINTR) continue; // Interrupted by signal
            perror("select");
            break;
        } else if (ready == 0) {
            printf("Game over: No player input received within timeout\n");
            game_state->game_over = true;
            break;
        }
        
        // Process player inputs in round-robin fashion
        bool any_processed = false;
        
        for (int i = 0; i < player_count; i++) {
            int player_idx = (start_index + i) % player_count;
            
            if (game_state->players[player_idx].is_blocked) {
                continue;
            }
            
            if (FD_ISSET(players[player_idx].pipe_fd[READ_END], &read_fds)) {
                unsigned char direction;
                ssize_t bytes_read = read(players[player_idx].pipe_fd[READ_END], &direction, 1);
                
                if (bytes_read > 0) {
                    // Process the movement
                    sem_wait(&game_sync->master_access_mutex);
                    sem_wait(&game_sync->game_state_mutex);
                    sem_post(&game_sync->master_access_mutex);
                    
                    bool valid = process_movement(player_idx, direction);
                    
                    // Check if any player is now blocked
                    blocked_players = 0;
                    for (int j = 0; j < player_count; j++) {
                        if (game_state->players[j].is_blocked) {
                            blocked_players++;
                        } else if (!can_player_move(j)) {
                            game_state->players[j].is_blocked = true;
                            blocked_players++;
                        }
                    }
                    
                    sem_post(&game_sync->game_state_mutex);
                    
                    // Signal the player that their move was processed
                    sem_post(&game_sync->player_move_sem[player_idx]);
                    
                    // If valid movement, update last valid move time
                    if (valid) {
                        gettimeofday(&last_valid_move_time, NULL);
                        
                        if (view.binary_path != NULL) {
                            sem_post(&game_sync->view_update_sem);
                            sem_wait(&game_sync->view_done_sem);
                        }
                        
                        usleep(delay * 1000);
                    }
                    
                    any_processed = true;
                    
                    if (blocked_players == player_count) {
                        printf("Game over: All players are blocked\n");
                        game_state->game_over = true;
                        break;
                    }
                    
                    break; // Process one player per iteration
                } else if (bytes_read == 0) {
                    printf("Player %d has closed its pipe\n", player_idx);
                    game_state->players[player_idx].is_blocked = true;
                }
            }
        }
        
        // Update starting index for next round
        if (any_processed) {
            start_index = (start_index + 1) % player_count;
        }
        
        usleep(1000); // Small delay to prevent CPU hogging
    }
    
    // Game has ended
    game_state->game_over = true;
    
    // Signal all players to wake them up from sem_wait
    for (int i = 0; i < player_count; i++) {
        sem_post(&game_sync->player_move_sem[i]);
    }
    
    // Notify view of the final game state
    if (view.binary_path != NULL) {
        sem_post(&game_sync->view_update_sem);
        sem_wait(&game_sync->view_done_sem);
    }
    
    // Give players a chance to exit cleanly
    usleep(500000);  // 500ms
    
    // Force terminate any processes that didn't exit
    for (int i = 0; i < player_count; i++) {
        if (players[i].pid > 0) {
            kill(players[i].pid, SIGTERM);
        }
    }
    
    if (view.binary_path != NULL && view.pid > 0) {
        kill(view.pid, SIGTERM);
    }
}

bool process_movement(int player_idx, unsigned char direction) {
    bool result = false;
    
    if (direction > 7) {
        // Invalid direction
        game_state->players[player_idx].invalid_moves++;
        return false;
    }
    
    int width = game_state->width;
    int height = game_state->height;
    int x = game_state->players[player_idx].x;
    int y = game_state->players[player_idx].y;
    int new_x = x + movement[direction][0];
    int new_y = y + movement[direction][1];
    
    // Check if new position is valid
    if (new_x < 0 || new_x >= width || new_y < 0 || new_y >= height) {
        // Out of bounds
        game_state->players[player_idx].invalid_moves++;
    } else {
        int cell_value = game_state->board[new_y * width + new_x];
        if (cell_value <= 0) {
            // Cell already captured
            game_state->players[player_idx].invalid_moves++;
        } else {
            // Valid move
            game_state->players[player_idx].valid_moves++;
            game_state->players[player_idx].score += cell_value;
            game_state->players[player_idx].x = new_x;
            game_state->players[player_idx].y = new_y;
            
            // Mark the cell as captured by the player
            game_state->board[new_y * width + new_x] = -(player_idx );
            
            result = true;
        }
    }
    
    return result;
}

bool can_player_move(int player_idx) {
    int width = game_state->width;
    int height = game_state->height;
    int x = game_state->players[player_idx].x;
    int y = game_state->players[player_idx].y;
    
    // Check all 8 directions
    for (int dir = 0; dir < 8; dir++) {
        int new_x = x + movement[dir][0];
        int new_y = y + movement[dir][1];
        
        if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
            int cell_value = game_state->board[new_y * width + new_x];
            if (cell_value > 0) {
                return true; // Player can move
            }
        }
    }
    
    return false; // Player cannot move
}

void cleanup(void) {
    // Close all pipes
    for (int i = 0; i < player_count; i++) {
        if (players[i].pipe_fd[READ_END] > 0) {
            close(players[i].pipe_fd[READ_END]);
            players[i].pipe_fd[READ_END] = -1;
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
        close_shared_memory(game_state, NAME_BOARD, game_state_size);
        game_state = NULL;
    }
    
    if (game_sync != NULL) {
        close_shared_memory(game_sync, NAME_SYNC, sizeof(GameSync));
        game_sync = NULL;
    }
}

void sig_handler(int signo) {
    printf("Received signal %d. Cleaning up and exiting...\n", signo);
    
    if (game_state != NULL) {
        game_state->game_over = true;
    }
    
    if (game_sync != NULL) {
        for (int i = 0; i < player_count; i++) {
            sem_post(&game_sync->player_move_sem[i]);
        }
    }
    
    if (game_sync != NULL && view.binary_path != NULL) {
        sem_post(&game_sync->view_update_sem);
    }
    
    usleep(100000); // 100ms
    
    if (view.binary_path != NULL && view.pid > 0) {
        kill(view.pid, SIGTERM);
    }
    
    for (int i = 0; i < player_count; i++) {
        if (players[i].pid > 0) {
            kill(players[i].pid, SIGTERM);
        }
    }
    
    usleep(100000); // 100ms
    cleanup();
    exit(EXIT_SUCCESS);
}