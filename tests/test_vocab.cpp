#include <catch2/catch_test_macros.hpp>

#include "../src/util.hpp"
#include "../src/vocab.hpp"

TEST_CASE("Vocab: validate processFile() output", "[vocab]") {
    SECTION("Targets") {
        wordle::vocab::Vocab targets;
        targets = wordle::vocab::__impl::processFile(wordle::config::TARGET_FILE, wordle::config::NUM_TARGETS);
        REQUIRE(targets.size() == wordle::config::NUM_TARGETS);
        REQUIRE(std::all_of(targets.begin(), targets.end(), wordle::util::isValidWord));
    }

    SECTION("Fillers") {
        wordle::vocab::Vocab fillers;
        fillers = wordle::vocab::__impl::processFile(wordle::config::FILLER_FILE, wordle::config::NUM_FILLERS);;
        REQUIRE(fillers.size() == wordle::config::NUM_FILLERS);
        REQUIRE(std::all_of(fillers.begin(), fillers.end(), wordle::util::isValidWord));
    }
}

TEST_CASE("Vocab: validate constructVocab() output", "[vocab]") {
    auto targets = wordle::vocab::__impl::processFile(wordle::config::TARGET_FILE, wordle::config::NUM_TARGETS);
    auto fillers = wordle::vocab::__impl::processFile(wordle::config::FILLER_FILE, wordle::config::NUM_FILLERS);
    auto vocab = wordle::vocab::constructVocab();
    REQUIRE(vocab.size() == (targets.size() + fillers.size()));
    REQUIRE(std::equal(targets.begin(), targets.end(), vocab.begin()));
    REQUIRE(std::equal(fillers.begin(), fillers.end(), vocab.begin() + wordle::config::NUM_TARGETS));
}