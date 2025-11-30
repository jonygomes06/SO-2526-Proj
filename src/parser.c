#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include "parser.h"
#include "board.h"
#include "utils.h"

int parse_levels_directory(board_t* board) {
    char* dir_path = board->assets_dir;
    debug("Parsing levels in directory: %s\n", dir_path);
    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        perror("opendir failed");
        return -1;
    }
    
    struct dirent* entry;
    int lvl_files_count = 0;
    const char *extension = ".lvl";
    size_t ext_len = strlen(extension);

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        size_t entry_len = strlen(entry->d_name);

        if (entry_len < ext_len + 1)  continue;

        if (strcmp(entry->d_name + (entry_len - ext_len), extension) != 0) continue;

        size_t prefix_len = entry_len - ext_len;

        char prefix[prefix_len + 1];
        strncpy(prefix, entry->d_name, prefix_len);
        prefix[prefix_len] = '\0';

        if (prefix_len == 0) continue;

        int num_read;
        if (sscanf(prefix, "%d", &num_read) != 1) continue;

        lvl_files_count++;

        if (num_read > board->n_levels) {
            board->n_levels = num_read;
        }
    }
    closedir(dir);

    if (lvl_files_count != board->n_levels) {
        debug("Warning: Number of level files found (%d) does not match highest level index (%d)\n", lvl_files_count, board->n_levels);
    }

    debug("Total levels found: %d\n", board->n_levels);

    board->current_level = 1;

    return 0;
}


int parse_level_file(board_t* board) {
    const char* filepath = board->level_file;

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("Error: Could not open level file.\n");
        return -1;
    }
    
    board->width = 0;
    board->height = 0;
    board->n_ghosts = 0;
    board->n_pacmans = 1;
    board->pacmans = calloc(board->n_pacmans, sizeof(pacman_t));
    board->ghosts = NULL;
    board->board = NULL;
    board->pacman_file[0] = '\0';            // Default Pacman file to empty in case it's manual

    char line_buffer[LINE_BUFFER_SIZE];
    int map_cell_index = 0; 

    while (read_line(fd, line_buffer, LINE_BUFFER_SIZE) > 0) {
        if (strlen(line_buffer) == 0) continue;
        if (line_buffer[0] == '#') continue;

        char line_work[LINE_BUFFER_SIZE]; 
        strcpy(line_work, line_buffer);      // Copy for tokenization because strtok modifies the string

        char* token = strtok(line_work, " \t\r");
        if (token == NULL) continue;

        if (strcmp(token, "DIM") == 0) {
            char* h_str = strtok(NULL, " \t\r");
            char* w_str = strtok(NULL, " \t\r");
            if (w_str && h_str) {
                board->height = atoi(h_str);
                board->width = atoi(w_str);
                
                board->board = calloc(board->width * board->height, sizeof(board_pos_t));
            }
        }
        else if (strcmp(token, "TEMPO") == 0) {
            char *t_str = strtok(NULL, " \t\r");
            if (t_str) board->tempo = atoi(t_str);
        }
        else if (strcmp(token, "PAC") == 0) {
            char *p_file = strtok(NULL, " \t\r");
            if (p_file) {
                snprintf(board->pacman_file, MAX_FILENAME, "%s%s", board->assets_dir, p_file);
            }
        }
        else if (strcmp(token, "MON") == 0) {
            char *m_file = strtok(NULL, " \t\r");
            
            while (m_file != NULL && board->n_ghosts < MAX_GHOSTS) {
                snprintf(board->ghosts_files[board->n_ghosts], MAX_FILENAME, "%s%s", board->assets_dir, m_file);
                board->n_ghosts++;
                m_file = strtok(NULL, " \t\r");
            }
            
            if (board->n_ghosts > 0) {
                board->ghosts = calloc(board->n_ghosts, sizeof(ghost_t));
            }
        }
        else {
            // --- Map Data Processing ---
            if (board->board != NULL) {
                int len = strlen(line_buffer);
                for (int i = 0; i < len; i++) {
                    char c = line_buffer[i];
                    
                    // Stop if we filled the board
                    if (map_cell_index >= board->width * board->height) break;

                    // Map characters to board_pos_t
                    if (c == 'X' || c == 'o' || c == '@') {
                        board_pos_t *pos = &board->board[map_cell_index];
                        
                        pos->has_dot = 0; 
                        pos->has_portal = 0;
                        pos->content = ' '; 

                        if (c == 'X') {
                            pos->content = 'W';
                        } 
                        else if (c == '@') {
                            pos->has_portal = 1;
                            pos->content = ' '; 
                        } 
                        else if (c == 'o') {
                            pos->has_dot = 1;
                            pos->content = ' ';
                        }
                        
                        map_cell_index++;
                    }
                }
            }
        }
    }

    close(fd);
    
    if (board->board == NULL) {
        perror("Error: Board dimensions not found or allocation failed.\n");
        return -1;
    }

    return 0;
}


int parse_pacman_file(board_t* board) {
    const char* filepath = board->pacman_file;
    debug("Parsing pacman file: %s\n", filepath);

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("Error: Could not open pacman file");
        return -1;
    }

    pacman_t* pacman = &board->pacmans[0];

    char line_buffer[1024];
    char line_work[1024];

    while (read_line(fd, line_buffer, 1024) > 0) {
        if (strlen(line_buffer) == 0) continue;
        if (line_buffer[0] == '#') continue;

        strcpy(line_work, line_buffer);
        char* token = strtok(line_work, " \t\r\n");
        if (token == NULL) continue;

        // --- DIRECTIVES ---
        if (strcmp(token, "PASSO") == 0) {
            char* val = strtok(NULL, " \t\r\n");
            if (val) pacman->passo = atoi(val);
        }
        else if (strcmp(token, "POS") == 0) {
            char* row = strtok(NULL, " \t\r\n");
            char* col = strtok(NULL, " \t\r\n");
            if (row && col) {
                pacman->pos_y = atoi(row); // Line is Y
                pacman->pos_x = atoi(col); // Column is X
            }
        }
        // --- COMMANDS ---
        else {
            if (pacman->n_moves < MAX_MOVES) {
                command_t* cmd = &pacman->moves[pacman->n_moves];
                
                cmd->command = token[0]; // 'A', 'W', 'S', etc.
                cmd->turns = 0;
                cmd->turns_left = 0;

                // Handle 'T' (Wait) argument
                if (cmd->command == 'T') {
                    char* arg = strtok(NULL, " \t\r\n");
                    if (arg) {
                        cmd->turns = atoi(arg);
                        cmd->turns_left = cmd->turns;
                    }
                }
                pacman->n_moves++;
            }
        }
    }

    close(fd);
    return 0;
}


int parse_ghost_file(board_t* board, int ghost_idx) {
    const char* filepath = board->ghosts_files[ghost_idx];
    debug("Parsing ghost file: %s\n", filepath);

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("Error: Could not open ghost file");
        return -1;
    }

    ghost_t* ghost = &board->ghosts[ghost_idx];

    char line_buffer[1024];
    char line_work[1024];

    while (read_line(fd, line_buffer, 1024) > 0) {
        if (strlen(line_buffer) == 0) continue;
        if (line_buffer[0] == '#') continue;

        strcpy(line_work, line_buffer);
        char* token = strtok(line_work, " \t\r\n");
        if (token == NULL) continue;

        // --- DIRECTIVES ---
        if (strcmp(token, "PASSO") == 0) {
            char* val = strtok(NULL, " \t\r\n");
            if (val) ghost->passo = atoi(val);
        }
        else if (strcmp(token, "POS") == 0) {
            char* row = strtok(NULL, " \t\r\n");
            char* col = strtok(NULL, " \t\r\n");
            if (row && col) {
                ghost->pos_y = atoi(row); // Line is Y
                ghost->pos_x = atoi(col); // Column is X
            }
        }
        // --- COMMANDS ---
        else {
            if (ghost->n_moves < MAX_MOVES) {
                command_t* cmd = &ghost->moves[ghost->n_moves];
                
                cmd->command = token[0]; // 'A', 'W', 'S', etc.
                cmd->turns = 0;
                cmd->turns_left = 0;

                // Handle 'T' (Wait) argument
                if (cmd->command == 'T') {
                    char* arg = strtok(NULL, " \t\r\n");
                    if (arg) {
                        cmd->turns = atoi(arg);
                        cmd->turns_left = cmd->turns;
                    }
                }
                ghost->n_moves++;
            }
        }
    }

    close(fd);
    return 0;
}
