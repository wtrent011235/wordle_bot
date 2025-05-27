#pragma once

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/range/join.hpp>
#include <functional>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>
#include <thread>
#include <unordered_set>

#include "config.hpp"
#include "feedback_encoding.hpp"
#include "util.hpp"
#include "timing.hpp"

namespace wordle {

enum class State : char {
    NOT_OVER,
    WON,
    DID_NOT_FIND_TARGET
};


struct OptimalMove {
public:
    size_t guessIndex;
    double entropy;
};
inline constexpr bool operator>(const OptimalMove& left, const OptimalMove& right) noexcept {
    return left.entropy > right.entropy;
}
inline constexpr bool operator<(const OptimalMove& left, const OptimalMove& right) noexcept {
    return left.entropy < right.entropy;
}
inline constexpr bool operator==(const OptimalMove& left, const OptimalMove& right) noexcept {
    return left.entropy == right.entropy;
}

struct FindWordInfo {
    size_t index;
    bool valid;
};

struct BotBase {
public:
    // using Vocab = std::array<std::string, config::NUM_WORDS>;
    using Vocab = std::array<std::string, config::NUM_WORDS>;
    using WordCountType = boost::uint_t<std::bit_width(config::NUM_WORDS)>::least;
    static constexpr auto NO_POSSIBLE_MOVE = OptimalMove{std::numeric_limits<size_t>::max(), std::numeric_limits<double>::min()};
    Vocab vocab{};
    State state;
    FeedbackMap fMap{};
    std::vector<OptimalMove> moves;
    std::vector<std::thread> threadPool;

protected:
    BotBase() : state(State::NOT_OVER), threadPool(std::thread::hardware_concurrency()) {
        initVocab();
        initFeedbackMap();
    }

private:
    void initVocab() {
        auto parFunc = [this](std::string_view fileName, size_t startIndex, size_t stopIndex) {
            std::ifstream file(fileName);
            if (!file) {
                util::formatError("Failed to open {}", fileName);
            }
            std::string buffer;
            size_t index = startIndex;

            while (std::getline(file, buffer) && index < stopIndex) {
                boost::trim(buffer);
                boost::to_lower(buffer);
                if (buffer.size() != config::WORD_LENGTH) continue;
                for (char c : buffer) {
                    if (c < 'a' || c > 'z') {
                        util::formatError("Invalid word found in {}: {}", fileName, buffer);
                    }
                }

                vocab[index++] = buffer;
            }

            util::formatGuard(!std::getline(file, buffer), "{} has too many words", fileName);
            util::formatGuard(index == stopIndex, "Unable to add {} words from {}, added: ", stopIndex - startIndex, fileName, index - startIndex);
        };

        threadPool[0] = std::thread(parFunc, config::TARGET_FILE, 0, config::NUM_TARGETS);
        threadPool[1] = std::thread(parFunc, config::FILLER_FILE, config::NUM_TARGETS, config::NUM_WORDS);
        threadPool[0].join();
        threadPool[1].join();
    }
    void initFeedbackMap() {
        auto parFunc = [this](size_t startIndex, size_t stopIndex) {
            FeedbackEncoder encoder{};
            for (size_t guessIndex = startIndex; guessIndex < stopIndex; ++guessIndex) {
                encoder.guess = vocab[guessIndex];
                for (size_t solutionIndex = 0; solutionIndex < vocab.size(); ++solutionIndex) {
                    fMap(guessIndex, solutionIndex) = encoder(vocab[solutionIndex]);
                }
            }
        };

        const size_t baseWork = config::NUM_WORDS / threadPool.size();
        const size_t extraWork = config::NUM_WORDS % threadPool.size();
        size_t guessIndex = 0;

        for (size_t threadID = 0; threadID < threadPool.size(); ++threadID) {
            size_t chunkWork = baseWork + (threadID < extraWork);
            size_t stopIndex = guessIndex + chunkWork;
            threadPool[threadID] = std::thread(parFunc, guessIndex, stopIndex);
            guessIndex = stopIndex;
        }

        for (size_t threadID = 0; threadID < threadPool.size(); ++threadID) {
            threadPool[threadID].join();
        }
    }
};

template <bool HardMode>
struct Bot : public BotBase {
    Bot() : BotBase() {}
};

template <>
struct Bot<true> : public BotBase {
    std::vector<size_t> validTargetIndices;
    std::vector<size_t> validFillerIndices;
    std::vector<std::array<WordCountType, FeedbackEncoder::NUM_ENCODINGS>> threadBinCounts;
    std::vector<std::array<std::vector<size_t>, FeedbackEncoder::NUM_ENCODINGS>> threadBinTargetIndices;
    std::vector<std::array<std::vector<size_t>, FeedbackEncoder::NUM_ENCODINGS>> threadBinFillerIndices;

    Bot() :
    BotBase(),
    threadBinCounts(threadPool.size()),
    threadBinTargetIndices(threadPool.size()),
    threadBinFillerIndices(threadPool.size()) {
        util::formatGuard(threadPool.size() > 1, "We need at least 2 parallel threads, got : {}", threadPool.size());
        reset();
    }

    FindWordInfo findWordIndex(std::string_view word) {
        FindWordInfo info{0, false};

        constexpr size_t nThreads = 2;
        for (size_t threadID = 0; threadID < nThreads; ++threadID) {
            threadPool[threadID] = std::thread([threadIDX = threadID, word, &info, this]() {
                size_t left = threadIDX == 0 ? 0 : config::NUM_TARGETS;
                size_t right = threadIDX == 0 ? config::NUM_TARGETS : config::NUM_WORDS;

                while (left <= right) {
                    size_t middle = (left + right) / 2;
                    std::string_view candidate = vocab[middle];
                    if (candidate == word) {
                        info.index = middle;
                        info.valid = true;
                        return;
                    } else if (candidate < word) {
                        left = middle + 1;
                    } else {
                        right = middle - 1;
                    }
                }
            });
        }

        for (size_t threadID = 0; threadID < nThreads; ++threadID) {
            threadPool[threadID].join();
        }

        return info;
    }

    void reset() {
        constexpr size_t nThreads = 2;
        for (size_t threadID = 0; threadID < nThreads; ++threadID) {
            threadPool[threadID] = std::thread([threadIDX = threadID, this]() {
                auto iter = threadIDX == 0 ? std::views::iota(size_t{0}, config::NUM_TARGETS) : std::views::iota(config::NUM_TARGETS, config::NUM_WORDS);  // <-- fixed NUM_FILLERS upper bound too
                auto& container = threadIDX == 0 ? validTargetIndices : validFillerIndices;
                container.assign(iter.begin(), iter.end());
            });
        }
        for (size_t threadID = 0; threadID < nThreads; ++threadID) threadPool[threadID].join();

        state = State::NOT_OVER;
    }

    OptimalMove suggest() {
        if (validTargetIndices.empty()) return NO_POSSIBLE_MOVE;

        if (validTargetIndices.size() <= 2) {
            size_t guessIndex = validTargetIndices[0];
            return {guessIndex, static_cast<double>(validTargetIndices.size() - 1)};
        }

        // State of possible moves
        moves.resize(validTargetIndices.size() + validFillerIndices.size());
        std::atomic<OptimalMove> atomicBestMove = NO_POSSIBLE_MOVE;

        // Step 1: Find entropy candidates
        size_t baseWork = moves.size() / threadPool.size();
        size_t extraWork = moves.size() % threadPool.size();
        size_t numThreads = std::min(threadPool.size(), moves.size());

        size_t startIndex = 0;
        for (size_t threadID = 0; threadID < numThreads; ++threadID) {
            size_t threadWork = baseWork + (threadID < extraWork);
            size_t endIndex = startIndex + threadWork;
            threadPool[threadID] = std::thread([startIDX = startIndex, endIDX = endIndex, threadIDX = threadID, &atomicBestMove, this]() {
                OptimalMove candidateMove = NO_POSSIBLE_MOVE;

                for (size_t i = startIDX; i < endIDX; ++i) {
                    size_t guessIndex = i < validTargetIndices.size() ? validTargetIndices[i] : validFillerIndices[i - validTargetIndices.size()];
                    moves[i] = OptimalMove{guessIndex, findBaseEntropy(guessIndex, threadIDX)};
                    candidateMove = std::max(candidateMove, moves[i]);
                }
                
                OptimalMove currentBestMove = atomicBestMove.load();
                while (candidateMove > currentBestMove && !atomicBestMove.compare_exchange_weak(currentBestMove, candidateMove, std::memory_order_relaxed)) {
                    //spinnnnn
                }
            });
            startIndex = endIndex;
        }

        for (size_t threadID = 0; threadID < numThreads; ++threadID) threadPool[threadID].join();

        const double maxBaseEntropy = atomicBestMove.load(std::memory_order_relaxed).entropy;  // No parallel threads -- safe to quickly load
        auto entropyFunc = [maxBaseEntropy](const OptimalMove& oMove) -> bool { return oMove.entropy * 1.25 >= maxBaseEntropy; };
        const size_t MOVES_END = util::shiftFilter(moves, entropyFunc);

        for (size_t i = 0; i < MOVES_END; ++i) {
            util::formatGuard(entropyFunc(moves[i]), "Filter failed to exclude guess{} with guess index {}", vocab[moves[i].guessIndex], moves[i].guessIndex);
        }
        for (size_t i = MOVES_END; i < moves.size(); ++i) {
            util::formatGuard(!entropyFunc(moves[i]), "Filter failed to include guess {} with guess index {}", vocab[moves[i].guessIndex], moves[i].guessIndex);
        }

        // Now, moves [0, right] are our possible candidates
        baseWork = MOVES_END / threadPool.size();
        extraWork = MOVES_END % threadPool.size();
        numThreads = std::min(MOVES_END, threadPool.size());
        startIndex = 0;

        for (size_t threadID = 0; threadID < numThreads; ++threadID) {
            size_t threadWork = baseWork + (threadID < extraWork    );
            size_t endIndex = startIndex + threadWork;
            threadPool[threadID] = std::thread([startIDX = startIndex, endIDX = endIndex, threadIDX = threadID, &atomicBestMove, this]() {
                OptimalMove candidateMove = NO_POSSIBLE_MOVE;
            
                // Find best move from given moves
                for (size_t moveIndex = startIDX; moveIndex < endIDX; ++moveIndex) {
                    auto& possibleMove = moves[moveIndex];
                    possibleMove.entropy += findWeightedNextLevelEntropy(possibleMove.guessIndex, threadIDX);
                    candidateMove = std::max(candidateMove, possibleMove);
                }

                // If this is the best move, set it
                auto currentBestMove = atomicBestMove.load(std::memory_order_relaxed);
                while (candidateMove > currentBestMove && !atomicBestMove.compare_exchange_weak(currentBestMove, candidateMove, std::memory_order_relaxed)) {
                    // spiiiiinnnnnnnnnn
                }
            }); 

            startIndex = endIndex;
        }
        for (size_t threadID = 0; threadID < numThreads; ++threadID) threadPool[threadID].join();

        // We now have our best move
        return atomicBestMove.load(std::memory_order_relaxed);
    }

    void update(size_t guessIndex, Feedback feedback) {
        if (feedback == FeedbackEncoder::CORRECT_FEEDBACK) {
            validTargetIndices.resize(1);
            validTargetIndices[0] = guessIndex;
            validFillerIndices.clear();
            state = State::WON;
            return;
        }

        // Filter validTargetIndices and validFillerIndices
        auto conditionFunc = [this, guessIndex, feedback](size_t wordIndex) -> bool { return fMap(guessIndex, wordIndex) == feedback; };

        constexpr size_t nThreads = 2;
        for (size_t threadID = 0; threadID < nThreads; ++threadID) {
            threadPool[threadID] = std::thread([threadIDX = threadID, &conditionFunc, this]() {
                auto& container = threadIDX == 0 ? validTargetIndices : validFillerIndices;
                size_t n = util::shiftFilter(container, conditionFunc);
                container.resize(n); // Will this preserve the first n elements or overwrite?
            });
        }
        for (size_t threadID = 0; threadID < nThreads; ++threadID) threadPool[threadID].join();
        
        state = validTargetIndices.size() > 0 ? State::NOT_OVER : State::DID_NOT_FIND_TARGET;
    }


private:
    // Find entropy of a guess given guessIndex and targetIndices
    double findBaseEntropy(size_t guessIndex, size_t threadID) {
        util::formatGuard(validTargetIndices.size() > 2, "findEntropy called with {} remaining valid targets", validTargetIndices.size());
        return findBaseEntropy(guessIndex, threadID, validTargetIndices);
    }

    double findBaseEntropy(size_t guessIndex, size_t threadID, const std::vector<size_t>& activeTargetIndices) {
        util::formatGuard(activeTargetIndices.size() > 0, "Cannot find entropy with 0 solutions: {} and {}", guessIndex, threadID);
        if (activeTargetIndices.size() <= 2) return activeTargetIndices.size() - 1;

        // Get count of targets that belong in each feedback bin
        auto& feedbackBins = threadBinCounts[threadID];
        feedbackBins.fill(0);
        for (size_t targetIndex : activeTargetIndices) {
            size_t feedback = fMap(guessIndex, targetIndex);
            ++feedbackBins[feedback];
        }

        // Find entropy
        double entropy = 0;
        for (auto count : feedbackBins) {
            if (count == 0) continue;
            double p = static_cast<double>(count) / activeTargetIndices.size();
            entropy -= (std::log2(p) * p);
        }

        return entropy;
    }

    double findWeightedNextLevelEntropy(size_t guessIndex, size_t threadID) {
        util::formatGuard(validTargetIndices.size() > 2, "findWeightedNextEntropy called with {} remaining valid targets", validTargetIndices.size());

        auto& targetBins = threadBinTargetIndices[threadID];
        auto& fillerBins = threadBinFillerIndices[threadID];

        // Fill bins with corresponding indices
        for (auto& bin : boost::join(targetBins, fillerBins)) bin.clear();

        for (size_t targetIndex : validTargetIndices) {
            size_t feedback = fMap(guessIndex, targetIndex);
            targetBins[feedback].push_back(targetIndex);
        }
        for (size_t fillerIndex : validFillerIndices) {
            size_t feedback = fMap(guessIndex, fillerIndex);
            fillerBins[feedback].push_back(fillerIndex);
        }

        // Find weighted entropy of each bin with a target
        double entropy = 0;
        for (size_t i = 0; i < targetBins.size(); ++i) {
            size_t count = targetBins[i].size();
            if (count == 0) continue;
            if (count <= 2) {
                entropy += count * (count - 1);  // Entropy is count - 1
                continue;
            }
            // Find best entropy from this bin
            double maxEntropy = std::numeric_limits<double>::min();
            for (size_t guessIndex : boost::join(targetBins[i], fillerBins[i])) {
                maxEntropy = std::max(maxEntropy, findBaseEntropy(guessIndex, threadID, targetBins[i]));
            }

            // Add to result
            entropy += count * maxEntropy;
        }
        entropy /= validTargetIndices.size();  // Turn counts into weights

        return entropy;
    }
};

}