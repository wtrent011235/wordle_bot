#pragma once

#include <cstddef>
namespace wordle::config {

    constexpr inline auto TARGET_FILE = "wordle_targets.csv";
    constexpr inline auto FILLER_FILE = "wordle_fillers.csv";
    constexpr inline size_t ALPHABET_SIZE = 'z' - 'a' + 1;
    constexpr inline size_t WORD_LENGTH = 5;
    constexpr inline size_t NUM_TARGETS = 2315;
    constexpr inline size_t NUM_FILLERS = 10657;
    constexpr inline size_t NUM_WORDS = NUM_TARGETS + NUM_FILLERS;
    constexpr inline size_t CACHE_LINE_SIZE = 128;       // On my architecture, cache line size is 128
    constexpr inline size_t HARDWARE_CONCURRENCY = 8ul;  // I have 8 cores on my machine
    constexpr inline bool IS_HARD_MODE = true;
}

