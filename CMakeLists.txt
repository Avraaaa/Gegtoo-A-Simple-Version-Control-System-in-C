cmake_minimum_required(VERSION 3.10)
project(gegtoo C)

set(CMAKE_C_STANDARD 99)

# Include header files
include_directories(include)

# Find all source files
file(GLOB SOURCES
    src/*.c
    src/core/*.c
    src/commands/*.c
    src/utils/*.c
)

# Create the executable
add_executable(geg ${SOURCES})
