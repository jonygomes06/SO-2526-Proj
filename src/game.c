#include "parser.h"
#include "utils.h"
#include "board.h"
#include "display.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <level_directory>\n", argv[0]);
        exit(1);
    }

    // Random seed for any random movements
    srand((unsigned int)time(NULL));

    open_debug_file("debug.log");

    terminal_init();
    
    board_t game_board;
    int accumulated_points = 0;
    bool end_game = false;

    strncpy(game_board.assets_dir, argv[1], MAX_DIRNAME);
    game_board.has_saved = 0;
    game_board.is_backup_instance = 0;

    parse_levels_directory(&game_board);

    while (!end_game && game_board.current_level <= game_board.n_levels) {
        load_level(&game_board, accumulated_points);
        screen_refresh(&game_board, DRAW_MENU);

        pthread_t ui_level_tid;

        if (pthread_create(&ui_level_tid, NULL, ui_level_thread, (void*)&game_board) != 0) {
            debug("Error creating UI thread.\n");
            break;
        }

        pthread_join(ui_level_tid, NULL);
        
        accumulated_points = game_board.pacmans[0].points;
        end_game = (game_board.level_result == QUIT_GAME);

        print_board(&game_board);
        unload_level(&game_board);
    }

    if (game_board.is_backup_instance) {
        debug("Backup instance exiting.\n");
        exit(0);
    }

    terminal_cleanup();

    close_debug_file();

    return 0;
}
