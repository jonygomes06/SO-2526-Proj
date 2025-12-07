#ifndef BOARD_H
#define BOARD_H

#define MAX_MOVES 20
#define MAX_LEVELS 20
#define MAX_DIRNAME 256
#define MAX_FILENAME 320
#define MAX_GHOSTS 25

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2

typedef enum {
    CREATE_BACKUP = 2,
    REACHED_PORTAL = 1,
    VALID_MOVE = 0,
    INVALID_MOVE = -1,
    DEAD_PACMAN = -2,
} move_t;

typedef struct {
    char command;
    int turns;
    int turns_left;
} command_t;

typedef struct {
    int pos_x, pos_y;            // current position
    int alive;                   // if is alive
    int points;                  // how many points have been collected
    int passo;                   // number of plays to wait before starting
    command_t moves[MAX_MOVES];
    int n_moves;                 // number of predefined moves, 0 if controlled by user, >0 if readed from level file
    int current_move;
    int waiting;
} pacman_t;

typedef struct {
    int pos_x, pos_y;            // current position
    int passo;                   // number of plays to wait between each move
    command_t moves[MAX_MOVES];
    int n_moves;                 // number of predefined moves from level file
    int current_move;
    int waiting;
    int charged;
} ghost_t;

typedef struct {
    char content;   // stuff like 'P' for pacman 'M' for monster/ghost and 'W' for wall
    int has_dot;    // whether there is a dot in this position or not
    int has_portal; // whether there is a portal in this position or not
} board_pos_t;

typedef struct {
    char assets_dir[MAX_DIRNAME];    // directory where assets are located
    int width, height;               // dimensions of the board
    board_pos_t* board;              // actual board, a row-major matrix
    int n_pacmans;                   // number of pacmans in the board
    pacman_t* pacmans;               // array containing every pacman in the board to iterate through when processing (Just 1)
    int n_ghosts;                    // number of ghosts in the board
    ghost_t* ghosts;                 // array containing every ghost in the board to iterate through when processing
    int n_levels;                    // number of levels available
    int current_level;               // index of the current level being played
    char level_file[MAX_FILENAME];   // file with the level layout
    char pacman_file[MAX_FILENAME];  // file with pacman movements
    char ghosts_files[MAX_GHOSTS][MAX_FILENAME]; // files with monster movements
    int tempo;                       // Duration of each play
    int has_saved;                   // flag to indicate if game state has already been saved
    int is_backup_instance;          // flag to indicate if this instance is a backup
    int play_result;                 // result of the last play
    int level_result;                // result of the last level played
} board_t;

typedef struct {
    board_t* board;
} pacman_thread_arg_t;

typedef struct {
    board_t* board;
    int ghost_id;
} ghost_thread_arg_t;


int play_board(board_t* game_board);

/*UI Level Thread*/
void* ui_level_thread(void* arg);

/*Pacman Thread*/
void* pacman_thread(void* arg);

/*Ghost(Monster) Thread*/
void* ghost_thread(void* arg);


/*Processes a command for Pacman or Ghost(Monster)
*_index - corresponding index in board's pacman_t/ghost_t array
command - command to be processed*/
int move_pacman(board_t* board, int pacman_index, command_t* command);
int move_ghost(board_t* board, int ghost_index, command_t* command);

/*Process the death of a Pacman*/
void kill_pacman(board_t* board, int pacman_index);

/*Adds a pacman to the board*/
int load_pacman(board_t* board, int points);

/*Adds a ghost(monster) to the board*/
int load_ghost(board_t* board);

/*Loads a level into board*/
int load_level(board_t* board, int accumulated_points);

/*Unloads levels loaded by load_level*/
void unload_level(board_t * board);

static int find_and_kill_pacman(board_t* board, int new_x, int new_y);

static inline int get_board_index(board_t* board, int x, int y);

static inline int is_valid_position(board_t* board, int x, int y);

#endif
