#pragma once

#include <vector>
#include <thread>

#include "guard.hpp"

namespace wordle::parallel {
    template <typename Func, typename... Args>
    concept VoidCallable = std::is_invocable_r_v<void, Func, Args...>;

    struct TaskQueue {    
    private:
        std::vector<std::thread> threadPool;

    public:
        TaskQueue() noexcept = default;
        TaskQueue(std::size_t initialCapacity) noexcept {
            threadPool.reserve(initialCapacity);
        }
        ~TaskQueue() { wait(); }

        template <typename Func, typename... Args>
            requires VoidCallable<Func, Args...>
        void push(Func&& func, Args&&... args) {
            threadPool.emplace_back(std::forward<Func>(func), std::forward<Args>(args)...);
        }

        // Blocks local thread until all tasks finish
        void wait() {
            while (!threadPool.empty()) {
                auto thread = std::move(threadPool.back());
                threadPool.pop_back();
                thread.join();
            }
        }

        // Returns number of threads in TaskQueue
        size_t size() const noexcept {
            return threadPool.size();
        }

        // Returns capacity of threadPool
        size_t capacity() const noexcept {
            return threadPool.capacity();
        }
    };
}