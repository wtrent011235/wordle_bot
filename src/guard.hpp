#pragma once

#include <format>
#include <stdexcept>
#include <utility>
#include <string> // Required for std::string(msg) in formatError overload

namespace wordle::guard {

// Throws RuntimeException using fmt and ...args
template <typename RuntimeException = std::runtime_error, typename ...Args>
[[noreturn]] inline void formatError(std::format_string<Args...> fmt, Args&&... args) {
    throw RuntimeException(std::format(fmt, std::forward<Args>(args)...));
}

// Throws staticMsg at compile time, otherwise Exception(staticMsg) at compile time
template <typename Exception = std::runtime_error>
[[noreturn]] constexpr inline void hybridError(std::string_view staticMsg) {
    if (std::is_constant_evaluated()) {
        throw staticMsg;
    } else {
        throw Exception(std::string{staticMsg});
    }
}

/* 
Guard noExceptCond at compile time (if possible) otherwise runtime
*/
template <typename Exception = std::runtime_error>
constexpr inline void hybridGuard(bool noExceptCond, std::string_view staticMsg) {
    // Both branches call throwError, which handles the constant_evaluated check internally.
    if (std::is_constant_evaluated()) {
        if (!noExceptCond) throw staticMsg;
    } else {
        if (!noExceptCond) throw Exception(std::string{staticMsg});
    }
}

// Guard noExceptCond at runtime
template <typename Exception = std::runtime_error, typename ...Args>
inline void runtimeGuard(bool noExceptCond, std::format_string<Args...> fmt, Args&&... args) {
    if (!noExceptCond) formatError(fmt, std::forward<Args>(args)...);
}

} // end namespace wordle::guard