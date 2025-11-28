# ==========================================
#   Pacmanist Project Makefile
# ==========================================

# --- Variables ---
CC        := gcc
CFLAGS    := -g -Wall -Wextra -Werror -std=c17 -D_POSIX_C_SOURCE=200809L
# Automatic dependency generation flags
DEPFLAGS  := -MMD -MP
LDFLAGS   := -lncurses
INCLUDES  := -Iinclude

# --- Directories ---
SRC_DIR   := src
OBJ_DIR   := obj
BIN_DIR   := bin

# --- Targets ---
TARGET_NAME := Pacmanist
TARGET      := $(BIN_DIR)/$(TARGET_NAME)

# --- Files ---
# Find all .c files in src directory automatically
SRCS      := $(wildcard $(SRC_DIR)/*.c)
# Create a list of .o files based on .c files, but in the obj dir
OBJS      := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
# Define dependency files (.d) corresponding to objects
DEPS      := $(OBJS:.o=.d)

# ==========================================
#   Rules
# ==========================================

.PHONY: all clean run

all: $(TARGET)

# Link the executable
# LDFLAGS is usually best placed at the end for Linux linkers
$(TARGET): $(OBJS) | $(BIN_DIR)
	@echo "Linking $(TARGET_NAME)..."
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# Compile source files into object files
# The | $(OBJ_DIR) ensures the folder exists before compiling
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) $(DEPFLAGS) -c $< -o $@

# Create directories if they don't exist
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# Run the game
run: all
	@echo "Running $(TARGET_NAME)..."
	@./$(TARGET) $(ARGS)

# Clean up build artifacts
clean:
	@echo "Cleaning..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR) *.log

# Include automatically generated dependencies
-include $(DEPS)