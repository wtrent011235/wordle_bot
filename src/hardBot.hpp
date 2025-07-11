#pragma once

#include <numeric>

#include "botBase.hpp"

namespace wordle::bot {

class HardBot : private BotBase {
    std::vector<double> entropies;
    std::vector<WordCountT> aliveIndices{};
    std::vector<WordCountT> topCandidates{};
    const size_t beamCandidates;
    const size_t maxThreads;
    std::vector<WordCountT>::const_iterator fillerStart;

    size_t aliveTargets() const noexcept {
        return std::distance(aliveIndices.cbegin(), fillerStart);
    }

    template <bool ReturnTargetBin>
    BotBase::WordIndexBins __getCandidateBin(size_t candidateIndex, BotBase::BinCounts& binCounts) {
        const auto& candidateSlice = fMap[candidateIndex];
        binCounts.fill(0);

        // Step 1: Count number of words that fall in each feedback category given guessing candidate
        std::vector<WordCountT>::const_iterator wordIndexStart;
        std::vector<WordCountT>::const_iterator wordIndexStop;
        if constexpr (ReturnTargetBin) {
            wordIndexStart = aliveIndices.cbegin();
            wordIndexStop = fillerStart;
        } else {
            wordIndexStart = fillerStart;
            wordIndexStop = aliveIndices.cend();
        }
        for (auto it = wordIndexStart; it != wordIndexStop; ++it) {
            size_t wordIndex = *it;
            size_t fbIndex = candidateSlice[wordIndex];
            ++binCounts[fbIndex];
        }

        // Step 2: Initialize output vector and reserve exact space for each bin
        BotBase::WordIndexBins output(wordle::feedback::NUM_FEEDBACKS);
        for (size_t i = 0; i < wordle::feedback::NUM_FEEDBACKS; ++i) {
            output[i].reserve(binCounts[i]);
        }

        // Step 3: Add words in their respective bin
        for (auto it = wordIndexStart; it != wordIndexStop; ++it) {
            WordCountT wordIndex = *it;
            size_t fbIndex = candidateSlice[wordIndex];
            output[fbIndex].push_back(wordIndex);
        }

        return output;
    }

    BotBase::WordIndexBins getTargetIndexBins(size_t candidateIndex, BotBase::BinCounts& binCounts) {
        return __getCandidateBin<true>(candidateIndex, binCounts);
    }

    BotBase::WordIndexBins getFillerIndexBins(size_t candidateIndex, BotBase::BinCounts& binCounts) {
        return __getCandidateBin<false>(candidateIndex, binCounts);
    }

    BotBase::GuessValidation validateGuess(std::string_view guess) {
        GuessValidation gv{0, false};
        if (!util::isValidWord(guess)) return gv;

        // Binary search for word
        auto search = [&](bool searchTargets) {
            size_t numAliveTargets = aliveTargets(); 
            size_t left = searchTargets ? 0 : numAliveTargets;
            size_t right = searchTargets ? numAliveTargets - 1 : aliveIndices.size() - 1;

            while (left <= right) {
                size_t middle = (left + right) / 2;
                size_t candidateIndex = aliveIndices[middle];
                std::string_view candidate = vocab[candidateIndex];

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
                gv.index = candidateIndex;
                gv.isValid = true;
                return;
            }
         };

        taskQueue.push(search, true);
        taskQueue.push(search, false);
        taskQueue.wait();
        return gv;
    }

    template <concepts::WordIndexIterator TargetIndexIterator, concepts::WordIndexIterator FillerIndexIterator>
    double binEntropy(TargetIndexIterator tStart, TargetIndexIterator tStop, FillerIndexIterator fStart, FillerIndexIterator fStop, BotBase::BinCounts& binCounts) {
        const size_t N = std::distance(tStart, tStop);
        if (N <= 2) {
            return std::max(0.0, static_cast<double>(N) - 1.0);
        }

        // Resulting maximum entropy from each possible guess in this bin
        double bestEntropy = std::numeric_limits<double>::min();

        // Find entropy of guessing each target in this bin, compare with out best
        for (auto it = tStart; it != tStop; ++it) {
            double entropy = BotBase::baseEntropy(*it, tStart, tStop, binCounts);
            bestEntropy = std::max(bestEntropy, entropy);
        }

        // Find entropy of guessing each fillers in this bin, compare with our best
        for (auto it = fStart; it != fStop; ++it) {
            double entropy = BotBase::baseEntropy(*it, tStart, tStop, binCounts);
            bestEntropy = std::max(bestEntropy, entropy);
        }
        return bestEntropy;
    }

public:
    HardBot(size_t _maxThreads = config::HARDWARE_CONCURRENCY, size_t _beamCandidates = config::HARDWARE_CONCURRENCY)
    : BotBase{_maxThreads},
      entropies(config::NUM_WORDS),
      topCandidates(_beamCandidates),
      beamCandidates{_beamCandidates},
      maxThreads{_maxThreads} {
        reset();
    }

    void reset() noexcept {
        aliveIndices.resize(wordle::config::NUM_WORDS);
        std::iota(aliveIndices.begin(), aliveIndices.end(), 0);
        fillerStart = aliveIndices.cbegin() + wordle::config::NUM_TARGETS;
    }

    const auto& getFMap() const noexcept {
        return fMap;
    }

    const auto& getVocab() const noexcept {
        return vocab;
    }

    Suggestion suggest();

    void filter(size_t guessIndex, wordle::feedback::Encoding fbEncoding) {
        // Filter aliveIndices
        aliveIndices.erase(
            std::remove_if(aliveIndices.begin(), aliveIndices.end(),
                [guessIndex, fbEncoding, this](WordCountT i) { return this->fMap[i][guessIndex] != fbEncoding; }),
            aliveIndices.end()
        );

        // Find number of targets left
        fillerStart = std::upper_bound(aliveIndices.begin(), aliveIndices.end(), static_cast<WordCountT>(wordle::config::NUM_TARGETS - 1));
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