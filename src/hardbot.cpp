#include <atomic>
#include <barrier>

#include <boost/iterator/zip_iterator.hpp>
#include <boost/range/combine.hpp>

#include "hardBot.hpp"

namespace wordle {

bot::Suggestion bot::HardBot::suggest() {
    static_assert(bot::Suggestion{}.isValid == false);

    const size_t numAliveTargets = aliveTargets();
    if (!numAliveTargets) return {};
    if (numAliveTargets <= 2) return {static_cast<double>(numAliveTargets) - 1.0, vocab[aliveIndices.front()], aliveIndices.front(), true};

    // Atomic index for threads to find 2-depth entropy of top candidates
    std::atomic_size_t topCandidateIndex = 0;

    // Reset entropies for valid suggestions
    std::fill(entropies.begin(), entropies.end(), std::numeric_limits<double>::min());

    // Initialize barrier func
    const size_t N = aliveIndices.size();
    const size_t threadsLaunched = std::min(N, maxThreads);
    std::barrier syncPoint(threadsLaunched, [this] noexcept {
        std::partial_sort_copy(
            aliveIndices.begin(), aliveIndices.end(),
            topCandidates.begin(), topCandidates.end(),
            [this](WordCountT i, WordCountT j) { return entropies[i] > entropies[j]; }
        );
    });

    // Initialize Worker func
    auto worker = [&](std::vector<WordCountT>::const_iterator firstPassGuessStart, std::vector<WordCountT>::const_iterator firstPassGuessStop) {
        // Step 1: Calculate entropy of first guess for all valid guesses
        BotBase::BinCounts binCounts{};
        for (auto it = firstPassGuessStart; it < firstPassGuessStop; ++it) {
            size_t guessIndex = *it;
            double entropy = BotBase::baseEntropy(guessIndex, aliveIndices.cbegin(), fillerStart, binCounts);
            entropies[guessIndex] = entropy;
        }

        // Step 2: One thread gets top candidates while others wait
        syncPoint.arrive_and_wait();

        // Step 3: Take as many candidates as possible, calculate their 2-depth entropy, and update candidate entropy
        std::vector<std::vector<WordCountT>> targetBins;
        std::vector<std::vector<WordCountT>> fillerBins;

        while (true) {
            size_t idx = topCandidateIndex.fetch_add(1, std::memory_order_relaxed);
            if (idx >= topCandidates.size()) break;

            size_t candidateIndex = topCandidates[idx];
            double entropyDelta = 0.0;
            targetBins = getTargetIndexBins(candidateIndex, binCounts);
            fillerBins = getFillerIndexBins(candidateIndex, binCounts);

            for (size_t i = 0; i < wordle::feedback::NUM_FEEDBACKS; ++i) {
                // Get weight of this bin (AKA how probable we are to see a solution land in this bin compared to others)
                const auto& targets = targetBins[i];
                const auto& fillers = fillerBins[i];
                const double weight = static_cast<double>(targets.size()) / static_cast<double>(numAliveTargets);
                double bEntropy = binEntropy(targets.cbegin(), targets.cend(), fillers.cbegin(), fillers.cend(), binCounts);
                entropyDelta += weight * bEntropy;
            }

            entropies[candidateIndex] += entropyDelta;
        }
    };
    topCandidates.resize(std::min(numAliveTargets, beamCandidates));
    const size_t baseWork = N / maxThreads;
    const size_t extraWork = N % maxThreads;
    std::vector<WordCountT>::const_iterator it = aliveIndices.cbegin();
    std::vector<WordCountT>::const_iterator stop = aliveIndices.cend();

    for (size_t threadID = 0; it != stop; ++threadID) {
        std::vector<WordCountT>::const_iterator nextIt = it + baseWork + (threadID < extraWork ? 1 : 0);
        taskQueue.push(worker, it, nextIt);
        it = nextIt;
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