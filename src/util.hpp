#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <boost/integer.hpp>

#include "config.hpp"
#include "guard.hpp"

namespace wordle::util {

// COMPILE TIME: pow implementation
template <std::unsigned_integral T = size_t, typename R = T>
consteval inline R constevalPow(T base, T power) {
    if (power == T{0}) return R{1};
    if (base <= T{1}) return static_cast<R>(base);
    if (power % T{2} == T{0}) return constevalPow(base * base, power / T{2});
    return static_cast<R>(base) * constevalPow(base, power - 1);
}

template <std::unsigned_integral T = size_t, typename R = T>
constexpr inline R constexprPow(T base, T power) {
    if (power == T{0}) return R{1};
    if (base <= T{1}) return static_cast<R>(base);
    if (power % T{2} == T{0}) return constexprPow(base * base, power / T{2});
    return static_cast<R>(base) * constexprPow(base, power - 1);
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

/*
Given the bits needed to hold the type, returns the fastest/smallest unsigned/signed type that holds it
*/
template <int Bits, bool Least = true, bool Unsigned = true>
struct TypeMaker {
private:
    template <bool L, bool U>
    struct Selector;

    // Least + Unsigned
    template <>
    struct Selector<true, true> {
        using type = typename boost::uint_t<Bits>::least;
    };

    // Least + Signed
    template <>
    struct Selector<true, false> {
        using type = typename boost::int_t<Bits>::least;
    };

    // Fast + Unsigned
    template <>
    struct Selector<false, true> {
        using type = typename boost::uint_t<Bits>::fast;
    };

    // Fast + Signed
    template <>
    struct Selector<false, false> {
        using type = typename boost::int_t<Bits>::fast;
    };

public:
    using Type = typename Selector<Least, Unsigned>::type;
};

/*
Encodes a domain of continuous values [minVal, maxVal] into a compressed range.
UNSAFE: Must validate val is in domain and encoding is in range.

static constexpr int BIT_WIDTH = number of bits needed to encode range
Encoding : smallest uint required to encode all values
Decoding : T
static constexpr Encoding encode(Decoding) noexcept : encodes a value into the compressed range. (NOTE: USER must ensure that val lies within templated domain.)
static constexpr Decoding decode(Encoding) noexcept : decodes an encoding.  (NOTE: USER must ensure that encoding lies within compressed range.)
*/
template <std::integral T, T minVal, T maxVal, bool Least = true>
requires (minVal < maxVal)
struct RangeEncoder {
    static constexpr unsigned BIT_WIDTH = std::bit_width(static_cast<unsigned>(maxVal - minVal));
    static constexpr bool IS_VALID = BIT_WIDTH <= 64u;
    using Encoding = TypeMaker<BIT_WIDTH, Least, true>::Type;
    using Decoding = T;

public:
    static constexpr Encoding encode(Decoding val) noexcept {
        return static_cast<Encoding>(val - minVal);
    }

    static constexpr Decoding decode(Encoding encoding) noexcept {
        return static_cast<Decoding>(encoding) + minVal;
    }
};

/*
Efficiently maps a bounded integral domain to vector indices without storing a map or sorting.
*/
template <std::integral T, T minVal, T maxVal, bool Least = true>
requires (minVal < maxVal)
struct BitIndexMap {
private:
    static constexpr unsigned BITS = static_cast<unsigned>(maxVal - minVal) + 1u;
    static constexpr bool IS_VALID = BITS <= 64u;

public:
    using Encoding = TypeMaker<BITS, Least, true>::Type;

private:
    // Returns true if val is within range
    constexpr bool isValidVal(T val) const noexcept {
        return (minVal <= val && val <= maxVal);
    }

    // Returns index of val. Undefined behavior if val is not in range
    constexpr Encoding unsafeBit(T val) const noexcept {
        return Encoding{1} << static_cast<unsigned>(val - minVal);
    }

    // Returns index of val. Errors if val is not in range
    constexpr Encoding safeBit(T val) const {
        guard::hybridGuard<std::out_of_range>(isValidVal(val), "val is out of range");
        return unsafeBit(val);
    }
private:
    Encoding bits = 0;  // Underlying bits

public:
    constexpr BitIndexMap() noexcept = default;
    constexpr BitIndexMap(const BitIndexMap&) noexcept = default;
    constexpr BitIndexMap(BitIndexMap&&) noexcept = default;
    constexpr BitIndexMap& operator=(const BitIndexMap&) noexcept = default;
    constexpr BitIndexMap& operator=(BitIndexMap&&) noexcept = default;
    ~BitIndexMap() noexcept = default;

    constexpr bool operator==(const BitIndexMap& other) const noexcept { return bits == other.bits; }

    constexpr bool operator!=(const BitIndexMap& other) const noexcept { return bits != other.bits; }
    
    // Adds val
    constexpr void set(T val) {
        bits |= safeBit(val);
    }

    // Removes val
    constexpr void unset(T val) {
        bits &= ~safeBit(val);
    }

    // Returns true if val is contained
    constexpr bool contains(T val) const noexcept {
        return isValidVal(val) && (bits & unsafeBit(val));
    }

    // Returns index of val.
    constexpr Encoding index(T val) const {
        guard::hybridGuard<std::out_of_range>(isValidVal(val), "Invalid val passed");
        guard::hybridGuard<std::out_of_range>(contains(val), "val is not in bitmap");
        auto mask = unsafeBit(val) - Encoding{1};
        return std::popcount(bits & mask);
    }

    // Removes all indices from bitset
    constexpr void reset() noexcept {
        bits = 0;
    }

    // Returns underlying bits
    constexpr Encoding raw() const noexcept {
        return bits;
    }

    // Value iterator
    struct Iterator {
    private:
        Encoding bits;
    public:
        constexpr Iterator(Encoding bits) noexcept : bits{bits} {}

        constexpr T operator*() const {
            guard::hybridGuard(bits != 0, "Stopped attempt to dereference exhausted iterator");
            Encoding bit = std::countr_zero(bits);  // Find least significant bit
            return static_cast<T>(bit) + minVal;    // Decode and return
        }

        constexpr Iterator& operator++() {
            guard::hybridGuard<std::out_of_range>(bits != 0, "Stopped attempt to increment exhausted iterator");
            bits &= (bits - 1);  // Clear the least significant bit
            return *this;
        }

        constexpr Iterator operator++(int) {
            guard::hybridGuard<std::out_of_range>(bits != 0, "Stopped attempt to increment exhausted iterator");

            Iterator retVal = *this;
            ++(*this);
            return retVal;
        }

        constexpr bool operator==(const Iterator& other) const noexcept { return bits == other.bits; }

        constexpr bool operator!=(const Iterator& other) const noexcept { return bits != other.bits; }
    };

    constexpr Iterator begin() const noexcept {
        return Iterator{bits};
    }

    constexpr Iterator end() const noexcept {
        return Iterator{0};
    }
};

constexpr inline bool isValidLetter(char c) noexcept {
    return 'a' <= c && c <= 'z';
}

// Checks size and letters
constexpr inline bool isValidWord(std::string_view word) noexcept {
    if (word.size() != config::WORD_LENGTH) return false;
    return std::all_of(word.begin(), word.end(), isValidLetter);
}

}
