#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>

#include <clu/quantifier.h>
#include <clu/flat_forest.h>
#include <clu/enumerate.h>
#include <clu/indices.h>

TEST(Quantifier, AllOf)
{
    EXPECT_TRUE(clu::all_of(0, 1) != 2);
    EXPECT_FALSE(clu::all_of(0, 1) != 1);
    EXPECT_TRUE(clu::all_of(0, 0) == 0);
    EXPECT_FALSE(clu::all_of(0, 1) == 0);
    EXPECT_TRUE(clu::all_of(0, 1.0f) != -0.5f);
    EXPECT_TRUE(clu::all_of(0, 1) < 1.5);
    EXPECT_FALSE(clu::all_of(0, 1) < -1.0);
    EXPECT_TRUE(2 > clu::all_of(0, 1));
    EXPECT_FALSE(clu::all_of(0, 1) > 0);
    EXPECT_TRUE(clu::all_of(0, 1) <= 1);
    EXPECT_TRUE(clu::all_of(0, 1) >= 0);
}

TEST(Quantifier, AnyOf)
{
    EXPECT_TRUE(clu::any_of(0, 1) == 0);
    EXPECT_TRUE(clu::any_of(0, 1) != 0);
    EXPECT_TRUE(clu::any_of(0, 1) == 1);
    EXPECT_FALSE(clu::any_of(0, 1) == 2);
    EXPECT_FALSE(clu::any_of(0, 0) != 0);
    EXPECT_TRUE(clu::any_of(0, 1.0f) == -0.0);
    EXPECT_TRUE(clu::any_of(0, 1) < 1);
    EXPECT_FALSE(clu::any_of(0, 1) < -1.0);
    EXPECT_TRUE(1 > clu::any_of(0, 1));
    EXPECT_TRUE(clu::any_of(0, 1) > 0);
    EXPECT_TRUE(clu::any_of(0, 1) <= 0.5f);
    EXPECT_TRUE(clu::any_of(0, 1) >= 0.0f);
}

TEST(Quantifier, NoneOf)
{
    EXPECT_FALSE(clu::none_of(0, 1) == 0);
    EXPECT_FALSE(clu::none_of(0, 1) != 0);
    EXPECT_FALSE(clu::none_of(0, 1) == 1);
    EXPECT_TRUE(clu::none_of(0, 1) == 2);
    EXPECT_TRUE(clu::none_of(0, 0) != 0);
    EXPECT_FALSE(clu::none_of(0, 1.0f) == -0.0);
    EXPECT_FALSE(clu::none_of(0, 1) < 1);
    EXPECT_TRUE(clu::none_of(0, 1) < -1.0);
    EXPECT_FALSE(1 > clu::none_of(0, 1));
    EXPECT_FALSE(clu::none_of(0, 1) > 0);
    EXPECT_FALSE(clu::none_of(0, 1) <= 0.5f);
    EXPECT_FALSE(clu::none_of(0, 1) >= 0.0f);
}

TEST(FlatForest, Construction)
{
    const clu::flat_forest<int> forest;
    EXPECT_TRUE(forest.empty());
    EXPECT_TRUE(forest.begin().is_sentinel());
    EXPECT_TRUE(forest.end().is_sentinel());
    EXPECT_EQ(forest.begin(), forest.end());
}

TEST(FlatForest, EmplaceChild)
{
    clu::flat_forest<int> forest;
    const auto first = forest.emplace_child(forest.end(), 1);
    EXPECT_THAT(forest, testing::ElementsAre(1));
    EXPECT_EQ(*first, 1);
    EXPECT_EQ(&first.forest(), &forest);
    EXPECT_TRUE(first.is_root());
    EXPECT_TRUE(first.is_leaf());
    EXPECT_TRUE(first.is_last_child_of_parent());
    const auto second = forest.emplace_child(first, 2);
    const auto third = forest.emplace_child(first, 3);
    EXPECT_EQ(second.parent(), first);
    EXPECT_EQ(third.parent(), first);
    EXPECT_FALSE(first.is_leaf());
    EXPECT_THAT(forest, testing::ElementsAre(1, 3, 2));
}

// TODO: More flat_forest tests needed
// TODO: enumerate tests needed

TEST(Indices, OneDimension)
{
    EXPECT_THAT(clu::indices(5), testing::ElementsAre(0, 1, 2, 3, 4));
    EXPECT_THAT(clu::indices(0), testing::ElementsAre());
}

TEST(Indices, MultiDimension)
{
    using index_type = std::array<size_t, 3>;
    std::array<index_type, 3 * 4 * 2> elements{};
    size_t index = 0;
    for (size_t i = 0; i < 3; i++)
        for (size_t j = 0; j < 4; j++)
            for (size_t k = 0; k < 2; k++)
                elements[index++] = index_type{ i, j, k };
    EXPECT_THAT(clu::indices(3, 4, 2), testing::ElementsAreArray(elements));
}
