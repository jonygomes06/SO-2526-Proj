#ifndef PARSER_H
#define PARSER_H

#include "board.h"

#define LINE_BUFFER_SIZE 1024

int parse_levels_directory(board_t* board);

int parse_level_file(board_t* board);

int parse_pacman_file(board_t* board);

int parse_ghost_file(board_t* board, int ghost_idx);

#endif