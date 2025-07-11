#include <benchmark/benchmark.h>

#include "../src/feedback.hpp"

static inline const auto vocab{wordle::vocab::constructVocab()};

static void testConstructFeedbackEncodingBasic(benchmark::State& state) {
    for (auto _ : state) {
        auto fMap{wordle::feedback::constructFeedbackMapBasic(vocab)};
        benchmark::DoNotOptimize(fMap);
    }
}

BENCHMARK(testConstructFeedbackEncodingBasic);

static void testConstructFeedbackEncoding(benchmark::State& state) {
    size_t numThreads = state.range(0);
    wordle::parallel::TaskQueue queue{numThreads};
    for (auto _ : state) {
        auto fMap{wordle::feedback::constructFeedbackMap(vocab, queue, numThreads)};
        benchmark::DoNotOptimize(fMap);
    }
}

BENCHMARK(testConstructFeedbackEncoding)
    ->Arg(1ul)
    ->Arg(2ul)
    ->Arg(4ul)
    ->Arg(6ul)
    ->Arg(8ul)
    ->Arg(10ul)
    ->Arg(12ul);

static void testConstructFlatFeedbackEncoding(benchmark::State& state) {
    size_t numThreads = state.range(0);
    wordle::parallel::TaskQueue queue{numThreads};
    for (auto _ : state) {
        auto fMap{wordle::feedback::constructFlatFeedbackMap(vocab, queue, numThreads)};
        benchmark::DoNotOptimize(fMap);
    }
}

BENCHMARK(testConstructFlatFeedbackEncoding)
    ->Arg(1ul)
    ->Arg(2ul)
    ->Arg(4ul)
    ->Arg(6ul)
    ->Arg(8ul)
    ->Arg(10ul)
    ->Arg(12ul);