#include <catch2/catch_test_macros.hpp>

#include "clu/rational.h"

TEST_CASE("rational constructors", "[rational]")
{
    SECTION("default")
    {
        constexpr clu::rational<int> r;
        REQUIRE(r.numerator() == 0);
        REQUIRE(r.denominator() == 1);
    }

    SECTION("from int")
    {
        constexpr clu::rational r = 5;
        REQUIRE(r.numerator() == 5);
        REQUIRE(r.denominator() == 1);
    }

    SECTION("from numerator and denominator")
    {
        constexpr clu::rational r1(3, 4);
        REQUIRE(r1.numerator() == 3);
        REQUIRE(r1.denominator() == 4);

        constexpr clu::rational r2(0, -2);
        REQUIRE(r2.numerator() == 0);
        REQUIRE(r2.denominator() == 1);

        constexpr clu::rational r3(-6, -8);
        REQUIRE(r3.numerator() == 3);
        REQUIRE(r3.denominator() == 4);
    }
}

TEST_CASE("rational arithmetic operators", "[rational]")
{
    SECTION("addition")
    {
        constexpr clu::rational r1 = clu::rational(1, 2) + clu::rational(1, 3);
        REQUIRE(r1.numerator() == 5);
        REQUIRE(r1.denominator() == 6);
        constexpr clu::rational r2 = clu::rational(1, 2) + 2;
        REQUIRE(r2.numerator() == 5);
        REQUIRE(r2.denominator() == 2);
    }

    SECTION("subtraction")
    {
        constexpr clu::rational r1 = clu::rational(1, 2) - clu::rational(1, 3);
        REQUIRE(r1.numerator() == 1);
        REQUIRE(r1.denominator() == 6);
        constexpr clu::rational r2 = clu::rational(1, 2) - 2;
        REQUIRE(r2.numerator() == -3);
        REQUIRE(r2.denominator() == 2);
    }

    SECTION("multiplication")
    {
        constexpr clu::rational r1 = clu::rational(1, 2) * clu::rational(1, 3);
        REQUIRE(r1.numerator() == 1);
        REQUIRE(r1.denominator() == 6);
        constexpr clu::rational r2 = clu::rational(1, 2) * 2;
        REQUIRE(r2.numerator() == 1);
        REQUIRE(r2.denominator() == 1);
    }

    SECTION("division")
    {
        constexpr clu::rational r1 = clu::rational(3, 4) / clu::rational(1, 6);
        REQUIRE(r1.numerator() == 9);
        REQUIRE(r1.denominator() == 2);
        constexpr clu::rational r2 = clu::rational(3, 4) / 6;
        REQUIRE(r2.numerator() == 1);
        REQUIRE(r2.denominator() == 8);
    }

    SECTION("absolute value")
    {
        constexpr clu::rational r1 = abs(clu::rational(3, 4));
        REQUIRE(r1.numerator() == 3);
        REQUIRE(r1.denominator() == 4);
        constexpr clu::rational r2 = abs(clu::rational(-3, 4));
        REQUIRE(r2.numerator() == 3);
        REQUIRE(r2.denominator() == 4);
        constexpr clu::rational r3 = abs(clu::rational(0));
        REQUIRE(r3.numerator() == 0);
        REQUIRE(r3.denominator() == 1);
    }
}

TEST_CASE("rational comparison", "[rational]")
{
    SECTION("equality")
    {
        REQUIRE(clu::rational(1, 2) == clu::rational(5, 10));
        REQUIRE(clu::rational(1, 2) != clu::rational(5, 9));
        REQUIRE(clu::rational(4, 2) == 2);
        REQUIRE(clu::rational(4, 2) != 3);
        REQUIRE(clu::rational(5, 3) == clu::rational(-10, -6));
    }

    SECTION("inequality")
    {
        REQUIRE(clu::rational(1, 2) > clu::rational(1, 3));
        REQUIRE(clu::rational(1, 2) > clu::rational(-1, 3));
        REQUIRE(clu::rational(6, 2) < clu::rational(7, 2));
    }
}
