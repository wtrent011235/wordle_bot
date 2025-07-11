#include <atomic>
#include <barrier>
#include <ranges>

#include "easyBot.hpp"

namespace wordle {

bot::Suggestion wordle::bot::EasyBot::suggest() {
    static_assert(bot::Suggestion{}.isValid == false);
    const size_t numTargets = aliveTargets.size();
    if (!numTargets) return {};
    if (numTargets <= 2) return {static_cast<double>(numTargets) - 1.0, vocab[aliveTargets.front()], aliveTargets.front(), true};
    
    // Atomic index to find 2-depth entropy of top candidates
    std::atomic_size_t topCandidateIndex = 0;

    // Reset entropies for valid suggestions
    std::fill(entropies.begin(), entropies.end(), 0.0);

    // Initialize barrier func
    const size_t N = wordle::config::NUM_WORDS;
    const size_t threadsLaunched = std::min(N, maxThreads);
    constexpr auto allGuessIndices = std::views::iota(size_t{0}, wordle::config::NUM_WORDS);
    std::barrier syncPoint(threadsLaunched, [this, &allGuessIndices] noexcept {
        std::partial_sort_copy(
            allGuessIndices.begin(), allGuessIndices.end(),
            topCandidates.begin(), topCandidates.end(),
            [this](WordCountT i, WordCountT j) { return entropies[i] > entropies[j]; }
        );
    });

    // Initialize worker func
    auto worker = [&](size_t fpGuessStart, size_t fpGuessEnd) {
        // Step 1: Calculate depth-1 entropy for all valid guesses
        BotBase::BinCounts binCounts{};
        for (size_t guessIndex = fpGuessStart; guessIndex < fpGuessEnd; ++guessIndex) {
            double entropy = BotBase::baseEntropy(guessIndex, aliveTargets.cbegin(), aliveTargets.cend(), binCounts);
            entropies[guessIndex] = entropy;
        }

        // Step 2: One thread gets top candidates while others wait at barrier
        syncPoint.arrive_and_wait();

        // Step 3: Take as many candidates as possible, calculate their 2-depth entropy, and update candidate entropy
        BotBase::WordIndexBins targetBins;

        while (true) {
            size_t idx = topCandidateIndex.fetch_add(1, std::memory_order_relaxed);
            if (idx >= topCandidates.size()) break;

            size_t candidateIndex = topCandidates[idx];
            double entropyDelta = 0.0;
            targetBins = getCandidateTargets(candidateIndex, binCounts);

            for (const auto& targets : targetBins) {
                // Get weight of this bin (AKA how probable it is that a target has this feedback when guessing candidate)
                double weight = static_cast<double>(targets.size()) / numTargets;
                double bEntropy = binEntropy(targets.cbegin(), targets.cend(), binCounts);
                entropyDelta += weight * bEntropy;
            }

            entropies[candidateIndex] += entropyDelta;
        }
    };
    
    topCandidates.resize(std::min(numTargets, beamCandidates));  // Resize candidates accordingly
    const size_t baseWork = wordle::config::NUM_WORDS / maxThreads;
    const size_t extraWork = wordle::config::NUM_WORDS % maxThreads;
    size_t guessStartIndex = 0;

    for (size_t threadID = 0; guessStartIndex < wordle::config::NUM_WORDS; ++threadID) {
        size_t guessStopIndex = guessStartIndex + baseWork + (threadID < extraWork ? 1ul : 0ul);
        taskQueue.push(worker, guessStartIndex, guessStopIndex);
        guessStartIndex = guessStopIndex;
    }
    taskQueue.wait();

    double bestEntropy = std::numeric_limits<double>::min();
    size_t wordIndex;

    for (size_t index : topCandidates) {
        double entropy = entropies[index];
        if (bestEntropy < entropy) {
            bestEntropy = entropy;
            wordIndex = index;
        }
    }
    return {bestEntropy, vocab[wordIndex], wordIndex, true};
}
}