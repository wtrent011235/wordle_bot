#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_predicate.hpp>
#include "../src/guard.hpp"

using namespace wordle::guard;

template<typename E = std::exception>
auto ExceptionMessageContains(std::string_view part) {
    return Catch::Matchers::Predicate<E>(
        [part](const E& e) { return std::string(e.what()).find(part) != std::string::npos; },
        "exception message contains: " + std::string(part)
    );
}

TEST_CASE("Test format Error", "[guard][error]") {
    auto errMsg = std::format("basic error {}", "message");

    REQUIRE_THROWS_MATCHES(
        formatError<std::runtime_error>("basic error {}", "message"),
        std::runtime_error,
        ExceptionMessageContains<>("basic error message")
    );
}

TEST_CASE("Test hybrid Error", "[guard][error][constexpr]") {
    SECTION("Runtime") {
        REQUIRE_THROWS_MATCHES(
            hybridError<std::runtime_error>("weeee"),
            std::runtime_error,
            ExceptionMessageContains<>("weeee")
        );
        REQUIRE_THROWS_MATCHES(
            hybridError<std::domain_error>("woooo"),
            std::domain_error,
            ExceptionMessageContains<>("woooo")
        );
    }

    SECTION("Compile time (uncomment to check...)") {
        // constexpr auto noCompileLambda = []() -> bool { hybridError("no compile"); return true; };
        // STATIC_CHECK(noCompileLambda());
    }
}

TEST_CASE("Test hybrid Guard", "[guard][guard][constexpr]") {
    SECTION("Runtime") {
        REQUIRE_NOTHROW(hybridGuard(true, "this should be fine"));
        REQUIRE_THROWS_MATCHES(
            hybridGuard<std::overflow_error>(false, "oops"),
            std::overflow_error,
            ExceptionMessageContains<>("oops")
        );
    }

    SECTION("Compile time (uncomment to check no compile tests)") {
        constexpr auto goodLambda = []() { hybridGuard(true, ""); return true; };
        STATIC_REQUIRE(goodLambda());
        // constexpr auto badLambda = []() { hybridGuard(false, ""); return true; };
        // STATIC_REQUIRE(!badLambda());
    }
}
