#include <catch2/catch.hpp>

#include "clu/expected.h"

TEST_CASE("expected default constructor", "[expected]")
{
    const clu::expected<int, int> ex;
    REQUIRE(ex);
    REQUIRE(*ex == 0);
    static_assert(std::is_trivially_copyable_v<clu::expected<int, int>>);
}

TEST_CASE("expected with reference type", "[expected]")
{
    int val = 0;
    const clu::expected<int&, int> ex = val;
    REQUIRE(ex);
    REQUIRE(*ex == 0);
    val = 1;
    REQUIRE(*ex == 1);
}
