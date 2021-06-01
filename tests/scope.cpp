#include <gtest/gtest.h>
#include <clu/scope.h>

TEST(Scope, OnSuccess)
{
    bool exit = false, success = false, fail = false;
    {
        clu::scope_exit se([&] { exit = true; });
        clu::scope_success ss([&] { success = true; });
        clu::scope_fail sf([&] { fail = true; });
    }
    EXPECT_TRUE(exit);
    EXPECT_TRUE(success);
    EXPECT_FALSE(fail);
}

TEST(Scope, OnException)
{
    bool exit = false, success = false, fail = false;
    try
    {
        clu::scope_exit se([&] { exit = true; });
        clu::scope_success ss([&] { success = true; });
        clu::scope_fail sf([&] { fail = true; });
    }
    catch (...)
    {
        EXPECT_TRUE(exit);
        EXPECT_FALSE(success);
        EXPECT_TRUE(fail);
    }
}
