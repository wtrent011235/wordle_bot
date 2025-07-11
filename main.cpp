#include <chrono>
#include <cstring>
#include <cctype>
#include <iostream>
#include <numeric>

#include "src/easyBot.hpp"
#include "src/hardBot.hpp"

template <bool HardMode>
void statsImpl() {
    using Bot = std::conditional_t<HardMode, wordle::bot::HardBot, wordle::bot::EasyBot>;

    if constexpr (HardMode) {
        std::cout << "Hard Mode Stats: \n";
    } else {
        std::cout << "Easy Mode Stats: \n";
    }
    
    // Get first guess
    Bot bot{};
    const auto firstSuggestion = bot.suggest();
    wordle::guard::hybridGuard(firstSuggestion.isValid, "Unable to retrieve first guess");

    std::cout << "First guess: " << firstSuggestion.guess << " with 2-depth entropy " << firstSuggestion.entropy << "\n";

    // Simulate all games
    std::vector<size_t> games{};
    games.reserve(wordle::config::NUM_TARGETS);
    
    auto start = std::chrono::steady_clock::now();
    for (size_t solutionIndex = 0; solutionIndex < wordle::config::NUM_TARGETS; ++solutionIndex) {
        bot.reset();
        size_t guesses = 1;
        wordle::bot::Suggestion suggestion = firstSuggestion;
        const auto& fMapSlice = bot.getFMap()[solutionIndex];

        for (; suggestion.isValid && suggestion.guessIndex != solutionIndex && guesses < 100; ++guesses) {
            auto fbEncoding = fMapSlice[suggestion.guessIndex];
            bot.filter(suggestion.guessIndex, fbEncoding);
            suggestion = bot.suggest();
        }

        wordle::guard::runtimeGuard(suggestion.isValid, "Unable to find target {}", bot.getVocab()[solutionIndex]);
        wordle::guard::runtimeGuard(guesses < 100, "Failed to find {} within 100 guesses", bot.getVocab()[solutionIndex]);
        games.push_back(guesses);
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    std::cout << "Simulation Time: " << duration << " ms\n";

    double mean = std::accumulate(games.begin(), games.end(), 0.0) / static_cast<double>(wordle::config::NUM_TARGETS);
    auto winPred = [](size_t guesses) noexcept -> bool { return guesses <= 6; };
    size_t gamesWon = std::count_if(games.begin(), games.end(), winPred);
    double winProb = static_cast<double>(gamesWon) / static_cast<double>(wordle::config::NUM_TARGETS);

    std::cout << "Mean guesses: " << mean << "\n";
    std::cout << "Win Percentage: " << winProb * 100.0 << "%\n";
    std::cout << "Games lost: " << wordle::config::NUM_TARGETS - gamesWon << "\n";
}

inline void stats(std::string_view mode) {
    if (mode == "hard") {
        statsImpl<true>();
        return;
    }

    if (mode == "easy") {
        statsImpl<false>();
        return;
    }

    std::cerr << "Argument error: " << "please pass \"hard\" or \"easy\" (without quotations, case-sensitive)\n";
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Argument error: invalid command (see README.txt)\n";
        return 1;
    }

    std::string_view flagOne{argv[1]};

    if (flagOne == "stats") {
        stats(argv[2]);
        return 0;
    }

    std::cerr << "Argument error: invalid command (see README.txt)\n";
}