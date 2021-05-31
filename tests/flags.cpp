#include <gtest/gtest.h>
#include <clu/flags.h>

using namespace clu::flag_enum_operators;

TEST(Flags, FlagEnumConcept)
{
    enum class no_member {};
    static_assert(!clu::flag_enum<no_member>);
    enum class size_zero { flags_bit_size };
    static_assert(!clu::flag_enum<size_zero>);
    enum class real_flag_enum { first, second, flags_bit_size };
    static_assert(clu::flag_enum<real_flag_enum>);
}

TEST(Flags, ClassSize)
{
    enum class one { flags_bit_size = 8 };
    static_assert(sizeof(clu::flags<one>) == 1);
    enum class eight { flags_bit_size = 64 };
    static_assert(sizeof(clu::flags<eight>) == 8);
}

enum class bits
{
    b1, b2, b3, b4,
    flags_bit_size
};

using flags = clu::flags<bits>;

TEST(Flags, Invert)
{
    using enum bits;
    EXPECT_EQ(flags::none_set(), ~flags::all_set());
    EXPECT_EQ(b1 | b2, ~(b3 | b4));
}

TEST(Flags, TestSetBit)
{
    using enum bits;
    EXPECT_TRUE((b1 | b2).is_set(b2));
    EXPECT_TRUE((b1 | b2).is_unset(b3));
    EXPECT_TRUE((~b4).is_set(b1 | b2));
}
