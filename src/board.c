#include "board.h"
#include "parser.h"
#include "utils.h"
#include "display.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>


static void move_ghost_charged(board_t* board, ghost_t* ghost, char direction);
static int move_ghost(board_t* board, ghost_t* ghost, int new_x, int new_y);
static int find_and_kill_pacman(board_t* board, int new_x, int new_y);
static inline int get_board_index(board_t* board, int x, int y);
static inline int is_valid_position(board_t* board, int x, int y);
static inline void finish_play(board_t *board, int *level_state);
static inline void lock_for_move(board_t* board, int old_index, int new_index);
static inline void unlock_after_move(board_t* board, int old_index, int new_index);


sem_t sem_start_turn;      // Controls start of logic
sem_t sem_finished_plays;  // Controls end of logic (waiting for UI)
sem_t sem_render_complete; // Controls end of frame (releasing threads)


void play_level(board_t* board) {
    debug("UI thread: Starting level\n");
    board->level_result = CONTINUE_PLAY;

    pthread_t pacman_tid;
    pthread_t ghosts_tid[board->n_ghosts];

    sem_init(&sem_start_turn, 0, 0);
    sem_init(&sem_finished_plays, 0, 0);
    sem_init(&sem_render_complete, 0, 0);

    pacman_thread_arg_t pacman_args;
    ghost_thread_arg_t ghost_args[board->n_ghosts];

    pacman_args.board = board;
    pacman_args.pacman_id = 0;

    if (pthread_create(&pacman_tid, NULL, pacman_thread, (void*)&pacman_args) != 0) {
        debug("UI thread: Error creating pacman thread.\n");
        return;
    }

    for (int i = 0; i < board->n_ghosts; i++) {
        ghost_args[i].board = board;
        ghost_args[i].ghost_id = i;
        if (pthread_create(&ghosts_tid[i], NULL, ghost_thread, (void*)&ghost_args[i]) != 0) {
            debug("UI thread: Error creating ghost thread for ghost %d.\n", i);
            return;
        }
    }

    int n_entities = board->n_pacmans + board->n_ghosts; // pacmans + ghosts
    debug("UI thread: Starting level loop with %d entities.\n", n_entities);

    screen_refresh(board, DRAW_MENU);

    while (board->level_result == CONTINUE_PLAY) {
        sleep_ms(board->tempo);

        // Release "Start the turn" semaphores to all entity threads
        for (int i = 0; i < n_entities; i++) {
            sem_post(&sem_start_turn);
        }

        // Wait for everyone to finish moving
        for (int i = 0; i < n_entities; i++) {
            sem_wait(&sem_finished_plays);
        }

        debug("=== ALL ENTITIES MOVED - RENDERING ===\n");

        // Safe multithreaded enviorenment, other threads are waiting, no need to use locks

        if (board->play_result == CREATE_BACKUP) {
            debug("UI thread: Creating backup...\n");
            int result = create_backup(board, &pacman_tid, ghosts_tid, &pacman_args, ghost_args);
            board->play_result = CONTINUE;

            if (result == -1) {
                debug("UI thread: Backup creation failed, quitting game\n");
                board->level_result = QUIT_GAME_FORCED;
            } else if (result == 1) {
                debug("UI thread: Backup instance created\n"); 
                continue; // Restart the loop with fresh state
            }

        } else if (board->play_result == REACHED_PORTAL) {
            debug("UI thread: Level completed, moving to next level\n");
            board->level_result = NEXT_LEVEL;
        } else if (board->play_result == DEAD_PACMAN) {
            debug("UI thread: Pacman is dead, quitting game\n");
            board->level_result = QUIT_GAME;
        } else if (board->play_result == QUIT_PRESSED) {
            debug("UI thread: User forced quit, quitting game\n");
            board->level_result = QUIT_GAME_FORCED;
        }

        board->pacmans[0].ui_key = get_input();
        debug("UI thread: Got input %c\n", board->pacmans[0].ui_key);

        screen_refresh(board, DRAW_MENU);

        debug("\n");

        sleep_ms(board->tempo);


        // Unlock threads to play next turn, unsafe enviorenment

        debug("=== RENDER COMPLETE - RELEASING - NEW PLAY ===\n");

        // Release threads to complete the loop
        for (int i = 0; i < n_entities; i++) {
            sem_post(&sem_render_complete);
        }
    }
    
    pthread_join(pacman_tid, NULL);
    for (int i = 0; i < board->n_ghosts; i++) {
        pthread_join(ghosts_tid[i], NULL);
    }

    sem_destroy(&sem_finished_plays);
    sem_destroy(&sem_start_turn);
    sem_destroy(&sem_render_complete);

    return;
}

void* pacman_thread(void* arg) {
    pacman_thread_arg_t* args = (pacman_thread_arg_t*)arg;
    board_t* board = args->board;
    pacman_t* pacman = &board->pacmans[args->pacman_id];

    debug("Pacman %d thread started.\n", args->pacman_id);

    int level_state = board->level_result;
    command_t* play;
    command_t c; 

    while (level_state == CONTINUE_PLAY) {
        sem_wait(&sem_start_turn);

        debug("Pacman thread: RUNNING - KEY %c\n", pacman->ui_key);
        if (pacman->waiting > 0) {
            pacman->waiting -= 1;
            finish_play(board, &level_state);
            continue;
        }
        pacman->waiting = pacman->passo;

        if (pacman->n_moves == 0) { // if is user input
            c.command = pacman->ui_key;

            if(c.command == '\0') {
                finish_play(board, &level_state);
                continue;
            } else if (c.command == 'G' || c.command == 'Q') { 
                pthread_rwlock_wrlock(&board->play_res_rwlock);
                if (c.command == 'G' && board->play_result == CONTINUE) {
                    board->play_result = CREATE_BACKUP;
                } else if (c.command == 'Q') {
                    board->play_result = QUIT_PRESSED;
                }
                pthread_rwlock_unlock(&board->play_res_rwlock);

                finish_play(board, &level_state);
                continue;
            }

            c.turns = 1;
            play = &c;
        }
        else {
            play = &pacman->moves[pacman->current_move%pacman->n_moves];
        }

        int new_x = pacman->pos_x;
        int new_y = pacman->pos_y;

        char direction = play->command;

        if (direction == 'R') {
            char directions[] = {'W', 'S', 'A', 'D'};
            direction = directions[rand() % 4];
        }

        switch (direction) {
            case 'W': // Up
                new_y--;
                break;
            case 'S': // Down
                new_y++;
                break;
            case 'A': // Left
                new_x--;
                break;
            case 'D': // Right
                new_x++;
                break;
            case 'T': // Wait
                if (play->turns_left == 1) {
                    pacman->current_move += 1;
                    play->turns_left = play->turns;
                }
                else play->turns_left -= 1;
                finish_play(board, &level_state);
                continue;
            default:
                finish_play(board, &level_state);
                continue; // Invalid direction
        }

        // Logic for the auto movement
        pacman->current_move+=1;

        if (!is_valid_position(board, new_x, new_y)) {
            finish_play(board, &level_state);
            continue;
        }

        int new_index = get_board_index(board, new_x, new_y);
        int old_index = get_board_index(board, pacman->pos_x, pacman->pos_y);
        lock_for_move(board, old_index, new_index);

        // Ensure pacman still alive after locks acquired
        if (!pacman->alive) {
            unlock_after_move(board, old_index, new_index);
            finish_play(board, &level_state);
            continue;
        }

        char target_content = board->board[new_index].content;

        if (board->board[new_index].has_portal) {
            board->board[old_index].content = ' ';
            board->board[new_index].content = 'P';
            pthread_rwlock_wrlock(&board->play_res_rwlock);
            board->play_result = REACHED_PORTAL;
            pthread_rwlock_unlock(&board->play_res_rwlock);
            unlock_after_move(board, old_index, new_index);
            finish_play(board, &level_state);
            continue;
        }

        // Check for walls
        if (target_content == 'W') {
            unlock_after_move(board, old_index, new_index);
            finish_play(board, &level_state);
            continue;
        }

        // Check for ghosts
        if (target_content == 'M') {
            kill_pacman(board, args->pacman_id);
            pthread_rwlock_wrlock(&board->play_res_rwlock);
            board->play_result = DEAD_PACMAN;
            pthread_rwlock_unlock(&board->play_res_rwlock);
            unlock_after_move(board, old_index, new_index);
            finish_play(board, &level_state);
            continue;
        }

        // Collect points
        if (board->board[new_index].has_dot) {
            pacman->points++;
            board->board[new_index].has_dot = 0;
        }

        // Update board
        pacman->pos_x = new_x;
        pacman->pos_y = new_y;

        board->board[old_index].content = ' ';
        board->board[new_index].content = 'P';

        unlock_after_move(board, old_index, new_index);
        finish_play(board, &level_state);
    }

    return NULL;
}

void* ghost_thread(void* arg) {
    ghost_thread_arg_t* args = (ghost_thread_arg_t*)arg;
    board_t* board = args->board;
    int ghost_id = args->ghost_id;
    ghost_t* ghost = &board->ghosts[ghost_id];

    debug("Ghost %d thread started.\n", ghost_id);

    int level_state = board->level_result;
    command_t* play;

    while (level_state == CONTINUE_PLAY) {
        sem_wait(&sem_start_turn);
        debug("Ghost %d thread: RUNNING\n", ghost_id);

        if (ghost->waiting > 0) {
            ghost->waiting -= 1;
            finish_play(board, &level_state);
            continue;
        }
        ghost->waiting = ghost->passo;

        play = &ghost->moves[ghost->current_move%ghost->n_moves];

        int new_x = ghost->pos_x;
        int new_y = ghost->pos_y;

        char direction = play->command;

         if (direction == 'R') {
            char directions[] = {'W', 'S', 'A', 'D'};
            direction = directions[rand() % 4];
        }

        debug("Ghost %d thread: KEY %c\n", ghost_id, play->command);

        // Calculate new position based on direction
        switch (direction) {
            case 'W': // Up
                new_y--;
                break;
            case 'S': // Down
                new_y++;
                break;
            case 'A': // Left
                new_x--;
                break;
            case 'D': // Right
                new_x++;
                break;
            case 'C': // Charge
                ghost->current_move += 1;
                ghost->charged = 1;
                finish_play(board, &level_state);
                continue;
            case 'T': // Wait
                if (play->turns_left == 1) {
                    ghost->current_move += 1; // move on
                    play->turns_left = play->turns;
                }
                else play->turns_left -= 1;
                finish_play(board, &level_state);
                continue;
            default:
                finish_play(board, &level_state);
                continue; // Invalid direction
        }

        // Logic for the WASD movement
        ghost->current_move++;
        if (ghost->charged) {
            move_ghost_charged(board, ghost, direction);
            finish_play(board, &level_state);
            continue;
        }

        move_ghost(board, ghost, new_x, new_y);
        
        finish_play(board, &level_state);
    }

    return NULL;
}

static void move_ghost_charged(board_t* board, ghost_t* ghost, char direction) {
    int new_x = ghost->pos_x;
    int new_y = ghost->pos_y;
    int dx = 0;
    int dy = 0;

    ghost->charged = 0;

    switch (direction) {
        case 'W': dy = -1; break;
        case 'S': dy = 1;  break;
        case 'A': dx = -1; break;
        case 'D': dx = 1;  break;
        default: return;
    }

    while (true) {
        new_x = new_x + dx;
        new_y = new_y + dy;

        if (move_ghost(board, ghost, new_x, new_y) == INVALID_MOVE) {
            break;
        }
    }
}

static int move_ghost(board_t* board, ghost_t* ghost, int new_x, int new_y) {
    if (!is_valid_position(board, new_x, new_y)) {
        return INVALID_MOVE;
    }

    int new_index = get_board_index(board, new_x, new_y);
    int old_index = get_board_index(board, ghost->pos_x, ghost->pos_y);
    lock_for_move(board, old_index, new_index);

    char target_content = board->board[new_index].content;

    // Check for walls and ghosts
    if (target_content == 'W' || target_content == 'M') {
        unlock_after_move(board, old_index, new_index);
        return INVALID_MOVE;
    }

    if (target_content == 'P') {
        int result = find_and_kill_pacman(board, new_x, new_y);
        pthread_rwlock_wrlock(&board->play_res_rwlock);
        board->play_result = result;
        pthread_rwlock_unlock(&board->play_res_rwlock);
    }

    // Update board
    ghost->pos_x = new_x;
    ghost->pos_y = new_y;

    board->board[old_index].content = ' ';
    board->board[new_index].content = 'M';

    unlock_after_move(board, old_index, new_index);
    return VALID_MOVE;
}

void kill_pacman(board_t* board, int pacman_index) {
    debug("Killing %d pacman\n\n", pacman_index);
    pacman_t* pac = &board->pacmans[pacman_index];
    int index = pac->pos_y * board->width + pac->pos_x;

    // Remove pacman from the board
    board->board[index].content = ' ';

    // Mark pacman as dead
    pac->alive = 0;
}

// Static Loading
int load_pacman(board_t* board, int points) {
    pacman_t* pacman = &board->pacmans[0];

    // Initialize defaults
    pacman->pos_x = 0;
    pacman->pos_y = 0;
    pacman->alive = 1;
    pacman->points = points;
    pacman->passo = 0;
    pacman->n_moves = 0;
    pacman->current_move = 0;
    pacman->waiting = 0;

    parse_pacman_file(board);

    board_pos_t* start_pos = &board->board[pacman->pos_y * board->width + pacman->pos_x];
    start_pos->content = 'P';
    if (start_pos->has_dot) {
        start_pos->has_dot = 0;
        pacman->points++;
    }
    
    return 0;
}

// Static Loading
int load_ghosts(board_t* board) {

    for (int i = 0; i < board->n_ghosts; i++) {
        ghost_t* ghost = &board->ghosts[i];
        // Initialize defaults
        ghost->pos_x = 0;
        ghost->pos_y = 0;
        ghost->passo = 0;
        ghost->n_moves = 0;
        ghost->current_move = 0;
        ghost->waiting = 0;
        ghost->charged = 0;

        parse_ghost_file(board, i);

        board->board[ghost->pos_y * board->width + ghost->pos_x].content = 'M';
    }
    
    return 0;
}

int load_level(board_t *board, int points) {
    snprintf(board->level_file, MAX_FILENAME, "%s%d.lvl", board->assets_dir, board->current_level);
    debug("Loading level file: %s\n", board->level_file);

    // Also allocates board, pacmans and ghosts arrays
    parse_level_file(board);

    load_pacman(board, points);
    load_ghosts(board);

    return 0;
}

void unload_level(board_t * board) {
    free(board->board);
    free(board->pacmans);
    free(board->ghosts);
}

int create_backup(board_t* board, pthread_t* pacman_tid, pthread_t* ghosts_tid, pacman_thread_arg_t* pacman_args, ghost_thread_arg_t* ghost_args) {
    if (board->has_saved) {
        debug("State has been already saved.\n");
        return 0;
    }

    debug("Creating backup process.\n");

    int pid = fork();
    if (pid < 0) {
        debug("Failed to create backup process.\n");
        return -1;
    }

    board->has_saved = 1;
    
    if (pid != 0) {
        // Parent process
        debug("Parent process waiting.\n");
        board->is_backup_instance = 0;
        int status;
        wait(&status); // Wait for child to finish and get its exit status
        debug("Parent process resumed from backup with result %d.\n", board->level_result);
        if (WIFEXITED(status)) {
            board->level_result = WEXITSTATUS(status);
        } else {
            debug("Backup process did not terminate normally.\n");
            board->level_result = QUIT_GAME_FORCED;
        }
        debug("Parent process restored from backup.\n");

        return 0;
    } else {
        // Child process - Backup instance
        debug("Backup instance running.\n");
        board->is_backup_instance = 1;
        board->level_result = CONTINUE_PLAY;

        // The semaphores contain values copied from the parent. 
        // We must reset them to 0 so the new threads start in a blocked state 
        // waiting for the UI loop to release them.
        sem_destroy(&sem_start_turn);
        sem_destroy(&sem_finished_plays);
        sem_destroy(&sem_render_complete);

        sem_init(&sem_start_turn, 0, 0);
        sem_init(&sem_finished_plays, 0, 0);
        sem_init(&sem_render_complete, 0, 0);

        // Recreate threads after fork and Pass the same args as before
        if (pthread_create(pacman_tid, NULL, pacman_thread, (void*)pacman_args) != 0) {
            debug("Error recreating pacman thread.\n");
            return -1;
        }

        for (int i = 0; i < board->n_ghosts; i++) {
            if (pthread_create(&ghosts_tid[i], NULL, ghost_thread, (void*)&ghost_args[i]) != 0) {
                debug("Error creating ghost thread for ghost %d.\n", i);
                return -1;
            }
        }

        // Indicate we are in backup instance, so it needs to skip sem post of ui 
        // because of sync issues with newly created threads, basically force restart of the loop
        return 1; 
    }
}


// Helper private function to find and kill pacman at specific position
static int find_and_kill_pacman(board_t* board, int new_x, int new_y) {
    for (int p = 0; p < board->n_pacmans; p++) {
        pacman_t* pac = &board->pacmans[p];
        if (pac->pos_x == new_x && pac->pos_y == new_y && pac->alive) {
            pac->alive = 0;
            kill_pacman(board, p);
            return DEAD_PACMAN;
        }
    }
    return CONTINUE;
}

// Helper private function for getting board position index
static inline int get_board_index(board_t* board, int x, int y) {
    return y * board->width + x;
}

// Helper private function for checking valid position
static inline int is_valid_position(board_t* board, int x, int y) {
    return (x >= 0 && x < board->width) && (y >= 0 && y < board->height); // Inside of the board boundaries
}

static inline void finish_play(board_t *board, int *level_state) {
    // 1. Tell UI we are done with our move
    sem_post(&sem_finished_plays);

    // 2. Wait for UI to finish rendering and game state checks
    // This acts as a barrier so we don't loop around too fast
    sem_wait(&sem_render_complete);

    // 3. Update local state (safe now because UI has finished writing to it)
    *level_state = board->level_result;
}

static inline void lock_for_move(board_t* board, int old_index, int new_index) {
    int locks_acquired = 0;
    int n_tries = 1;
    int backoff_range = (int)(0.05 * board->tempo);
    if (backoff_range < 1) backoff_range = 1;

    while (!locks_acquired) {
        pthread_rwlock_wrlock(&board->board[old_index].rwlock);
        if (pthread_rwlock_trywrlock(&board->board[new_index].rwlock) == 0) {
            locks_acquired = 1;
        } else {
            pthread_rwlock_unlock(&board->board[old_index].rwlock);
            sleep_ms(rand() % (n_tries * backoff_range));
            n_tries++;
        }
    }
}

static inline void unlock_after_move(board_t* board, int old_index, int new_index) {
    pthread_rwlock_unlock(&board->board[old_index].rwlock);
    pthread_rwlock_unlock(&board->board[new_index].rwlock);
}
