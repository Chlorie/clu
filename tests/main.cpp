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
    clu::flat_tree<int> tree(42);
    EXPECT_THAT(tree, testing::ElementsAre(42));
    const auto root = tree.root();
    EXPECT_EQ(*root, 42);
    EXPECT_EQ(&root.tree(), &tree);
    EXPECT_TRUE(root.is_root());
    EXPECT_TRUE(root.is_leaf());
    EXPECT_TRUE(root.is_last_child_of_parent());
}

TEST(FlatTree, Actions)
{
    clu::flat_tree<int> tree;
    const auto root = tree.root();
    const auto first_child = tree.add_child(root, 1);
    tree.add_child(root, 2);
    tree.add_child(first_child, 3);
    EXPECT_THAT(tree, testing::ElementsAre(0, 1, 3, 2));
    tree.prune(first_child);
    EXPECT_THAT(tree, testing::ElementsAre(0, 2));
    const auto fourth = tree.add_child(root.first_child(), 4);
    EXPECT_THAT(tree, testing::ElementsAre(0, 2, 4));
    tree.add_child(fourth, 5);
    tree.add_child(fourth, 6);
    tree.set_as_root(fourth);
    EXPECT_THAT(tree, testing::ElementsAre(4, 5, 6));
}
