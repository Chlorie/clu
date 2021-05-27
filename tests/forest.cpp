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
    EXPECT_THAT(g.roots(), tst::elements_are(ilist{ 1, 2, 3 }));
    h = std::move(f);
    EXPECT_THAT(h.roots(), tst::elements_are(ilist{ 1, 2, 3 }));
}

TEST(Forest, EmplaceChild)
{
    clu::forest<int> f;
    EXPECT_TRUE(f.roots().empty());
    f.emplace_child_back(f.end(), 1);
    f.emplace_child_back(f.end(), 2);
    const auto roots = f.roots();
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
    EXPECT_THAT(f.roots(), tst::elements_are(ilist{ 1, 2 }));
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

TEST(Forest, InsertChildrenSibling)
{
    clu::forest<int> f;
    const auto root = f.emplace_child_back(f.end(), 1);
    f.insert_children_back(root, { 2, 3, 4 });
    f.insert_children_front(root, { 5, 6, 7 });
    const auto iter8 = f.insert_children_back(root, { 8, 9 });
    EXPECT_THAT(root.children(), tst::elements_are(ilist{ 5, 6, 7, 2, 3, 4, 8, 9 }));
    f.insert_siblings_after(iter8, { 10, 11 });
    f.insert_siblings_before(iter8, { 12 });
    EXPECT_THAT(root.children(), tst::elements_are(ilist{ 5, 6, 7, 2, 3, 4, 12, 8, 10, 11, 9 }));
}

TEST(Forest, Detach)
{
    clu::forest<int> f;
    const auto iter1 = f.insert_children_back(f.end(), { 1, 2 });
    const auto iter2 = iter1.next_sibling();
    f.insert_children_back(iter1, { 4, 5 });
    f.insert_children_back(iter2, { 4, 5 });
    const auto g = f.detach(iter1);
    EXPECT_THAT(f.roots(), tst::elements_are(ilist{ 2 }));
    EXPECT_THAT(g.roots(), tst::elements_are(ilist{ 1 }));
    EXPECT_THAT(g.begin().children(), tst::elements_are(ilist{ 4, 5 }));
    const auto h = f.detach_children(iter2);
    EXPECT_THAT(f.roots(), tst::elements_are(ilist{ 2 }));
    EXPECT_TRUE(iter2.is_leaf());
    EXPECT_THAT(h.roots(), tst::elements_are(ilist{ 4, 5 }));
}

TEST(Forest, CopyPart)
{
    const clu::forest<int> f = []
    {
        clu::forest<int> res;
        const auto iter1 = res.insert_children_back(res.end(), { 1, 2 });
        const auto iter2 = iter1.next_sibling();
        res.insert_children_back(iter1, { 4, 5 });
        res.insert_children_back(iter2, { 4, 5 });
        return res;
    }();

    const auto g = f.copy_subtree(f.begin());
    EXPECT_THAT(g.roots(), tst::elements_are(ilist{ 1 }));
    EXPECT_THAT(g.begin().children(), tst::elements_are(ilist{ 4, 5 }));
    const auto h = f.copy_children(f.begin().next_sibling());
    EXPECT_THAT(h.roots(), tst::elements_are(ilist{ 4, 5 }));
}

TEST(Forest, Attach)
{
    clu::forest<int> f;
    clu::forest<int> g;
    g.insert_children_back(g.end(), { 1, 2 });
    g.insert_children_back(g.begin(), { 3, 4 });
    f.attach_children_back(f.end(), g); // { 1: { 3, 4 }, 2 }
    f.attach_siblings_before(std::next(f.begin()), std::move(g)); // { 1: { 1: { 3, 4 }, 2, 3, 4 }, 2 }
    EXPECT_THAT(f, tst::elements_are(ilist{ 1, 1, 3, 4, 2, 3, 4, 2 }));
}
