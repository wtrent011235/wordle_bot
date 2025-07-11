#pragma once

#include <numeric>

#include "botBase.hpp"

namespace wordle::bot {

class EasyBot : private BotBase {
    friend struct EasyBotInspector;

    std::vector<double> entropies;
    std::vector<WordCountT> aliveTargets{};
    std::vector<WordCountT> topCandidates;
    const size_t beamCandidates;
    const size_t maxThreads;

    BotBase::WordIndexBins getCandidateTargets(size_t candidateIndex, BotBase::BinCounts& binCounts) {
        binCounts.fill(0);
        const auto& candidateSlice = fMap[candidateIndex];

        // Step 1: Count number of targets in corresponding feedback bin
        for (size_t solutionIndex : aliveTargets) {
            size_t fbIndex = candidateSlice[solutionIndex];
            ++binCounts[fbIndex];
        }

        // Step 2: Initialize output and reserve correct size for bins
        BotBase::WordIndexBins output(wordle::feedback::NUM_FEEDBACKS);
        for (size_t i = 0; i < wordle::feedback::NUM_FEEDBACKS; ++i) {
            output[i].reserve(binCounts[i]);
        }

        // Step 3: Push indices into targets
        for (WordCountT solutionIndex : aliveTargets) {
            size_t fbIndex = candidateSlice[solutionIndex];
            output[fbIndex].push_back(solutionIndex);
        }
        return output;
    }

    BotBase::GuessValidation validateGuess(std::string_view guess) {
        GuessValidation gv{0, false};
        if (!util::isValidWord(guess)) return gv;

        // Binary search for word
        auto search = [&](bool searchTargets) { 
            size_t left = searchTargets ? 0 : wordle::config::NUM_TARGETS;
            size_t right = searchTargets ? wordle::config::NUM_TARGETS : wordle::config::NUM_WORDS;

            while (left <= right) {
                size_t middle = (left + right) / 2;
                std::string_view candidate = vocab[middle];

                bool found = true;
                for (size_t i = 0; i < wordle::config::WORD_LENGTH; ++i) {
                    char gChar = guess[i];
                    char cChar = candidate[i];

                    if (gChar == cChar) continue;
                    found = false;
                    if (cChar < gChar) {
                        left = middle + 1;
                    } else {
                        right = middle - 1;
                    }
                }

                if (!found) continue;

                // NOTE: Atomic not necessary due to unique targets and fillers
                gv.index = middle;
                gv.isValid = true;
                return;
            }
         };

        taskQueue.push(search, true);
        taskQueue.push(search, false);
        taskQueue.wait();
        return gv;
    }

    template <concepts::WordIndexIterator TargetIterator>
    double binEntropy(TargetIterator tStart, TargetIterator tEnd, BotBase::BinCounts& binCounts) {
        const size_t N = std::distance(tStart, tEnd);
        if (N <= 2) {
            return std::max(0.0, static_cast<double>(N) - 1.0);
        }

        // Resulting max entropy
        double bestEntropy = std::numeric_limits<double>::min();

        // Find entropy of each possible guess, compare with best
        for (size_t guessIndex = 0; guessIndex < wordle::config::NUM_WORDS; ++guessIndex) {
            double entropy = BotBase::baseEntropy(guessIndex, tStart, tEnd, binCounts);
            bestEntropy = std::max(bestEntropy, entropy);
        }
        return bestEntropy;
    }

public:
    EasyBot(size_t _maxThreads = config::HARDWARE_CONCURRENCY, size_t _beamCandidates = config::HARDWARE_CONCURRENCY)
    : BotBase{_maxThreads},
      entropies(config::NUM_WORDS),
      topCandidates(_beamCandidates),
      beamCandidates{_beamCandidates},
      maxThreads{_maxThreads} {
        reset();
    }

    void reset() {
        aliveTargets.resize(wordle::config::NUM_TARGETS);
        std::iota(aliveTargets.begin(), aliveTargets.end(), 0);
    }

    const auto& getFMap() const noexcept {
        return fMap;
    }

    const auto& getVocab() const noexcept {
        return vocab;
    }

    Suggestion suggest();

    void filter(size_t guessIndex, wordle::feedback::Encoding fbEncoding) {
        // Filter aliveTargets
        aliveTargets.erase(
            std::remove_if(aliveTargets.begin(), aliveTargets.end(),
                [guessIndex, fbEncoding, this](WordCountT i) { return this->fMap[i][guessIndex] != fbEncoding; }),
            aliveTargets.end()
        );
    }

    FilterFlag tryFilter(std::string_view guess, std::string_view fbString) {
        auto gv = validateGuess(guess);
        bool validFeedback = feedback::isValidFeedbackString(fbString);

        uint8_t flagUnderlying = (static_cast<uint8_t>(gv.isValid) << 1u) | static_cast<uint8_t>(validFeedback);
        FilterFlag flag{flagUnderlying};

        if (flag == FilterFlag::VALID) {
            filter(gv.index, feedback::encodeFeedbackString(fbString));
        }
        return flag;
    }
};

}