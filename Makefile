# Makefile for cterm

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -Iinclude
LDFLAGS = -lssl -lcrypto -lncurses

# GTK+ flags
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LDFLAGS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = .

# Common source files
COMMON_SOURCES = $(SRC_DIR)/config.c \
                 $(SRC_DIR)/network.c \
                 $(SRC_DIR)/imap.c \
                 $(SRC_DIR)/smtp.c

# TUI-specific sources
TUI_SOURCES = $(SRC_DIR)/main.c \
              $(SRC_DIR)/ui.c

# GUI-specific sources
GUI_SOURCES = $(SRC_DIR)/main_gui.c \
              $(SRC_DIR)/gui.c

# Object files
COMMON_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(COMMON_SOURCES))
TUI_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(TUI_SOURCES))
GUI_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/gui_%.o,$(GUI_SOURCES))

# Target executables
TARGET_TUI = $(BIN_DIR)/cterm
TARGET_GUI = $(BIN_DIR)/cterm-gui

# Default target - build both
all: $(TARGET_TUI) $(TARGET_GUI)

# Build only TUI version
tui: $(TARGET_TUI)

# Build only GUI version
gui: $(TARGET_GUI)

# Create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile common source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile GUI source files with GTK+ flags
$(OBJ_DIR)/gui_%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

# Link TUI executable
$(TARGET_TUI): $(COMMON_OBJECTS) $(TUI_OBJECTS)
	$(CC) $(COMMON_OBJECTS) $(TUI_OBJECTS) $(LDFLAGS) -o $(TARGET_TUI)
	@echo "Build complete: $(TARGET_TUI)"

# Link GUI executable
$(TARGET_GUI): $(COMMON_OBJECTS) $(GUI_OBJECTS)
	@if [ -z "$(GTK_LDFLAGS)" ]; then \
		echo "Error: GTK+3 development files not found. Install with:"; \
		echo "  Debian/Ubuntu: sudo apt-get install libgtk-3-dev"; \
		echo "  Fedora/RHEL:   sudo dnf install gtk3-devel"; \
		echo "  Arch Linux:    sudo pacman -S gtk3"; \
		exit 1; \
	fi
	$(CC) $(COMMON_OBJECTS) $(GUI_OBJECTS) -lssl -lcrypto $(GTK_LDFLAGS) -o $(TARGET_GUI)
	@echo "Build complete: $(TARGET_GUI)"

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(TARGET_TUI) $(TARGET_GUI)
	@echo "Clean complete"

# Install to /usr/local/bin
install: all
	install -m 0755 $(TARGET_TUI) /usr/local/bin/
	install -m 0755 $(TARGET_GUI) /usr/local/bin/
	@echo "Installed to /usr/local/bin/"

# Uninstall from /usr/local/bin
uninstall:
	rm -f /usr/local/bin/cterm /usr/local/bin/cterm-gui
	@echo "Uninstalled from /usr/local/bin"

# Debug build
debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all

# Release build
release: CFLAGS += -O2 -DNDEBUG
release: clean all

.PHONY: all clean install uninstall debug release
