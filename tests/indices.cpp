#include "matchers.h"
#include <gtest/gtest.h>
#include <clu/indices.h>

namespace gtst = testing;

TEST(Indices, OneDimensional)
{
    using arr = std::array<size_t, 1>;
    const arr elems[]{ { 0 }, { 1 }, { 2 }, { 3 }, { 4 } };
    const auto idx = clu::indices(5);
    EXPECT_THAT(idx, gtst::ElementsAreArray(elems));
    EXPECT_EQ(idx.size(), 5);
    EXPECT_EQ(decltype(idx)::dimension(), 1);
    EXPECT_EQ(idx.extents(), arr{ 5 });
    EXPECT_TRUE(std::ranges::empty(clu::indices(0)));
}

TEST(Indices, MultiDimensional)
{
    using arr = std::array<size_t, 2>;
    const arr elems[]
    {
        { 0, 0 }, { 0, 1 }, { 0, 2 },
        { 1, 0 }, { 1, 1 }, { 1, 2 }
    };
    const auto idx = clu::indices(2, 3);
    EXPECT_THAT(idx, gtst::ElementsAreArray(elems));
    EXPECT_EQ(idx.size(), 6);
    EXPECT_EQ(decltype(idx)::dimension(), 2);
    EXPECT_EQ(idx.extents(), (arr{ 2, 3 }));
}

TEST(Indices, RandomAccess)
{
    using arr = std::array<size_t, 3>;
    const auto idx = clu::indices(3, 4, 5);
    EXPECT_EQ(idx.size(), 60);
    EXPECT_EQ(idx.begin()[29], (arr{ 1, 1, 4 }));
    EXPECT_EQ(*(idx.begin() + 40 - 23), (arr{ 0, 3, 2 }));
    EXPECT_GT(idx.begin() + 30, idx.begin() + 12);
    EXPECT_EQ((idx.begin() + 12) - (idx.begin() + 25), -13);
}
