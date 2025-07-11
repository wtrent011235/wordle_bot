#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <algorithm>
#include <unordered_map>
#include <map>

#include "../src/feedback.hpp"
#include "../src/parallelTaskQueue.hpp"
#include "../src/vocab.hpp"

using namespace wordle::feedback;

TEST_CASE("Feedback: Test multipliers", "[feedback]") {
    STATIC_REQUIRE(__impl::multipliers.size() == wordle::config::WORD_LENGTH);
    STATIC_REQUIRE(__impl::multipliers[0] == Encoding{1});
    for (size_t i = 1; i < wordle::config::WORD_LENGTH; ++i) {
        REQUIRE(__impl::multipliers[i] == (__impl::multipliers[i - 1] * __impl::BASE));
    }
}

TEST_CASE("Feedback: Test encodeFeedbackString()", "[feedback]") {
    REQUIRE_THROWS(__impl::encodeFeedbackString(std::string{wordle::config::WORD_LENGTH - 1, 'x'}));
    REQUIRE_THROWS(__impl::encodeFeedbackString(std::string{wordle::config::WORD_LENGTH + 1, 'x'}));
    REQUIRE_THROWS(__impl::encodeFeedbackString(std::string{wordle::config::WORD_LENGTH, 'A'}));

    // Sort keys in order of encoding value
    std::map<Encoding, char> mapping{};
    mapping[__impl::EXHAUSTED] = position::EXHAUSTED;
    mapping[__impl::WRONG_POSITION] = position::WRONG_POSITION;
    mapping[__impl::CORRECT] = position::CORRECT;

    // Get keys
    std::array<char, wordle::config::WORD_LENGTH> keys{};
    size_t index = 0;
    for (auto it : mapping) {
        keys[index++] = it.second;
    }

    // Ensure feedback string encodes correctly
    for (Encoding encoding = 0; encoding < NUM_FEEDBACKS; ++encoding) {
        // Generate expected encoding fbString
        std::array<decltype(keys)::value_type, wordle::config::WORD_LENGTH + 1> fbString{};
        fbString.fill('\0');
        size_t val = encoding;
        for (size_t i = 0; i < wordle::config::WORD_LENGTH; ++i) {
            fbString[i] = keys[val % __impl::BASE];
            val /= __impl::BASE;
        }

        // Generate actual encoding
        REQUIRE_NOTHROW(__impl::encodeFeedbackString(std::string_view{fbString.begin(), wordle::config::WORD_LENGTH}));
        Encoding actual = __impl::encodeFeedbackString(std::string_view{fbString.begin(), wordle::config::WORD_LENGTH});
        REQUIRE(encoding == actual);
    }
}

TEST_CASE("Feedback: Test ArrEncoder on all words", "[feedback][slow]") {
    struct TestEncoder {
        std::unordered_map<char, Encoding> unmatchedCounts{};
        std::array<Encoding, wordle::config::WORD_LENGTH> positionEncodings{};

        Encoding operator()(std::string_view guess, std::string_view solution) {
            unmatchedCounts.clear();
            positionEncodings.fill(__impl::EXHAUSTED);

            for (size_t i = 0; i < guess.size(); ++i) {
                char solutionLetter = solution[i];
                if (solutionLetter == guess[i]) {
                    positionEncodings[i] = __impl::CORRECT;
                } else {
                    ++unmatchedCounts[solutionLetter];
                }
            }

            for (size_t i = 0; i < guess.size(); ++i) {
                if (positionEncodings[i] == __impl::CORRECT) continue;
                char guessLetter = guess[i];
                auto it = unmatchedCounts.find(guessLetter);
                if (it == unmatchedCounts.end()) continue;
                positionEncodings[i] = __impl::WRONG_POSITION;

                if (--it->second == 0) unmatchedCounts.erase(guessLetter);
            }

            Encoding encoding = 0;
            for (size_t i = 0; i < positionEncodings.size(); ++i) {
                encoding += (positionEncodings[i] * __impl::multipliers[i]);
            }
            return encoding;
        }
    };

    wordle::parallel::TaskQueue queue(wordle::config::HARDWARE_CONCURRENCY);
    const auto vocab = wordle::vocab::constructVocab();

    // Construct fMap
    const auto fMap = wordle::feedback::constructFeedbackMap(vocab, queue, wordle::config::HARDWARE_CONCURRENCY);

    alignas(wordle::config::CACHE_LINE_SIZE) std::atomic_bool passes{true};

    // Verify fMap
    const size_t baseWork = wordle::config::NUM_WORDS / wordle::config::HARDWARE_CONCURRENCY;
    const size_t extraWork = wordle::config::NUM_WORDS % wordle::config::HARDWARE_CONCURRENCY;
    size_t threadID = 0;

    for (size_t start = 0; start < wordle::config::NUM_WORDS; ++threadID) {  // start modified in body
        size_t newStart = start + baseWork + static_cast<size_t>(threadID < extraWork);
        queue.push(
            [&vocab, &fMap, &passes, start, newStart]() {
                thread_local TestEncoder encoder{};
                for (size_t guessIndex = start; guessIndex < newStart; ++guessIndex) {
                    const auto& flatSlice = fMap[guessIndex];
                    std::string_view guess = vocab[guessIndex];
                    for (size_t solutionIndex = 0; solutionIndex < wordle::config::NUM_WORDS; ++solutionIndex) {
                        if (encoder(guess, vocab[solutionIndex]) != flatSlice[solutionIndex]) {
                            passes.store(false);
                            return;
                        }
                    }
                }
            }
        );
        start = newStart;
    }
    queue.wait();

    REQUIRE(passes);
}
