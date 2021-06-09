#include <gtest/gtest.h>
#include <clu/pipeable.h>

const auto add2 = clu::make_pipeable([](const int a, const int b) noexcept { return a + b; });

TEST(Pipeable, NonOverloaded)
{
    EXPECT_EQ(add2(40, 2), 42);
    EXPECT_EQ(40 | add2(2), 42);
    EXPECT_EQ(1 | add2(2) | add2(3), 6);
}

template <typename T, typename U>
concept add2_pipeable = requires(T t, U u) { t | add2(u); };

TEST(Pipeable, SfinaeFriendly)
{
    struct S {};
    static_assert(std::is_nothrow_invocable_v<decltype(add2), int, int>);
    static_assert(add2_pipeable<int, int>);
    static_assert(!add2_pipeable<int, S>);
    static_assert(!add2_pipeable<S, int>);
}
