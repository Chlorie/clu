#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "clu/take.h"

TEST(Take, Primitive)
{
    int x = 42;
    EXPECT_EQ(clu::take(x), 42);
    EXPECT_EQ(x, 0);

    int* ptr = &x;
    EXPECT_EQ(clu::take(ptr), &x);
    EXPECT_EQ(ptr, nullptr);
}

TEST(Take, NonPrimitive)
{
    auto ptr = std::make_unique<int>();
    int* value = ptr.get();
    EXPECT_EQ(clu::take(ptr).get(), value);
    EXPECT_EQ(ptr, nullptr);

    std::string str = "test";
    EXPECT_EQ(clu::take(str), "test");
    EXPECT_TRUE(str.empty());
}
