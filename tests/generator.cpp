#include <catch2/catch_test_macros.hpp>
#include <ranges>

#include "clu/generator.h"
#include "matchers.h"

namespace ct = clu::testing;
namespace sv = std::views;

TEST_CASE("infinite generator", "[generator]")
{
    const auto infinite = []() -> clu::generator<int>
    {
        for (int i = 0;; i++)
            co_yield i;
    };
    REQUIRE_THAT(infinite() | sv::take(5), ct::EqualsRange({0, 1, 2, 3, 4}));
}

TEST_CASE("single generator", "[generator]")
{
    const auto single = []() -> clu::generator<int> { co_yield 42; };
    REQUIRE_THAT(single(), ct::EqualsRange({42}));
}

TEST_CASE("recursive generator", "[generator]")
{
    SECTION("with generator")
    {
        const auto f = []() -> clu::generator<int>
        {
            co_yield 1;
            co_yield 2;
        };
        const auto g = [&]() -> clu::generator<int>
        {
            co_yield clu::elements_of(f());
            co_yield 3;
        };
        REQUIRE_THAT(g(), ct::EqualsRange({1, 2, 3}));
    }

    SECTION("with other ranges")
    {
        const auto g = [&]() -> clu::generator<int>
        {
            std::vector vec{1, 2};
            co_yield clu::elements_of(vec);
            co_yield 3;
        };
        REQUIRE_THAT(g(), ct::EqualsRange({1, 2, 3}));
    }
}
