configure_file(${CMAKE_SOURCE_DIR}/wordle_targets.csv ${CMAKE_CURRENT_BINARY_DIR}/wordle_targets.csv COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/wordle_fillers.csv ${CMAKE_CURRENT_BINARY_DIR}/wordle_fillers.csv COPYONLY)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(run_tests ${TEST_SOURCES})

target_include_directories(run_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

# Link to shared code
target_link_libraries(run_tests PRIVATE
    wordle_lib
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)

catch_discover_tests(run_tests
    TEST_PREFIX wordle::
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
