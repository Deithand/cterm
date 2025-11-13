# Makefile for cterm

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -Iinclude
LDFLAGS = -lssl -lcrypto
NCURSES_LDFLAGS = -lncurses
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null || echo "")
GTK_LDFLAGS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null || echo "")

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = .

# Common source files
COMMON_SOURCES = $(SRC_DIR)/config.c \
                 $(SRC_DIR)/network.c \
                 $(SRC_DIR)/imap.c \
                 $(SRC_DIR)/smtp.c \
                 $(SRC_DIR)/addressbook.c \
                 $(SRC_DIR)/html_parser.c

# ncurses TUI sources
NCURSES_SOURCES = $(COMMON_SOURCES) \
                  $(SRC_DIR)/main.c \
                  $(SRC_DIR)/ui.c

# GTK GUI sources
GTK_SOURCES = $(COMMON_SOURCES) \
              $(SRC_DIR)/gui.c

# Object files
NCURSES_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%-ncurses.o,$(NCURSES_SOURCES))
GTK_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%-gtk.o,$(GTK_SOURCES))

# Target executables
TARGET_NCURSES = $(BIN_DIR)/cterm
TARGET_GTK = $(BIN_DIR)/cterm-gui

# Default target - build both
all: ncurses

# Build ncurses version
ncurses: $(TARGET_NCURSES)

# Build GTK version (if GTK is available)
gui: check-gtk $(TARGET_GTK)

# Build both versions
both: ncurses gui

# Check if GTK is available
check-gtk:
	@if [ -z "$(GTK_CFLAGS)" ]; then \
		echo "Error: GTK+ 3.0 not found. Install libgtk-3-dev or gtk3-devel"; \
		exit 1; \
	fi

# Create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile ncurses object files
$(OBJ_DIR)/%-ncurses.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile GTK object files
$(OBJ_DIR)/%-gtk.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -DUSE_GTK -c $< -o $@

# Need separate main for GTK
$(OBJ_DIR)/main-gtk.o: $(SRC_DIR)/main_gui.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -DUSE_GTK -c $< -o $@

# Link ncurses version
$(TARGET_NCURSES): $(NCURSES_OBJECTS)
	$(CC) $(NCURSES_OBJECTS) $(LDFLAGS) $(NCURSES_LDFLAGS) -o $(TARGET_NCURSES)
	@echo "Build complete: $(TARGET_NCURSES)"

# Link GTK version
$(TARGET_GTK): $(GTK_OBJECTS) $(OBJ_DIR)/main-gtk.o
	$(CC) $(GTK_OBJECTS) $(OBJ_DIR)/main-gtk.o $(LDFLAGS) $(GTK_LDFLAGS) -o $(TARGET_GTK)
	@echo "Build complete: $(TARGET_GTK)"

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(TARGET_NCURSES) $(TARGET_GTK)
	@echo "Clean complete"

# Install to /usr/local/bin
install: $(TARGET_NCURSES)
	install -m 0755 $(TARGET_NCURSES) /usr/local/bin/
	@if [ -f $(TARGET_GTK) ]; then \
		install -m 0755 $(TARGET_GTK) /usr/local/bin/; \
		echo "Installed both cterm and cterm-gui to /usr/local/bin"; \
	else \
		echo "Installed cterm to /usr/local/bin"; \
	fi

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

.PHONY: all ncurses gui both check-gtk clean install uninstall debug release
