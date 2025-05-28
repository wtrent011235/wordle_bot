#pragma once

#include <array>
#include <bit>
#include <boost/integer.hpp>
#include <vector>

#include "config.hpp"
#include "util.hpp"

namespace wordle {

namespace {
    constexpr inline char GREY = 0;
    constexpr inline char YELLOW = 1;
    constexpr inline char GREEN = 2;
    constexpr inline char NUM_COLORS = 3;
    constexpr inline size_t NUM_FEEDBACKS = util::staticPow(NUM_COLORS, config::WORD_LENGTH);
    constexpr inline unsigned int FEEDBACK_BIT_WIDTH = std::bit_width(NUM_FEEDBACKS - 1);
}

using Feedback = boost::uint_t<FEEDBACK_BIT_WIDTH>::least;


struct FeedbackEncoder {
private:
    using count_type = boost::uint_t<std::bit_width(config::WORD_LENGTH)>::least;
    std::array<count_type, 'z' - 'a' + 1> counts;
    static constexpr Feedback bitWidth = FEEDBACK_BIT_WIDTH;
    static constexpr Feedback grey = GREY;
    static constexpr Feedback yellow = YELLOW;
    static constexpr Feedback green = GREEN;
    static constexpr char greyChar = '_';
    static constexpr char yellowChar = 'O';
    static constexpr char greenChar = 'X';
    static constexpr Feedback base = NUM_COLORS;
public:
    static constexpr size_t NUM_ENCODINGS = NUM_FEEDBACKS;
    static constexpr Feedback CORRECT_FEEDBACK = NUM_FEEDBACKS - 1;
    
    // WordEncoding guess;
    std::string_view guess;
    FeedbackEncoder() noexcept = default;

    // Feedback operator()(WordEncoding solution) {
    Feedback operator()(std::string_view solution) {
    counts.fill(0);
        for (size_t i = 0; i < config::WORD_LENGTH; ++i) {
            // ++counts[WordEncoder::at(solution, i)];
            ++counts[solution[i] - 'a'];
        }

        Feedback feedback = 0;
        Feedback multiplier = 1;
        for (size_t i = 0; i < config::WORD_LENGTH; ++i) {
            // auto gEncoding = WordEncoder::at(guess, i);
            // auto sEncoding = WordEncoder::at(solution, i);
            auto gEncoding = guess[i];
            auto sEncoding = solution[i];
            bool isGreen = gEncoding == sEncoding;
            feedback += (isGreen * green * multiplier);
            // counts[sEncoding] -= isGreen;
            counts[sEncoding - 'a'] -= isGreen;
            multiplier *= base;
        }

        multiplier = 1;
        for (size_t i = 0; i < config::WORD_LENGTH; ++i) {
            // auto gEncoding = WordEncoder::at(guess, i);
            // auto sEncoding = WordEncoder::at(solution, i);
            // bool isYellow = gEncoding != sEncoding && counts[gEncoding] > 0;
            auto gEncoding = guess[i];
            auto sEncoding = solution[i];
            bool isYellow = gEncoding != sEncoding && counts[gEncoding - 'a'] > 0;
            feedback += (isYellow * yellow * multiplier);
            counts[gEncoding - 'a'] -= isYellow;
            // counts[gEncoding] -= isYellow;
            multiplier *= base;
        }

        return feedback;
    }

    static Feedback encode(std::string_view feedbackBuff) {
        Feedback feedback = 0, multiplier = 1;

        for (char c : feedbackBuff) {
            switch (c) {
                case greyChar: feedback += grey * multiplier; break;
                case yellowChar: feedback += yellow * multiplier; break;
                case greenChar: feedback += green * multiplier; break;
                default: {
                    util::formatError("Invalid feedback character: {}", c);
                }
            }
            multiplier *= base;
        }

        return feedback;
    }
};

struct FeedbackMap {
private:
    std::vector<Feedback> data;

    size_t toIndex(size_t i, size_t j) const noexcept {
        return i * config::NUM_WORDS + j;
    }
public:
    FeedbackMap() noexcept : data(config::NUM_WORDS * config::NUM_WORDS) {}

    Feedback& operator()(size_t guessIndex, size_t solutionIndex) {
        return data[toIndex(guessIndex, solutionIndex)];
    }

    const Feedback& operator()(size_t guessIndex, size_t solutionIndex) const {
        return data[toIndex(guessIndex, solutionIndex)];
    }
};

}