# Makefile for cterm

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -Iinclude
LDFLAGS = -lssl -lcrypto -lncurses

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = .

# Source files
SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/config.c \
          $(SRC_DIR)/network.c \
          $(SRC_DIR)/imap.c \
          $(SRC_DIR)/smtp.c \
          $(SRC_DIR)/ui.c

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

# Target executable
TARGET = $(BIN_DIR)/cterm

# Default target
all: $(TARGET)

# Create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)
	@echo "Build complete: $(TARGET)"

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	@echo "Clean complete"

# Install to /usr/local/bin
install: $(TARGET)
	install -m 0755 $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/cterm"

# Uninstall from /usr/local/bin
uninstall:
	rm -f /usr/local/bin/cterm
	@echo "Uninstalled from /usr/local/bin"

# Debug build
debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all

# Release build
release: CFLAGS += -O2 -DNDEBUG
release: clean all

.PHONY: all clean install uninstall debug release
