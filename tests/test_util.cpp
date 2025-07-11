#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "../src/util.hpp"

using namespace wordle::util;


TEST_CASE("Test constevalPow", "[util][consteval]") {
    constexpr size_t b1 = 0, p1 = 0, output1 = constevalPow(b1, p1);
    constexpr size_t b2 = 9, p2 = 3, output2 = constevalPow(b2, p2);
    constexpr size_t b3 = 11, p3 = 11, output3 = constevalPow(b3, p3);
    REQUIRE(output1 == static_cast<size_t>(std::pow(b1, p1)));
    REQUIRE(output2 == static_cast<size_t>(std::pow(b2, p2)));
    REQUIRE(output3 == static_cast<size_t>(std::pow(b3, p3)));
}


TEST_CASE("Test Typemaker", "[util][constexpr]") {
    STATIC_REQUIRE(std::is_same_v<TypeMaker<10, true, true>::Type, boost::uint_t<10>::least>);
    STATIC_REQUIRE(std::is_same_v<TypeMaker<10, true, false>::Type, boost::int_t<10>::least>);
    STATIC_REQUIRE(std::is_same_v<TypeMaker<10, false, false>::Type, boost::int_t<10>::fast>);
    STATIC_REQUIRE(std::is_same_v<TypeMaker<10, false, true>::Type, boost::uint_t<10>::fast>);
}

template <std::integral T, T minVal, T maxVal>
constexpr bool testRangeEncoder() noexcept {
    using encoder = RangeEncoder<T, minVal, maxVal>;

    if constexpr (!encoder::IS_VALID) return false;
    if constexpr (encoder::encode(minVal)) return false;

    for (T valOne = minVal; valOne <= maxVal; ++valOne) {
        auto encodingOne = encoder::encode(valOne);
        auto decodingOne = encoder::decode(encodingOne);

        if (valOne != decodingOne) return false;

        for (T valTwo = valOne; valTwo <= maxVal; ++valTwo) {
            auto encodingTwo = encoder::encode(valTwo);
            auto decodingTwo = encoder::decode(encodingTwo);

            auto valDelta = static_cast<size_t>(valTwo - valOne);
            auto encodingDelta = static_cast<size_t>(encodingTwo - encodingOne);
            auto decodingDelta = static_cast<size_t>(decodingTwo - decodingOne);

            if (valDelta != encodingDelta || encodingDelta != decodingDelta) return false;
        }
    }

    return true;
}

TEST_CASE("Test Range Encoder", "[util][constexpr]") {
    STATIC_REQUIRE(testRangeEncoder<char, 'a', 'z'>());
    STATIC_REQUIRE(testRangeEncoder<char, -10, 0>());
}


TEST_CASE("BitIndexMap", "[util]") {
    using Letter = char;
    constexpr Letter minLetter = 'a', maxLetter = 'z';
    using BitIndexMapT = BitIndexMap<Letter, minLetter, maxLetter>;
    BitIndexMapT bm{};
    using LetterEncoding = decltype(bm)::Encoding;

    REQUIRE(bm.raw() == LetterEncoding{0});
    for (Letter letter = 'a'; letter <= 'z'; ++letter) {
        REQUIRE_NOTHROW(bm.set(letter)); // set letter
        REQUIRE(bm.contains(letter));  // assert letter was set
        REQUIRE_NOTHROW(bm.index(letter));
        LetterEncoding index = bm.index(letter);
        REQUIRE(index == static_cast<LetterEncoding>(letter - minLetter));  // assert correct index
        REQUIRE_NOTHROW(bm.unset(letter));  // unset letter
        REQUIRE_FALSE(bm.contains(letter));  // ensure that letter is unset
        REQUIRE_THROWS(bm.index(letter));  // ensure calling index throws
        REQUIRE_NOTHROW(bm.set(letter));  // set letter
        REQUIRE(bm.contains(letter));  // ensure that letter is still set
        REQUIRE(bm.index(letter) == index);  // ensure that index has not changed
    }

    // Check copy
    BitIndexMapT bmCopy{bm};
    REQUIRE(bm == bmCopy);
    REQUIRE(bm.begin() == bmCopy.begin());
    REQUIRE(bm.end() == bmCopy.end());

    // Check copy assignment
    BitIndexMapT bmCA = bm;
    REQUIRE(bm == bmCA);

    // Check move
    BitIndexMapT bmMove{std::move(bmCA)};
    REQUIRE(bm == bmMove);

    // Check move assignment
    bmCopy = std::move(bmMove);
    REQUIRE(bm == bmCopy);

    // Check iterator
    auto start = bm.begin(), end = bm.end();
    bm.reset();  // Ensure that iterator is independent of bm state
    REQUIRE(bm.raw() == LetterEncoding{0});
    
    Letter letter = minLetter;
    while (start != end) {
        REQUIRE(*start == letter);
        ++letter;
        ++start;
    }
    REQUIRE(start == end);
    REQUIRE(letter > maxLetter);

    // Check insertion
    REQUIRE_NOTHROW(bm.set(maxLetter));
    REQUIRE(bm.index(maxLetter) == LetterEncoding{0});

    REQUIRE_NOTHROW(bm.set(minLetter));
    REQUIRE(bm.index(minLetter) == LetterEncoding{0});
    REQUIRE(bm.index(maxLetter) == LetterEncoding{1});

    REQUIRE_NOTHROW(bm.unset(minLetter));
    REQUIRE_FALSE(bm.contains(minLetter));
    REQUIRE_THROWS(bm.index(minLetter));
    REQUIRE(bm.index(maxLetter) == LetterEncoding{0});
}