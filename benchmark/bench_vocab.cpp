#include <benchmark/benchmark.h>

#include "../src/vocab.hpp"

static void BM_processTargetFile(benchmark::State& state) {
    for (auto _ : state) {
        auto x = wordle::vocab::__impl::processFile(wordle::config::TARGET_FILE, wordle::config::NUM_TARGETS);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_processTargetFile);

static void BM_processFillerFile(benchmark::State& state) {
    for (auto _ : state) {
        auto x = wordle::vocab::__impl::processFile(wordle::config::FILLER_FILE, wordle::config::NUM_FILLERS);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_processFillerFile);

static void BM_constructVocab(benchmark::State& state) {
    for (auto _ : state) {
        auto x = wordle::vocab::constructVocab();
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_constructVocab);

BENCHMARK_MAIN();