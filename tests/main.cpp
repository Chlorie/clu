#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>

#include <clu/quantifier.h>
#include <clu/flat_tree.h>
#include <clu/enumerate.h>

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

TEST(FlatTree, Construction)
{
    clu::flat_tree<int> tree;
    EXPECT_THAT(tree.preorder_traverser(), testing::BeginEndDistanceIs(1));
    auto& root = tree.root();
    EXPECT_EQ(*root, 0);
    EXPECT_EQ(&root.tree(), &tree);
    EXPECT_TRUE(root.is_root());
    EXPECT_TRUE(root.is_leaf());
    EXPECT_TRUE(root.is_last_child_of_parent());
}

TEST(FlatTree, Insertion)
{
    clu::flat_tree<int> tree;
    auto& root = tree.root();
    root.add_child(1)
        .parent.add_child(2)
        .parent.first_child().add_child(3);
    for (auto& node : tree.preorder_traverser())
    {
        const int value = *node;
        std::cout << value << ' ';
    }
}
