#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include "clu/forest.h"
#include "matchers.h"

namespace ct = clu::testing;

TEST_CASE("forest rule of five", "[forest]")
{
    clu::forest<int> f;
    REQUIRE(f.empty());
    clu::forest<int> g = f;
    REQUIRE(g.empty());
    clu::forest<int> h = std::move(g);
    REQUIRE(h.empty());

    f.insert_children_back(f.end(), {1, 2, 3});
    g = f;
    REQUIRE_THAT(g.roots(), ct::EqualsRange({1, 2, 3}));
    h = std::move(f);
    REQUIRE_THAT(h.roots(), ct::EqualsRange({1, 2, 3}));
}

TEST_CASE("forest emplace child", "[forest]")
{
    clu::forest<int> f;
    REQUIRE(f.roots().empty());
    f.emplace_child_back(f.end(), 1);
    f.emplace_child_back(f.end(), 2);
    const auto roots = f.roots();
    REQUIRE_THAT(roots, ct::EqualsRange({1, 2}));
    const auto iter1 = roots.begin();
    const auto iter2 = std::next(iter1);
    REQUIRE(f.is_root(iter1));
    REQUIRE(iter1.is_leaf());
    REQUIRE(f.is_root(iter2));
    REQUIRE(iter2.is_leaf());
    f.emplace_child_back(iter1, 3);
    f.emplace_child_front(iter1, 4);
    f.emplace_child_front(iter2, 5);
    REQUIRE_THAT(f.roots(), ct::EqualsRange({1, 2}));
    REQUIRE_THAT(iter1.children(), ct::EqualsRange({4, 3}));
    REQUIRE_THAT(iter2.children(), ct::EqualsRange({5}));
    REQUIRE_FALSE(iter1.is_leaf());
    REQUIRE(iter1.children().begin().is_leaf());
}

TEST_CASE("forest traversal", "[forest]")
{
    clu::forest<int> f;
    const auto iter1 = f.insert_children_back(f.end(), {1, 2, 3});
    f.insert_children_back(iter1, {4, 5});
    const auto iter6 = f.insert_children_back(iter1.next_sibling(), {6, 7, 8});
    f.emplace_child_back(iter6.next_sibling(), 9);
    // { 1: { 4, 5 }, 2: { 6, 7: { 9 }, 8 }, 3 }
    REQUIRE_THAT(f, ct::EqualsRange({1, 4, 5, 2, 6, 7, 9, 8, 3}));
    REQUIRE_THAT(f.full_order_traverse(), //
        ct::EqualsRange({1, 4, 4, 5, 5, 1, 2, 6, 6, 7, 9, 9, 7, 8, 8, 2, 3, 3}));
}

TEST_CASE("forest insert children/siblings", "[forest]")
{
    clu::forest<int> f;
    const auto root = f.emplace_child_back(f.end(), 1);
    f.insert_children_back(root, {2, 3, 4});
    f.insert_children_front(root, {5, 6, 7});
    const auto iter8 = f.insert_children_back(root, {8, 9});
    REQUIRE_THAT(root.children(), ct::EqualsRange({5, 6, 7, 2, 3, 4, 8, 9}));
    f.insert_siblings_after(iter8, {10, 11});
    f.insert_siblings_before(iter8, {12});
    REQUIRE_THAT(root.children(), ct::EqualsRange({5, 6, 7, 2, 3, 4, 12, 8, 10, 11, 9}));
}

TEST_CASE("forest detach", "[forest]")
{
    clu::forest<int> f;
    const auto iter1 = f.insert_children_back(f.end(), {1, 2});
    const auto iter2 = iter1.next_sibling();
    f.insert_children_back(iter1, {4, 5});
    f.insert_children_back(iter2, {4, 5});
    const auto g = f.detach(iter1);
    REQUIRE_THAT(f.roots(), ct::EqualsRange({2}));
    REQUIRE_THAT(g.roots(), ct::EqualsRange({1}));
    REQUIRE_THAT(g.begin().children(), ct::EqualsRange({4, 5}));
    const auto h = f.detach_children(iter2);
    REQUIRE_THAT(f.roots(), ct::EqualsRange({2}));
    REQUIRE(iter2.is_leaf());
    REQUIRE_THAT(h.roots(), ct::EqualsRange({4, 5}));
}

TEST_CASE("forest copy part", "[forest]")
{
    const clu::forest<int> f = []
    {
        clu::forest<int> res;
        const auto iter1 = res.insert_children_back(res.end(), {1, 2});
        const auto iter2 = iter1.next_sibling();
        res.insert_children_back(iter1, {4, 5});
        res.insert_children_back(iter2, {4, 5});
        return res;
    }();

    const auto g = f.copy_subtree(f.begin());
    REQUIRE_THAT(g.roots(), ct::EqualsRange({1}));
    REQUIRE_THAT(g.begin().children(), ct::EqualsRange({4, 5}));
    const auto h = f.copy_children(f.begin().next_sibling());
    REQUIRE_THAT(h.roots(), ct::EqualsRange({4, 5}));
}

TEST_CASE("forest attach", "[forest]")
{
    clu::forest<int> f;
    clu::forest<int> g;
    g.insert_children_back(g.end(), {1, 2});
    g.insert_children_back(g.begin(), {3, 4});
    f.attach_children_back(f.end(), g); // { 1: { 3, 4 }, 2 }
    f.attach_siblings_before(std::next(f.begin()), std::move(g)); // { 1: { 1: { 3, 4 }, 2, 3, 4 }, 2 }
    REQUIRE_THAT(f, ct::EqualsRange({1, 1, 3, 4, 2, 3, 4, 2}));
}
