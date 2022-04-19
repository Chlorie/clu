#include <catch2/catch.hpp>

#include "clu/parse.h"

TEST_CASE("parse int", "[parse]")
{
    SECTION("success")
    {
        std::string_view str = "42";
        REQUIRE(clu::parse<int>(str) == 42);
        REQUIRE(clu::parse_consume<int>(str) == 42);
        REQUIRE(str.empty());
    }
    SECTION("no pattern")
    {
        std::string_view str = "hello";
        REQUIRE_FALSE(clu::parse<int>(str));
        REQUIRE_FALSE(clu::parse_consume<int>(str));
    }
    SECTION("too large")
    {
        std::string_view str = "1000000000000";
        REQUIRE_FALSE(clu::parse<int>(str));
        REQUIRE_FALSE(clu::parse_consume<int>(str));
    }
    SECTION("not only int")
    {
        std::string_view str = "42hello";
        REQUIRE_FALSE(clu::parse<int>(str));
        REQUIRE(clu::parse_consume<int>(str) == 42);
        REQUIRE(str == "hello");
    }
}

TEST_CASE("parse float", "[parse]")
{
    SECTION("success")
    {
        std::string_view str = "42.";
        REQUIRE(clu::parse<float>(str) == 42.f);
        REQUIRE(clu::parse_consume<float>(str) == 42.f);
        REQUIRE(str.empty());
    }
    SECTION("no pattern")
    {
        std::string_view str = "hello";
        REQUIRE_FALSE(clu::parse<float>(str));
        REQUIRE_FALSE(clu::parse_consume<float>(str));
    }
    SECTION("too large")
    {
        std::string_view str = "1e+1000";
        REQUIRE_FALSE(clu::parse<float>(str));
        REQUIRE_FALSE(clu::parse_consume<float>(str));
    }
    SECTION("not only float")
    {
        std::string_view str = "42.f";
        REQUIRE_FALSE(clu::parse<float>(str));
        REQUIRE(clu::parse_consume<float>(str) == 42.f);
        REQUIRE(str == "f");
    }
}
