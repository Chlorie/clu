#include <catch2/catch.hpp>
#include <clu/functional.h>

TEST_CASE("optional and_then", "[functional]")
{
    using opt = std::optional<int>;
    constexpr auto half = [](const int value) -> opt
    {
        if (value % 2 == 0) return value / 2;
        return std::nullopt;
    };
    REQUIRE((opt() | clu::and_then(half)) == std::nullopt);
    REQUIRE((opt(1) | clu::and_then(half)) == std::nullopt);
    REQUIRE((opt(2) | clu::and_then(half)) == 1);
}

TEST_CASE("optional transform", "[functional]")
{
    using opt = std::optional<int>;
    constexpr auto is_even = [](const int value) { return value % 2 == 0; };
    REQUIRE((opt() | clu::transform(is_even)) == std::nullopt);
    REQUIRE((opt(1) | clu::transform(is_even)) == false);
    REQUIRE(opt(2) | clu::transform(is_even));
}

TEST_CASE("optional or_else", "[functional]")
{
    using opt = std::optional<int>;
    constexpr auto just_zero = [] { return 0; };
    constexpr auto just_throw = [] { throw std::runtime_error("nope"); };
    REQUIRE((opt() | clu::or_else(just_zero)) == 0);
    REQUIRE((opt(1) | clu::or_else(just_zero)) == 1);
    REQUIRE_THROWS(opt() | clu::or_else(just_throw));
    REQUIRE_NOTHROW(opt(1) | clu::or_else(just_throw));
}
