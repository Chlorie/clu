#include <catch2/catch.hpp>
#include <clu/experimental/wide_integer.h>

using namespace clu::wide_integer_literals;

TEST_CASE("uint128 literals", "[wide_integer]")
{
    REQUIRE(0_u128 == 0); // 0
    REQUIRE(99_u128 == 99); // Not 0
    REQUIRE(0xff_u128 == 0xff); // Hexadecimal
    REQUIRE(077_u128 == 077); // Octal
    REQUIRE(0b11_u128 == 0b11); // Binary
    REQUIRE(1'000'000_u128 == 1'000'000); // Digit separator
    REQUIRE(18'446'744'073'709'551'615_u128 == ~uint64_t()); // Max uint64
    REQUIRE(18'446'744'073'709'551'616_u128 == clu::uint128_t(0, 1)); // Greater than max uint64
    REQUIRE( // Really large number
        0xfedc'ba98'7654'3210'0123'4567'89ab'cdef_u128 ==
        clu::uint128_t(0x123456789abcdefull, 0xfedcba9876543210ull));
    REQUIRE(340'282'366'920'938'463'463'374'607'431'768'211'455_u128 == ~clu::uint128_t()); // Max uint128
}

TEST_CASE("even wider literals", "[wide_integer]")
{
    REQUIRE(0_u256 == 0);
    REQUIRE(0xffff'ffff'ffff'ffff_u256 == ~uint64_t());
    REQUIRE(0xffff'ffff'ffff'ffff'ffff'ffff'ffff'ffff_u256 == ~clu::uint128_t());
    REQUIRE(340'282'366'920'938'463'463'374'607'431'768'211'456_u256 ==
        clu::uint256_t(0, 1)); // Greater than max uint128

    constexpr uint64_t desc = 0xfedc'ba98'7654'3210ull;
    constexpr uint64_t asc = 0x0123'4567'89ab'cdefull;
    REQUIRE(0xfedc'ba98'7654'3210'0123'4567'89ab'cdef'0123'4567'89ab'cdef'fedc'ba98'7654'3210_u256 ==
        clu::uint256_t(
            clu::uint128_t(desc, asc),
            clu::uint128_t(asc, desc)
        )); // Just a stupidly large number, nothing special
}

// TODO: more tests

TEST_CASE("wider integer division", "[wide_integer]")
{
    REQUIRE(0_u128 / 1 == 0);
    REQUIRE(8_u128 / 2 == 4);
    REQUIRE(2_u128 / 3 == 0);
    REQUIRE(42_u128 / 5 == 8);
    REQUIRE((1_u128 << 64) / 2 == (1ull << 63));
    REQUIRE((1_u128 << 64) / 3 == 0x5555'5555'5555'5555ull);
}
