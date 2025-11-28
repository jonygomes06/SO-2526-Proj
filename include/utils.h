#ifndef UTILS_H
#define UTILS_H

#include "board.h"

/*Reads a line from file descriptor 'fd' into 'buffer' up to 'max_len' characters.
  Returns the number of characters read, 0 on EOF, or -1 on error.*/
int read_line(int fd, char *buffer, int max_len);

/*Makes the current thread sleep for 'int milliseconds' miliseconds*/
void sleep_ms(int milliseconds);

// DEBUG FILE

/*Opens the debug file*/
void open_debug_file(char *filename);

/*Closes the debug file*/
void close_debug_file();

/*Writes to the open debug file*/
void debug(const char * format, ...);

/*Writes the board and its contents to the open debug file*/
void print_board(board_t* board);

#endif