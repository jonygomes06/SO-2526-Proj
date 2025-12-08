#include "board.h"
#include "utils.h"
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/wait.h>


FILE * debugfile;

int read_line(int fd, char *buffer, int max_len) {
    int n_read = 0;
    char c;
    int status;

    while (n_read < max_len - 1) {
        status = read(fd, &c, 1);
        
        if (status == 0) {
            if (n_read == 0) return 0; 
            break;
        }
        if (status < 0) return -1;

        if (c == '\n') break;

        buffer[n_read++] = c;
    }
    buffer[n_read] = '\0';
    return n_read;
}

void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int create_backup(board_t* board) {
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
        board->is_backup_instance = 0;
        wait(&board->level_result); // Wait for child to finish and get its exit status
        debug("Pacman restored from backup.\n");
    } else {
        // Child process - Backup instance
        board->is_backup_instance = 1;
    }

    return 0;
}

void open_debug_file(char *filename) {
    debugfile = fopen(filename, "w");
}

void close_debug_file() {
    fclose(debugfile);
}

void debug(const char * format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(debugfile, format, args);
    va_end(args);

    fflush(debugfile);
}

void print_board(board_t *board) {
    if (!board || !board->board) {
        debug("[%d] Board is empty or not initialized.\n", getpid());
        return;
    }

    // Large buffer to accumulate the whole output
    char buffer[8192];
    size_t offset = 0;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "=== [%d] LEVEL INFO ===\n"
                       "Dimensions: %d x %d\n"
                       "Tempo: %d\n"
                       "Pacman file: %s\n",
                       getpid(), board->height, board->width, board->tempo, board->pacman_file);

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "Monster files (%d):\n", board->n_ghosts);

    for (int i = 0; i < board->n_ghosts; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "  - %s\n", board->ghosts_files[i]);
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n=== BOARD ===\n");

    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            int idx = y * board->width + x;
            if (offset < sizeof(buffer) - 2) {
                buffer[offset++] = board->board[idx].content;
            }
        }
        if (offset < sizeof(buffer) - 2) {
            buffer[offset++] = '\n';
        }
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "==================\n");

    buffer[offset] = '\0';

    debug("%s", buffer);
}
