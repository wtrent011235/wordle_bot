#include <benchmark/benchmark.h>

#include "../src/botBase.hpp"

struct Dummy : wordle::bot::BotBase {
    Dummy() noexcept = default;
    ~Dummy() noexcept = default;
};

static void BM_BotBaseConstructor(benchmark::State& state) {
    for (auto _ : state) {
        Dummy base{};
        benchmark::DoNotOptimize(base);
    }
}

BENCHMARK(BM_BotBaseConstructor);

