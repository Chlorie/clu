#include <utility>
#include <catch2/catch_test_macros.hpp>

#include "clu/overload.h"

int f() { return 42; }

inline constexpr struct final_callable final
{
    int operator()(int) noexcept { return 0; }
    int operator()(int) const noexcept { return 1; }
} g;

inline constexpr auto h = [](int, int) { return 100; };

TEST_CASE("overload", "[overload]")
{
    auto ov = clu::overload(f, g, h);
    SECTION("final callable and C function")
    {
        REQUIRE(ov() == 42);
        REQUIRE(ov(0) == 0);
        REQUIRE(ov(0, 0) == 100);
    }
    SECTION("const correctness")
    {
        REQUIRE(ov(0) == 0);
        REQUIRE(std::as_const(ov)(0) == 1);
    }
    SECTION("overload selection")
    {
        // clang-format off
        constexpr auto o = clu::overload(
            [](long) { return 1; },
            [](int) { return 2; }
        );
        // clang-format on
        REQUIRE(o(0l) == 1);
        REQUIRE(o(0) == 2);
    }
}

TEST_CASE("first overload", "[overload]")
{
    auto ov = clu::first_overload(f, g, h);
    SECTION("final callable and C function")
    {
        REQUIRE(ov() == 42);
        REQUIRE(ov(0) == 0);
        REQUIRE(ov(0, 0) == 100);
    }
    SECTION("const correctness")
    {
        REQUIRE(ov(0) == 0);
        REQUIRE(std::as_const(ov)(0) == 1);
    }
    SECTION("overload selection")
    {
        // clang-format off
        constexpr auto o = clu::first_overload(
            [](long) { return 1; },
            [](int) { return 2; }
        );
        // clang-format on
        REQUIRE(o(0l) == 1);
        REQUIRE(o(0) == 1);
    }
}
