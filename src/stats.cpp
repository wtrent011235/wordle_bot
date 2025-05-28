#include "stats.hpp"

#include <chrono>
#include <iostream>
#include <numeric>

#include "bot.hpp"
#include "config.hpp"
#include "timing.hpp"

void bestStartingWord() {
    timing::Timer timer;

    // Time: Construct Bot
    timer.start();
    wordle::Bot<wordle::config::IS_HARD_MODE> bot;
    auto constructorDuration = timer.end(); timer.reset();
    std::cout << "Time to construct bot: " << constructorDuration.count() << " s\n";

    // Time: Suggest starting word (and find best starting word move candidates in ascending order
    timer.start();
    auto _firstSuggestion = bot.suggest();
    auto firstSuggestionDuration = timer.end(); timer.reset();
    std::cout << "Time to find first suggestion: " << firstSuggestionDuration.count() << " s\n";

    // Construct firstGuess candidates in descending order of first 2-depth entropy
    std::array<size_t, wordle::config::NUM_WORDS> candidateIndices;
    std::sort(bot.moves.begin(), bot.moves.end(), std::greater<>());
    for (size_t i = 0; i < candidateIndices.size(); ++i) {
        candidateIndices[i] = bot.moves[i].guessIndex;
    }
    wordle::util::formatGuard(_firstSuggestion.guessIndex == candidateIndices[0], "Expected {} as first candidate, got: {}", bot.vocab[_firstSuggestion.guessIndex], bot.vocab[candidateIndices[0]]);

    // Initialize containers for storing results
    std::vector<size_t> bestStarts;
    size_t minGuesses = std::numeric_limits<double>::max();
    constexpr int LOOP_GUARD = 200;

    // Time: Run sims to find best word
    timer.start();
    for (size_t i = 0; i < candidateIndices.size(); ++i) {
        size_t firstGuessIndex = candidateIndices[i];
        size_t totalGuesses = wordle::config::NUM_TARGETS;
        wordle::util::showProgressBar(i, candidateIndices.size());
        
        for (size_t solutionIndex = 0; solutionIndex < wordle::config::NUM_TARGETS && totalGuesses < minGuesses; ++solutionIndex) {
            bot.reset();

            auto feedback = bot.fMap(firstGuessIndex, solutionIndex);
            bot.update(firstGuessIndex, feedback);

            size_t loopIter = 0;
            for (; loopIter < LOOP_GUARD && bot.state == wordle::State::NOT_OVER; ++loopIter) {
                size_t guessIndex = bot.suggest().guessIndex;
                feedback = bot.fMap(guessIndex, solutionIndex);
                bot.update(guessIndex, feedback);
            }

            wordle::util::formatGuard(loopIter <= LOOP_GUARD, "First guess ({}) hit loop guard ({}) when simulating {}", bot.vocab[firstGuessIndex], LOOP_GUARD, bot.vocab[solutionIndex]);
            wordle::util::formatGuard(bot.state == wordle::State::WON, "First guess ({}) was unable to find solution {}", bot.vocab[firstGuessIndex], bot.vocab[solutionIndex]);
            totalGuesses += loopIter;
        }

        if (totalGuesses < minGuesses) {
            bestStarts.resize(1);
            bestStarts[0] = firstGuessIndex;
            minGuesses = totalGuesses;
        } else if (totalGuesses == minGuesses) {
            bestStarts.push_back(firstGuessIndex);
        }
    }
    auto simDuration = timer.end();

    std::cout << "Simulation time: " << simDuration.count() << " s\n";
    std::cout << "Best starting word(s): ";

    bool printedOne = false;
    for (size_t idx : bestStarts) {
        if (printedOne) {
            std::cout << ", ";
        } else {
            printedOne = true;
        }

        std::cout << bot.vocab[idx];
    }
    std::cout << std::endl;
}