#include "board.h"
#include "parser.h"
#include "utils.h"
#include "display.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>


static int find_and_kill_pacman(board_t* board, int new_x, int new_y);

static inline int get_board_index(board_t* board, int x, int y);

static inline int is_valid_position(board_t* board, int x, int y);

static inline void finish_play(board_t *board, int *level_state);


sem_t sem_finished_plays;
sem_t sem_ui_thread;


void* ui_level_thread(void* arg) {
    debug("UI level thread started.\n");
    board_t* board = (board_t*)arg;
    board->level_result = CONTINUE_PLAY;

    pthread_t pacman_tid;
    pthread_t ghosts_tid[board->n_ghosts];

    sem_init(&sem_finished_plays, 0, 0);
    sem_init(&sem_ui_thread, 0, 0);

    pacman_thread_arg_t pacman_args;
    ghost_thread_arg_t ghost_args[board->n_ghosts];

    pacman_args.board = board;
    pacman_args.pacman_id = 0;

    if (pthread_create(&pacman_tid, NULL, pacman_thread, (void*)&pacman_args) != 0) {
        debug("Error creating pacman thread.\n");
        return NULL;
    }

    for (int i = 0; i < board->n_ghosts; i++) {
        ghost_args[i].board = board;
        ghost_args[i].ghost_id = i;
        if (pthread_create(&ghosts_tid[i], NULL, ghost_thread, (void*)&ghost_args[i]) != 0) {
            debug("Error creating ghost thread for ghost %d.\n", i);
            return NULL;
        }
    }

    // int n_entities = board->n_pacmans + board->n_ghosts; // pacmans + ghosts
    int n_entities = board->n_pacmans; // pacmans for now TODO remove this line when ghosts are implemented

    screen_refresh(board, DRAW_MENU);

    while (board->level_result == CONTINUE_PLAY) {
        sleep_ms(board->tempo);

        if (board->has_saved) {
            debug("UI thread: Hi there I am %d, waiting HERE 1.\n", board->is_backup_instance);
        }
        for (int i = 0; i < n_entities; i++) {
            sem_wait(&sem_finished_plays);
        }
        if (board->has_saved) {
            debug("UI thread: Hi there I am %d, waiting HERE 2.\n", board->is_backup_instance);
        }

        // Safe multithreaded enviorenment, other threads are waiting, no need to use locks

        if (board->play_result == CREATE_BACKUP) {
            debug("UI thread: Creating backup...\n");
            create_backup(board);
            board->play_result = CONTINUE;
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

        if (board->has_saved) {
            debug("UI thread: Hi there I am %d.\n", board->is_backup_instance);
        }

        board->pacmans[0].ui_key = get_input();

        screen_refresh(board, DRAW_MENU);

        // Unlock threads to play next turn, unsafe enviorenment

        for (int i = 0; i < n_entities; i++) {
            sem_post(&sem_ui_thread);
        }
    }
    

    pthread_join(pacman_tid, NULL);
    for (int i = 0; i < board->n_ghosts; i++) {
        pthread_join(ghosts_tid[i], NULL);
    }
    for (int i = 0; i < board->n_ghosts; i++) {
        pthread_join(ghosts_tid[i], NULL);
    }

    sem_destroy(&sem_finished_plays);
    sem_destroy(&sem_ui_thread);

    return NULL;
}

void* pacman_thread(void* arg) {
    debug("Pacman thread started.\n");
    pacman_thread_arg_t* args = (pacman_thread_arg_t*)arg;
    board_t* board = args->board;
    pacman_t* pacman = &board->pacmans[args->pacman_id];

    int level_state = board->level_result;
    command_t* play;

    while (level_state == CONTINUE_PLAY) {
        if (board->has_saved) {
            debug("Pacman thread: Hi there I am %d, and got key %c\n", board->is_backup_instance, pacman->ui_key);
        }
        if (pacman->waiting > 0) {
            pacman->waiting -= 1;
            finish_play(board, &level_state);
            continue;
        }
        pacman->waiting = pacman->passo;


        if (pacman->n_moves == 0) { // if is user input
            command_t c; 
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

                debug("Pacman thread before: Hi there I am %d, and got key %c\n", board->is_backup_instance, pacman->ui_key);
                finish_play(board, &level_state);
                
                debug("Pacman thread: Hi there I am %d, and got key %c\n", board->is_backup_instance, pacman->ui_key);

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

        debug("Pacman thread: KEY %c\n", play->command);

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
                return CONTINUE; // Invalid direction
        }

        // Logic for the auto movement
        pacman->current_move+=1;

        if (!is_valid_position(board, new_x, new_y)) {
            finish_play(board, &level_state);
            continue;
        }

        int new_index = get_board_index(board, new_x, new_y);
        int old_index = get_board_index(board, pacman->pos_x, pacman->pos_y);

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

        // Ensure pacman still alive after locks acquired
        if (!pacman->alive) {
            pthread_rwlock_unlock(&board->board[old_index].rwlock);
            pthread_rwlock_unlock(&board->board[new_index].rwlock);
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
            pthread_rwlock_unlock(&board->board[old_index].rwlock);
            pthread_rwlock_unlock(&board->board[new_index].rwlock);
            finish_play(board, &level_state);
            continue;
        }

        // Check for walls
        if (target_content == 'W') {
            pthread_rwlock_unlock(&board->board[old_index].rwlock);
            pthread_rwlock_unlock(&board->board[new_index].rwlock);
            finish_play(board, &level_state);
            continue;
        }

        // Check for ghosts
        if (target_content == 'M') {
            kill_pacman(board, args->pacman_id);
            pthread_rwlock_wrlock(&board->play_res_rwlock);
            board->play_result = DEAD_PACMAN;
            pthread_rwlock_unlock(&board->play_res_rwlock);
            pthread_rwlock_unlock(&board->board[old_index].rwlock);
            pthread_rwlock_unlock(&board->board[new_index].rwlock);
            finish_play(board, &level_state);
            continue;
        }

        // Collect points
        if (board->board[new_index].has_dot) {
            pacman->points++;
            board->board[new_index].has_dot = 0;
        }

        board->board[old_index].content = ' ';
        pacman->pos_x = new_x;
        pacman->pos_y = new_y;
        board->board[new_index].content = 'P';
        pthread_rwlock_unlock(&board->board[old_index].rwlock);
        pthread_rwlock_unlock(&board->board[new_index].rwlock);
        finish_play(board, &level_state);
    }

    
    if (board->has_saved) {
        debug("Pacman thread: Hi there I am %d (2), and got key %c\n", board->is_backup_instance, pacman->ui_key);
    }

    return NULL;
}

void* ghost_thread(void* arg) {
    ghost_thread_arg_t* args = (ghost_thread_arg_t*)arg;
    board_t* board = args->board;

    int i = 0;

    while (i < 5) { // TODO remove this when ghosts are implemented
        sleep_ms(board->tempo);
        i++;
    }

    return NULL;
}

// Helper private function for charged ghost movement in one direction
static int move_ghost_charged_direction(board_t* board, ghost_t* ghost, char direction, int* new_x, int* new_y) {
    int x = ghost->pos_x;
    int y = ghost->pos_y;
    *new_x = x;
    *new_y = y;
    
    switch (direction) {
        case 'W': // Up
            if (y == 0) return CONTINUE;
            *new_y = 0; // In case there is no colision
            for (int i = y - 1; i >= 0; i--) {
                char target_content = board->board[get_board_index(board, x, i)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_y = i + 1; // stop before colision
                    return CONTINUE;
                }
                else if (target_content == 'P') {
                    *new_y = i;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;

        case 'S': // Down
            if (y == board->height - 1) return CONTINUE;
            *new_y = board->height - 1; // In case there is no colision
            for (int i = y + 1; i < board->height; i++) {
                char target_content = board->board[get_board_index(board, x, i)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_y = i - 1; // stop before colision
                    return CONTINUE;
                }
                if (target_content == 'P') {
                    *new_y = i;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;

        case 'A': // Left
            if (x == 0) return CONTINUE;
            *new_x = 0; // In case there is no colision
            for (int j = x - 1; j >= 0; j--) {
                char target_content = board->board[get_board_index(board, j, y)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_x = j + 1; // stop before colision
                    return CONTINUE;
                }
                if (target_content == 'P') {
                    *new_x = j;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;

        case 'D': // Right
            if (x == board->width - 1) return CONTINUE;
            *new_x = board->width - 1; // In case there is no colision
            for (int j = x + 1; j < board->width; j++) {
                char target_content = board->board[get_board_index(board, j, y)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_x = j - 1; // stop before colision
                    return CONTINUE;
                }
                if (target_content == 'P') {
                    *new_x = j;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;
        default:
            debug("DEFAULT CHARGED MOVE - direction = %c\n", direction);
            return CONTINUE;
    }
    return CONTINUE;
}   

int move_ghost_charged(board_t* board, int ghost_index, char direction) {
    ghost_t* ghost = &board->ghosts[ghost_index];
    int x = ghost->pos_x;
    int y = ghost->pos_y;
    int new_x = x;
    int new_y = y;

    ghost->charged = 0; //uncharge
    int result = move_ghost_charged_direction(board, ghost, direction, &new_x, &new_y);
    if (result == CONTINUE) {
        debug("DEFAULT CHARGED MOVE - direction = %c\n", direction);
        return CONTINUE;
    }

    // Get board indices
    int old_index = get_board_index(board, ghost->pos_x, ghost->pos_y);
    int new_index = get_board_index(board, new_x, new_y);

    // Update board - clear old position (restore what was there)
    board->board[old_index].content = ' '; // Or restore the dot if ghost was on one
    // Update ghost position
    ghost->pos_x = new_x;
    ghost->pos_y = new_y;
    // Update board - set new position
    board->board[new_index].content = 'M';
    return result;
}

int move_ghost(board_t* board, int ghost_index, command_t* command) {
    ghost_t* ghost = &board->ghosts[ghost_index];
    int new_x = ghost->pos_x;
    int new_y = ghost->pos_y;

    // check passo
    if (ghost->waiting > 0) {
        ghost->waiting -= 1;
        return CONTINUE;
    }
    ghost->waiting = ghost->passo;

    char direction = command->command;
    
    if (direction == 'R') {
        char directions[] = {'W', 'S', 'A', 'D'};
        direction = directions[rand() % 4];
    }

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
            return CONTINUE;
        case 'T': // Wait
            if (command->turns_left == 1) {
                ghost->current_move += 1; // move on
                command->turns_left = command->turns;
            }
            else command->turns_left -= 1;
            return CONTINUE;
        default:
            return CONTINUE; // Invalid direction
    }

    // Logic for the WASD movement
    ghost->current_move++;
    if (ghost->charged)
        return move_ghost_charged(board, ghost_index, direction);

    // Check boundaries
    if (!is_valid_position(board, new_x, new_y)) {
        return CONTINUE;
    }

    // Check board position
    int new_index = get_board_index(board, new_x, new_y);
    int old_index = get_board_index(board, ghost->pos_x, ghost->pos_y);
    char target_content = board->board[new_index].content;

    // Check for walls and ghosts
    if (target_content == 'W' || target_content == 'M') {
        return CONTINUE;
    }

    int result = CONTINUE;
    // Check for pacman
    if (target_content == 'P') {
        result = find_and_kill_pacman(board, new_x, new_y);
    }

    // Update board - clear old position (restore what was there)
    board->board[old_index].content = ' '; // Or restore the dot if ghost was on one

    // Update ghost position
    ghost->pos_x = new_x;
    ghost->pos_y = new_y;

    // Update board - set new position
    board->board[new_index].content = 'M';
    return result;
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

    board->board[pacman->pos_y * board->width + pacman->pos_x].content = 'P';
    
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

// Helper private function to finish play and synchronize with UI thread level state
static inline void finish_play(board_t *board, int *level_state) {
    sem_post(&sem_finished_plays);
    sem_wait(&sem_ui_thread);
    *level_state = board->level_result;
}
