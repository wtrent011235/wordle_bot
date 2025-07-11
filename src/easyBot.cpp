#include <memory>
#include <ranges>

#include "easyBot.hpp"

namespace wordle {

bot::Suggestion wordle::bot::EasyBot::suggest() {
    bot::Suggestion suggestion{};
    if (aliveTargets.empty()) return suggestion;
    if (aliveTargets.size() <= 2) {
        suggestion.entropy = static_cast<double>(aliveTargets.size() - 1);
        suggestion.guessIndex = aliveTargets.front();
        suggestion.guess = vocab[suggestion.guessIndex];
        suggestion.isValid = true;
        return suggestion;
    }

    size_t threadsAtBarrier = 0;
    std::condition_variable cv{};
    std::mutex mtx{};
    std::fill(entropies.begin(), entropies.end(), std::numeric_limits<double>::min());
    
    auto worker = [&](size_t threadID, size_t fpStart, size_t fpEnd) {
        std::array<WordCountT, feedback::NUM_FEEDBACKS> binCounts;
        for (size_t guessIndex = fpStart; guessIndex < fpEnd; ++guessIndex) {
            binCounts.fill(0);
            const auto& fpSlice = fMap[guessIndex];
            for (size_t targetIndex : aliveTargets) {
                size_t fbIndex = fpSlice[targetIndex];
                ++binCounts[fbIndex];
            }

            double entropy = 0;
            const double numerator = static_cast<double>(aliveTargets.size());
            for (double count : binCounts) {
                if (!count) continue;
                double prob = count / numerator;
                entropy -= prob * std::log2(prob);
            }

            entropies[guessIndex] = entropy;
        }

        std::unique_lock<std::mutex> lock(mtx);
        if (++threadsAtBarrier < maxThreads) {
            cv.wait(lock, [&]() { return threadsAtBarrier >= maxThreads; });
        } else {
            constexpr size_t ZERO = 0;
            constexpr auto guessIt = std::ranges::iota_view{ZERO, wordle::config::NUM_WORDS};
            std::partial_sort_copy(
                guessIt.begin(), guessIt.end(),
                topCandidates.begin(), topCandidates.end(),
                [&](size_t i, size_t j) { return entropies[i] > entropies[j]; }
            );
            cv.notify_all();
        }

        const size_t candidateIndex = topCandidates[threadID];
        const auto& candidateSlice = fMap[candidateIndex];
        std::vector<std::vector<WordCountT>> targetBins(wordle::config::NUM_TARGETS);

        binCounts.fill(0);
        for (size_t targetIndex : aliveTargets) {
            size_t fbIndex = candidateSlice[targetIndex];
            ++binCounts[fbIndex];
        }
        for (size_t i = 0; i < binCounts.size(); ++i) {
            targetBins[i].resize(binCounts[i]);
        }
        for (WordCountT targetIndex : aliveTargets) {
            size_t fbIndex = candidateSlice[targetIndex];
            targetBins[fbIndex].push_back(targetIndex);
        }

        double entropyDelta = 0;
        for (const auto& targets : targetBins) {
            const double numerator = targets.size();
            const double weight = numerator / static_cast<double>(aliveTargets.size());

            if (targets.size() <= 2) {
                entropyDelta += weight * std::max(0.0, static_cast<double>(targets.size() - 1));
                continue;
            } 

            double binEntropy = std::numeric_limits<double>::min();
            for (size_t guessIndex = 0; guessIndex < wordle::config::NUM_WORDS; ++guessIndex) {
                binCounts.fill(0);
                const auto& guessSlice = fMap[guessIndex];
                for (size_t solutionIndex : targets) {
                    size_t fbIndex = guessSlice[solutionIndex];
                    ++binCounts[fbIndex];
                }

                double guessEntropy = 0;
                for (double count : binCounts) {
                    if (!count) continue;
                    double prob = count / numerator;
                    guessEntropy -= prob * std::log2(prob);
                }

                binEntropy = std::max(binEntropy, guessEntropy);
            }

            entropyDelta += weight * binEntropy;
        }

        entropies[candidateIndex] += entropyDelta;
    };
    
    constexpr size_t N = config::NUM_WORDS;
    const size_t baseWork = N / maxThreads;
    const size_t extraWork = N % maxThreads;
    size_t index = 0;

    for (size_t threadID = 0; index < N; ++threadID) {
        size_t stopIndex = index + baseWork + static_cast<size_t>(threadID < extraWork);
        taskQueue.push(worker, threadID, index, stopIndex);
        index = stopIndex;
    }
    taskQueue.wait();

    // Get index of best word and its entropy
    size_t bestGuessIndex = 0;
    double bestEntropy = std::numeric_limits<double>::min();

    for (size_t guessIndex : topCandidates) {
        double entropy = entropies[guessIndex];
        if (entropy > bestEntropy) {
            bestEntropy = entropy;
            bestGuessIndex = guessIndex;
        }
    }

    return {bestEntropy, vocab[bestGuessIndex], bestGuessIndex, true};
}

}