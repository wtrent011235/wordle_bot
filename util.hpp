#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace wordle {

namespace util {
// static time pow
consteval inline size_t staticPow(size_t base, size_t exp) {
    if (exp == 0) return 1;
    if (base <= 1) return base;
    if (exp % 2 == 0) return staticPow(base * base, exp / 2);
    return base * staticPow(base, exp - 1);
}

template <std::integral T>
consteval inline T smallestMask(T bitWidth) {
    T mask = 0;
    for (T i = 0; i < bitWidth; ++i) {
        mask = (mask << T{1}) | T{1};
    }
    return mask;
}

// Throws a formatted exception using the provided message and arguments.
template <typename Exception = std::runtime_error, typename... Args>
[[noreturn]] inline void formatError(std::format_string<Args...> msg, Args&&... args) {
    throw Exception(std::format(msg, std::forward<Args>(args)...));
}

// Throws a formatted exception if the provided condition is false.
template <typename Exception = std::runtime_error, typename...Args>
inline void formatGuard(bool noExceptCond, std::format_string<Args...> msg, Args&&... args) {
    if (noExceptCond) return;
    formatError<Exception>(msg, std::forward<Args>(args)...);
}

template <typename Exception = std::runtime_error>
inline void guard(bool noExceptCond, std::string_view msg) {
    if (noExceptCond) return;
    throw Exception(msg);
}

template <typename T>
concept RandomAccessContainer = 
    std::ranges::sized_range<T> &&               // has size()
    std::ranges::random_access_range<T> &&       // supports operator[], +, -, etc.
    requires(T a, size_t i) {
        { a[i] } -> std::convertible_to<typename T::value_type>;
        { a.size() } -> std::convertible_to<std::size_t>;
    };

/*
Takes a function that returns true if a container element is valid.
Returns an index n, where elements 0 through n - 1 in the container are valid.
*/
template <typename T, typename const_reference = const typename T::value_type&>
requires RandomAccessContainer<T>
inline size_t shiftFilter(T& container, auto&& conditionFunc) {
    size_t left = 0;
    size_t right = container.size(); // One-past-the-end of valid elements

    while (left < right) {
        if (conditionFunc(container[left])) {
            ++left;
        } else {
            std::swap(container[left], container[--right]);
        }
    }
    return right;
}

inline void showProgressBar(size_t current, size_t total, size_t barWidth = 50) {
    double progress = static_cast<double>(current) / total;
    size_t pos = static_cast<size_t>(barWidth * progress);

    std::cout << "\r[";
    for (size_t i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] "
              << std::fixed << std::setprecision(2)
              << std::setw(6) << (progress * 100.0) << "%   "
              << std::flush;
}

}

}