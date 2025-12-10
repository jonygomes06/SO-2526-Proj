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
        game_board.play_result = CONTINUE;
        game_board.level_result = CONTINUE_PLAY;
        load_level(&game_board, accumulated_points);
        screen_refresh(&game_board, DRAW_MENU);

        play_level(&game_board);

        if (game_board.level_result == NEXT_LEVEL) {
            screen_refresh(&game_board, DRAW_WIN);
            sleep_ms(2000);
            game_board.current_level++;
        } else if (game_board.level_result == QUIT_GAME) {
            screen_refresh(&game_board, DRAW_GAME_OVER);
            sleep_ms(2000);
            end_game = true;
        } else if (game_board.level_result == QUIT_GAME_FORCED) {
            debug("Main thread: User forced quit, exiting game.\n");
            end_game = true;
        } else if (game_board.level_result == BACKUP_WON_GAME) {
            debug("Main thread: Backup instance won the game, exiting.\n");
            end_game = true;
        }
        
        accumulated_points = game_board.pacmans[0].points;

        print_board(&game_board);
        unload_level(&game_board);
    }

    if (game_board.is_backup_instance) {
        if (game_board.level_result == QUIT_GAME) {
            game_board.level_result = CONTINUE_PLAY;
        } else if (game_board.level_result == NEXT_LEVEL && game_board.current_level > game_board.n_levels) {
            game_board.level_result = BACKUP_WON_GAME;
        }
        debug("Backup instance exiting with result %d.\n", game_board.level_result);
        exit(game_board.level_result);
    }

    terminal_cleanup();

    close_debug_file();

    return 0;
}
