cmake_minimum_required(VERSION 3.10)
project(cterm C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.c
    src/config.c
    src/network.c
    src/imap.c
    src/smtp.c
    src/ui.c
)

# Find required libraries
find_package(OpenSSL REQUIRED)
find_package(Curses REQUIRED)

# Executable
add_executable(cterm ${SOURCES})

# Link libraries
target_link_libraries(cterm
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CURSES_LIBRARIES}
)

# Include directories for ncurses
target_include_directories(cterm PRIVATE ${CURSES_INCLUDE_DIRS})

# Install target
install(TARGETS cterm DESTINATION bin)
