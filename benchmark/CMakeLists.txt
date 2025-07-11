configure_file(${CMAKE_SOURCE_DIR}/wordle_targets.csv ${CMAKE_CURRENT_BINARY_DIR}/wordle_targets.csv COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/wordle_fillers.csv ${CMAKE_CURRENT_BINARY_DIR}/wordle_fillers.csv COPYONLY)

# Automatically collect only benchmark source files
file(GLOB BENCH_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(benchmarks ${BENCH_SOURCES})

target_include_directories(benchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

# Only link the core library. Ensure no bench_*.cpp defines functions also implemented in wordle_lib.
target_link_libraries(benchmarks
    PRIVATE
        wordle_lib
        benchmark::benchmark
        pthread
)

set_target_properties(benchmarks PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
