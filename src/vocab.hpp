#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <thread>

#include "config.hpp"
#include "guard.hpp"

namespace wordle::vocab {

using Vocab = std::vector<std::string>;

namespace __impl {
    inline Vocab processFile(std::string_view fileName, size_t numWords) {
    // Open file
    std::ifstream file{fileName};
    if (!file) guard::formatError("failed to open {}", fileName);

    // Reserve space for words
    Vocab words;
    words.reserve(numWords);

    // Add processed words from file
    std::string buff;
    while (std::getline(file, buff)) {
        // Trim in-place
        auto first = buff.find_first_not_of(" \t\r\n");
        auto last = buff.find_last_not_of(" \t\r\n");
        if (first == std::string::npos || last == std::string::npos) continue;
        buff = buff.substr(first, last - first + 1);

        // Lowercase in-place
        for (char& c : buff) {
            char letter = c;
            if ('A' <= letter && letter <= 'Z') c += ('a' - 'A');
        }
        words.push_back(std::move(buff));
    }
    return words;
}
}


inline Vocab constructVocab() {
    // Get targets and fillers
    auto targets = __impl::processFile(config::TARGET_FILE, config::NUM_TARGETS);
    auto fillers = __impl::processFile(config::FILLER_FILE, config::NUM_FILLERS);

    // Form result
    Vocab vocab;
    vocab.reserve(targets.size() + fillers.size());

    vocab.insert(vocab.end(), std::make_move_iterator(targets.begin()), std::make_move_iterator(targets.end()));
    vocab.insert(vocab.end(), std::make_move_iterator(fillers.begin()), std::make_move_iterator(fillers.end()));
    return vocab;
}

}