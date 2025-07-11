cmake_minimum_required(VERSION 3.20)
project(wordle_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
  -O3
  -march=native
  -flto
  -Wall -Wextra -Wpedantic -Werror
)

add_link_options(
  -flto
)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Specify Boost path for Brew installations
cmake_policy(SET CMP0144 NEW)
set(BOOST_ROOT /opt/homebrew)
find_package(Boost REQUIRED)

configure_file(${CMAKE_SOURCE_DIR}/wordle_targets.csv ${CMAKE_BINARY_DIR}/wordle_targets.csv COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/wordle_fillers.csv ${CMAKE_BINARY_DIR}/wordle_fillers.csv COPYONLY)

# Core library — source files compiled once here only
add_library(wordle_lib
  ${SRC_DIR}/easyBot.cpp
  ${SRC_DIR}/feedback.cpp
  ${SRC_DIR}/hardBot.cpp
)

target_include_directories(wordle_lib PUBLIC
  ${SRC_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${Boost_INCLUDE_DIRS}
)

add_executable(wordle_bot main.cpp)
target_link_libraries(wordle_bot PRIVATE wordle_lib)

find_package(Catch2 3 REQUIRED)

enable_testing()
add_subdirectory(tests)

option(BUILD_BENCHMARKS "Build benchmark targets" ON)
if (BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    add_subdirectory(benchmark)
endif()
