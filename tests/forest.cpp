#include "matchers.h"
#include <gtest/gtest.h>
#include <clu/forest.h>

namespace tst = clu::testing;
using ilist = std::initializer_list<int>;

TEST(Forest, RuleOfFive)
{
    clu::forest<int> f;
    EXPECT_TRUE(f.empty());
    clu::forest<int> g = f;
    EXPECT_TRUE(g.empty());
    clu::forest<int> h = std::move(g);
    EXPECT_TRUE(h.empty());

    f.insert_children_back(f.end(), { 1, 2, 3 });
    g = f;
    EXPECT_THAT(g.end().children(), tst::elements_are(ilist{ 1, 2, 3 }));
    h = std::move(f);
    EXPECT_THAT(h.end().children(), tst::elements_are(ilist{ 1, 2, 3 }));
}

TEST(Forest, EmplaceChild)
{
    clu::forest<int> f;
    EXPECT_TRUE(f.end().children().empty());
    f.emplace_child_back(f.end(), 1);
    f.emplace_child_back(f.end(), 2);
    const auto roots = f.end().children();
    EXPECT_THAT(roots, tst::elements_are(ilist{ 1, 2 }));
    const auto iter1 = roots.begin();
    const auto iter2 = std::next(iter1);
    EXPECT_TRUE(f.is_root(iter1));
    EXPECT_TRUE(iter1.is_leaf());
    EXPECT_TRUE(f.is_root(iter2));
    EXPECT_TRUE(iter2.is_leaf());
    f.emplace_child_back(iter1, 3);
    f.emplace_child_front(iter1, 4);
    f.emplace_child_front(iter2, 5);
    EXPECT_THAT(f.end().children(), tst::elements_are(ilist{ 1, 2 }));
    EXPECT_THAT(iter1.children(), tst::elements_are(ilist{ 4, 3 }));
    EXPECT_THAT(iter2.children(), tst::elements_are(ilist{ 5 }));
    EXPECT_FALSE(iter1.is_leaf());
    EXPECT_TRUE(iter1.children().begin().is_leaf());
}

TEST(Forest, Traversal)
{
    clu::forest<int> f;
    const auto iter1 = f.insert_children_back(f.end(), { 1, 2, 3 });
    f.insert_children_back(iter1, { 4, 5 });
    const auto iter6 = f.insert_children_back(iter1.next_sibling(), { 6, 7, 8 });
    f.emplace_child_back(iter6.next_sibling(), 9);
    // { 1: { 4, 5 }, 2: { 6, 7: { 9 }, 8 }, 3 }
    EXPECT_THAT(f, tst::elements_are(ilist{ 1, 4, 5, 2, 6, 7, 9, 8, 3 }));
    EXPECT_THAT(f.full_order_traverse(), 
        tst::elements_are(ilist{ 1, 4, 4, 5, 5, 1, 2, 6, 6, 7, 9, 9, 7, 8, 8, 2, 3, 3 }));
}
