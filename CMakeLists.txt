cmake_minimum_required(VERSION 3.20)
project(wordle_bot LANGUAGES CXX)

# Use C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings (recommended)
add_compile_options(-Wall -Wextra -Wpedantic)

# Find Boost (for header-only use)
find_package(Boost REQUIRED)

# Copy data files into the build directory
configure_file(${CMAKE_SOURCE_DIR}/wordle_targets.csv ${CMAKE_BINARY_DIR}/wordle_targets.csv COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/wordle_fillers.csv ${CMAKE_BINARY_DIR}/wordle_fillers.csv COPYONLY)

# Executable target
add_executable(wordle_bot
    main.cpp
    solver.cpp
    stats.cpp
)

# Include your own headers and Boost
target_include_directories(wordle_bot PRIVATE ${Boost_INCLUDE_DIRS} .)
