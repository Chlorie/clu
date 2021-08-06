#include <gtest/gtest.h>
#include <clu/expected.h>

TEST(Expected, DefCtor)
{
    const clu::expected<int, int> ex;
    EXPECT_TRUE(ex);
    EXPECT_EQ(*ex, 0);
    static_assert(std::is_trivially_copyable_v<clu::expected<int, int>>);
}

TEST(Expected, RefExpected)
{
    int val = 0;
    const clu::expected<int&, int> ex = val;
    EXPECT_TRUE(ex);
    EXPECT_EQ(*ex, 0);
    val = 1;
    EXPECT_EQ(*ex, 1);
}
