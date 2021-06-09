#include <gtest/gtest.h>
#include <clu/functional.h>

TEST(Functional, AndThen)
{
    using opt = std::optional<int>;
    const auto half = [](const int value) -> opt
    {
        if (value % 2 == 0) return value / 2;
        return std::nullopt;
    };
    EXPECT_EQ(opt() | clu::and_then(half), std::nullopt);
    EXPECT_EQ(opt(1) | clu::and_then(half), std::nullopt);
    EXPECT_EQ(opt(2) | clu::and_then(half), 1);
}

TEST(Functional, Transform)
{
    using opt = std::optional<int>;
    const auto is_even = [](const int value) { return value % 2 == 0; };
    EXPECT_EQ(opt() | clu::transform(is_even), std::nullopt);
    EXPECT_EQ(opt(1) | clu::transform(is_even), false);
    EXPECT_EQ(opt(2) | clu::transform(is_even), true);
}

TEST(Functional, OrElse)
{
    using opt = std::optional<int>;
    const auto just_zero = [] { return 0; };
    const auto just_throw = [] { throw std::runtime_error("nope"); };
    EXPECT_EQ(opt() | clu::or_else(just_zero), 0);
    EXPECT_EQ(opt(1) | clu::or_else(just_zero), 1);
    EXPECT_ANY_THROW(opt() | clu::or_else(just_throw));
    EXPECT_NO_THROW(opt(1) | clu::or_else(just_throw));
}
