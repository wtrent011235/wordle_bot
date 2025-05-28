#include "solver.hpp"

#include "bot.hpp"

inline void printFeedbackKey() noexcept {
    std::cout << "Feedback Key (case-sensitive):\n"
              << "X: correct letter in the correct location\n"
              << "O: correct letter in the wrong location\n"
              << "_: incorrect letter\n"
              << "-----------------------------------------\n";
}

inline void getFeedback(std::string& buff) {
    while (true) {
        std::cout << "Please enter feedback: ";
        std::cin >> buff;

        if (buff.size() != wordle::config::WORD_LENGTH) {
            std::cout << std::format("Please enter {} feedback characters (X, O, or _)\n", wordle::config::WORD_LENGTH);
            continue;
        }
        for (char c : buff) {
            if (c != 'X' && c != 'O' && c != '_') {
                std::cout << std::format("Invalid feedback character provided: {}\n", c);
                printFeedbackKey();
                continue;
            }
        }
        break;
    }
}

void solve() {
    std::cout << "Loading Wordle Bot ";
    if constexpr (wordle::config::IS_HARD_MODE) {
        std::cout << "(hard mode)";
    } else {
        std::cout << "(easy mode)";
    }
    std::cout << "\n\n";

    wordle::Bot<wordle::config::IS_HARD_MODE> bot;
    std::string buff;
    auto firstGuessInfo = bot.findWordIndex("slate");
    if (!firstGuessInfo.valid) throw std::runtime_error("Failed to find first guess index");

    while (true) { 
        size_t guessIndex = firstGuessInfo.index;
        printFeedbackKey();


        while (bot.state == wordle::State::NOT_OVER) {
            std::cout << std::format("Best guess: {}. Number of solutions remaining: {}\n", bot.vocab[guessIndex], bot.validTargetIndices.size());
            getFeedback(buff);
            bot.update(guessIndex, wordle::FeedbackEncoder::encode(buff));
            guessIndex = bot.suggest().guessIndex;
        }

        if (bot.state == wordle::State::WON) {
            std::cout << "Found word! ";
        } else {
            std::cout << "Unable to find word... ";
        }
        std::cout << "Play again? (Enter y for yes, anything else for no): ";
        std::cin >> buff;
        if (buff.size() != 1 || buff[0] != 'y') break;
        bot.reset();
    }
    
}