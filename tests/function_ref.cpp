#include <catch2/catch.hpp>

#include "clu/function_ref.h"

TEST_CASE("empty function reference", "[function_ref]")
{
    const clu::function_ref<void()> fref;
    REQUIRE_FALSE(fref);
}

void cfunc(bool& value) { value = true; }

TEST_CASE("C function reference", "[function_ref]")
{
    const clu::function_ref fref = cfunc;
    REQUIRE(fref);
    bool value = false;
    fref(value);
    REQUIRE(value);
}

TEST_CASE("lambda reference", "[function_ref]")
{
    const auto lambda = [i = 40](const int value) { return i + value; };
    const clu::function_ref<int(int)> fref = lambda;
    REQUIRE(fref(2) == 42);
}
