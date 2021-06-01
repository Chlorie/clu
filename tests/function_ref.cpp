#include <gtest/gtest.h>
#include <clu/function_ref.h>

TEST(FunctionRef, Empty)
{
    const clu::function_ref<void()> fref;
    EXPECT_FALSE(fref);
}

void cfunc(bool& value) { value = true; }

TEST(FunctionRef, CFunc)
{
    const clu::function_ref fref = cfunc;
    EXPECT_TRUE(fref);
    bool value = false;
    fref(value);
    EXPECT_TRUE(value);
}

TEST(FunctionRef, Lambda)
{
    const auto lambda = [i = 40](const int value) { return i + value; };
    const clu::function_ref<int(int)> fref = lambda;
    EXPECT_EQ(fref(2), 42);
}
