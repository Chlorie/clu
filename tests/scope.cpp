#include <catch2/catch.hpp>

#include "clu/scope.h"

TEST_CASE("scope on success", "[scope]")
{
    bool exit = false, success = false, fail = false;
    {
        clu::scope_exit se([&] { exit = true; });
        clu::scope_success ss([&] { success = true; });
        clu::scope_fail sf([&] { fail = true; });
    }
    REQUIRE(exit);
    REQUIRE(success);
    REQUIRE_FALSE(fail);
}

TEST_CASE("scope on exception", "[scope]")
{
    bool exit = false, success = false, fail = false;
    try
    {
        clu::scope_exit se([&] { exit = true; });
        clu::scope_success ss([&] { success = true; });
        clu::scope_fail sf([&] { fail = true; });
    }
    catch (...)
    {
        REQUIRE(exit);
        REQUIRE_FALSE(success);
        REQUIRE(fail);
    }
}
