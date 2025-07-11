#pragma once

#include "vocab.hpp"
#include "feedback.hpp"
#include "parallelTaskQueue.hpp"
#include "vocab.hpp"

namespace wordle::bot {
    using WordCountT = boost::uint_t<std::bit_width(config::NUM_WORDS - 1)>::least;

    struct Suggestion {
        double entropy = std::numeric_limits<double>::min();
        std::string_view guess;
        size_t guessIndex;
        bool isValid = false;
    };

    enum class FilterFlag: uint8_t { INVALID_GUESS_AND_FEEDBACK = 0, INVALID_FEEDBACK, INVALID_GUESS, VALID };

    namespace concepts {
        template <typename T>
        concept SizeType = std::same_as<T, size_t> || std::same_as<T, WordCountT>;

        template <typename It>
        concept WordIndexIterator = std::random_access_iterator<It> && SizeType<std::iter_value_t<It>>;
    }


    struct BotBase {
    protected:
        using BinCounts = std::array<WordCountT, wordle::feedback::NUM_FEEDBACKS>;
        using WordIndexBins = std::vector<std::vector<WordCountT>>;

        struct GuessValidation {
            size_t index;
            bool isValid;
        };
        
        wordle::feedback::FeedbackMap fMap;
        wordle::vocab::Vocab vocab; 
        wordle::parallel::TaskQueue taskQueue;
        
        BotBase(size_t maxThreads = wordle::config::HARDWARE_CONCURRENCY) :
        vocab{wordle::vocab::constructVocab()},
        taskQueue{maxThreads} {
            this->fMap = wordle::feedback::constructFeedbackMap(vocab, taskQueue, maxThreads);
        }

        ~BotBase() = default;


        template <concepts::WordIndexIterator TargetIndexIterator>
        double baseEntropy(size_t guessIndex, TargetIndexIterator start, TargetIndexIterator stop, BinCounts& binCounts) {
            const size_t N = std::distance(start, stop);
            if (N <= 2) return std::max(0.0, static_cast<double>(N) - 1.0);
            // Clear bin counts
            binCounts.fill(0);

            // Get bin counts
            const auto& guessSlice = fMap[guessIndex];
            for (auto it = start; it != stop; ++it) {
                size_t targetIndex = *it;
                size_t fbIndex = guessSlice[targetIndex];
                ++binCounts[fbIndex];
            }

            // Calculate Entropy of guess
            double entropy = 0;
            for (double count : binCounts) {
                if (count > 0) {
                    double prob = count / N;
                    entropy -= prob * std::log2(prob);
                }
            }
            return entropy;
        }

    };
} // namespace wordle::bot