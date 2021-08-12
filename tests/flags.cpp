#include <clu/flags.h>
#include <catch2/catch.hpp>

using namespace clu::flag_enum_operators;

TEST_CASE("flag_enum concept", "[flags]")
{
    enum class no_member {};
    static_assert(!clu::flag_enum<no_member>);
    enum class size_zero { flags_bit_size };
    static_assert(!clu::flag_enum<size_zero>);
    enum class real_flag_enum { first, second, flags_bit_size };
    static_assert(clu::flag_enum<real_flag_enum>);
}

TEST_CASE("flags class size", "[flags]")
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

TEST_CASE("flags operator~", "[flags]")
{
    using enum bits;
    REQUIRE(flags::none_set() == ~flags::all_set());
    REQUIRE((b1 | b2) == ~(b3 | b4));
}

TEST_CASE("flags test set bit", "[flags]")
{
    using enum bits;
    REQUIRE((b1 | b2).is_set(b2));
    REQUIRE((b1 | b2).is_unset(b3));
    REQUIRE((~b4).is_set(b1 | b2));
}
