#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>

#include "../src/config.hpp"
#include "../src/parallelTaskQueue.hpp"

struct SimpleTimer {
private:
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> __start;

public:
    SimpleTimer() noexcept = default;
    void start() { __start = std::chrono::steady_clock::now(); }
    std::chrono::nanoseconds stop() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - __start);
    }
};

SimpleTimer timer{};

TEST_CASE("Parallel Task Queue: Task queue push() 1 thread sleep does not hang", "[parallelTaskQueue][parallel]") {
    wordle::parallel::TaskQueue queue(wordle::config::HARDWARE_CONCURRENCY);
    
    timer.start();
    queue.push([]() { std::this_thread::sleep_for(std::chrono::nanoseconds{5}); } );
    queue.wait();
    auto elapsed = timer.stop();
    REQUIRE(elapsed < std::chrono::seconds{1});
}

TEST_CASE("Parallel Task Queue: Task queue push() 8 thread sleep does not hang", "[parallelTaskQueue][parallel]") {
    wordle::parallel::TaskQueue queue(wordle::config::HARDWARE_CONCURRENCY);

    timer.start();
    for (size_t i = 0; i < queue.capacity(); ++i) {
        queue.push([]() { std::this_thread::sleep_for(std::chrono::nanoseconds{5}); } );
    }
    queue.wait();
    auto elapsed = timer.stop();
    REQUIRE(elapsed < std::chrono::seconds{1});
}

TEST_CASE("Parallel Task Queue: Task queue push() 1 thread basic function", "[parallelTaskQueue][parallel]") {
    wordle::parallel::TaskQueue queue(wordle::config::HARDWARE_CONCURRENCY);
    size_t x = 0;

    queue.push([&x]() { ++x; });
    queue.wait();
    REQUIRE(x == 1);
}

TEST_CASE("Parallel Task Queue: Task queue push() 8 thread function works", "[parallelTaskQueue][parallel]") {
    wordle::parallel::TaskQueue queue(wordle::config::HARDWARE_CONCURRENCY);
    std::atomic<size_t> x = 0;

    for (size_t i = 0; i < queue.capacity(); ++i) {
        queue.push([&x]() { x.fetch_add(1); });
    }
    queue.wait();
    REQUIRE(x == queue.capacity());
}

TEST_CASE("Parallel Task Queue: pushFor() works with extreme thread contention", "[parallelTaskQueue][parallel]") {
    std::atomic<std::size_t> numThreadsRan = 0;
    std::atomic<std::size_t> counter = 0;
    constexpr std::size_t expectedThreadsRan = 1000;
    constexpr std::size_t incrementsPerThread = 10000;
    constexpr std::size_t expectedCounterValue = expectedThreadsRan * incrementsPerThread;

    wordle::parallel::TaskQueue queue(expectedThreadsRan);
    for (size_t threadID = 0; threadID < expectedThreadsRan; ++threadID) {
        queue.push([&numThreadsRan, &counter]() {
            for (size_t i = 0; i < incrementsPerThread; ++i) {
                counter.fetch_add(1);
            }
            numThreadsRan.fetch_add(1);
        });
    }
    queue.wait();
    REQUIRE(numThreadsRan == expectedThreadsRan);
    REQUIRE(counter == expectedCounterValue);
}