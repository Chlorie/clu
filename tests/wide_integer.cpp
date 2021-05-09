#include <gtest/gtest.h>
#include <clu/wide_integer.h>

using namespace clu::wide_integer_literals;

TEST(WideInteger, Uint128Udl)
{
    EXPECT_EQ(0_u128, 0); // 0
    EXPECT_EQ(99_u128, 99); // Not 0
    EXPECT_EQ(0xff_u128, 0xff); // Hexadecimal
    EXPECT_EQ(077_u128, 077); // Octal
    EXPECT_EQ(0b11_u128, 0b11); // Binary
    EXPECT_EQ(1'000'000_u128, 1'000'000); // Digit separator
    EXPECT_EQ(18'446'744'073'709'551'615_u128, ~uint64_t()); // Max uint64
    EXPECT_EQ(18'446'744'073'709'551'616_u128, clu::uint128_t(0, 1)); // Greater than max uint64
    EXPECT_EQ( // Really large number
        0xfedc'ba98'7654'3210'0123'4567'89ab'cdef_u128,
        clu::uint128_t(0x123456789abcdefull, 0xfedcba9876543210ull));
    EXPECT_EQ(340'282'366'920'938'463'463'374'607'431'768'211'455_u128, ~clu::uint128_t()); // Max uint128
}

TEST(WideInteger, EvenWiderUdl)
{
    EXPECT_EQ(0_u256, 0);
    EXPECT_EQ(0xffff'ffff'ffff'ffff_u256, ~uint64_t());
    EXPECT_EQ(0xffff'ffff'ffff'ffff'ffff'ffff'ffff'ffff_u256, ~clu::uint128_t());
    EXPECT_EQ(340'282'366'920'938'463'463'374'607'431'768'211'456_u256,
        clu::uint256_t(0, 1)); // Greater than max uint128

    constexpr uint64_t desc = 0xfedc'ba98'7654'3210ull;
    constexpr uint64_t asc = 0x0123'4567'89ab'cdefull;
    EXPECT_EQ(0xfedc'ba98'7654'3210'0123'4567'89ab'cdef'0123'4567'89ab'cdef'fedc'ba98'7654'3210_u256,
        clu::uint256_t(
            clu::uint128_t(desc, asc),
            clu::uint128_t(asc, desc)
        )); // Just a stupidly large number, nothing special
}

// TODO: more tests

TEST(WideInteger, Division)
{
    EXPECT_EQ(0_u128 / 1, 0);
    EXPECT_EQ(8_u128 / 2, 4);
    EXPECT_EQ(2_u128 / 3, 0);
    EXPECT_EQ(42_u128 / 5, 8);
    EXPECT_EQ((1_u128 << 64) / 2, 1ull << 63);
    EXPECT_EQ((1_u128 << 64) / 3, 0x5555'5555'5555'5555ull);
}
