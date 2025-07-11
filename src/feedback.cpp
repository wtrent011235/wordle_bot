#include "config.hpp"
#include "feedback.hpp"

wordle::feedback::FeedbackMap wordle::feedback::constructFeedbackMapBasic(const wordle::vocab::Vocab& vocab) {
    wordle::feedback::FeedbackMap fMap(wordle::config::NUM_WORDS, std::vector<wordle::feedback::Encoding>(wordle::config::NUM_WORDS));
    wordle::feedback::Encoder encoder{};
    for (size_t i = 0; i < wordle::config::NUM_WORDS; ++i) {
        std::string_view guess = vocab[i];
        auto& fSlice = fMap[i];
        for (size_t j = 0; j < wordle::config::NUM_WORDS; ++j) {
            fSlice[j] = encoder(guess, vocab[j]);
        }
    }
    return fMap;
}

wordle::feedback::FeedbackMap wordle::feedback::constructFeedbackMap(const wordle::vocab::Vocab& vocab, wordle::parallel::TaskQueue& queue, size_t numThreads) {
    constexpr size_t ENCODING_SIZE = sizeof(wordle::feedback::Encoding);
    constexpr size_t PADS = (wordle::config::NUM_WORDS * ENCODING_SIZE + wordle::config::CACHE_LINE_SIZE - 1) / wordle::config::CACHE_LINE_SIZE;

    wordle::feedback::FeedbackMap fMap(wordle::config::NUM_WORDS, std::vector<wordle::feedback::Encoding>(wordle::config::NUM_WORDS + PADS));

    constexpr size_t numJobs = wordle::config::NUM_WORDS;
    const size_t baseWork = numJobs / numThreads;
    const size_t extraWork = numJobs % numThreads;
    size_t threadID = 0;
    for (size_t start = 0; start < numJobs; ++threadID) {
        size_t newStart = start + baseWork + static_cast<size_t>(threadID < extraWork);
        queue.push(
            [&vocab, &fMap, start, newStart]() {
                thread_local wordle::feedback::Encoder encoder{};
                for (size_t guessIndex = start; guessIndex < newStart; ++guessIndex) {
                    auto& fMapSlice = fMap[guessIndex];
                    std::string_view guess = vocab[guessIndex];
                    for (size_t solutionIndex = 0; solutionIndex < wordle::config::NUM_WORDS; ++solutionIndex) {
                        fMapSlice[solutionIndex] = encoder(guess, vocab[solutionIndex]);
                    }
                }
            }
        );

        start = newStart;
    }
    queue.wait();
    return fMap;
}

wordle::feedback::FlatFeedbackMap wordle::feedback::constructFlatFeedbackMap(
    const wordle::vocab::Vocab& vocab,
    wordle::parallel::TaskQueue& queue,
    size_t numThreads
) {
    // Choose number of jobs based on number of threads (e.g., fewer than threads if work is heavy)
    const size_t totalWords = wordle::config::NUM_WORDS;

    // Padded stride to avoid false sharing (align each row to 64 bytes)
    constexpr size_t CACHE_LINE_SIZE = wordle::config::CACHE_LINE_SIZE;
    constexpr size_t ENCODING_SIZE = sizeof(wordle::feedback::Encoding);
    const size_t stride = ((totalWords * ENCODING_SIZE + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * (CACHE_LINE_SIZE / ENCODING_SIZE);

    // Flat map: contiguous and cache-aligned
    std::vector<wordle::feedback::Encoding> flatMap(totalWords * stride);

    // Dispatch encoding jobs
    const size_t baseWork = totalWords / numThreads;
    const size_t extraWork = totalWords % numThreads;

    size_t threadID = 0;
    for (size_t start = 0; start < totalWords; ++threadID) {
        const size_t workSize = baseWork + static_cast<size_t>(threadID < extraWork);
        const size_t stop = start + workSize;

        queue.push([start, stop, &flatMap, &vocab]() {
            thread_local wordle::feedback::Encoder encoder{};
            for (size_t guessIndex = start; guessIndex < stop; ++guessIndex) {
                const std::string_view guess = vocab[guessIndex];
                wordle::feedback::Encoding* row = &flatMap[guessIndex * stride];
                for (size_t solutionIndex = 0; solutionIndex < wordle::config::NUM_WORDS; ++solutionIndex) {
                    row[solutionIndex] = encoder(guess, vocab[solutionIndex]);
                }
            }
        });

        start = stop;
    }

    queue.wait();
    return flatMap;
}

