#include <catch2/catch_test_macros.hpp>

#include "clu/indices.h"
#include "matchers.h"

namespace ct = clu::testing;

TEST_CASE("one dimensional index", "[indices]")
{
    using arr = std::array<size_t, 1>;
    const std::vector<arr> elems{{0}, {1}, {2}, {3}, {4}};
    const auto idx = clu::indices(5);
    REQUIRE_THAT(idx, ct::EqualsRange(elems));
    REQUIRE(idx.size() == 5);
    REQUIRE(decltype(idx)::dimension() == 1);
    REQUIRE(idx.extents() == arr{5});
    REQUIRE(std::ranges::empty(clu::indices(0)));
}

TEST_CASE("multi-dimensional indices", "[indices]")
{
    using arr = std::array<size_t, 2>;
    const std::vector<arr> elems{{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}};
    const auto idx = clu::indices(2, 3);
    REQUIRE_THAT(idx, ct::EqualsRange(elems));
    REQUIRE(idx.size() == 6);
    REQUIRE(decltype(idx)::dimension() == 2);
    REQUIRE(idx.extents() == (arr{2, 3}));
}

TEST_CASE("index random access", "[indices]")
{
    using arr = std::array<size_t, 3>;
    const auto idx = clu::indices(3, 4, 5);
    REQUIRE(idx.size() == 60);
    REQUIRE(idx.begin()[29] == (arr{1, 1, 4}));
    REQUIRE(*(idx.begin() + 40 - 23) == (arr{0, 3, 2}));
    REQUIRE(idx.begin() + 30 > idx.begin() + 12);
    REQUIRE((idx.begin() + 12) - (idx.begin() + 25) == -13);
}
