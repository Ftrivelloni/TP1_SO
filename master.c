#define _GNU_SOURCE // For strdup
#include "master_utils.h"

// Global variables
GameState* game_state = NULL;
GameSync* game_sync = NULL;
PlayerProcess players[MAX_PLAYERS];
ViewProcess view;
int player_count = 0;
size_t game_state_size = 0;

int main(int argc, char* argv[]) {
    int width = MIN_WIDTH;
    int height = MIN_HEIGHT;
    int delay = DEFAULT_DELAY;
    int timeout = DEFAULT_TIMEOUT;
    unsigned int seed = time(NULL);
    char* view_path = NULL;
    char** player_paths = NULL;
    
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    parse_args(argc, argv, &width, &height, &delay, &timeout, &seed, 
               &view_path, &player_paths, &player_count);
    
    if (player_count < 1) {
        fprintf(stderr, "Error: At least one player must be specified\n");
        exit(EXIT_FAILURE);
    }
    
    // Print game parameters
    printf("width: %d\n", width);
    printf("height: %d\n", height);
    printf("delay: %d\n", delay);
    printf("timeout: %d\n", timeout);
    printf("seed: %d\n", seed);
    printf("view: %s\n", view_path ? view_path : "None");
    printf("num_players: %d\n", player_count);
    for (int i = 0; i < player_count; i++) {
        printf("Player %d: %s\n", i, player_paths[i]);
    }
    
    init_game_state(width, height, player_count, seed);
    init_game_sync(player_count);
    place_players_on_board();
    
    view.binary_path = view_path;
    for (int i = 0; i < player_count; i++) {
        players[i].binary_path = player_paths[i];
    }
    start_players_and_view(width, height);
    
    sleep(1); // Give processes time to initialize
    
    game_loop(delay, timeout);
    display_winner();
    
    // Wait for all child processes and print results
    int status;
    pid_t pid;
    
    for (int i = 0; i < player_count; i++) {
        pid = waitpid(players[i].pid, &status, 0);
        if (pid >= 0) {
            if (WIFEXITED(status)) {
                printf("Player %s (%d) exited (%d) with a score of %d/%d/%d\n", 
                       game_state->players[i].name, i, WEXITSTATUS(status), 
                       game_state->players[i].score, game_state->players[i].valid_moves, 
                       game_state->players[i].invalid_moves);
            } else if (WIFSIGNALED(status)) {
                printf("Player %s (%d) terminated by signal %d with a score of %d/%d/%d\n", 
                       game_state->players[i].name, i, WTERMSIG(status), 
                       game_state->players[i].score, game_state->players[i].valid_moves, 
                       game_state->players[i].invalid_moves);
            }
        }
    }
    
    if (view_path != NULL) {
        pid = waitpid(view.pid, &status, 0);
        if (pid >= 0) {
            if (WIFEXITED(status)) {
                printf("View exited (%d)\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("View terminated by signal %d\n", WTERMSIG(status));
            }
        }
    }
    
    cleanup();
    
    free(view_path);
    for (int i = 0; i < player_count; i++) {
        free(player_paths[i]);
    }
    free(player_paths);
    
    return 0;
}
