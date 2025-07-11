#pragma once

#include "config.hpp"
#include "parallelTaskQueue.hpp"
#include "util.hpp"
#include "vocab.hpp"

namespace wordle::feedback::position {
    constexpr inline char EXHAUSTED = '_';       // Letter is not at this position and we know how many occurences of it exist in the word
    constexpr inline char WRONG_POSITION = 'x';  // Letter is in word, but is at wrong position
    constexpr inline char CORRECT = 'X';         // Letter is in word at correct position

}  // namespace wordle::feedback::position

namespace wordle::feedback {
    constexpr inline size_t NUM_FEEDBACKS = wordle::util::constevalPow(3ul, config::WORD_LENGTH);  // Number of unique wordle feedbacks
    using Encoding = boost::uint_t<std::bit_width(NUM_FEEDBACKS - 1)>::least;                      // Smallest type that can represent all unique wordle feedbacks
    using FeedbackMap = std::vector<std::vector<Encoding>>;
    using FlatFeedbackMap = std::vector<Encoding>;

    // Variations of function implementations for Encoder
    namespace __impl {
        constexpr inline Encoding EXHAUSTED = 0;
        constexpr inline Encoding WRONG_POSITION = 1;
        constexpr inline Encoding CORRECT = 2;
        constexpr inline Encoding BASE = 3;

        // Returns mulipliers for Encoding
        consteval inline std::array<Encoding, config::WORD_LENGTH> initMultipliers() {
            std::array<Encoding, config::WORD_LENGTH> multipliers{};
            Encoding multiplier = 1;
            for (size_t i = 0; i < multipliers.size(); ++i) {
                multipliers[i] = multiplier;
                multiplier *= BASE;
            }
            return multipliers;
        }

        // Multipliers for encoding
        constexpr inline std::array<Encoding, config::WORD_LENGTH> multipliers = initMultipliers();

        // Returns 0 (currently), but just adding in case I change encoding values for robustness
        consteval inline Encoding encodeAsExhausted() {
            Encoding encoding = 0;
            for (size_t i = 0; i < multipliers.size(); ++i) {
                encoding += EXHAUSTED * multipliers[i];
            }
            return encoding;
        }

        struct ArrEncoder {
        protected:
            using Count = boost::uint_t<std::bit_width(wordle::config::WORD_LENGTH)>::least;
            std::array<Count, wordle::config::ALPHABET_SIZE> unmatchedCounts{};

            [[nodiscard]] constexpr Count& getCount(char letter) {
                return unmatchedCounts[letter - 'a'];
            }

            [[nodiscard]] constexpr const Count& getCount(char letter) const {
                return unmatchedCounts[letter - 'a'];
            }

            constexpr void resetUnmatchedCounts() {
                unmatchedCounts.fill(0);
            }

        public:
            constexpr ArrEncoder() noexcept = default;
            ~ArrEncoder() noexcept = default;

            [[nodiscard]] Encoding operator()(std::string_view guess, std::string_view solution) {
                Encoding encoding = encodeAsExhausted();
                resetUnmatchedCounts();  // Reset counts

                // Step 1: Add CORRECT to encoding and update solution counts
                for (size_t i = 0; i < config::WORD_LENGTH; ++i) {
                    char solutionLetter = solution[i];
                    bool isCorrect = guess[i] == solutionLetter;
                    encoding += (CORRECT * multipliers[i] * static_cast<Encoding>(isCorrect));
                    getCount(solutionLetter) += static_cast<Count>(!isCorrect);
                }
                
                // Step 2: Add WRONG_POSITION to encoding and update solution counts
                for (size_t i = 0; i < config::WORD_LENGTH; ++i) {
                    char guessLetter = guess[i];
                    Count& count = getCount(guessLetter);
                    bool isWrongPosition = (solution[i] != guessLetter) && (count > 0);
                    encoding += (WRONG_POSITION * multipliers[i] * static_cast<Encoding>(isWrongPosition));
                    getCount(guessLetter) -= static_cast<Count>(isWrongPosition);
                }
                return encoding;
            }
        };

        [[nodiscard]] constexpr inline Encoding encodeFeedbackString(std::string_view fbString) {
            wordle::guard::hybridGuard(fbString.size() == wordle::config::WORD_LENGTH, "fbString is not expected size");
            Encoding encoding = 0;
            
            for (size_t i = 0; i < wordle::config::WORD_LENGTH; ++i) {
                Encoding pf;
                switch (fbString[i]) {
                    case (wordle::feedback::position::CORRECT) : pf = CORRECT; break;
                    case (wordle::feedback::position::EXHAUSTED) : pf = EXHAUSTED; break;
                    case (wordle::feedback::position::WRONG_POSITION) : pf = WRONG_POSITION; break;
                    default : wordle::guard::hybridError("invalid character encountered in fbString");
                }
                encoding += (pf * multipliers[i]);
            }
            return encoding;
        }

        
    }  // namespace __impl

    constexpr inline bool isValidFeedbackString(std::string_view fbString) {
        if (fbString.size() != wordle::config::WORD_LENGTH) return false;
        auto pred = [](char c) noexcept -> bool { return c == position::CORRECT || c == position::EXHAUSTED || c == position::WRONG_POSITION; };
        return std::all_of(fbString.begin(), fbString.end(), pred);
    }

    FeedbackMap constructFeedbackMapBasic(const wordle::vocab::Vocab& vocab);
    FeedbackMap constructFeedbackMap(const wordle::vocab::Vocab& vocab, wordle::parallel::TaskQueue& queue, size_t numThreads = wordle::config::HARDWARE_CONCURRENCY);
    FlatFeedbackMap constructFlatFeedbackMap(const wordle::vocab::Vocab& vocab, wordle::parallel::TaskQueue& queue, size_t numThreads = wordle::config::HARDWARE_CONCURRENCY);
    using Encoder = __impl::ArrEncoder;
    inline auto encodeFeedbackString = __impl::encodeFeedbackString;
    
}