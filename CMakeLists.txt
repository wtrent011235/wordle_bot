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
    src/solver.cpp
    src/stats.cpp
)

# Include headers and Boost
target_include_directories(wordle_bot PRIVATE ${Boost_INCLUDE_DIRS} .)

# Add ImGui sources
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/glfw" ${CMAKE_PREFIX_PATH})


# Add GLFW and OpenGL dependencies
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(imgui PRIVATE glfw OpenGL::GL)

# Link imgui to your executable
target_link_libraries(wordle_bot PRIVATE imgui glfw OpenGL::GL)
target_include_directories(wordle_bot PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)



